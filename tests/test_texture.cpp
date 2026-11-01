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

    // Halation blooms around a highlight and leaves the far corner alone.
    {
        std::vector<float> image = flat(w, h, 0.02f);
        const int cx = w / 2;
        const int cy = h / 2;
        for (int y = cy - 1; y <= cy + 1; ++y) {
            for (int x = cx - 1; x <= cx + 1; ++x) {
                float* px = &image[(static_cast<size_t>(y) * w + x) * 3];
                px[0] = px[1] = px[2] = 1.0f;
            }
        }
        sr::HalationParams params;
        params.amount = 0.8f;
        params.radius = 0.05f;
        params.threshold = 0.7f;
        std::vector<float> before = image;
        sr::applyHalation(image.data(), w, h, params);

        auto at = [&](int x, int y, int c) {
            return image[(static_cast<size_t>(y) * w + x) * 3 + static_cast<size_t>(c)];
        };
        check(at(cx + 6, cy, 0) > 0.02f, "halation brightens red near the highlight");
        check(at(cx + 6, cy, 0) > at(cx + 6, cy, 2), "halation is warm");
        check(std::fabs(at(1, 1, 0) - 0.02f) < 1e-4f, "far corner is untouched");
        check(!identical(image, before), "halation changes the image");
    }

    // Glare lifts the blacks with a wide veil of light; diffusion softens.
    {
        std::vector<float> image = flat(w, h, 0.05f);
        float* corner = &image[0];
        corner[0] = corner[1] = corner[2] = 1.0f;  // one bright pixel
        sr::GlareParams glare;
        glare.amount = 0.5f;
        glare.radius = 0.3f;
        std::vector<float> before = image;
        sr::applyGlare(image.data(), w, h, glare);
        check(!identical(image, before), "glare changes the image");
        const float far = image[(static_cast<size_t>(h - 1) * w + (w - 1)) * 3];
        check(far > 0.05f, "glare lifts the far corner");
        check(far < 0.5f, "glare stays subtle");

        std::vector<float> smooth = flat(w, h, 0.05f);
        sr::DiffusionParams diffusion;
        diffusion.amount = 0.0f;
        sr::applyDiffusion(smooth.data(), w, h, diffusion);
        check(identical(smooth, flat(w, h, 0.05f)), "diffusion 0 is a no-op");

        diffusion.amount = 0.8f;
        diffusion.radius = 0.1f;
        std::vector<float> edge = flat(w, h, 0.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = w / 2; x < w; ++x) {
                float* px = &edge[(static_cast<size_t>(y) * w + x) * 3];
                px[0] = px[1] = px[2] = 1.0f;
            }
        }
        std::vector<float> edgeBefore = edge;
        sr::applyDiffusion(edge.data(), w, h, diffusion);
        const float leftOfEdge = edge[(static_cast<size_t>(h / 2) * w + (w / 2 - 2)) * 3];
        check(leftOfEdge > 0.0f, "diffusion bleeds across the edge");
        check(edge[(static_cast<size_t>(h / 2) * w + (w / 2 + 2)) * 3] < 1.0f,
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
