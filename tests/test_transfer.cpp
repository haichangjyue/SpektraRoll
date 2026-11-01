// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Apple Log and BT.2020 primaries. Expected numbers come from colour-science
// 0.4.7 (log_decoding_AppleLogProfile / log_encoding_AppleLogProfile and
// RGB_to_RGB(BT2020, sRGB) with no transfer functions applied).
#include <cmath>
#include <cstdio>

#include "sr/transfer.hpp"

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
        std::fprintf(stderr, "FAIL: %s (expected %.12f, got %.12f)\n", what, expected, actual);
        ++g_failures;
    }
}

void checkRgb3(const double actual[3], const double expected[3], double tolerance,
               const char* what) {
    for (int c = 0; c < 3; ++c) {
        if (!(std::fabs(actual[c] - expected[c]) <= tolerance)) {
            std::fprintf(stderr, "FAIL: %s (expected %.12f %.12f %.12f, got %.12f %.12f %.12f)\n",
                         what, expected[0], expected[1], expected[2], actual[0], actual[1],
                         actual[2]);
            ++g_failures;
            return;
        }
    }
}

}  // namespace

int main() {
    // Apple Log decoding: code value -> linear scene light.
    const double decodeIn[] = {0.0, 0.1, 0.15045, 0.2, 0.3, 0.48827245852686763, 0.6, 0.8, 1.0};
    const double decodeOut[] = {-0.05641088,   -0.010424565733, -0.000004958461, 0.008623589319,
                                0.031578255524, 0.18,           0.459476849975,  2.363950001036,
                                12.000002102816};
    for (size_t i = 0; i < sizeof(decodeIn) / sizeof(decodeIn[0]); ++i) {
        checkNear(sr::appleLogDecode(decodeIn[i]), decodeOut[i], 1e-9, "apple log decode");
    }

    // Apple Log encoding: linear -> code value. 18% grey lands on 0.48827.
    const double encodeIn[] = {0.0, 0.001, 0.01, 0.05, 0.18, 0.5, 1.0, 2.0, 5.9409};
    const double encodeOut[] = {0.150476452301, 0.155858754655, 0.208555318703,
                                0.345573299658, 0.488272458527, 0.610220478710,
                                0.694552983055, 0.779467425925, 0.913375046519};
    for (size_t i = 0; i < sizeof(encodeIn) / sizeof(encodeIn[0]); ++i) {
        checkNear(sr::appleLogEncode(encodeIn[i]), encodeOut[i], 1e-9, "apple log encode");
    }

    // The decode is the inverse of the encode across the usable range.
    for (double x = 0.0; x <= 8.0; x += 0.125) {
        checkNear(sr::appleLogDecode(sr::appleLogEncode(x)), x, 1e-9, "apple log round trip");
    }

    // True black sits at code 0.1504764523; code 0 is below black.
    checkNear(sr::appleLogDecode(0.150476452301), 0.0, 1e-9, "apple log black point");
    check(sr::appleLogDecode(0.0) < 0.0, "apple log code 0 is below black");

    // BT.2020 (D65) -> linear sRGB (D65), same composition as colour 0.4.7.
    const double in1[3] = {0.4, 0.5, 0.6};
    const double out1[3] = {0.326602286774, 0.51165116028, 0.613725479457};
    double got[3];
    sr::bt2020ToSrgb(in1, got);
    checkRgb3(got, out1, 1e-9, "BT2020 -> sRGB sample 1");

    const double in2[3] = {0.18, 0.18, 0.18};
    const double out2[3] = {0.179971790881, 0.180017846809, 0.180013398784};
    sr::bt2020ToSrgb(in2, got);
    checkRgb3(got, out2, 1e-9, "BT2020 -> sRGB neutral stays neutral");

    const double in3[3] = {1.0, 0.2, 0.05};
    const double out3[3] = {1.539144953904, 0.101773314119, 0.017709514646};
    sr::bt2020ToSrgb(in3, got);
    checkRgb3(got, out3, 1e-9, "BT2020 -> sRGB out-of-gamut colour");

    const double white[3] = {1.0, 1.0, 1.0};
    const double whiteOut[3] = {0.99984328, 1.00009915, 1.00007444};
    sr::bt2020ToSrgb(white, got);
    checkRgb3(got, whiteOut, 1e-7, "BT2020 white -> sRGB white");

    sr::InputTransfer parsed;
    check(sr::parseInputTransfer("apple-log", parsed) && parsed == sr::InputTransfer::AppleLog,
          "parse apple-log");
    check(sr::parseInputTransfer("srgb", parsed) && parsed == sr::InputTransfer::Srgb,
          "parse srgb");
    check(!sr::parseInputTransfer("rec709", parsed), "reject unknown input transfer");

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
