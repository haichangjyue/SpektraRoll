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

    std::remove(path16.c_str());
    std::remove(path8.c_str());

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
