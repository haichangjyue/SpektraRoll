// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <string>
#include <vector>

namespace sr {

// Interleaved RGB float image, nominally 0..1.
//
// 8-bit files are scaled by 255, 16-bit files by 65535, so the in-memory
// representation is identical and precision is only lost at write time.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<float> pixels;  // width * height * 3

    bool valid() const {
        return width > 0 && height > 0 &&
               pixels.size() == static_cast<size_t>(width) * height * 3;
    }

    float* at(int x, int y) { return &pixels[(static_cast<size_t>(y) * width + x) * 3]; }
    const float* at(int x, int y) const {
        return &pixels[(static_cast<size_t>(y) * width + x) * 3];
    }
};

// `sourceBits` (optional) receives 8, 16 or 32, the bit depth actually decoded
// (32 for a float/half EXR).
bool loadImage(const std::string& path, Image& out, std::string& error,
               int* sourceBits = nullptr);
// `bits` is 8 or 16 for PNG. A path ending in `.exr` is always written as
// OpenEXR: 32-bit float by default, 16-bit half with `exrHalf`. Unlike PNG the
// EXR path does not clamp, so out-of-range values survive (D055).
bool saveImage(const std::string& path, const Image& image, std::string& error, int bits = 8,
               bool exrHalf = false);

// Synthetic test chart: grey ramp on top, colour patches below. Used by the
// `gen` command and by the demo, so there is always something to feed the LUT.
Image makeTestChart(int width, int height);

}  // namespace sr
