// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

#include "sr/lut3d.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

void checkNear(float actual, float expected, float tolerance, const char* what) {
    if (std::fabs(actual - expected) > tolerance) {
        std::fprintf(stderr, "FAIL: %s (expected %f, got %f)\n", what, expected, actual);
        ++g_failures;
    }
}

bool near(sr::Rgb a, sr::Rgb b, float tolerance) {
    return std::fabs(a.r - b.r) <= tolerance && std::fabs(a.g - b.g) <= tolerance &&
           std::fabs(a.b - b.b) <= tolerance;
}

void testIdentityRoundTrip() {
    const sr::Lut3D lut = sr::Lut3D::identity(17);
    for (int i = 0; i <= 10; ++i) {
        const float v = static_cast<float>(i) / 10.0f;
        const sr::Rgb out = lut.apply({v, 1.0f - v, v * 0.5f});
        checkNear(out.r, v, 1e-5f, "identity red");
        checkNear(out.g, 1.0f - v, 1e-5f, "identity green");
        checkNear(out.b, v * 0.5f, 1e-5f, "identity blue");
    }
}

void testClamping() {
    const sr::Lut3D lut = sr::Lut3D::identity(5);
    const sr::Rgb low = lut.apply({-2.0f, -0.5f, -1.0f});
    checkNear(low.r, 0.0f, 1e-6f, "clamp below domain");
    const sr::Rgb high = lut.apply({3.0f, 1.5f, 9.0f});
    checkNear(high.r, 1.0f, 1e-6f, "clamp above domain");
}

void testInterpolatorsAgreeOnLinearFunction() {
    const sr::Lut3D lut = sr::Lut3D::identity(33);
    const sr::Rgb point{0.123f, 0.456f, 0.789f};
    const sr::Rgb tetra = lut.apply(point, sr::Interp::Tetrahedral);
    const sr::Rgb tri = lut.apply(point, sr::Interp::Trilinear);
    check(near(tetra, tri, 1e-5f), "tetrahedral and trilinear agree on identity");
    check(near(tetra, point, 1e-4f), "tetrahedral reproduces the input");
}

void testMix() {
    const sr::Rgb a{0.0f, 0.5f, 1.0f};
    const sr::Rgb b{1.0f, 1.0f, 0.0f};
    check(near(sr::mix(a, b, 0.0f), a, 1e-6f), "mix at 0 returns the source");
    check(near(sr::mix(a, b, 1.0f), b, 1e-6f), "mix at 1 returns the destination");
    const sr::Rgb half = sr::mix(a, b, 0.5f);
    checkNear(half.r, 0.5f, 1e-6f, "mix midpoint red");
    checkNear(half.g, 0.75f, 1e-6f, "mix midpoint green");
    checkNear(half.b, 0.5f, 1e-6f, "mix midpoint blue");
}

void testCubeParsingRoundTrip() {
    // 2^3 LUT that swaps red and blue and halves green.
    const char* path = "test_roundtrip.cube";
    {
        std::ofstream out(path);
        check(static_cast<bool>(out), "temp cube file opens for writing");
        out << "TITLE \"swap rb\"\n";
        out << "LUT_3D_SIZE 2\n";
        out << "DOMAIN_MIN 0.0 0.0 0.0\n";
        out << "DOMAIN_MAX 1.0 1.0 1.0\n";
        for (int b = 0; b < 2; ++b) {
            for (int g = 0; g < 2; ++g) {
                for (int r = 0; r < 2; ++r) {
                    out << b << " " << (g * 0.5f) << " " << r << "\n";
                }
            }
        }
    }

    sr::Lut3D lut;
    std::string error;
    check(lut.loadCube(path, error), "2^3 cube parses");
    check(lut.size() == 2, "2^3 cube reports size 2");
    check(lut.title() == "swap rb", "2^3 cube keeps its title");

    const sr::Rgb out = lut.apply({1.0f, 1.0f, 0.0f});
    checkNear(out.r, 0.0f, 1e-6f, "parsed cube swaps red -> blue");
    checkNear(out.g, 0.5f, 1e-6f, "parsed cube halves green");
    checkNear(out.b, 1.0f, 1e-6f, "parsed cube swaps blue -> red");

    std::remove(path);
}

void testCubeRejectsWrongCount() {
    const char* path = "test_bad_count.cube";
    {
        std::ofstream out(path);
        out << "LUT_3D_SIZE 3\n";
        out << "0 0 0\n";  // 3^3 = 27 entries expected
    }

    sr::Lut3D lut;
    std::string error;
    check(!lut.loadCube(path, error), "short cube is rejected");
    check(!error.empty(), "short cube reports an error message");

    std::remove(path);
}

}  // namespace

int main() {
    testIdentityRoundTrip();
    testClamping();
    testInterpolatorsAgreeOnLinearFunction();
    testMix();
    testCubeParsingRoundTrip();
    testCubeRejectsWrongCount();

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
