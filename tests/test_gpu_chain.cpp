// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Parity test for the fused colour+texture submit (D049). The film and texture
// backends share one D3D11 device, the film pass leaves its result on the GPU
// and the texture chain consumes it without an upload. The reference is the CPU
// chain: FilmModel::transformFrom followed by sr::applyTexture.
//
// Exits 77 (ctest: skipped) when no D3D11 device is available.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "sr/film_model.hpp"
#include "sr/profile.hpp"
#include "sr/texture.hpp"

#include "../src/cli/gpu_film.hpp"
#include "../src/cli/gpu_texture.hpp"

namespace {

std::string sourceDir() { return SR_SOURCE_DIR; }

// A small frame with gradients and a highlight, enough to exercise halation,
// glare, diffusion and grain.
std::vector<float> makeFrame(int width, int height) {
    std::vector<float> pixels(static_cast<size_t>(width) * height * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t i = (static_cast<size_t>(y) * width + x) * 3;
            const float u = static_cast<float>(x) / static_cast<float>(width - 1);
            const float v = static_cast<float>(y) / static_cast<float>(height - 1);
            pixels[i + 0] = 0.10f + 0.70f * u;
            pixels[i + 1] = 0.12f + 0.55f * v;
            pixels[i + 2] = 0.18f + 0.35f * (1.0f - u);
            const float dx = static_cast<float>(x) - width * 0.35f;
            const float dy = static_cast<float>(y) - height * 0.40f;
            if (dx * dx + dy * dy < width * width * 0.0025f) {
                pixels[i + 0] = 0.97f;
                pixels[i + 1] = 0.95f;
                pixels[i + 2] = 0.90f;
            }
        }
    }
    return pixels;
}

}  // namespace

int main() {
    const std::string root = sourceDir();
    sr::Profile film;
    sr::Profile paper;
    std::string error;
    if (!sr::loadProfile(root + "/data/profiles/kodak_portra_400.json", film, error) ||
        !sr::loadProfile(root + "/data/profiles/kodak_portra_endura.json", paper, error)) {
        std::fprintf(stderr, "FAIL: profile load: %s\n", error.c_str());
        return 1;
    }

    std::string probe;
    if (!sr::GpuFilm::available(probe)) {
        std::printf("skipped: %s\n", probe.c_str());
        return 77;
    }
    // The fused submit needs both backends on one device; Metal texture is not
    // there yet (D056), so macOS skips this test instead of failing.
    if (!sr::GpuTexture::available(probe)) {
        std::printf("skipped: %s\n", probe.c_str());
        return 77;
    }
    std::printf("GPU fused colour+texture parity (D049)\n");

    const std::array<double, 3> filters{0.0, 51.57, 52.53};
    sr::FilmModelOptions modelOptions;
    modelOptions.couplers = true;
    sr::FilmModel model;
    if (!model.configure(film, paper, filters, error, modelOptions)) {
        std::fprintf(stderr, "FAIL: %s\n", error.c_str());
        return 1;
    }

    sr::BakeOptions bake;
    bake.input = sr::InputTransfer::AppleLog;
    bake.exposureEv = 0.7;
    bake.scan = sr::ScanMode::Negative;
    bake.contrast = 1.15;
    bake.gamutCompress = true;

    const int width = 320;
    const int height = 180;
    const size_t count = static_cast<size_t>(width) * height;
    const std::vector<float> input = makeFrame(width, height);

    sr::TextureParams texture;
    texture.grain.amount = 0.4f;
    texture.grain.size = 1.4f;
    texture.grain.rate = 3;
    texture.halation.amount = 0.45f;
    texture.glare.amount = 0.2f;
    texture.diffusion.amount = 0.1f;
    const int frameIndex = 5;

    // Reference: CPU film + CPU texture.
    std::vector<float> cpu = input;
    for (size_t i = 0; i < count; ++i) {
        float* px = &cpu[i * 3];
        const sr::Rgb src{px[0], px[1], px[2]};
        const sr::Rgb mapped = model.transformFrom(bake, src);
        px[0] = mapped.r;
        px[1] = mapped.g;
        px[2] = mapped.b;
    }
    sr::GrainFieldCache cache;
    sr::applyTexture(cpu.data(), width, height, texture, frameIndex, &cache);

    // Fused GPU: one device, film result stays on the card.
    sr::GpuFilm gpuFilm;
    if (!gpuFilm.configure(model, bake, error)) {
        std::fprintf(stderr, "FAIL: film configure: %s\n", error.c_str());
        return 1;
    }
    sr::GpuTexture gpuTexture;
    if (!gpuTexture.attach(gpuFilm.deviceHandle(), error) ||
        !gpuTexture.prepare(error)) {
        std::fprintf(stderr, "FAIL: texture attach: %s\n", error.c_str());
        return 1;
    }

    std::vector<float> gpu = input;
    if (!gpuFilm.processToDevice(gpu.data(), count, 1.0f, error)) {
        std::fprintf(stderr, "FAIL: fused film pass: %s\n", error.c_str());
        return 1;
    }
    if (!gpuTexture.processBuffer(gpuFilm.resultBuffer(), width, height, texture, frameIndex,
                                  gpu.data(), error)) {
        std::fprintf(stderr, "FAIL: fused texture pass: %s\n", error.c_str());
        return 1;
    }

    double sum = 0.0;
    double worst = 0.0;
    for (size_t i = 0; i < count * 3; ++i) {
        const double diff = static_cast<double>(gpu[i]) - static_cast<double>(cpu[i]);
        sum += diff * diff;
        worst = std::max(worst, std::fabs(diff));
    }
    const double mse = sum / static_cast<double>(count * 3);
    const double psnr = mse > 0.0 ? 10.0 * std::log10(1.0 / mse) : 999.0;
    std::printf("  fused vs CPU           %6.2f dB  worst %.3e\n", psnr, worst);
    if (!(psnr >= 60.0)) {
        std::fprintf(stderr, "FAIL: %.2f dB is below 60 dB\n", psnr);
        return 1;
    }

    // The fused submit must also be deterministic frame to frame.
    std::vector<float> again = input;
    if (!gpuFilm.processToDevice(again.data(), count, 1.0f, error) ||
        !gpuTexture.processBuffer(gpuFilm.resultBuffer(), width, height, texture, frameIndex,
                                  again.data(), error)) {
        std::fprintf(stderr, "FAIL: second fused pass: %s\n", error.c_str());
        return 1;
    }
    if (again != gpu) {
        std::fprintf(stderr, "FAIL: fused output is not deterministic\n");
        return 1;
    }

    std::printf("fused GPU parity passed\n");
    return 0;
}
