// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <string>
#include <vector>

namespace sr {

// Interleaved RGB float image, nominally 0..1.
//
// MVP limitation: no colour management and no transfer-function handling.
// Values are decoded exactly as stored and written back the same way.
// 16-bit / EXR support arrives with OpenImageIO in milestone P1.
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

bool loadImage(const std::string& path, Image& out, std::string& error);
bool saveImage(const std::string& path, const Image& image, std::string& error);

// Synthetic test chart: grey ramp on top, colour patches below. Used by the
// `gen` command and by the demo, so there is always something to feed the LUT.
Image makeTestChart(int width, int height);

}  // namespace sr
