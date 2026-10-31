// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <cstddef>
#include <string>

#include "sr/lut3d.hpp"

namespace sr {

struct LutComparison {
    size_t samples = 0;
    double mse = 0.0;        // mean squared error over encoded RGB, 0..1
    double psnr = 0.0;       // dB; infinity when the tables are identical
    double maxAbs = 0.0;     // largest absolute channel difference
    double meanAbs = 0.0;
    double meanDeltaE = 0.0; // CIE76 on sRGB/D65 Lab
    double p95DeltaE = 0.0;
    double maxDeltaE = 0.0;
    Rgb worstInput;          // grid coordinate of the largest absolute error
};

// Compares two LUTs entry by entry. Both must be the same size (the M1 bakes
// always are); the domain is treated as the unit cube.
bool compareLuts(const Lut3D& a, const Lut3D& b, LutComparison& out, std::string& error);

}  // namespace sr
