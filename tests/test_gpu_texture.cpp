// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Parity test for the D3D11 texture backend (D047). sr::applyTexture stays the
// reference; every case runs the same frame through both chains and compares
// them pixel by pixel. No D3D11 device -> exit 77, which ctest reports as
// skipped (same contract as test_gpu_film).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "sr/texture.hpp"

#include "../src/cli/gpu_texture.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

// A frame with a grey ramp, bright spots (halation/glare/diffusion) and
// saturated patches. Deterministic, so a hash mismatch shows up every run.
std::vector<float> makeFrame(int width, int height) {
    std::vector<float> pixels(static_cast<size_t>(width) * height * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t i = (static_cast<size_t>(y) * width + x) * 3;
            const float u = static_cast<float>(x) / static_cast<float>(width - 1);
            const float v = static_cast<float>(y) / static_cast<float>(height - 1);
            pixels[i + 0] = std::min(1.0f, 0.10f + 0.75f * u);
            pixels[i + 1] = std::min(1.0f, 0.08f + 0.55f * v + 0.25f * u);
            pixels[i + 2] = std::min(1.0f, 0.20f + 0.30f * (1.0f - u) * (1.0f - v));

            // Two hard highlights and one deep shadow patch.
            const float dx = static_cast<float>(x) - width * 0.30f;
            const float dy = static_cast<float>(y) - height * 0.35f;
            const float d2 = (dx * dx + dy * dy) / (width * 0.05f * width * 0.05f);
            if (d2 < 1.0f) {
                const float lift = 1.0f - d2;
                pixels[i + 0] = std::min(1.0f, pixels[i + 0] + lift);
                pixels[i + 1] = std::min(1.0f, pixels[i + 1] + lift * 0.98f);
                pixels[i + 2] = std::min(1.0f, pixels[i + 2] + lift * 0.90f);
            }
            const float ex = static_cast<float>(x) - width * 0.72f;
            const float ey = static_cast<float>(y) - height * 0.62f;
            if (ex * ex + ey * ey < width * width * 0.0006f) {
                pixels[i + 0] = 0.98f;
                pixels[i + 1] = 0.94f;
                pixels[i + 2] = 0.86f;
            }
            if (x > width * 0.55f && x < width * 0.68f && y > height * 0.08f &&
                y < height * 0.22f) {
                pixels[i + 0] = 0.03f;
                pixels[i + 1] = 0.05f;
                pixels[i + 2] = 0.04f;
            }
        }
    }
    return pixels;
}

bool runCase(const std::vector<float>& input, int width, int height,
             const sr::TextureParams& params, int frame, const char* name, double minPsnr) {
    std::vector<float> cpu = input;
    std::vector<float> gpu = input;
    sr::GrainFieldCache cache;
    sr::applyTexture(cpu.data(), width, height, params, frame, &cache);

    std::string error;
    sr::GpuTexture backend;
    if (!backend.process(gpu.data(), width, height, params, frame, gpu.data(), error)) {
        std::fprintf(stderr, "FAIL: %s: GPU process failed: %s\n", name, error.c_str());
        ++g_failures;
        return false;
    }

    double sum = 0.0;
    double worst = 0.0;
    const size_t count = static_cast<size_t>(width) * height * 3;
    for (size_t i = 0; i < count; ++i) {
        const double diff = static_cast<double>(gpu[i]) - static_cast<double>(cpu[i]);
        sum += diff * diff;
        worst = std::max(worst, std::fabs(diff));
    }
    const double mse = sum / static_cast<double>(count);
    const double psnr = mse > 0.0 ? 10.0 * std::log10(1.0 / mse) : 999.0;
    std::printf("  %-38s %6.2f dB  worst %.3e\n", name, psnr, worst);
    if (!(psnr >= minPsnr)) {
        std::fprintf(stderr, "FAIL: %s: %.2f dB below %.1f dB\n", name, psnr, minPsnr);
        ++g_failures;
        return false;
    }

    // Determinism: the same frame must produce the same grain field twice.
    std::vector<float> again = input;
    if (!backend.process(again.data(), width, height, params, frame, again.data(), error)) {
        std::fprintf(stderr, "FAIL: %s: second GPU pass failed: %s\n", name, error.c_str());
        ++g_failures;
        return false;
    }
    if (again != gpu) {
        std::fprintf(stderr, "FAIL: %s: GPU output is not deterministic\n", name);
        ++g_failures;
        return false;
    }
    return true;
}

}  // namespace

int main() {
    std::string error;
    if (!sr::GpuTexture::available(error)) {
        std::printf("skipped: %s\n", error.c_str());
        return 77;
    }
    std::printf("GPU texture parity (D047)\n");

    // 640 wide keeps radius >= factor * 2 for halation/glare/diffusion, which is
    // the downsample-blur-upsample branch of blurWide.
    const int width = 640;
    const int height = 360;
    const std::vector<float> frame = makeFrame(width, height);

    const double minPsnr = 60.0;

    sr::TextureParams texture;

    texture = sr::TextureParams{};
    texture.halation.amount = 0.45f;
    runCase(frame, width, height, texture, 0, "halation", minPsnr);

    texture = sr::TextureParams{};
    texture.glare.amount = 0.2f;
    runCase(frame, width, height, texture, 0, "glare", minPsnr);

    texture = sr::TextureParams{};
    texture.diffusion.amount = 0.1f;
    runCase(frame, width, height, texture, 0, "diffusion", minPsnr);

    texture = sr::TextureParams{};
    texture.grain.amount = 0.4f;
    texture.grain.rate = 1;
    runCase(frame, width, height, texture, 3, "grain (rate 1)", minPsnr);

    texture = sr::TextureParams{};
    texture.grain.amount = 0.4f;
    texture.grain.rate = 3;
    runCase(frame, width, height, texture, 7, "grain (rate 3, frame 7)", minPsnr);

    texture = sr::TextureParams{};
    texture.grain.amount = 0.4f;
    texture.grain.rate = 3;
    runCase(frame, width, height, texture, 8, "grain (rate 3, frame 8)", minPsnr);

    texture = sr::TextureParams{};
    texture.grain.amount = 0.4f;
    texture.grain.size = 0.0f;
    runCase(frame, width, height, texture, 1, "grain (raw noise, size 0)", minPsnr);

    // The same chain the UI and demo-m3 use.
    texture = sr::TextureParams{};
    texture.grain.amount = 0.4f;
    texture.grain.size = 1.4f;
    texture.grain.rate = 3;
    texture.halation.amount = 0.45f;
    texture.glare.amount = 0.2f;
    texture.diffusion.amount = 0.1f;
    runCase(frame, width, height, texture, 5, "all effects (demo-m3)", minPsnr);

    // A small frame takes blurWide's full-resolution three-pass branch.
    const int smallW = 192;
    const int smallH = 128;
    const std::vector<float> small = makeFrame(smallW, smallH);
    runCase(small, smallW, smallH, texture, 5, "all effects (small frame)", minPsnr);

    if (g_failures != 0) {
        std::fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("all GPU texture parity cases passed\n");
    return 0;
}
