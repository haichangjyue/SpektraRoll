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
    return false;
}

const char* inputTransferName(InputTransfer value) {
    return value == InputTransfer::AppleLog ? "apple-log" : "srgb";
}

}  // namespace sr
