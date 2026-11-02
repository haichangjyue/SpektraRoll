// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Spatial effects: grain and halation must be deterministic (same seed and
// frame give the same field) and must be a no-op at amount 0.
#include <cmath>
#include <cstdio>
#include <vector>

#include "sr/texture.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

std::vector<float> flat(int width, int height, float value) {
    return std::vector<float>(static_cast<size_t>(width) * height * 3, value);
}

bool identical(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

double meanDifference(const std::vector<float>& a, const std::vector<float>& b) {
    double total = 0.0;
    for (size_t i = 0; i < a.size(); ++i) total += std::fabs(a[i] - b[i]);
    return a.empty() ? 0.0 : total / static_cast<double>(a.size());
}

}  // namespace

int main() {
    constexpr int w = 64;
    constexpr int h = 48;

    // Zero amounts are a no-op.
    {
        std::vector<float> image = flat(w, h, 0.5f);
        sr::applyGrain(image.data(), w, h, sr::GrainParams{});
        check(identical(image, flat(w, h, 0.5f)), "grain 0 is a no-op");
        sr::HalationParams off;
        off.amount = 0.0f;
        sr::applyHalation(image.data(), w, h, off);
        check(identical(image, flat(w, h, 0.5f)), "halation 0 is a no-op");
    }

    // Grain is deterministic for a given seed and frame, and changes per frame.
    {
        sr::GrainParams params;
        params.amount = 0.5f;
        params.seed = 7;
        params.frame = 3;

        std::vector<float> a = flat(w, h, 0.45f);
        std::vector<float> b = flat(w, h, 0.45f);
        sr::applyGrain(a.data(), w, h, params);
        sr::applyGrain(b.data(), w, h, params);
        check(identical(a, b), "grain is deterministic for the same seed and frame");

        sr::GrainParams next = params;
        next.frame = 4;
        std::vector<float> c = flat(w, h, 0.45f);
        sr::applyGrain(c.data(), w, h, next);
        check(!identical(a, c), "grain changes with the frame");
        check(meanDifference(a, c) > 1e-4, "grain difference is measurable");

        // Stays inside the cube.
        bool inRange = true;
        for (float value : a) inRange = inRange && value >= 0.0f && value <= 1.0f;
        check(inRange, "grain stays in 0..1");
    }

    // Grain fades in the toe and shoulder compared with mid-grey.
    {
        sr::GrainParams params;
        params.amount = 0.6f;
        params.frame = 1;
        std::vector<float> mid = flat(w, h, 0.5f);
        std::vector<float> black = flat(w, h, 0.0f);
        sr::applyGrain(mid.data(), w, h, params);
        sr::applyGrain(black.data(), w, h, params);
        check(meanDifference(black, flat(w, h, 0.0f)) <
                  meanDifference(mid, flat(w, h, 0.5f)),
              "grain is weaker at black than at mid-grey");
    }

    // Physical sizing (D054): the same distance on the negative stays the same
    // physical size at every render resolution.
    {
        const float half = sr::spreadSigmaPixels(225.0f, 36.0f, 1080);
        const float full = sr::spreadSigmaPixels(225.0f, 36.0f, 2160);
        check(std::fabs(full - 2.0f * half) < 1e-3f, "spread scales with the frame width");
        check(std::fabs(full - 13.5f) < 0.01f, "225 um on 36 mm film is 13.5 px at 2160 wide");
        check(sr::spreadDownsampleFactor(sr::SpreadKind::Halation, 0.4f) == 1,
              "sub-pixel spreads are not downsampled");
        check(sr::spreadDownsampleFactor(sr::SpreadKind::Halation, 400.0f) > 1,
              "wide spreads are downsampled");

        const sr::SpreadKernel kernel = sr::buildSpreadKernel(sr::SpreadKind::Halation, 8.0f);
        check(kernel.radius == 53, "kernel is truncated at three sigma of its widest part");
        check(kernel.taps.size() == 107, "kernel has one tap per pixel in its radius");
        float sum = 0.0f;
        for (float tap : kernel.taps) sum += tap;
        check(std::fabs(sum - 1.0f) < 1e-4f, "kernel taps are normalised");
        bool symmetric = true;
        for (int i = 0; i < kernel.radius; ++i) {
            symmetric = symmetric &&
                        kernel.taps[static_cast<size_t>(i)] ==
                            kernel.taps[kernel.taps.size() - 1 - static_cast<size_t>(i)];
        }
        check(symmetric, "kernel is symmetric");

        // The skirt carries far more energy beyond three sigma than a plain
        // Gaussian (erfc(3 / sqrt(2)) ~= 0.0027), which is what makes the
        // spread read as film and glass instead of a generic blur.
        float tail = 0.0f;
        for (int i = 0; i < kernel.radius; ++i) {
            const float distance = static_cast<float>(kernel.radius - i);
            if (distance > 24.0f) tail += 2.0f * kernel.taps[static_cast<size_t>(i)];
        }
        check(tail > 0.01f, "the point spread has a heavy skirt");
    }

    // Halation blooms around a highlight and leaves the far corner alone.
    {
        constexpr int hw = 640;
        constexpr int hh = 480;
        std::vector<float> image = flat(hw, hh, 0.02f);
        const int cx = hw / 2;
        const int cy = hh / 2;
        for (int y = cy - 1; y <= cy + 1; ++y) {
            for (int x = cx - 1; x <= cx + 1; ++x) {
                float* px = &image[(static_cast<size_t>(y) * hw + x) * 3];
                px[0] = px[1] = px[2] = 1.0f;
            }
        }
        sr::HalationParams params;
        params.amount = 0.8f;
        params.spreadMicrons = 225.0f;
        params.threshold = 0.7f;
        std::vector<float> before = image;
        sr::applyHalation(image.data(), hw, hh, params);

        auto at = [&](int x, int y, int c) {
            return image[(static_cast<size_t>(y) * hw + x) * 3 + static_cast<size_t>(c)];
        };
        check(at(cx + 6, cy, 0) > 0.02f, "halation brightens red near the highlight");
        check(at(cx + 6, cy, 0) > at(cx + 6, cy, 2), "halation is warm");
        check(std::fabs(at(1, 1, 0) - 0.02f) < 1e-4f, "far corner is untouched");
        check(!identical(image, before), "halation changes the image");

        // The box model stays available and is a different kernel.
        std::vector<float> boxed = before;
        sr::SpreadContext box;
        box.model = sr::SpreadModel::Box;
        sr::applyHalation(boxed.data(), hw, hh, params, box);
        check(!identical(boxed, image), "the box model still blooms");
    }

    // Temporal coherence: a cross-faded field changes much less between frames
    // than a brand new one, which is what stops video grain from strobing.
    {
        sr::GrainParams fast;
        fast.amount = 0.5f;
        fast.rate = 1;
        sr::GrainParams slow = fast;
        slow.rate = 4;

        auto frames = [&](sr::GrainParams params) {
            std::vector<float> first = flat(w, h, 0.5f);
            std::vector<float> second = flat(w, h, 0.5f);
            params.frame = 5;
            sr::applyGrain(first.data(), w, h, params);
            params.frame = 6;
            sr::applyGrain(second.data(), w, h, params);
            return meanDifference(first, second);
        };

        const double fastDelta = frames(fast);
        const double slowDelta = frames(slow);
        check(slowDelta < fastDelta * 0.6, "cross-faded grain changes less per frame");
        check(slowDelta > 1e-6, "cross-faded grain still moves");
    }

    // The field cache must reproduce the uncached pixels bit for bit while
    // generating one new field per field index instead of two per frame (D039).
    {
        sr::GrainParams params;
        params.amount = 0.5f;
        params.seed = 11;
        params.rate = 3;

        sr::GrainFieldCache cache;
        for (int frame = 0; frame < 9; ++frame) {
            params.frame = frame;
            std::vector<float> uncached = flat(w, h, 0.5f);
            std::vector<float> cached = uncached;
            sr::applyGrain(uncached.data(), w, h, params);
            sr::applyGrain(cached.data(), w, h, params, &cache);
            check(identical(uncached, cached), "cached grain matches the uncached path");
        }
        // Frames 0..8 with rate 3 use field indices 0..3, so four builds happen
        // instead of the 18 the uncached path performs.
        check(cache.builds() == 4, "grain cache builds one field per field index");

        // A different seed must not reuse the old fields.
        params.seed = 12;
        params.frame = 0;
        std::vector<float> uncached = flat(w, h, 0.5f);
        std::vector<float> cached = uncached;
        sr::applyGrain(uncached.data(), w, h, params);
        sr::applyGrain(cached.data(), w, h, params, &cache);
        check(identical(uncached, cached), "cached grain matches after a seed change");
        check(cache.builds() == 6, "grain cache rebuilds for a new seed");
    }

    // Glare lifts the blacks with a wide veil of light; diffusion softens.
    {
        constexpr int gw = 640;
        constexpr int gh = 480;
        std::vector<float> image = flat(gw, gh, 0.05f);
        float* corner = &image[0];
        corner[0] = corner[1] = corner[2] = 1.0f;  // one bright pixel
        sr::GlareParams glare;
        glare.amount = 0.5f;
        glare.spreadMicrons = 725.0f;
        std::vector<float> before = image;
        sr::applyGlare(image.data(), gw, gh, glare);
        check(!identical(image, before), "glare changes the image");
        const float near = image[(static_cast<size_t>(20) * gw + 20) * 3];
        check(near > 0.05f, "glare lifts the area around the highlight");
        const float far = image[(static_cast<size_t>(gh - 1) * gw + (gw - 1)) * 3];
        check(far > 0.05f - 1e-4f, "glare does not darken the far corner");
        check(far < 0.5f, "glare stays subtle");

        std::vector<float> smooth = flat(w, h, 0.05f);
        sr::DiffusionParams diffusion;
        diffusion.amount = 0.0f;
        sr::applyDiffusion(smooth.data(), w, h, diffusion);
        check(identical(smooth, flat(w, h, 0.05f)), "diffusion 0 is a no-op");

        diffusion.amount = 0.8f;
        diffusion.spreadMicrons = 122.0f;
        std::vector<float> edge = flat(gw, gh, 0.0f);
        for (int y = 0; y < gh; ++y) {
            for (int x = gw / 2; x < gw; ++x) {
                float* px = &edge[(static_cast<size_t>(y) * gw + x) * 3];
                px[0] = px[1] = px[2] = 1.0f;
            }
        }
        std::vector<float> edgeBefore = edge;
        sr::applyDiffusion(edge.data(), gw, gh, diffusion);
        const float leftOfEdge = edge[(static_cast<size_t>(gh / 2) * gw + (gw / 2 - 2)) * 3];
        check(leftOfEdge > 0.0f, "diffusion bleeds across the edge");
        check(edge[(static_cast<size_t>(gh / 2) * gw + (gw / 2 + 2)) * 3] < 1.0f,
              "diffusion softens the bright side too");
        check(!identical(edge, edgeBefore), "diffusion changes the image");
    }

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
