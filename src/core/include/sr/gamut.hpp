// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include "sr/lut3d.hpp"

namespace sr {

struct GamutCompressParams {
    // Knee on the normalized chroma d = C / C_max. Upstream starts at 0
    // (compress everything); we start at 0.8 so in-gamut colour is left alone
    // and only the last 20% plus anything outside gets rolled in (D027).
    double threshold = 0.8;
    double limit = 1.0;
    double power = 2.0;
};

// Reinhard knee: identity below `threshold`, asymptotic at `limit` above it.
double reinhardKnee(double d, const GamutCompressParams& params);

// Map an out-of-gamut linear sRGB triple into the sRGB cube by reducing OkLCh
// chroma at constant lightness and hue. In-gamut colours (all channels in
// [0, 1]) are returned unchanged.
Rgb compressGamutOklch(Rgb linear, const GamutCompressParams& params = {});

// OkLab <-> linear sRGB, exposed for tests.
void linearSrgbToOklab(const double rgb[3], double lab[3]);
void oklabToLinearSrgb(const double lab[3], double rgb[3]);

}  // namespace sr
