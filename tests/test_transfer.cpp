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

    // Camera log decodes (D060), references from colour-science 0.4.7
    // (log_decoding_SLog3 / log_decoding_CanonLog3 / log_decoding_VLog).
    checkNear(sr::slog3Decode(0.0), -0.014023695936, 1e-9, "s-log3 code 0");
    checkNear(sr::slog3Decode(0.410557185), 0.180000000427, 1e-9, "s-log3 18% grey");
    checkNear(sr::slog3Decode(1.0), 38.420934337203, 1e-6, "s-log3 white");

    checkNear(sr::cLog3Decode(0.0), -0.073813340025, 1e-9, "c-log3 code 0");
    checkNear(sr::cLog3Decode(0.343389370), 0.179999999437, 1e-9, "c-log3 18% grey");
    checkNear(sr::cLog3Decode(1.0), 14.668301411196, 1e-6, "c-log3 white");

    checkNear(sr::vLogDecode(0.0), -0.022321428571, 1e-9, "v-log code 0");
    checkNear(sr::vLogDecode(0.423311449), 0.180000000432, 1e-9, "v-log 18% grey");
    checkNear(sr::vLogDecode(1.0), 46.085527956740, 1e-6, "v-log white");

    // Full input decode to linear sRGB, including the gamut matrix and the
    // negative clamp. References: colour-science matrix_RGB_to_RGB.
    {
        const double code[3] = {0.5, 0.25, 0.75};
        const double expected[3] = {0.681716281, 0.0, 4.304315652};
        double linear[3];
        sr::decodeInputToLinearSrgb(sr::InputTransfer::CLog3, code, linear);
        checkRgb3(linear, expected, 1e-5, "c-log3 to linear sRGB");
    }
    {
        const double code[3] = {0.5, 0.25, 0.75};
        const double expected[3] = {0.203067013, 0.0, 4.990814381};
        double linear[3];
        sr::decodeInputToLinearSrgb(sr::InputTransfer::VLog, code, linear);
        checkRgb3(linear, expected, 1e-5, "v-log to linear sRGB");
    }
    {
        const double code[3] = {0.410557185, 0.410557185, 0.410557185};
        const double expected[3] = {0.180000000, 0.180000000, 0.180000000};
        double linear[3];
        sr::decodeInputToLinearSrgb(sr::InputTransfer::SLog3, code, linear);
        checkRgb3(linear, expected, 1e-6, "s-log3 18% grey to linear sRGB");
    }

    // Parsing accepts the CLI spellings.
    {
        sr::InputTransfer transfer = sr::InputTransfer::Srgb;
        check(sr::parseInputTransfer("s-log3", transfer) && transfer == sr::InputTransfer::SLog3,
              "s-log3 parses");
        check(sr::parseInputTransfer("clog3", transfer) && transfer == sr::InputTransfer::CLog3,
              "clog3 parses");
        check(sr::parseInputTransfer("vlog", transfer) && transfer == sr::InputTransfer::VLog,
              "vlog parses");
        check(sr::parseInputTransfer("apple-log", transfer) &&
                  transfer == sr::InputTransfer::AppleLog,
              "apple-log still parses");
    }

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

    // SMPTE ST 2084 (PQ), reference values from colour-science 0.4.7.
    checkNear(sr::pqEncode(100.0), 0.508078422, 1e-8, "PQ 100 nits");
    checkNear(sr::pqEncode(203.0), 0.580688881, 1e-8, "PQ 203 nits");
    checkNear(sr::pqEncode(1000.0), 0.751827096, 1e-8, "PQ 1000 nits");
    checkNear(sr::pqEncode(10000.0), 1.0, 1e-9, "PQ 10000 nits is code 1");
    check(sr::pqEncode(0.0) < 1e-5, "PQ black is near zero");
    for (double nits : {0.5, 10.0, 100.0, 1000.0}) {
        checkNear(sr::pqDecode(sr::pqEncode(nits)), nits, 1e-6, "PQ round trip");
    }

    // ITU-R BT.2100 HLG OETF.
    checkNear(sr::hlgEncode(0.05), 0.387298335, 1e-8, "HLG 0.05");
    checkNear(sr::hlgEncode(0.18), 0.672358132, 1e-8, "HLG 0.18");
    checkNear(sr::hlgEncode(0.5), 0.871643471, 1e-8, "HLG 0.5");
    checkNear(sr::hlgEncode(1.0), 1.0, 1e-7, "HLG 1.0");
    for (double value : {0.01, 0.1, 0.5, 0.9}) {
        checkNear(sr::hlgDecode(sr::hlgEncode(value)), value, 1e-9, "HLG round trip");
    }

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
