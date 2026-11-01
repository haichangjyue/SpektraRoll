// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// OkLab conversion and OkLCh gamut compression. The OkLab reference values
// come from colour-science 0.4.7 (RGB_to_XYZ then XYZ_to_Oklab).
#include <cmath>
#include <cstdio>

#include "sr/gamut.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

void checkNear(double actual, double expected, double tolerance, const char* what) {
    if (!(std::fabs(actual - expected) <= tolerance)) {
        std::fprintf(stderr, "FAIL: %s (expected %.9f, got %.9f)\n", what, expected, actual);
        ++g_failures;
    }
}

double hueOf(const sr::Rgb& rgb) {
    const double v[3] = {rgb.r, rgb.g, rgb.b};
    double lab[3];
    sr::linearSrgbToOklab(v, lab);
    return std::atan2(lab[2], lab[1]);
}

bool inCube(const sr::Rgb& rgb) {
    const double v[3] = {rgb.r, rgb.g, rgb.b};
    for (double x : v) {
        if (x < -1e-6 || x > 1.0 + 1e-6) return false;
    }
    return true;
}

}  // namespace

int main() {
    // OkLab of linear sRGB (0.5, 0.3, 0.2), from colour-science.
    const double rgb[3] = {0.5, 0.3, 0.2};
    double lab[3];
    sr::linearSrgbToOklab(rgb, lab);
    checkNear(lab[0], 0.698819179, 1e-5, "OkLab L");
    checkNear(lab[1], 0.033866474, 1e-4, "OkLab a");
    checkNear(lab[2], 0.047887959, 1e-4, "OkLab b");

    // Round trip.
    double back[3];
    sr::oklabToLinearSrgb(lab, back);
    for (int c = 0; c < 3; ++c) checkNear(back[c], rgb[c], 1e-6, "OkLab round trip");

    // Neutral stays neutral and has no chroma.
    const double grey[3] = {0.18, 0.18, 0.18};
    sr::linearSrgbToOklab(grey, lab);
    checkNear(lab[1], 0.0, 1e-6, "neutral a is zero");
    checkNear(lab[2], 0.0, 1e-6, "neutral b is zero");

    // In-gamut colours pass through untouched.
    const sr::Rgb red{1.0f, 0.0f, 0.0f};
    const sr::Rgb redOut = sr::compressGamutOklch(red);
    checkNear(redOut.r, 1.0, 1e-9, "in-gamut red unchanged");
    checkNear(redOut.g, 0.0, 1e-9, "in-gamut red unchanged (g)");
    checkNear(redOut.b, 0.0, 1e-9, "in-gamut red unchanged (b)");

    // Out-of-gamut colours come back inside the cube.
    const sr::Rgb oog[4] = {{1.2f, -0.05f, 0.02f},
                            {0.1f, 1.1f, -0.1f},
                            {-0.08f, 0.4f, 0.9f},
                            {0.9f, 0.2f, -0.15f}};
    for (const sr::Rgb& value : oog) {
        const sr::Rgb mapped = sr::compressGamutOklch(value);
        check(inCube(mapped), "out-of-gamut colour maps into the cube");

        // Hue is preserved by the chroma-only step.
        double before = hueOf(value);
        double after = hueOf(mapped);
        const double twoPi = 6.283185307179586476925286766559;
        double delta = std::remainder(after - before, twoPi);
        checkNear(delta, 0.0, 5e-3, "hue preserved by gamut compression");
    }

    // The knee is identity below the threshold and asymptotic at the limit.
    sr::GamutCompressParams params;
    checkNear(sr::reinhardKnee(0.5, params), 0.5, 1e-12, "knee identity below threshold");
    checkNear(sr::reinhardKnee(0.8, params), 0.8, 1e-12, "knee identity at threshold");
    check(sr::reinhardKnee(1.0, params) < 1.0, "knee pulls the boundary inside");
    check(sr::reinhardKnee(50.0, params) < 1.0, "knee asymptotes below the limit");
    check(sr::reinhardKnee(1.5, params) > sr::reinhardKnee(1.0, params),
          "knee is monotonic");

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
