// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include "sr/transfer.hpp"

#include <algorithm>
#include <cmath>

namespace sr {
namespace {

// Apple Log Profile constants (Apple white paper, colour-science 0.4.7).
constexpr double kR0 = -0.05641088;
constexpr double kRt = 0.01;
constexpr double kSigma = 47.28711236;
constexpr double kBeta = 0.00964052;
constexpr double kGamma = 0.08550479;
constexpr double kDelta = 0.69336945;

// Linear BT.2020 (D65) -> linear sRGB (D65). Composed the same way upstream
// composes it: colour-science's RGB_to_RGB(BT2020, sRGB), i.e. the BT.2020
// RGB->XYZ matrix followed by the sRGB XYZ->RGB matrix (D65 -> D65 needs no
// chromatic adaptation). Verified against colour 0.4.7 to 1e-12.
constexpr double kBt2020ToSrgb[3][3] = {
    {1.660303485421445, -0.587570142531617, -0.072890060215056},
    {-0.124375595308386, 1.132834481431958, -0.008359737187401},
    {-0.018112279959916, -0.100583608507212, 1.118770326157098},
};

// Camera gamut -> linear Rec.709 (D65), from colour-science 0.4.7
// (matrix_RGB_to_RGB with chromatic adaptation off; all three are D65).
constexpr double kSGamut3CineToSrgb[3][3] = {
    {1.626947409865913, -0.540138538842170, -0.086808870733380},
    {-0.178515527182660, 1.417940927396144, -0.239425400446255},
    {-0.044436114956758, -0.195919966185631, 1.240356081207883},
};

constexpr double kCinemaGamutToSrgb[3][3] = {
    {1.923861295898551, -0.798760663248431, -0.125100632650120},
    {-0.204310848200921, 1.495898509750939, -0.291587661550018},
    {-0.023685020970341, -0.420127011030270, 1.443812032000612},
};

constexpr double kVGamutToSrgb[3][3] = {
    {1.806573570346048, -0.695698131346300, -0.110878567820157},
    {-0.170089035422901, 1.305954766066613, -0.135864821747590},
    {-0.025205692416949, -0.154467764144244, 1.179673668428944},
};

void multiplyMatrix(const double matrix[3][3], const double in[3], double out[3]) {
    for (int c = 0; c < 3; ++c) {
        out[c] = matrix[c][0] * in[0] + matrix[c][1] * in[1] + matrix[c][2] * in[2];
    }
}

double encodeSrgbUnclamped(double value) {
    if (value <= 0.0031308) return value * 12.92;
    return 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
}

}  // namespace

double srgbDecode(double encoded) {
    const double x = std::min(1.0, std::max(0.0, encoded));
    return x <= 0.04045 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4);
}

double srgbEncode(double linear) { return encodeSrgbUnclamped(linear); }

double appleLogEncode(double linear) {
    if (linear < kR0) return 0.0;
    if (linear < kRt) {
        const double t = linear - kR0;
        return kSigma * t * t;
    }
    return kGamma * std::log2(linear + kBeta) + kDelta;
}

double appleLogDecode(double encoded) {
    const double pT = kSigma * (kRt - kR0) * (kRt - kR0);
    if (encoded < 0.0) return kR0;
    if (encoded < pT) return std::sqrt(encoded / kSigma) + kR0;
    return std::exp2((encoded - kDelta) / kGamma) - kBeta;
}

void bt2020ToSrgb(const double rgb2020[3], double srgb[3]) {
    for (int c = 0; c < 3; ++c) {
        srgb[c] = kBt2020ToSrgb[c][0] * rgb2020[0] + kBt2020ToSrgb[c][1] * rgb2020[1] +
                  kBt2020ToSrgb[c][2] * rgb2020[2];
    }
}

double slog3Decode(double encoded) {
    // Sony S-Log3 (normalised code values, reflection output).
    const double y = encoded;
    if (y >= 171.2102946929 / 1023.0) {
        return std::pow(10.0, (y * 1023.0 - 420.0) / 261.5) * 0.19 - 0.01;
    }
    return (y * 1023.0 - 95.0) * 0.01125 / (171.2102946929 - 95.0);
}

double cLog3Decode(double encoded) {
    // Canon Log 3 v1.2 (colour-science 0.4.7 log_decoding_CanonLog3).
    const double y = encoded;
    double x;
    if (y < 0.097465473) {
        x = -(std::pow(10.0, (0.12783901 - y) / 0.36726845) - 1.0) / 14.98325;
    } else if (y <= 0.15277891) {
        x = (y - 0.12512219) / 1.9754798;
    } else {
        x = (std::pow(10.0, (y - 0.12240537) / 0.36726845) - 1.0) / 14.98325;
    }
    return x * 0.9;  // "reflection" output, like colour-science's default
}

double vLogDecode(double encoded) {
    // Panasonic V-Log (CONSTANTS_VLOG: cut2 0.181, b 0.00873, c 0.241514,
    // d 0.598206).
    const double y = encoded;
    if (y < 0.181) return (y - 0.125) / 5.6;
    return std::pow(10.0, (y - 0.598206) / 0.241514) - 0.00873;
}

void decodeInputToLinearSrgb(InputTransfer input, const double encoded[3],
                             double linear[3]) {
    if (input == InputTransfer::Srgb) {
        for (int c = 0; c < 3; ++c) linear[c] = srgbDecode(encoded[c]);
        return;
    }

    double camera[3];
    const double (*matrix)[3] = nullptr;
    switch (input) {
        case InputTransfer::AppleLog:
            for (int c = 0; c < 3; ++c) camera[c] = appleLogDecode(encoded[c]);
            matrix = kBt2020ToSrgb;
            break;
        case InputTransfer::SLog3:
            for (int c = 0; c < 3; ++c) camera[c] = slog3Decode(encoded[c]);
            matrix = kSGamut3CineToSrgb;
            break;
        case InputTransfer::CLog3:
            for (int c = 0; c < 3; ++c) camera[c] = cLog3Decode(encoded[c]);
            matrix = kCinemaGamutToSrgb;
            break;
        case InputTransfer::VLog:
            for (int c = 0; c < 3; ++c) camera[c] = vLogDecode(encoded[c]);
            matrix = kVGamutToSrgb;
            break;
        case InputTransfer::Srgb:
            break;
    }
    multiplyMatrix(matrix, camera, linear);
    for (int c = 0; c < 3; ++c) linear[c] = std::max(0.0, linear[c]);
}

double pqEncode(double nits) {
    constexpr double m1 = 0.1593017578125;
    constexpr double m2 = 78.84375;
    constexpr double c1 = 0.8359375;
    constexpr double c2 = 18.8515625;
    constexpr double c3 = 18.6875;
    const double y = std::max(0.0, nits) / 10000.0;
    const double p = std::pow(y, m1);
    return std::pow((c1 + c2 * p) / (1.0 + c3 * p), m2);
}

double pqDecode(double code) {
    constexpr double m1 = 0.1593017578125;
    constexpr double m2 = 78.84375;
    constexpr double c1 = 0.8359375;
    constexpr double c2 = 18.8515625;
    constexpr double c3 = 18.6875;
    const double n = std::min(1.0, std::max(0.0, code));
    const double p = std::pow(n, 1.0 / m2);
    const double num = std::max(p - c1, 0.0);
    const double den = c2 - c3 * p;
    return den > 0.0 ? 10000.0 * std::pow(num / den, 1.0 / m1) : 0.0;
}

double hlgEncode(double linear) {
    constexpr double a = 0.17883277;
    constexpr double b = 0.28466892;
    constexpr double c = 0.55991073;
    const double e = std::max(0.0, linear);
    if (e <= 1.0 / 12.0) return std::sqrt(3.0 * e);
    return a * std::log(12.0 * e - b) + c;
}

double hlgDecode(double code) {
    constexpr double a = 0.17883277;
    constexpr double b = 0.28466892;
    constexpr double c = 0.55991073;
    const double v = std::min(1.0, std::max(0.0, code));
    if (v <= 0.5) return v * v / 3.0;
    return (std::exp((v - c) / a) + b) / 12.0;
}

bool parseInputTransfer(const std::string& name, InputTransfer& out) {    if (name == "srgb" || name == "sRGB" || name == "Srgb") {
        out = InputTransfer::Srgb;
        return true;
    }
    if (name == "apple-log" || name == "applelog" || name == "apple_log" ||
        name == "AppleLog") {
        out = InputTransfer::AppleLog;
        return true;
    }
    if (name == "s-log3" || name == "slog3" || name == "S-Log3" || name == "s_log3") {
        out = InputTransfer::SLog3;
        return true;
    }
    if (name == "c-log3" || name == "clog3" || name == "C-Log3" || name == "c_log3") {
        out = InputTransfer::CLog3;
        return true;
    }
    if (name == "v-log" || name == "vlog" || name == "V-Log" || name == "v_log") {
        out = InputTransfer::VLog;
        return true;
    }
    return false;
}

const char* inputTransferName(InputTransfer value) {
    switch (value) {
        case InputTransfer::AppleLog: return "apple-log";
        case InputTransfer::SLog3: return "s-log3";
        case InputTransfer::CLog3: return "c-log3";
        case InputTransfer::VLog: return "v-log";
        case InputTransfer::Srgb: break;
    }
    return "srgb";
}

}  // namespace sr
