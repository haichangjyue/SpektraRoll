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

bool parseInputTransfer(const std::string& name, InputTransfer& out) {
    if (name == "srgb" || name == "sRGB" || name == "Srgb") {
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
