// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Checks the 16-bit PNG path: save, reload, confirm the exact bit depth is
// reported and that samples survive the round trip to within one code value.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "sr/image.hpp"
#include "sr/transfer.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

}  // namespace

int main() {
    const sr::Image chart = sr::makeTestChart(320, 180);
    check(chart.valid(), "test chart is valid");

    const std::filesystem::path base = std::filesystem::temp_directory_path();
    const std::string path16 = (base / "spektra_roll_test16.png").string();
    const std::string path8 = (base / "spektra_roll_test8.png").string();

    std::string error;
    check(sr::saveImage(path16, chart, error, 16), "16-bit PNG writes");
    check(sr::saveImage(path8, chart, error, 8), "8-bit PNG writes");

    sr::Image loaded16;
    int bits16 = 0;
    check(sr::loadImage(path16, loaded16, error, &bits16), "16-bit PNG reads");
    check(bits16 == 16, "16-bit PNG reports 16 bits");
    check(loaded16.width == chart.width && loaded16.height == chart.height, "16-bit size");

    double worst = 0.0;
    for (size_t i = 0; i < chart.pixels.size(); ++i) {
        worst = std::max<double>(worst, std::fabs(chart.pixels[i] - loaded16.pixels[i]));
    }
    check(worst <= 1.0 / 65535.0, "16-bit round trip within one code value");

    sr::Image loaded8;
    int bits8 = 0;
    check(sr::loadImage(path8, loaded8, error, &bits8), "8-bit PNG reads");
    check(bits8 == 8, "8-bit PNG reports 8 bits");

    // 16-bit saving is PNG-only by design.
    check(!sr::saveImage((base / "spektra_roll_test16.bmp").string(), chart, error, 16),
          "16-bit non-PNG output is rejected");

    // OpenEXR (D055): float output keeps values outside 0..1, PNG does not.
    const std::string pathExr = (base / "spektra_roll_test.exr").string();
    const std::string pathExrHalf = (base / "spektra_roll_test_half.exr").string();
    const std::string pathClamp = (base / "spektra_roll_test_clamp.png").string();

    sr::Image wide = chart;
    wide.pixels[0] = -0.25f;
    wide.pixels[1] = 2.5f;
    wide.pixels[2] = 0.5f;

    check(sr::saveImage(pathExr, wide, error), "float EXR writes");
    sr::Image loadedExr;
    int bitsExr = 0;
    check(sr::loadImage(pathExr, loadedExr, error, &bitsExr), "float EXR reads");
    check(bitsExr == 32, "EXR reports 32 bits");
    check(loadedExr.width == wide.width && loadedExr.height == wide.height, "EXR size matches");
    bool exact = loadedExr.pixels.size() == wide.pixels.size();
    for (size_t i = 0; exact && i < wide.pixels.size(); ++i) {
        exact = wide.pixels[i] == loadedExr.pixels[i];
    }
    check(exact, "float EXR round trip is exact");
    check(loadedExr.pixels[0] == -0.25f && loadedExr.pixels[1] == 2.5f,
          "EXR keeps values outside 0..1");

    check(sr::saveImage(pathExrHalf, wide, error, 8, true), "half EXR writes");
    sr::Image loadedHalf;
    int bitsHalf = 0;
    check(sr::loadImage(pathExrHalf, loadedHalf, error, &bitsHalf), "half EXR reads");
    check(bitsHalf == 32, "half EXR still reports 32-bit float");
    double worstHalf = 0.0;
    for (size_t i = 0; i < wide.pixels.size(); ++i) {
        worstHalf = std::max<double>(worstHalf,
                                     std::fabs(static_cast<double>(wide.pixels[i]) -
                                               static_cast<double>(loadedHalf.pixels[i])));
    }
    check(worstHalf <= 1.0 / 1024.0, "half round trip within half precision");
    check(loadedHalf.pixels[0] < 0.0f && loadedHalf.pixels[1] > 1.0f,
          "half EXR keeps values outside 0..1");

    // ZIP is the default; uncompressed scanlines must still round trip (D059).
    const std::string pathExrRaw = (base / "spektra_roll_test_raw.exr").string();
    check(sr::saveImage(pathExrRaw, wide, error, 8, false, false), "uncompressed EXR writes");
    sr::Image loadedRaw;
    int bitsRaw = 0;
    check(sr::loadImage(pathExrRaw, loadedRaw, error, &bitsRaw), "uncompressed EXR reads");
    bool rawExact = loadedRaw.pixels.size() == wide.pixels.size();
    for (size_t i = 0; rawExact && i < wide.pixels.size(); ++i) {
        rawExact = wide.pixels[i] == loadedRaw.pixels[i];
    }
    check(rawExact, "uncompressed EXR round trip is exact");

    check(sr::saveImage(pathClamp, wide, error, 16), "16-bit PNG still writes");
    sr::Image clamped;
    check(sr::loadImage(pathClamp, clamped, error), "16-bit PNG reads back");
    check(clamped.pixels[0] == 0.0f && clamped.pixels[1] == 1.0f,
          "16-bit PNG clamps out-of-range values");

    check(!sr::saveImage((base / "spektra_roll_test32.png").string(), chart, error, 32),
          "32-bit float PNG output is rejected");

    // Linear float delivery (D058): decode the output transfer without clipping.
    {
        sr::FloatTransfer transfer = sr::FloatTransfer::Srgb;
        check(sr::parseFloatTransfer("pq", transfer) && transfer == sr::FloatTransfer::Pq,
              "pq parses as a float transfer");
        check(sr::parseFloatTransfer("hlg", transfer) && transfer == sr::FloatTransfer::Hlg,
              "hlg parses as a float transfer");
        check(!sr::parseFloatTransfer("linear", transfer), "unknown transfers are rejected");

        sr::Image linear;
        linear.width = 2;
        linear.height = 1;
        linear.pixels = {0.5f, 1.5f, 0.0f, 1.0f, 0.0f, 0.0f};
        sr::linearizeImage(linear, sr::FloatTransfer::Srgb, 1000.0);
        check(std::fabs(linear.pixels[0] - 0.2140411f) < 1e-6f, "sRGB mid-grey decodes");
        check(linear.pixels[1] > 2.2f, "sRGB super-white survives linearisation");
        check(linear.pixels[3] == 1.0f, "sRGB white stays white");

        sr::Image pqImage;
        pqImage.width = 1;
        pqImage.height = 1;
        pqImage.pixels = {static_cast<float>(sr::pqEncode(1000.0)), 0.0f, 0.0f};
        sr::linearizeImage(pqImage, sr::FloatTransfer::Pq, 1000.0);
        check(std::fabs(pqImage.pixels[0] - 1.0f) < 1e-5f,
              "PQ white at 1000 nits maps to 1.0");

        sr::Image hlgImage;
        hlgImage.width = 1;
        hlgImage.height = 1;
        hlgImage.pixels = {static_cast<float>(sr::hlgEncode(0.25)), 0.0f, 0.0f};
        sr::linearizeImage(hlgImage, sr::FloatTransfer::Hlg, 1000.0);
        check(std::fabs(hlgImage.pixels[0] - 0.25f) < 1e-5f, "HLG round trips to 0.25");
    }

    // Random samples defeat ZIP, which exercises the "store the raw block"
    // fallback: it must store the original bytes, not the predictor output.
    const std::string pathExrNoise = (base / "spektra_roll_test_noise.exr").string();
    sr::Image noise;
    noise.width = 64;
    noise.height = 64;
    noise.pixels.resize(static_cast<size_t>(noise.width) * noise.height * 3);
    unsigned int state = 12345u;
    for (float& value : noise.pixels) {
        state = state * 1664525u + 1013904223u;
        value = static_cast<float>(state >> 8) / 16777216.0f;
    }
    check(sr::saveImage(pathExrNoise, noise, error), "incompressible EXR writes");
    sr::Image loadedNoise;
    check(sr::loadImage(pathExrNoise, loadedNoise, error), "incompressible EXR reads");
    bool noiseExact = loadedNoise.pixels.size() == noise.pixels.size();
    for (size_t i = 0; noiseExact && i < noise.pixels.size(); ++i) {
        noiseExact = noise.pixels[i] == loadedNoise.pixels[i];
    }
    check(noiseExact, "uncompressed fallback blocks round trip exactly");

    std::remove(path16.c_str());
    std::remove(path8.c_str());
    std::remove(pathExr.c_str());
    std::remove(pathExrHalf.c_str());
    std::remove(pathExrRaw.c_str());
    std::remove(pathExrNoise.c_str());
    std::remove(pathClamp.c_str());

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
