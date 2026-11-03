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

    // D061 camera logs. References: colour-science 0.4.7
    // (log_decoding_ARRILogC3 EI 800 SUP 3.x, log_decoding_ARRILogC4,
    // oetf_inverse_BlackmagicFilmGeneration5, oetf_inverse_DaVinciIntermediate).
    checkNear(sr::arriLogC3Decode(0.0), -0.017290418255, 1e-9, "logc3 code 0");
    checkNear(sr::arriLogC3Decode(0.391006832034), 0.18, 1e-9, "logc3 18% grey");
    checkNear(sr::arriLogC3Decode(0.25), 0.041519607534, 1e-9, "logc3 mid");
    checkNear(sr::arriLogC3Decode(1.0), 55.079576698813, 1e-6, "logc3 white");

    checkNear(sr::arriLogC4Decode(0.0), -0.018056996120, 1e-9, "logc4 code 0");
    checkNear(sr::arriLogC4Decode(0.278395836548), 0.18, 1e-9, "logc4 18% grey");
    checkNear(sr::arriLogC4Decode(0.75), 32.366195818159, 1e-6, "logc4 high");
    checkNear(sr::arriLogC4Decode(1.0), 469.8, 1e-6, "logc4 white");

    checkNear(sr::bmdFilmGen5Decode(0.0), -0.011162500266, 1e-9, "bmd-film code 0");
    checkNear(sr::bmdFilmGen5Decode(0.383561643836), 0.18, 1e-9, "bmd-film 18% grey");
    checkNear(sr::bmdFilmGen5Decode(0.1), 0.000909537059, 1e-9, "bmd-film low");
    checkNear(sr::bmdFilmGen5Decode(1.0), 222.860944203808, 1e-6, "bmd-film white");

    checkNear(sr::daVinciIntermediateDecode(0.0), 0.0, 1e-12, "davinci code 0");
    checkNear(sr::daVinciIntermediateDecode(0.336043272385), 0.18, 1e-9, "davinci 18% grey");
    checkNear(sr::daVinciIntermediateDecode(0.02), 0.001914925866, 1e-9, "davinci low");
    checkNear(sr::daVinciIntermediateDecode(1.0), 100.000012279423, 1e-6, "davinci white");

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
    // D061: gamut matrices. A neutral 18% grey must stay neutral, and a mixed
    // code triple must land on the colour colour-science computes.
    {
        const double code[3] = {0.5, 0.25, 0.75};
        const sr::InputTransfer inputs[] = {
            sr::InputTransfer::SLog3SGamut3, sr::InputTransfer::ArriLogC3,
            sr::InputTransfer::ArriLogC4, sr::InputTransfer::BmdFilmGen5,
            sr::InputTransfer::DaVinciIntermediate};
        const double expected[][3] = {
            {0.414270154, 0.0, 4.721026600},   {0.377921789, 0.0, 6.666029393},
            {0.441870159, 0.0, 37.657375902},  {0.508915290, 0.0, 16.293651977},
            {0.603499084, 0.0, 13.372073306},
        };
        const char* names[] = {"s-log3 + s-gamut3", "logc3", "logc4", "bmd-film", "davinci"};
        for (size_t i = 0; i < 5; ++i) {
            double linear[3];
            sr::decodeInputToLinearSrgb(inputs[i], code, linear);
            checkRgb3(linear, expected[i], 1e-6, names[i]);
        }

        const double greyCode[] = {0.391006832034, 0.278395836548, 0.383561643836,
                                   0.336043272385};
        const sr::InputTransfer greyInputs[] = {
            sr::InputTransfer::ArriLogC3, sr::InputTransfer::ArriLogC4,
            sr::InputTransfer::BmdFilmGen5, sr::InputTransfer::DaVinciIntermediate};
        for (size_t i = 0; i < 4; ++i) {
            const double grey[3] = {greyCode[i], greyCode[i], greyCode[i]};
            double linear[3];
            sr::decodeInputToLinearSrgb(greyInputs[i], grey, linear);
            // The matrices come from colour-science with its own rounded D65
            // white, so "neutral" lands within 3e-5 of 0.18, exactly like the
            // BT.2020 check above (0.179971790881 ...).
            const double neutral[3] = {0.18, 0.18, 0.18};
            checkRgb3(linear, neutral, 1e-4, "18% grey stays neutral");
        }
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
        // D061 spellings: case, dash and underscore variants, plus the aliases.
        check(sr::parseInputTransfer("LogC3", transfer) &&
                  transfer == sr::InputTransfer::ArriLogC3,
              "LogC3 parses");
        check(sr::parseInputTransfer("arri_logc4", transfer) &&
                  transfer == sr::InputTransfer::ArriLogC4,
              "arri_logc4 parses");
        check(sr::parseInputTransfer("bmd-film-gen5", transfer) &&
                  transfer == sr::InputTransfer::BmdFilmGen5,
              "bmd-film-gen5 parses");
        check(sr::parseInputTransfer("davinci-intermediate", transfer) &&
                  transfer == sr::InputTransfer::DaVinciIntermediate,
              "davinci-intermediate parses");
        check(sr::parseInputTransfer("s_gamut3", transfer) &&
                  transfer == sr::InputTransfer::SLog3SGamut3,
              "s_gamut3 parses");
        check(std::string(sr::inputTransferName(sr::InputTransfer::ArriLogC4)) == "logc4",
              "logc4 name");
        check(std::string(sr::inputTransferName(sr::InputTransfer::DaVinciIntermediate)) ==
                  "davinci",
              "davinci name");
        check(sr::isCameraLog(sr::InputTransfer::BmdFilmGen5), "bmd-film is a camera log");
        check(!sr::isCameraLog(sr::InputTransfer::Srgb), "srgb is not a camera log");
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
