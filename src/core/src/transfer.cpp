// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include "sr/transfer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

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

// D061: ARRI Wide Gamut 3 / 4, Blackmagic Wide Gamut and DaVinci Wide Gamut.
// Same source as the matrices above (colour-science 0.4.7, D65 -> D65, no
// chromatic adaptation needed). Blackmagic stores its white point with more
// digits (0.312717, 0.3290312), so its matrix is CAT02-adapted to sRGB's D65;
// a neutral Blackmagic pixel then stays exactly neutral.
constexpr double kSGamut3ToSrgb[3][3] = {
    {1.877703304158600, -0.794072272012260, -0.083787749334960},
    {-0.176609064916940, 1.350911870533990, -0.174203656726040},
    {-0.026158248305760, -0.148431249773440, 1.174663935814270},
};

constexpr double kArriWideGamut3ToSrgb[3][3] = {
    {1.617341953200000, -0.537215450400000, -0.080283108000000},
    {-0.070402521000000, 1.334552331200000, -0.264050721600000},
    {-0.021064084400000, -0.226959489200000, 1.248098278800000},
};

constexpr double kArriWideGamut4ToSrgb[3][3] = {
    {1.892909309143406, -0.780786358195487, -0.112279668273147},
    {-0.205500776543893, 1.340171181030512, -0.134571255550448},
    {-0.012662323539739, -0.152193809032254, 1.164930570261962},
};

constexpr double kBmdWideGamutToSrgb[3][3] = {
    {1.568229090933787, -0.522604320339746, -0.045781487919271},
    {-0.086193852150434, 1.344854361752704, -0.258561360666100},
    {-0.052011751203802, -0.249161501129390, 1.301247690023161},
};

constexpr double kDaVinciWideGamutToSrgb[3][3] = {
    {1.898404857216630, -0.792072675468742, -0.106488899073117},
    {-0.168748493825520, 1.488888136041571, -0.320040493279880},
    {-0.121499307902914, -0.315689508174336, 1.437263253767220},
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

double arriLogC3Decode(double encoded) {
    // ARRI LogC3, SUP 3.x firmware, EI 800, "Linear Scene Exposure Factor"
    // (colour-science log_decoding_ARRILogC3 defaults). 18% grey = 0.3910068320.
    constexpr double cut = 0.010591;
    constexpr double a = 5.555556;
    constexpr double b = 0.052272;
    constexpr double c = 0.247190;
    constexpr double d = 0.385537;
    constexpr double e = 5.367655;
    constexpr double f = 0.092809;
    const double t = encoded;
    if (t > e * cut + f) return (std::pow(10.0, (t - d) / c) - b) / a;
    return (t - f) / e;
}

double arriLogC4Decode(double encoded) {
    // ARRI LogC4 (colour-science CONSTANTS_ARRILOGC4). 18% grey = 0.2783958365.
    // The curve has no cut: non-negative code values use the log branch and
    // below-black values a linear extension.
    constexpr double a = 2231.8263090676883;
    constexpr double b = 0.9071358748778103;
    constexpr double c = 0.09286412512218964;
    constexpr double s = 0.1135972086105891;
    constexpr double t = -0.01805699611991131;
    const double ep = encoded;
    if (ep >= 0.0) return (std::exp2(14.0 * ((ep - c) / b) + 6.0) - 64.0) / a;
    return ep * s + t;
}

double bmdFilmGen5Decode(double encoded) {
    // Blackmagic Film Generation 5 (CONSTANTS_BLACKMAGIC_FILM_GENERATION_5).
    // 18% grey = 0.3835616438.
    constexpr double A = 0.08692876065491224;
    constexpr double B = 0.005494072432257808;
    constexpr double C = 0.5300133392291939;
    constexpr double D = 8.283605932402494;
    constexpr double E = 0.09246575342465753;
    constexpr double linCut = 0.005;
    constexpr double logCut = D * linCut + E;
    const double y = encoded;
    if (y < logCut) return (y - E) / D;
    return std::exp((y - C) / A) - B;
}

double daVinciIntermediateDecode(double encoded) {
    // DaVinci Intermediate (CONSTANTS_DAVINCI_INTERMEDIATE). 18% grey =
    // 0.3360432724.
    constexpr double m = 10.44426855;
    constexpr double a = 0.0075;
    constexpr double b = 7.0;
    constexpr double c = 0.07329248;
    constexpr double logCut = 0.02740668;
    const double v = encoded;
    if (v <= logCut) return v / m;
    return std::exp2(v / c - b) - a;
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
        case InputTransfer::SLog3SGamut3:
            for (int c = 0; c < 3; ++c) camera[c] = slog3Decode(encoded[c]);
            matrix = kSGamut3ToSrgb;
            break;
        case InputTransfer::ArriLogC3:
            for (int c = 0; c < 3; ++c) camera[c] = arriLogC3Decode(encoded[c]);
            matrix = kArriWideGamut3ToSrgb;
            break;
        case InputTransfer::ArriLogC4:
            for (int c = 0; c < 3; ++c) camera[c] = arriLogC4Decode(encoded[c]);
            matrix = kArriWideGamut4ToSrgb;
            break;
        case InputTransfer::BmdFilmGen5:
            for (int c = 0; c < 3; ++c) camera[c] = bmdFilmGen5Decode(encoded[c]);
            matrix = kBmdWideGamutToSrgb;
            break;
        case InputTransfer::DaVinciIntermediate:
            for (int c = 0; c < 3; ++c) camera[c] = daVinciIntermediateDecode(encoded[c]);
            matrix = kDaVinciWideGamutToSrgb;
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

namespace {

std::string normaliseInputName(const std::string& name) {
    std::string value;
    value.reserve(name.size());
    for (const char raw : name) {
        const unsigned char ch = static_cast<unsigned char>(raw);
        // Dashes and underscores are interchangeable ("s_log3" == "s-log3").
        value.push_back(ch == '_' ? '-' : static_cast<char>(std::tolower(ch)));
    }
    return value;
}

}  // namespace

bool parseInputTransfer(const std::string& name, InputTransfer& out) {
    const std::string key = normaliseInputName(name);
    struct Spelling {
        const char* text;
        InputTransfer value;
    };
    static constexpr Spelling kSpellings[] = {
        {"srgb", InputTransfer::Srgb},
        {"apple-log", InputTransfer::AppleLog},
        {"applelog", InputTransfer::AppleLog},
        {"s-log3", InputTransfer::SLog3},
        {"slog3", InputTransfer::SLog3},
        {"s-log3-sgamut3", InputTransfer::SLog3SGamut3},
        {"slog3-sgamut3", InputTransfer::SLog3SGamut3},
        {"s-gamut3", InputTransfer::SLog3SGamut3},
        {"sgamut3", InputTransfer::SLog3SGamut3},
        {"c-log3", InputTransfer::CLog3},
        {"clog3", InputTransfer::CLog3},
        {"v-log", InputTransfer::VLog},
        {"vlog", InputTransfer::VLog},
        {"logc3", InputTransfer::ArriLogC3},
        {"log-c3", InputTransfer::ArriLogC3},
        {"arri-logc3", InputTransfer::ArriLogC3},
        {"logc4", InputTransfer::ArriLogC4},
        {"log-c4", InputTransfer::ArriLogC4},
        {"arri-logc4", InputTransfer::ArriLogC4},
        {"bmd-film", InputTransfer::BmdFilmGen5},
        {"bmdfilm", InputTransfer::BmdFilmGen5},
        {"bmd-film-gen5", InputTransfer::BmdFilmGen5},
        {"blackmagic-film", InputTransfer::BmdFilmGen5},
        {"davinci", InputTransfer::DaVinciIntermediate},
        {"davinci-intermediate", InputTransfer::DaVinciIntermediate},
    };
    for (const Spelling& spelling : kSpellings) {
        if (key == spelling.text) {
            out = spelling.value;
            return true;
        }
    }
    return false;
}

const char* inputTransferName(InputTransfer value) {
    switch (value) {
        case InputTransfer::AppleLog: return "apple-log";
        case InputTransfer::SLog3: return "s-log3";
        case InputTransfer::SLog3SGamut3: return "s-log3-sgamut3";
        case InputTransfer::CLog3: return "c-log3";
        case InputTransfer::VLog: return "v-log";
        case InputTransfer::ArriLogC3: return "logc3";
        case InputTransfer::ArriLogC4: return "logc4";
        case InputTransfer::BmdFilmGen5: return "bmd-film";
        case InputTransfer::DaVinciIntermediate: return "davinci";
        case InputTransfer::Srgb: break;
    }
    return "srgb";
}

const char* inputTransferList() {
    return "srgb, apple-log, s-log3, s-log3-sgamut3, c-log3, v-log, logc3, logc4, "
           "bmd-film, davinci";
}

bool isCameraLog(InputTransfer value) { return value != InputTransfer::Srgb; }

}  // namespace sr
