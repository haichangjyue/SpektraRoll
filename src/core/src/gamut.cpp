// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include "sr/gamut.hpp"

#include <algorithm>
#include <cmath>

namespace sr {
namespace {

// Ottosson's OkLab, linear sRGB (D65) -> LMS -> cube root -> Lab.
// Constants from https://bottosson.github.io/posts/oklab/.
constexpr double kRgbToLms[3][3] = {
    {0.4122214708, 0.5363325363, 0.0514459929},
    {0.2119034982, 0.6806995451, 0.1073969566},
    {0.0883024619, 0.2817188376, 0.6299787005},
};

constexpr double kLmsToLab[3][3] = {
    {0.2104542553, 0.7936177850, -0.0040720468},
    {1.9779984951, -2.4285922050, 0.4505937099},
    {0.0259040371, 0.7827717662, -0.8086757660},
};

constexpr double kLabToLms[3][3] = {
    {1.0, 0.3963377774, 0.2158037573},
    {1.0, -0.1055613458, -0.0638541728},
    {1.0, -0.0894841775, -1.2914855480},
};

constexpr double kLmsToRgb[3][3] = {
    {4.0767416621, -3.3077115913, 0.2309699292},
    {-1.2684380046, 2.6097574011, -0.3413193965},
    {-0.0041960863, -0.7034186147, 1.7076147010},
};

bool inGamut(const double rgb[3]) {
    constexpr double kEps = 1e-7;
    for (int c = 0; c < 3; ++c) {
        if (rgb[c] < -kEps || rgb[c] > 1.0 + kEps) return false;
    }
    return true;
}

void oklchToLinearSrgb(double lightness, double chroma, double hue, double rgb[3]) {
    const double lab[3] = {lightness, chroma * std::cos(hue), chroma * std::sin(hue)};
    oklabToLinearSrgb(lab, rgb);
}

// Largest OkLCh chroma that still lands inside the sRGB cube at this
// lightness and hue. Bisection is enough: the cube is convex in linear RGB.
double maxChroma(double lightness, double hue) {
    if (lightness <= 0.0 || lightness >= 1.0) return 0.0;
    double rgb[3];
    oklchToLinearSrgb(lightness, 0.0, hue, rgb);
    if (!inGamut(rgb)) return 0.0;
    double low = 0.0;
    double high = 0.5;
    while (high < 4.0) {
        oklchToLinearSrgb(lightness, high, hue, rgb);
        if (!inGamut(rgb)) break;
        low = high;
        high *= 2.0;
    }
    for (int i = 0; i < 40; ++i) {
        const double mid = 0.5 * (low + high);
        oklchToLinearSrgb(lightness, mid, hue, rgb);
        if (inGamut(rgb)) {
            low = mid;
        } else {
            high = mid;
        }
    }
    return low;
}

}  // namespace

void linearSrgbToOklab(const double rgb[3], double lab[3]) {
    double lms[3];
    for (int c = 0; c < 3; ++c) {
        lms[c] = kRgbToLms[c][0] * rgb[0] + kRgbToLms[c][1] * rgb[1] + kRgbToLms[c][2] * rgb[2];
    }
    for (int c = 0; c < 3; ++c) lms[c] = std::cbrt(lms[c]);
    for (int c = 0; c < 3; ++c) {
        lab[c] = kLmsToLab[c][0] * lms[0] + kLmsToLab[c][1] * lms[1] + kLmsToLab[c][2] * lms[2];
    }
}

void oklabToLinearSrgb(const double lab[3], double rgb[3]) {
    double lms[3];
    for (int c = 0; c < 3; ++c) {
        lms[c] = kLabToLms[c][0] * lab[0] + kLabToLms[c][1] * lab[1] + kLabToLms[c][2] * lab[2];
    }
    for (int c = 0; c < 3; ++c) lms[c] = lms[c] * lms[c] * lms[c];
    for (int c = 0; c < 3; ++c) {
        rgb[c] = kLmsToRgb[c][0] * lms[0] + kLmsToRgb[c][1] * lms[1] + kLmsToRgb[c][2] * lms[2];
    }
}

double reinhardKnee(double d, const GamutCompressParams& params) {
    if (!(d > params.threshold)) return d;
    const double scale = params.limit - params.threshold;
    if (!(scale > 0.0)) return params.limit;
    const double x = (d - params.threshold) / scale;
    const double y = x / std::pow(1.0 + std::pow(x, params.power), 1.0 / params.power);
    return params.threshold + scale * y;
}

Rgb compressGamutOklch(Rgb linear, const GamutCompressParams& params) {
    const double rgb[3] = {linear.r, linear.g, linear.b};
    if (inGamut(rgb)) return linear;

    double lab[3];
    linearSrgbToOklab(rgb, lab);
    const double lightness = lab[0];
    const double a = lab[1];
    const double b = lab[2];
    const double chroma = std::hypot(a, b);
    if (!(chroma > 1e-9) || !(lightness > 0.0) || !(lightness < 1.0)) {
        // Neutral or outside the lightness range: leave it, the caller clamps.
        return linear;
    }

    const double hue = std::atan2(b, a);
    const double cMax = maxChroma(lightness, hue);
    if (!(cMax > 1e-9)) return linear;

    const double compressed = reinhardKnee(chroma / cMax, params);
    const double newChroma = compressed * cMax;
    double out[3];
    oklchToLinearSrgb(lightness, newChroma, hue, out);
    for (int c = 0; c < 3; ++c) {
        out[c] = std::min(1.0, std::max(0.0, out[c]));
    }
    return Rgb{static_cast<float>(out[0]), static_cast<float>(out[1]), static_cast<float>(out[2])};
}

}  // namespace sr
