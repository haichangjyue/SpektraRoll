// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 spektravideo contributors
#include "sv/image.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace sv {
namespace {

unsigned char toByte(float v) {
    return static_cast<unsigned char>(std::lround(std::min(1.0f, std::max(0.0f, v)) * 255.0f));
}

}  // namespace

bool loadImage(const std::string& path, Image& out, std::string& error) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 3);
    if (data == nullptr) {
        error = std::string("cannot decode image: ") + path + " (" +
                (stbi_failure_reason() ? stbi_failure_reason() : "unknown") + ")";
        return false;
    }

    out.width = width;
    out.height = height;
    out.pixels.resize(static_cast<size_t>(width) * height * 3);
    for (size_t i = 0; i < out.pixels.size(); ++i) {
        out.pixels[i] = static_cast<float>(data[i]) / 255.0f;
    }
    stbi_image_free(data);
    return true;
}

bool saveImage(const std::string& path, const Image& image, std::string& error) {
    if (!image.valid()) {
        error = "refusing to write an empty image";
        return false;
    }

    std::vector<unsigned char> bytes(image.pixels.size());
    for (size_t i = 0; i < image.pixels.size(); ++i) {
        bytes[i] = toByte(image.pixels[i]);
    }

    // stb writes 8-bit PNG/JPEG/BMP/TGA based on the extension.
    if (stbi_write_png(path.c_str(), image.width, image.height, 3, bytes.data(),
                       image.width * 3) == 0) {
        error = "cannot write image: " + path;
        return false;
    }
    return true;
}

Image makeTestChart(int width, int height) {
    width = std::max(64, width);
    height = std::max(64, height);

    Image image;
    image.width = width;
    image.height = height;
    image.pixels.assign(static_cast<size_t>(width) * height * 3, 0.0f);

    const int rampHeight = height * 55 / 100;
    for (int y = 0; y < rampHeight; ++y) {
        for (int x = 0; x < width; ++x) {
            const float t = static_cast<float>(x) / static_cast<float>(width - 1);
            float* px = image.at(x, y);
            // Horizontal grey ramp, split into 11 steps so banding is obvious.
            const float stepped = std::round(t * 10.0f) / 10.0f;
            px[0] = px[1] = px[2] = stepped;
        }
    }

    // Colour patches: primaries, secondaries, a few skin/foliage-ish tones.
    const Rgb patches[] = {
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f},
        {0.85f, 0.65f, 0.55f}, {0.35f, 0.55f, 0.25f}, {0.20f, 0.28f, 0.55f},
        {0.90f, 0.75f, 0.20f}, {0.55f, 0.30f, 0.20f}, {0.12f, 0.12f, 0.12f},
    };
    const int patchCount = static_cast<int>(sizeof(patches) / sizeof(patches[0]));
    const int cols = 6;
    const int rows = (patchCount + cols - 1) / cols;
    const int areaTop = rampHeight;
    const int areaHeight = height - areaTop;
    const int cellW = width / cols;
    const int cellH = areaHeight / rows;

    for (int i = 0; i < patchCount; ++i) {
        const int cx = i % cols;
        const int cy = i / cols;
        const int x0 = cx * cellW;
        const int y0 = areaTop + cy * cellH;
        const int x1 = std::min(width, x0 + cellW);
        const int y1 = std::min(height, y0 + cellH);
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                float* px = image.at(x, y);
                px[0] = patches[i].r;
                px[1] = patches[i].g;
                px[2] = patches[i].b;
            }
        }
    }

    return image;
}

}  // namespace sv
