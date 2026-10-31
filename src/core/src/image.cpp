// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include "sr/image.hpp"

#include "sr/lut3d.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// stb_image_write keeps this out of the public header but defines it with
// STBIWDEF, so it is a normal extern "C" symbol. We reuse stb's deflate
// implementation for our own 16-bit PNG writer.
extern "C" unsigned char* stbi_zlib_compress(unsigned char* data, int data_len, int* out_len,
                                             int quality);

namespace sr {
namespace {

unsigned char toByte(float v) {
    return static_cast<unsigned char>(std::lround(std::min(1.0f, std::max(0.0f, v)) * 255.0f));
}

unsigned short toShort(float v) {
    return static_cast<unsigned short>(
        std::lround(std::min(1.0f, std::max(0.0f, v)) * 65535.0f));
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string extensionOf(const std::string& path) {
    const auto dot = path.find_last_of('.');
    return dot == std::string::npos ? std::string() : lower(path.substr(dot));
}

uint32_t crc32(const unsigned char* data, size_t length) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        ready = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) c = table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void putU32(std::vector<unsigned char>& out, uint32_t value) {
    out.push_back(static_cast<unsigned char>(value >> 24));
    out.push_back(static_cast<unsigned char>(value >> 16));
    out.push_back(static_cast<unsigned char>(value >> 8));
    out.push_back(static_cast<unsigned char>(value));
}

bool writeChunk(std::FILE* file, const char type[5], const std::vector<unsigned char>& data) {
    std::vector<unsigned char> header;
    putU32(header, static_cast<uint32_t>(data.size()));
    if (std::fwrite(header.data(), 1, header.size(), file) != header.size()) return false;

    std::vector<unsigned char> crcInput(type, type + 4);
    crcInput.insert(crcInput.end(), data.begin(), data.end());
    const uint32_t crc = crc32(crcInput.data(), crcInput.size());

    if (std::fwrite(type, 1, 4, file) != 4) return false;
    if (!data.empty() && std::fwrite(data.data(), 1, data.size(), file) != data.size()) return false;

    std::vector<unsigned char> tail;
    putU32(tail, crc);
    return std::fwrite(tail.data(), 1, tail.size(), file) == tail.size();
}

// Minimal 16-bit RGB PNG writer: one IDAT, filter type 0 on every scanline.
bool writePng16(const std::string& path, const Image& image, std::string& error) {
    const size_t width = static_cast<size_t>(image.width);
    const size_t height = static_cast<size_t>(image.height);
    const size_t rowBytes = 1 + width * 6;  // filter byte + 3 * 2 bytes

    std::vector<unsigned char> scanlines(rowBytes * height);
    for (size_t y = 0; y < height; ++y) {
        unsigned char* row = &scanlines[y * rowBytes];
        row[0] = 0;  // filter: none
        for (size_t x = 0; x < width; ++x) {
            const float* px = image.at(static_cast<int>(x), static_cast<int>(y));
            for (int c = 0; c < 3; ++c) {
                const unsigned short value = toShort(px[c]);
                row[1 + x * 6 + static_cast<size_t>(c) * 2] =
                    static_cast<unsigned char>(value >> 8);
                row[2 + x * 6 + static_cast<size_t>(c) * 2] =
                    static_cast<unsigned char>(value & 0xFF);
            }
        }
    }

    int compressedLength = 0;
    unsigned char* compressed = stbi_zlib_compress(
        scanlines.data(), static_cast<int>(scanlines.size()), &compressedLength, 8);
    if (compressed == nullptr) {
        error = "cannot compress 16-bit PNG data";
        return false;
    }

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        std::free(compressed);
        error = "cannot write image: " + path;
        return false;
    }

    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    bool ok = std::fwrite(signature, 1, sizeof(signature), file) == sizeof(signature);

    std::vector<unsigned char> ihdr;
    putU32(ihdr, static_cast<uint32_t>(width));
    putU32(ihdr, static_cast<uint32_t>(height));
    ihdr.push_back(16);  // bit depth
    ihdr.push_back(2);   // colour type: truecolour
    ihdr.push_back(0);   // compression: deflate
    ihdr.push_back(0);   // filter method
    ihdr.push_back(0);   // interlace: none
    ok = ok && writeChunk(file, "IHDR", ihdr);

    std::vector<unsigned char> idat(compressed, compressed + compressedLength);
    std::free(compressed);
    ok = ok && writeChunk(file, "IDAT", idat);
    ok = ok && writeChunk(file, "IEND", {});

    if (std::fclose(file) != 0) ok = false;
    if (!ok) error = "cannot write image: " + path;
    return ok;
}

}  // namespace

bool loadImage(const std::string& path, Image& out, std::string& error, int* sourceBits) {
    int width = 0;
    int height = 0;
    int channels = 0;

    const bool is16 = stbi_is_16_bit(path.c_str()) != 0;
    stbi_us* data16 = nullptr;
    stbi_uc* data8 = nullptr;
    if (is16) {
        data16 = stbi_load_16(path.c_str(), &width, &height, &channels, 3);
    } else {
        data8 = stbi_load(path.c_str(), &width, &height, &channels, 3);
    }
    if (data16 == nullptr && data8 == nullptr) {
        error = std::string("cannot decode image: ") + path + " (" +
                (stbi_failure_reason() ? stbi_failure_reason() : "unknown") + ")";
        return false;
    }

    out.width = width;
    out.height = height;
    out.pixels.resize(static_cast<size_t>(width) * height * 3);
    if (is16) {
        for (size_t i = 0; i < out.pixels.size(); ++i) {
            out.pixels[i] = static_cast<float>(data16[i]) / 65535.0f;
        }
        stbi_image_free(data16);
    } else {
        for (size_t i = 0; i < out.pixels.size(); ++i) {
            out.pixels[i] = static_cast<float>(data8[i]) / 255.0f;
        }
        stbi_image_free(data8);
    }
    if (sourceBits != nullptr) *sourceBits = is16 ? 16 : 8;
    return true;
}

bool saveImage(const std::string& path, const Image& image, std::string& error, int bits) {
    if (!image.valid()) {
        error = "refusing to write an empty image";
        return false;
    }
    if (bits != 8 && bits != 16) {
        error = "bit depth must be 8 or 16";
        return false;
    }
    if (bits == 16) {
        if (extensionOf(path) != ".png") {
            error = "16-bit output is only supported for .png files";
            return false;
        }
        return writePng16(path, image, error);
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

}  // namespace sr
