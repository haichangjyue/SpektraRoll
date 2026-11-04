// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Parity test for the D3D11 spectral backend (D046). Every sample is compared
// against FilmModel::transformFrom, which stays the reference. When no D3D11
// device is available (macOS, CI runners without a GPU) the test exits 77 and
// ctest reports it as skipped instead of failed.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "sr/film_model.hpp"
#include "sr/lut3d.hpp"
#include "sr/profile.hpp"

#include "../src/cli/gpu_film.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

std::string sourceDir() { return SR_SOURCE_DIR; }

// Encoded-space samples: greys, primaries, and the kind of saturated colours
// that push the OkLCh gamut mapping hard.
const sr::Rgb kSamples[] = {
    {0.0f, 0.0f, 0.0f},    {0.05f, 0.05f, 0.05f},  {0.18f, 0.18f, 0.18f},
    {0.5f, 0.5f, 0.5f},    {0.8f, 0.8f, 0.8f},     {1.0f, 1.0f, 1.0f},
    {0.8f, 0.2f, 0.2f},    {0.2f, 0.8f, 0.2f},     {0.2f, 0.2f, 0.8f},
    {0.9f, 0.85f, 0.1f},   {0.1f, 0.75f, 0.85f},   {0.85f, 0.1f, 0.7f},
    {0.4f, 0.32f, 0.25f},  {0.15f, 0.35f, 0.6f},   {0.95f, 0.6f, 0.45f},
};
constexpr int kSampleCount = static_cast<int>(sizeof(kSamples) / sizeof(kSamples[0]));
constexpr int kPixels = kSampleCount + 2;  // two extra slots for the strength test

bool runCase(const sr::FilmModel& model, const sr::BakeOptions& options, const char* name,
             double tolerance) {
    std::string error;
    sr::GpuFilm gpu;
    if (!gpu.configure(model, options, error)) {
        std::fprintf(stderr, "FAIL: %s: GPU configure failed: %s\n", name, error.c_str());
        ++g_failures;
        return false;
    }

    std::vector<float> input(kPixels * 3);
    std::vector<float> output(kPixels * 3);
    for (int i = 0; i < kSampleCount; ++i) {
        input[static_cast<size_t>(i) * 3 + 0] = kSamples[i].r;
        input[static_cast<size_t>(i) * 3 + 1] = kSamples[i].g;
        input[static_cast<size_t>(i) * 3 + 2] = kSamples[i].b;
    }
    // Strength samples reuse the 18% grey and the warm red.
    input[static_cast<size_t>(kSampleCount) * 3 + 0] = 0.18f;
    input[static_cast<size_t>(kSampleCount) * 3 + 1] = 0.18f;
    input[static_cast<size_t>(kSampleCount) * 3 + 2] = 0.18f;
    input[static_cast<size_t>(kSampleCount + 1) * 3 + 0] = 0.8f;
    input[static_cast<size_t>(kSampleCount + 1) * 3 + 1] = 0.2f;
    input[static_cast<size_t>(kSampleCount + 1) * 3 + 2] = 0.2f;

    if (!gpu.process(input.data(), kPixels, output.data(), 1.0f, error)) {
        std::fprintf(stderr, "FAIL: %s: GPU process failed: %s\n", name, error.c_str());
        ++g_failures;
        return false;
    }

    double worst = 0.0;
    bool sawNan = false;
    for (int i = 0; i < kSampleCount; ++i) {
        const sr::Rgb expected = model.transformFrom(options, kSamples[i]);
        for (int c = 0; c < 3; ++c) {
            const double actual = output[static_cast<size_t>(i) * 3 + c];
            const double want = c == 0 ? expected.r : (c == 1 ? expected.g : expected.b);
            if (std::isnan(actual)) sawNan = true;
            worst = std::max(worst, std::fabs(actual - want));
        }
    }
    if (sawNan) {
        std::fprintf(stderr, "FAIL: %s: GPU output contains NaN\n", name);
        ++g_failures;
        return false;
    }
    if (!(worst <= tolerance)) {
        std::fprintf(stderr, "FAIL: %s: worst deviation %.6f > %.6f\n", name, worst, tolerance);
        for (int i = 0; i < kSampleCount; ++i) {
            const sr::Rgb expected = model.transformFrom(options, kSamples[i]);
            std::fprintf(stderr, "  %2d gpu %9.5f %9.5f %9.5f   cpu %9.5f %9.5f %9.5f\n", i,
                         output[static_cast<size_t>(i) * 3 + 0],
                         output[static_cast<size_t>(i) * 3 + 1],
                         output[static_cast<size_t>(i) * 3 + 2], expected.r, expected.g,
                         expected.b);
        }
        ++g_failures;
        return false;
    }

    // sr::mix blends the encoded input with the film result.
    for (int i = kSampleCount; i < kPixels; ++i) {
        const sr::Rgb src{input[static_cast<size_t>(i) * 3 + 0],
                          input[static_cast<size_t>(i) * 3 + 1],
                          input[static_cast<size_t>(i) * 3 + 2]};
        const sr::Rgb full = model.transformFrom(options, src);
        const sr::Rgb expected = sr::mix(src, full, 0.5f);
        if (!gpu.process(input.data(), kPixels, output.data(), 0.5f, error)) {
            std::fprintf(stderr, "FAIL: %s: GPU process (strength) failed: %s\n", name,
                         error.c_str());
            ++g_failures;
            return false;
        }
        const double want[3] = {expected.r, expected.g, expected.b};
        for (int c = 0; c < 3; ++c) {
            const double actual = output[static_cast<size_t>(i) * 3 + c];
            if (!(std::fabs(actual - want[c]) <= tolerance)) {
                std::fprintf(stderr,
                             "FAIL: %s: strength blend off by %.6f (gpu %.6f want %.6f)\n", name,
                             std::fabs(actual - want[c]), actual, want[c]);
                ++g_failures;
                return false;
            }
        }
    }
    std::printf("  %-34s worst %.6f\n", name, worst);
    return true;
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

    const std::array<double, 3> filters{0.0, 51.57, 52.53};

    if (!sr::GpuFilm::available(error)) {
        std::printf("skipped: %s\n", error.c_str());
        return 77;
    }
    std::printf("GPU spectral parity\n");

    const double tolerance = 2e-3;

    sr::FilmModel srgbModel;
    if (!srgbModel.configure(film, paper, filters, error)) {
        std::fprintf(stderr, "FAIL: %s\n", error.c_str());
        return 1;
    }
    runCase(srgbModel, sr::BakeOptions{}, "srgb / print", tolerance);

    sr::BakeOptions appleLog;
    appleLog.input = sr::InputTransfer::AppleLog;
    appleLog.exposureEv = 0.7;
    runCase(srgbModel, appleLog, "apple-log / print", tolerance);

    sr::BakeOptions negative = appleLog;
    negative.scan = sr::ScanMode::Negative;
    negative.contrast = 1.15;
    negative.gamutCompress = true;
    runCase(srgbModel, negative, "apple-log / negative +gamut", tolerance);

    sr::BakeOptions negativeHdr = negative;
    negativeHdr.hdr = sr::HdrMode::Pq;
    runCase(srgbModel, negativeHdr, "apple-log / negative / PQ", tolerance);

    // Camera log inputs (D060): the shader must decode the curve and the gamut
    // matrix exactly like the CPU reference.
    sr::BakeOptions slog3 = negative;
    slog3.input = sr::InputTransfer::SLog3;
    runCase(srgbModel, slog3, "s-log3 / negative", tolerance);

    sr::BakeOptions clog3 = negative;
    clog3.input = sr::InputTransfer::CLog3;
    runCase(srgbModel, clog3, "c-log3 / negative", tolerance);

    sr::BakeOptions vlog = negative;
    vlog.input = sr::InputTransfer::VLog;
    runCase(srgbModel, vlog, "v-log / negative", tolerance);

    // D061: the remaining camera logs, one parity case each.
    const std::pair<sr::InputTransfer, const char*> moreLogs[] = {
        {sr::InputTransfer::SLog3SGamut3, "s-log3 + s-gamut3 / negative"},
        {sr::InputTransfer::ArriLogC3, "logc3 / negative"},
        {sr::InputTransfer::ArriLogC4, "logc4 / negative"},
        {sr::InputTransfer::BmdFilmGen5, "bmd-film / negative"},
        {sr::InputTransfer::DaVinciIntermediate, "davinci / negative"},
    };
    for (const auto& entry : moreLogs) {
        sr::BakeOptions options = negative;
        options.input = entry.first;
        runCase(srgbModel, options, entry.second, tolerance);
    }

    sr::FilmModelOptions couplerOptions;
    couplerOptions.couplers = true;
    sr::FilmModel couplerModel;
    if (!couplerModel.configure(film, paper, filters, error, couplerOptions)) {
        std::fprintf(stderr, "FAIL: %s\n", error.c_str());
        return 1;
    }
    sr::BakeOptions couplerNegative = negative;
    runCase(couplerModel, couplerNegative, "couplers / negative +gamut", tolerance);
    runCase(couplerModel, sr::BakeOptions{}, "couplers / print", tolerance);

    // D065: the cinema chain. Vision3 500T uses the T reference illuminant and
    // the 2383 print is scanned under K75P, so both extra matrices and the
    // second viewing illuminant must cross the constant buffer intact.
    sr::Profile cineFilm;
    sr::Profile cinePrint;
    std::array<double, 3> cineFilters{};
    if (!sr::loadProfile(root + "/data/profiles/kodak_vision3_500t.json", cineFilm, error) ||
        !sr::loadProfile(root + "/data/profiles/kodak_2383.json", cinePrint, error) ||
        !sr::loadNeutralFilters(root + "/data/profiles/neutral_print_filters.json",
                                cinePrint.info.stock, cineFilm.info.stock, "TH-KG3",
                                cineFilters, error)) {
        std::fprintf(stderr, "FAIL: cine profile load: %s\n", error.c_str());
        return 1;
    }
    sr::FilmModel cineModel;
    if (!cineModel.configure(cineFilm, cinePrint, cineFilters, error)) {
        std::fprintf(stderr, "FAIL: cine configure: %s\n", error.c_str());
        return 1;
    }
    runCase(cineModel, sr::BakeOptions{}, "vision3 500T / 2383 / print", tolerance);
    runCase(cineModel, slog3, "vision3 500T / 2383 / s-log3 neg", tolerance);

    // D067: reversal direct scan crosses the new white/black/gamma fields and
    // the scan-mode branch. Couplers on exercises the positive silver model.
    sr::Profile slideFilm;
    std::array<double, 3> slideFilters{};
    if (!sr::loadProfile(root + "/data/profiles/kodak_ektachrome_100.json", slideFilm, error) ||
        !sr::loadNeutralFilters(root + "/data/profiles/neutral_print_filters.json",
                                paper.info.stock, slideFilm.info.stock, "TH-KG3",
                                slideFilters, error)) {
        std::fprintf(stderr, "FAIL: slide profile load: %s\n", error.c_str());
        return 1;
    }
    sr::FilmModelOptions slideCouplers;
    slideCouplers.couplers = true;
    sr::FilmModel slideModel;
    if (!slideModel.configure(slideFilm, paper, slideFilters, error, slideCouplers)) {
        std::fprintf(stderr, "FAIL: slide configure: %s\n", error.c_str());
        return 1;
    }
    sr::BakeOptions slide = slog3;
    slide.scan = sr::ScanMode::Positive;
    slide.contrast = 1.0;
    slide.gamutCompress = true;
    runCase(slideModel, slide, "ektachrome / slide +gamut", tolerance);

    if (g_failures != 0) {
        std::fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("all GPU parity cases passed\n");
    return 0;
}
