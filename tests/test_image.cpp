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

    check(sr::saveImage(pathClamp, wide, error, 16), "16-bit PNG still writes");
    sr::Image clamped;
    check(sr::loadImage(pathClamp, clamped, error), "16-bit PNG reads back");
    check(clamped.pixels[0] == 0.0f && clamped.pixels[1] == 1.0f,
          "16-bit PNG clamps out-of-range values");

    check(!sr::saveImage((base / "spektra_roll_test32.png").string(), chart, error, 32),
          "32-bit float PNG output is rejected");

    std::remove(path16.c_str());
    std::remove(path8.c_str());
    std::remove(pathExr.c_str());
    std::remove(pathExrHalf.c_str());
    std::remove(pathClamp.c_str());

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
