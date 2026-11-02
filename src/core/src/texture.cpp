// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include "sr/texture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "sr/parallel.hpp"

namespace sr {
namespace {

// Cheap integer hash (Murmur3-style finaliser): deterministic across platforms
// and threads, which is what the grain seed promises.
inline std::uint32_t hash32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

inline std::uint32_t mix(std::uint32_t a, std::uint32_t b) {
    return hash32(a ^ (b + 0x9e3779b9U + (a << 6) + (a >> 2)));
}

// Signed noise in [-1, 1].
inline float noiseAt(std::uint32_t seed, std::uint32_t frame, int x, int y) {
    const std::uint32_t h =
        mix(mix(seed, frame), mix(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)));
    return static_cast<float>(h) / 2147483648.0f - 1.0f;
}

// Separable box blur, run three times to approximate a Gaussian. O(n) per pass
// thanks to the running sum, regardless of radius.
void boxBlur(std::vector<float>& buffer, std::vector<float>& scratch, int width, int height,
             int radius) {
    if (radius < 1) return;
    const int window = radius * 2 + 1;

    // Horizontal.
    for (int y = 0; y < height; ++y) {
        const float* src = &buffer[static_cast<size_t>(y) * width];
        float* dst = &scratch[static_cast<size_t>(y) * width];
        float sum = 0.0f;
        for (int x = -radius; x <= radius; ++x) {
            sum += src[std::clamp(x, 0, width - 1)];
        }
        for (int x = 0; x < width; ++x) {
            dst[x] = sum / static_cast<float>(window);
            sum -= src[std::clamp(x - radius, 0, width - 1)];
            sum += src[std::clamp(x + radius + 1, 0, width - 1)];
        }
    }

    // Vertical.
    for (int x = 0; x < width; ++x) {
        float sum = 0.0f;
        for (int y = -radius; y <= radius; ++y) {
            sum += scratch[static_cast<size_t>(std::clamp(y, 0, height - 1)) * width + x];
        }
        for (int y = 0; y < height; ++y) {
            buffer[static_cast<size_t>(y) * width + x] = sum / static_cast<float>(window);
            const int outRow = std::clamp(y - radius, 0, height - 1);
            const int inRow = std::clamp(y + radius + 1, 0, height - 1);
            sum -= scratch[static_cast<size_t>(outRow) * width + x];
            sum += scratch[static_cast<size_t>(inRow) * width + x];
        }
    }
}

// Wide blurs do not need full resolution: downsample, blur small, upsample.
// A 4x reduction cuts the blur work by ~16x and looks identical for radii
// above ~8 px, which is where glare, halation and diffusion live (D037).
void blurWide(std::vector<float>& buffer, int width, int height, int radius, int factor) {
    if (radius < 1) return;
    factor = std::min(std::max(factor, 1), std::min(width, height));
    if (factor <= 1 || radius < factor * 2) {
        std::vector<float> scratch(static_cast<size_t>(width) * height);
        for (int pass = 0; pass < 3; ++pass) {
            boxBlur(buffer, scratch, width, height, radius);
        }
        return;
    }

    const int lowWidth = (width + factor - 1) / factor;
    const int lowHeight = (height + factor - 1) / factor;
    std::vector<float> low(static_cast<size_t>(lowWidth) * lowHeight, 0.0f);
    const float inv = 1.0f / static_cast<float>(factor * factor);
    for (int y = 0; y < height; ++y) {
        const int ly = y / factor;
        for (int x = 0; x < width; ++x) {
            low[static_cast<size_t>(ly) * lowWidth + x / factor] +=
                buffer[static_cast<size_t>(y) * width + x] * inv;
        }
    }

    std::vector<float> scratch(low.size());
    for (int pass = 0; pass < 2; ++pass) {
        boxBlur(low, scratch, lowWidth, lowHeight, std::max(1, radius / factor));
    }

    for (int y = 0; y < height; ++y) {
        const float fy = (static_cast<float>(y) + 0.5f) / static_cast<float>(factor) - 0.5f;
        const int y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, lowHeight - 1);
        const int y1 = std::min(lowHeight - 1, y0 + 1);
        const float ty = std::clamp(fy - static_cast<float>(y0), 0.0f, 1.0f);
        for (int x = 0; x < width; ++x) {
            const float fx = (static_cast<float>(x) + 0.5f) / static_cast<float>(factor) - 0.5f;
            const int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, lowWidth - 1);
            const int x1 = std::min(lowWidth - 1, x0 + 1);
            const float tx = std::clamp(fx - static_cast<float>(x0), 0.0f, 1.0f);
            const float top = low[static_cast<size_t>(y0) * lowWidth + x0] * (1.0f - tx) +
                              low[static_cast<size_t>(y0) * lowWidth + x1] * tx;
            const float bottom = low[static_cast<size_t>(y1) * lowWidth + x0] * (1.0f - tx) +
                                 low[static_cast<size_t>(y1) * lowWidth + x1] * tx;
            buffer[static_cast<size_t>(y) * width + x] = top * (1.0f - ty) + bottom * ty;
        }
    }
}

// Separable convolution with a symmetric kernel. The summation order is
// left-to-right (then top-to-bottom) on every thread because the D3D11 pass
// does exactly the same thing; keeping the order identical keeps the two
// backends within a few ULP (D047 parity, D054 kernels).
void convolveSeparable(std::vector<float>& buffer, std::vector<float>& scratch, int width,
                       int height, const SpreadKernel& kernel) {
    const int radius = kernel.radius;
    if (radius < 1 || static_cast<int>(kernel.taps.size()) != radius * 2 + 1) return;

    for (int y = 0; y < height; ++y) {
        const float* src = &buffer[static_cast<size_t>(y) * width];
        float* dst = &scratch[static_cast<size_t>(y) * width];
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            for (int k = -radius; k <= radius; ++k) {
                sum += kernel.taps[static_cast<size_t>(k + radius)] *
                       src[std::clamp(x + k, 0, width - 1)];
            }
            dst[x] = sum;
        }
    }

    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            float sum = 0.0f;
            for (int k = -radius; k <= radius; ++k) {
                sum += kernel.taps[static_cast<size_t>(k + radius)] *
                       scratch[static_cast<size_t>(std::clamp(y + k, 0, height - 1)) * width + x];
            }
            buffer[static_cast<size_t>(y) * width + x] = sum;
        }
    }
}

// Physical spread (D054): same downsample/blur/upsample structure as the box
// chain, but with the PSF built for this sigma. The kernel is rebuilt at the
// low resolution so its physical width stays the same after the reduction.
void spreadWide(std::vector<float>& buffer, int width, int height, SpreadKind kind,
                float sigmaPx) {
    const int factor = spreadDownsampleFactor(kind, sigmaPx);
    const int radiusFull = spreadKernelRadius(kind, sigmaPx);
    if (radiusFull < 1) return;

    std::vector<float> scratch(buffer.size());
    if (factor <= 1 || radiusFull < factor * 2) {
        convolveSeparable(buffer, scratch, width, height, buildSpreadKernel(kind, sigmaPx));
        return;
    }

    const int lowWidth = (width + factor - 1) / factor;
    const int lowHeight = (height + factor - 1) / factor;
    std::vector<float> low(static_cast<size_t>(lowWidth) * lowHeight, 0.0f);
    const float inv = 1.0f / static_cast<float>(factor * factor);
    for (int y = 0; y < height; ++y) {
        const int ly = y / factor;
        for (int x = 0; x < width; ++x) {
            low[static_cast<size_t>(ly) * lowWidth + x / factor] +=
                buffer[static_cast<size_t>(y) * width + x] * inv;
        }
    }

    std::vector<float> lowScratch(low.size());
    convolveSeparable(low, lowScratch, lowWidth, lowHeight,
                      buildSpreadKernel(kind, sigmaPx / static_cast<float>(factor)));

    for (int y = 0; y < height; ++y) {
        const float fy = (static_cast<float>(y) + 0.5f) / static_cast<float>(factor) - 0.5f;
        const int y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, lowHeight - 1);
        const int y1 = std::min(lowHeight - 1, y0 + 1);
        const float ty = std::clamp(fy - static_cast<float>(y0), 0.0f, 1.0f);
        for (int x = 0; x < width; ++x) {
            const float fx = (static_cast<float>(x) + 0.5f) / static_cast<float>(factor) - 0.5f;
            const int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, lowWidth - 1);
            const int x1 = std::min(lowWidth - 1, x0 + 1);
            const float tx = std::clamp(fx - static_cast<float>(x0), 0.0f, 1.0f);
            const float top = low[static_cast<size_t>(y0) * lowWidth + x0] * (1.0f - tx) +
                              low[static_cast<size_t>(y0) * lowWidth + x1] * tx;
            const float bottom = low[static_cast<size_t>(y1) * lowWidth + x0] * (1.0f - tx) +
                                 low[static_cast<size_t>(y1) * lowWidth + x1] * tx;
            buffer[static_cast<size_t>(y) * width + x] = top * (1.0f - ty) + bottom * ty;
        }
    }
}

// One blurred noise field. The grain is generated at full resolution and then
// softened twice with the box blur, which is what turns per-pixel noise into
// clumps that read as film grain rather than digital hash.
std::vector<float> buildGrainField(unsigned seed, int width, int height, int radius,
                                   std::uint32_t index) {
    const size_t count = static_cast<size_t>(width) * height;
    std::vector<float> field(count);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            field[static_cast<size_t>(y) * width + x] = noiseAt(seed, index, x, y);
        }
    }
    std::vector<float> scratch(count);
    for (int pass = 0; pass < 2; ++pass) boxBlur(field, scratch, width, height, radius);
    return field;
}

}  // namespace

float spreadSigmaPixels(float microns, float frameWidthMm, int width) {
    if (!(microns > 0.0f) || width <= 0) return 0.0f;
    const float widthMm = frameWidthMm > 0.0f ? frameWidthMm : 36.0f;
    return microns * 0.001f * static_cast<float>(width) / widthMm;
}

int spreadKernelRadius(SpreadKind kind, float sigmaPx) {
    if (!(sigmaPx > 0.5f)) return 0;
    // The widest component decides the truncation, so the skirt is not cut off
    // at three effective sigma.
    float widest = 1.0f;
    if (kind == SpreadKind::Halation) widest = 2.2f;
    if (kind == SpreadKind::Glare) widest = 2.8f;
    if (kind == SpreadKind::Diffusion) widest = 1.8f;
    return static_cast<int>(std::ceil(3.0f * widest * sigmaPx));
}

int spreadDownsampleFactor(SpreadKind kind, float sigmaPx) {
    if (!(sigmaPx > 0.5f)) return 1;
    // About 16 taps per side at the reduced resolution: cheap, and the kernel
    // is far wider than the pixel it samples.
    constexpr float kTargetRadius = 16.0f;
    int factor = static_cast<int>(
        std::ceil(static_cast<float>(spreadKernelRadius(kind, sigmaPx)) / kTargetRadius));
    factor = std::clamp(factor, 1, 8);
    // The D3D11 kernel buffer holds 132 taps; never build a wider kernel.
    while (factor < 32 && spreadKernelRadius(kind, sigmaPx / static_cast<float>(factor)) > 120) {
        ++factor;
    }
    return factor;
}

SpreadKernel buildSpreadKernel(SpreadKind kind, float sigmaPx) {
    if (!(sigmaPx > 0.5f)) return SpreadKernel{{1.0f}, 0};

    // Weights and relative sigmas. Normalising by the mixture's effective
    // sigma keeps the requested distance meaningful, while the wide, light
    // component carries the skirt: beyond three sigma this kernel keeps 7-10x
    // the energy of a single Gaussian with the same total variance, which is
    // what reads as halation through the base, veiling glare in the lens and
    // the long bloom of a diffusion filter (D054).
    struct Component {
        float weight;
        float scale;
    };
    static const Component kHalation[] = {{0.88f, 0.69f}, {0.12f, 2.20f}};
    static const Component kGlare[] = {{0.90f, 0.49f}, {0.10f, 2.80f}};
    static const Component kDiffusion[] = {{0.82f, 0.713f}, {0.18f, 1.80f}};

    const Component* parts = kHalation;
    int count = 2;
    if (kind == SpreadKind::Glare) parts = kGlare;
    if (kind == SpreadKind::Diffusion) parts = kDiffusion;

    float variance = 0.0f;
    for (int i = 0; i < count; ++i) {
        variance += parts[i].weight * parts[i].scale * parts[i].scale;
    }
    const float normalise = variance > 0.0f ? 1.0f / std::sqrt(variance) : 1.0f;

    const int radius = spreadKernelRadius(kind, sigmaPx);
    SpreadKernel kernel;
    kernel.radius = radius;
    kernel.taps.assign(static_cast<size_t>(radius) * 2 + 1, 0.0f);

    constexpr float kInvSqrtTwoPi = 0.39894228040143267794f;
    for (int i = 0; i < count; ++i) {
        const float sigma = sigmaPx * parts[i].scale * normalise;
        if (!(sigma > 0.0f)) continue;
        const float gain = parts[i].weight * kInvSqrtTwoPi / sigma;
        for (int x = -radius; x <= radius; ++x) {
            const float t = static_cast<float>(x) / sigma;
            kernel.taps[static_cast<size_t>(x + radius)] += gain * std::exp(-0.5f * t * t);
        }
    }

    float total = 0.0f;
    for (float tap : kernel.taps) total += tap;
    if (!(total > 0.0f)) return SpreadKernel{{1.0f}, 0};
    for (float& tap : kernel.taps) tap /= total;
    return kernel;
}

const std::vector<float>& GrainFieldCache::field(unsigned seed, int width, int height, int radius,
                                                 std::uint32_t index) {
    for (Entry& entry : entries_) {
        if (entry.valid && entry.seed == seed && entry.width == width && entry.height == height &&
            entry.radius == radius && entry.index == index) {
            entry.lastUse = ++clock_;
            return entry.data;
        }
    }

    // Evict the least recently used entry. The most recently used one is the
    // field the caller asked for a moment ago (the cross-fade holds on to it),
    // so it is never the victim.
    Entry* victim = nullptr;
    for (Entry& entry : entries_) {
        if (!entry.valid) {
            victim = &entry;
            break;
        }
        if (victim == nullptr || entry.lastUse < victim->lastUse) victim = &entry;
    }

    victim->valid = true;
    victim->seed = seed;
    victim->width = width;
    victim->height = height;
    victim->radius = radius;
    victim->index = index;
    victim->lastUse = ++clock_;
    victim->data = buildGrainField(seed, width, height, radius, index);
    ++builds_;
    return victim->data;
}

void applyHalation(float* pixels, int width, int height, const HalationParams& params,
                   const SpreadContext& spread) {
    if (params.amount <= 0.0f || width <= 0 || height <= 0) return;

    const size_t count = static_cast<size_t>(width) * height;
    std::vector<float> glow(count);
    const float threshold = std::min(0.99f, std::max(0.0f, params.threshold));
    const float span = std::max(1e-3f, 1.0f - threshold);
    for (size_t i = 0; i < count; ++i) {
        const float* px = &pixels[i * 3];
        const float luma = 0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2];
        const float excess = std::min(1.0f, std::max(0.0f, (luma - threshold) / span));
        glow[i] = excess * excess;  // soft knee, only real highlights bloom
    }

    if (spread.model == SpreadModel::Box) {
        const int radius =
            std::max(1, static_cast<int>(std::lround(params.radius * static_cast<float>(width))));
        blurWide(glow, width, height, radius, 2);
    } else {
        spreadWide(glow, width, height, SpreadKind::Halation,
                   spreadSigmaPixels(params.spreadMicrons, spread.frameWidthMm, width));
    }

    // Warm bleed: red scatters most, blue least, like the antihalation layer
    // failing to catch everything.
    const float gain = params.amount * 0.55f;
    const float tint[3] = {1.0f, 0.42f, 0.18f};
    sr::parallelChunks(count, resolveThreadCount(0), [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            float* px = &pixels[i * 3];
            const float amount = glow[i] * gain;
            for (int c = 0; c < 3; ++c) {
                px[c] = std::min(1.0f, px[c] + amount * tint[c]);
            }
        }
    });
}

void applyGlare(float* pixels, int width, int height, const GlareParams& params,
                const SpreadContext& spread) {
    if (params.amount <= 0.0f || width <= 0 || height <= 0) return;

    const size_t count = static_cast<size_t>(width) * height;
    std::vector<float> veil(count);
    for (size_t i = 0; i < count; ++i) {
        const float* px = &pixels[i * 3];
        veil[i] = 0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2];
    }
    // Veiling glare in a lens is essentially neutral, so one wide blur of the
    // luminance is enough; a slight warm tint keeps it from looking digital.
    if (spread.model == SpreadModel::Box) {
        const int radius =
            std::max(1, static_cast<int>(std::lround(params.radius * static_cast<float>(width))));
        blurWide(veil, width, height, radius, 4);
    } else {
        spreadWide(veil, width, height, SpreadKind::Glare,
                   spreadSigmaPixels(params.spreadMicrons, spread.frameWidthMm, width));
    }

    const float amount = std::min(1.0f, params.amount);
    const float tint[3] = {1.0f, 0.96f, 0.92f};
    sr::parallelChunks(count, resolveThreadCount(0), [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            float* px = &pixels[i * 3];
            for (int c = 0; c < 3; ++c) {
                const float veiling = amount * veil[i] * tint[c];
                // Screen blend: light adds, blacks lift, highlights barely move.
                px[c] = std::min(1.0f, 1.0f - (1.0f - px[c]) * (1.0f - veiling));
            }
        }
    });
}

void applyDiffusion(float* pixels, int width, int height, const DiffusionParams& params,
                    const SpreadContext& spread) {
    if (params.amount <= 0.0f || width <= 0 || height <= 0) return;

    const size_t count = static_cast<size_t>(width) * height;
    std::vector<float> blurred(count);
    for (size_t i = 0; i < count; ++i) {
        const float* px = &pixels[i * 3];
        blurred[i] = 0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2];
    }
    if (spread.model == SpreadModel::Box) {
        const int radius =
            std::max(1, static_cast<int>(std::lround(params.radius * static_cast<float>(width))));
        blurWide(blurred, width, height, radius, 2);
    } else {
        spreadWide(blurred, width, height, SpreadKind::Diffusion,
                   spreadSigmaPixels(params.spreadMicrons, spread.frameWidthMm, width));
    }

    // Add the local luminance difference back to every channel: bright areas
    // soften and dark areas next to them pick up a little light, while hue is
    // untouched because the same offset goes to R, G and B.
    const float amount = std::min(1.0f, params.amount);
    sr::parallelChunks(count, resolveThreadCount(0), [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            float* px = &pixels[i * 3];
            const float luma = 0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2];
            const float offset = (blurred[i] - luma) * amount;
            for (int c = 0; c < 3; ++c) {
                px[c] = std::min(1.0f, std::max(0.0f, px[c] + offset));
            }
        }
    });
}

void applyGrain(float* pixels, int width, int height, const GrainParams& params,
                GrainFieldCache* cache) {
    if (params.amount <= 0.0f || width <= 0 || height <= 0) return;

    const size_t count = static_cast<size_t>(width) * height;
    const int radius = std::max(0, static_cast<int>(std::lround(params.size)));
    const std::uint32_t seed = params.seed;
    const int frame = std::max(0, params.frame);
    const int rate = std::max(1, params.rate);

    // Cross-fade two independent fields over `rate` frames. rate = 1 is a fresh
    // field every frame, which reads as digital boiling at 24 fps (D036).
    const std::uint32_t fieldIndex = static_cast<std::uint32_t>(frame / rate);
    const float phase =
        rate > 1 ? static_cast<float>(frame % rate) / static_cast<float>(rate) : 0.0f;

    // Two independent fields mixed 50/50 would lose ~30% of their contrast, so
    // normalise the blend to keep the grain level steady.
    const float norm = 1.0f / std::sqrt((1.0f - phase) * (1.0f - phase) + phase * phase);

    // The cache keeps the fields between frames (D039). Without one, build the
    // same two fields locally so single-image results stay identical.
    std::vector<float> localA;
    std::vector<float> localB;
    const float* fieldA = nullptr;
    const float* fieldB = nullptr;
    if (cache != nullptr) {
        fieldA = cache->field(seed, width, height, radius, fieldIndex).data();
        if (rate > 1) {
            fieldB = cache->field(seed, width, height, radius, fieldIndex + 1).data();
        }
    } else {
        localA = buildGrainField(seed, width, height, radius, fieldIndex);
        fieldA = localA.data();
        if (rate > 1) {
            localB = buildGrainField(seed, width, height, radius, fieldIndex + 1);
            fieldB = localB.data();
        }
    }

    const float scale = params.amount * 0.14f;
    sr::parallelChunks(count, resolveThreadCount(0), [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            float* px = &pixels[i * 3];
            const float luma = 0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2];
            // Film grain is strongest in the mid-tones and fades in the toe and
            // shoulder, so weight by distance from 0.5.
            const float weight = 1.0f - std::fabs(2.0f * luma - 1.0f);
            const float noise = rate > 1
                                    ? (fieldA[i] * (1.0f - phase) + fieldB[i] * phase) * norm
                                    : fieldA[i];
            const float delta = noise * scale * weight;
            for (int c = 0; c < 3; ++c) {
                px[c] = std::min(1.0f, std::max(0.0f, px[c] + delta));
            }
        }
    });
}

void applyTexture(float* pixels, int width, int height, const TextureParams& params, int frame,
                  GrainFieldCache* cache) {
    if (params.halation.amount > 0.0f)
        applyHalation(pixels, width, height, params.halation, params.spread);
    if (params.glare.amount > 0.0f) applyGlare(pixels, width, height, params.glare, params.spread);
    if (params.diffusion.amount > 0.0f)
        applyDiffusion(pixels, width, height, params.diffusion, params.spread);
    if (params.grain.amount > 0.0f) {
        GrainParams grain = params.grain;
        grain.frame = frame;
        applyGrain(pixels, width, height, grain, cache);
    }
}

}  // namespace sr
