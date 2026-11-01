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

void applyHalation(float* pixels, int width, int height, const HalationParams& params) {
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

    const int radius =
        std::max(1, static_cast<int>(std::lround(params.radius * static_cast<float>(width))));
    std::vector<float> scratch(count);
    blurWide(glow, width, height, radius, 2);

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

void applyGlare(float* pixels, int width, int height, const GlareParams& params) {
    if (params.amount <= 0.0f || width <= 0 || height <= 0) return;

    const size_t count = static_cast<size_t>(width) * height;
    const int radius =
        std::max(1, static_cast<int>(std::lround(params.radius * static_cast<float>(width))));
    std::vector<float> veil(count);
    std::vector<float> scratch(count);
    for (size_t i = 0; i < count; ++i) {
        const float* px = &pixels[i * 3];
        veil[i] = 0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2];
    }
    // Veiling glare in a lens is essentially neutral, so one wide blur of the
    // luminance is enough; a slight warm tint keeps it from looking digital.
    blurWide(veil, width, height, radius, 4);

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

void applyDiffusion(float* pixels, int width, int height, const DiffusionParams& params) {
    if (params.amount <= 0.0f || width <= 0 || height <= 0) return;

    const size_t count = static_cast<size_t>(width) * height;
    const int radius =
        std::max(1, static_cast<int>(std::lround(params.radius * static_cast<float>(width))));
    std::vector<float> blurred(count);
    std::vector<float> scratch(count);
    for (size_t i = 0; i < count; ++i) {
        const float* px = &pixels[i * 3];
        blurred[i] = 0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2];
    }
    blurWide(blurred, width, height, radius, 2);

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

}  // namespace sr
