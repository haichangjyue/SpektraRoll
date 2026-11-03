// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include "sr/image.hpp"

#include "sr/lut3d.hpp"
#include "sr/transfer.hpp"

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

// --- OpenEXR scanline files (D055) ----------------------------------------
//
// A deliberately small EXR: one scanline block per row, no compression, RGB
// only, FLOAT or HALF samples. Compression would mean reimplementing the EXR
// predictor / zip block layout; uncompressed files open everywhere and the
// float path is about not clamping, not about disk size.

void putU32le(std::vector<unsigned char>& out, uint32_t value) {
    out.push_back(static_cast<unsigned char>(value));
    out.push_back(static_cast<unsigned char>(value >> 8));
    out.push_back(static_cast<unsigned char>(value >> 16));
    out.push_back(static_cast<unsigned char>(value >> 24));
}

void putU64le(std::vector<unsigned char>& out, uint64_t value) {
    putU32le(out, static_cast<uint32_t>(value & 0xFFFFFFFFu));
    putU32le(out, static_cast<uint32_t>(value >> 32));
}

void putFloatLe(std::vector<unsigned char>& out, float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    putU32le(out, bits);
}

void putCString(std::vector<unsigned char>& out, const char* text) {
    for (const char* p = text; *p != '\0'; ++p) out.push_back(static_cast<unsigned char>(*p));
    out.push_back(0);
}

void putAttribute(std::vector<unsigned char>& out, const char* name, const char* type,
                  const std::vector<unsigned char>& data) {
    putCString(out, name);
    putCString(out, type);
    putU32le(out, static_cast<uint32_t>(data.size()));
    out.insert(out.end(), data.begin(), data.end());
}

uint32_t readU32le(const std::vector<unsigned char>& data, size_t pos) {
    return static_cast<uint32_t>(data[pos]) | (static_cast<uint32_t>(data[pos + 1]) << 8) |
           (static_cast<uint32_t>(data[pos + 2]) << 16) |
           (static_cast<uint32_t>(data[pos + 3]) << 24);
}

uint64_t readU64le(const std::vector<unsigned char>& data, size_t pos) {
    return static_cast<uint64_t>(readU32le(data, pos)) |
           (static_cast<uint64_t>(readU32le(data, pos + 4)) << 32);
}

float readFloatLeAt(const unsigned char* data, size_t pos) {
    const uint32_t bits = static_cast<uint32_t>(data[pos]) |
                          (static_cast<uint32_t>(data[pos + 1]) << 8) |
                          (static_cast<uint32_t>(data[pos + 2]) << 16) |
                          (static_cast<uint32_t>(data[pos + 3]) << 24);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

// IEEE 754 binary32 -> binary16 with round-to-nearest-even. Subnormals and
// infinities are handled; NaN stays NaN.
uint16_t floatToHalf(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const uint32_t sign = (bits >> 16) & 0x8000u;
    bits &= 0x7FFFFFFFu;

    if (bits >= 0x47800000u) {  // 65536 and up
        return static_cast<uint16_t>(sign | 0x7C00u);
    }
    if (bits < 0x38800000u) {  // below 2^-14: half subnormal or zero
        if (bits < 0x33000000u) return static_cast<uint16_t>(sign);  // below 2^-25
        const uint32_t shift = 126u - (bits >> 23);
        uint32_t mantissa = (bits & 0x7FFFFFu) | 0x800000u;
        uint32_t half = mantissa >> shift;
        const uint32_t remainder = mantissa & ((1u << shift) - 1u);
        const uint32_t middle = 1u << (shift - 1u);
        if (remainder > middle || (remainder == middle && (half & 1u))) ++half;
        return static_cast<uint16_t>(sign | half);
    }

    uint32_t exponent = ((bits >> 23) - 112u) << 10u;
    uint32_t mantissa = (bits & 0x7FFFFFu) >> 13u;
    const uint32_t remainder = bits & 0x1FFFu;
    if (remainder > 0x1000u || (remainder == 0x1000u && (mantissa & 1u))) {
        ++mantissa;
        if (mantissa == 0x400u) {
            mantissa = 0;
            exponent += 0x400u;
        }
    }
    if (exponent >= 0x7C00u) return static_cast<uint16_t>(sign | 0x7C00u);
    return static_cast<uint16_t>(sign | exponent | mantissa);
}

float halfToFloat(uint16_t value) {
    const uint32_t sign = static_cast<uint32_t>(value & 0x8000u) << 16;
    uint32_t exponent = (value >> 10) & 0x1Fu;
    uint32_t mantissa = value & 0x3FFu;
    uint32_t bits = 0;
    if (exponent == 0) {
        if (mantissa == 0) {
            bits = sign;
        } else {
            exponent = 127 - 15 + 1;
            while ((mantissa & 0x400u) == 0) {
                mantissa <<= 1;
                --exponent;
            }
            mantissa &= 0x3FFu;
            bits = sign | (exponent << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) {
        bits = sign | 0x7F800000u | (mantissa << 13);
    } else {
        bits = sign | ((exponent + 112u) << 23) | (mantissa << 13);
    }
    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

// EXR ZIP pre/post-processing, matching ImfZip: split the block into its even
// bytes (first half) and odd bytes (second half), then apply a delta predictor.
// Decompression reverses both steps in the opposite order.
void exrZipSplit(std::vector<unsigned char>& data) {
    const std::vector<unsigned char> copy(data);
    const size_t half = (data.size() + 1) / 2;
    for (size_t i = 0; i < data.size(); ++i) {
        if (i % 2 == 0) {
            data[i / 2] = copy[i];
        } else {
            data[half + i / 2] = copy[i];
        }
    }
}

void exrZipMerge(std::vector<unsigned char>& data) {
    const std::vector<unsigned char> copy(data);
    const size_t half = (data.size() + 1) / 2;
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = (i % 2 == 0) ? copy[i / 2] : copy[half + i / 2];
    }
}

void exrZipPredict(std::vector<unsigned char>& data) {
    if (data.empty()) return;
    int previous = data[0];
    for (size_t i = 1; i < data.size(); ++i) {
        const int current = data[i];
        data[i] = static_cast<unsigned char>(current - previous + 384);
        previous = current;
    }
}

void exrZipUnpredict(std::vector<unsigned char>& data) {
    for (size_t i = 1; i < data.size(); ++i) {
        data[i] = static_cast<unsigned char>(static_cast<int>(data[i - 1]) +
                                             static_cast<int>(data[i]) - 128);
    }
}

bool writeExr(const std::string& path, const Image& image, bool half, bool zip,
              std::string& error) {
    const int width = image.width;
    const int height = image.height;
    const int sampleBytes = half ? 2 : 4;
    const size_t rowBytes = static_cast<size_t>(width) * 3 * static_cast<size_t>(sampleBytes);
    // ZIP stores 16 scanlines per chunk; NONE stores one per scanline (D059).
    const int chunkRows = zip ? 16 : 1;
    const int chunkCount = (height + chunkRows - 1) / chunkRows;

    std::vector<unsigned char> header;
    putU32le(header, 20000630u);  // magic
    putU32le(header, 2u);         // version 2, no flags

    std::vector<unsigned char> channels;
    static const char* kChannelNames[3] = {"B", "G", "R"};
    for (const char* name : kChannelNames) {
        putCString(channels, name);
        putU32le(channels, half ? 1u : 2u);  // 1 = HALF, 2 = FLOAT
        channels.push_back(0);               // pLinear
        channels.push_back(0);               // reserved
        channels.push_back(0);
        channels.push_back(0);
        putU32le(channels, 1u);  // xSampling
        putU32le(channels, 1u);  // ySampling
    }
    channels.push_back(0);
    putAttribute(header, "channels", "chlist", channels);

    putAttribute(header, "compression", "compression",
                 std::vector<unsigned char>{static_cast<unsigned char>(zip ? 3 : 0)});

    std::vector<unsigned char> window;
    putU32le(window, 0);
    putU32le(window, 0);
    putU32le(window, static_cast<uint32_t>(width - 1));
    putU32le(window, static_cast<uint32_t>(height - 1));
    putAttribute(header, "dataWindow", "box2i", window);
    putAttribute(header, "displayWindow", "box2i", window);

    putAttribute(header, "lineOrder", "lineOrder", std::vector<unsigned char>{0});

    std::vector<unsigned char> aspect;
    putFloatLe(aspect, 1.0f);
    putAttribute(header, "pixelAspectRatio", "float", aspect);

    std::vector<unsigned char> center;
    putFloatLe(center, 0.0f);
    putFloatLe(center, 0.0f);
    putAttribute(header, "screenWindowCenter", "v2f", center);

    std::vector<unsigned char> screenWidth;
    putFloatLe(screenWidth, 1.0f);
    putAttribute(header, "screenWindowWidth", "float", screenWidth);

    // sRGB / Rec.709 primaries with D65 white. The samples themselves stay
    // display-encoded (the same code values a PNG would carry), so a colour
    // managed host still needs the matching input transform.
    std::vector<unsigned char> chromaticities;
    static const float kChromaticities[8] = {0.64f, 0.33f, 0.30f, 0.60f,
                                             0.15f, 0.06f, 0.3127f, 0.3290f};
    for (float value : kChromaticities) putFloatLe(chromaticities, value);
    putAttribute(header, "chromaticities", "chromaticities", chromaticities);

    header.push_back(0);  // end of attributes

    // Build the chunks first: the offset table needs every packed size, and it
    // is written before the data.
    std::vector<std::vector<unsigned char>> chunks;
    chunks.reserve(static_cast<size_t>(chunkCount));
    for (int chunk = 0; chunk < chunkCount; ++chunk) {
        const int y0 = chunk * chunkRows;
        const int rows = std::min(chunkRows, height - y0);
        std::vector<unsigned char> raw(static_cast<size_t>(rows) * rowBytes);
        size_t p = 0;
        for (int r = 0; r < rows; ++r) {
            const int y = y0 + r;
            for (int channel = 2; channel >= 0; --channel) {  // stored B, G, R
                for (int x = 0; x < width; ++x) {
                    const float value = image.at(x, y)[channel];
                    if (half) {
                        const uint16_t sample = floatToHalf(value);
                        raw[p++] = static_cast<unsigned char>(sample);
                        raw[p++] = static_cast<unsigned char>(sample >> 8);
                    } else {
                        uint32_t bits = 0;
                        std::memcpy(&bits, &value, sizeof(bits));
                        raw[p++] = static_cast<unsigned char>(bits);
                        raw[p++] = static_cast<unsigned char>(bits >> 8);
                        raw[p++] = static_cast<unsigned char>(bits >> 16);
                        raw[p++] = static_cast<unsigned char>(bits >> 24);
                    }
                }
            }
        }

        if (zip) {
            // Prepare a copy: when compression does not help we must store the
            // original bytes, not the predictor/interleaved ones (the reader
            // treats packedSize == rawSize as uncompressed).
            std::vector<unsigned char> prepared(raw);
            exrZipSplit(prepared);
            exrZipPredict(prepared);
            int compressedLength = 0;
            unsigned char* compressed = stbi_zlib_compress(
                prepared.data(), static_cast<int>(prepared.size()), &compressedLength, 8);
            if (compressed != nullptr && compressedLength > 0 &&
                static_cast<size_t>(compressedLength) < raw.size()) {
                chunks.emplace_back(compressed, compressed + compressedLength);
            } else {
                chunks.push_back(std::move(raw));
            }
            std::free(compressed);
        } else {
            chunks.push_back(std::move(raw));
        }
    }

    std::vector<unsigned char> offsets;
    uint64_t offset = header.size() + static_cast<uint64_t>(chunkCount) * 8;
    for (const auto& chunk : chunks) {
        putU64le(offsets, offset);
        offset += 8 + chunk.size();
    }

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        error = "cannot write image: " + path;
        return false;
    }
    bool ok = std::fwrite(header.data(), 1, header.size(), file) == header.size();
    ok = ok && std::fwrite(offsets.data(), 1, offsets.size(), file) == offsets.size();

    std::vector<unsigned char> block;
    for (int chunk = 0; chunk < chunkCount && ok; ++chunk) {
        block.clear();
        block.reserve(8 + chunks[chunk].size());
        putU32le(block, static_cast<uint32_t>(chunk * chunkRows));
        putU32le(block, static_cast<uint32_t>(chunks[chunk].size()));
        block.insert(block.end(), chunks[chunk].begin(), chunks[chunk].end());
        ok = std::fwrite(block.data(), 1, block.size(), file) == block.size();
    }

    if (std::fclose(file) != 0) ok = false;
    if (!ok) error = "cannot write image: " + path;
    return ok;
}

bool readExr(const std::string& path, Image& out, std::string& error) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        error = "cannot open image: " + path;
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (length < 64) {
        std::fclose(file);
        error = "not an OpenEXR file: " + path;
        return false;
    }
    std::vector<unsigned char> data(static_cast<size_t>(length));
    const size_t read = std::fread(data.data(), 1, data.size(), file);
    std::fclose(file);
    if (read != data.size()) {
        error = "cannot read image: " + path;
        return false;
    }
    if (readU32le(data, 0) != 20000630u) {
        error = "not an OpenEXR file: " + path;
        return false;
    }

    struct Channel {
        std::string name;
        int type = 0;  // 1 = HALF, 2 = FLOAT
    };
    std::vector<Channel> channels;
    int compression = -1;
    int xMin = 0;
    int yMin = 0;
    int xMax = -1;
    int yMax = -1;

    const auto readCString = [&](size_t& pos, std::string& text) {
        text.clear();
        while (pos < data.size() && data[pos] != 0) text.push_back(static_cast<char>(data[pos++]));
        if (pos < data.size()) ++pos;
    };

    size_t pos = 8;
    while (pos < data.size()) {
        std::string name;
        readCString(pos, name);
        if (name.empty()) break;
        std::string type;
        readCString(pos, type);
        if (pos + 4 > data.size()) {
            error = "damaged OpenEXR header: " + path;
            return false;
        }
        const uint32_t size = readU32le(data, pos);
        pos += 4;
        if (pos + size > data.size()) {
            error = "damaged OpenEXR header: " + path;
            return false;
        }
        const size_t begin = pos;
        const size_t end = pos + size;

        if (name == "channels") {
            size_t p = begin;
            while (p < end) {
                Channel channel;
                readCString(p, channel.name);
                if (channel.name.empty()) break;
                if (p + 16 > end) {
                    error = "damaged OpenEXR channel list: " + path;
                    return false;
                }
                channel.type = static_cast<int>(readU32le(data, p));
                p += 16;  // type, pLinear + reserved, xSampling, ySampling
                channels.push_back(channel);
            }
        } else if (name == "compression") {
            compression = data[begin];
        } else if (name == "dataWindow") {
            xMin = static_cast<int>(readU32le(data, begin));
            yMin = static_cast<int>(readU32le(data, begin + 4));
            xMax = static_cast<int>(readU32le(data, begin + 8));
            yMax = static_cast<int>(readU32le(data, begin + 12));
        }
        pos = end;
    }

    if (compression != 0 && compression != 3) {
        error = "only NONE and ZIP compressed OpenEXR files are supported";
        return false;
    }
    const int width = xMax - xMin + 1;
    const int height = yMax - yMin + 1;
    if (width <= 0 || height <= 0 || width > 65536 || height > 65536) {
        error = "damaged OpenEXR image size: " + path;
        return false;
    }

    int indexR = -1;
    int indexG = -1;
    int indexB = -1;
    for (size_t i = 0; i < channels.size(); ++i) {
        if (channels[i].name == "R") indexR = static_cast<int>(i);
        if (channels[i].name == "G") indexG = static_cast<int>(i);
        if (channels[i].name == "B") indexB = static_cast<int>(i);
        if (channels[i].type != 1 && channels[i].type != 2) {
            error = "OpenEXR channel type must be HALF or FLOAT: " + path;
            return false;
        }
    }
    if (indexR < 0 || indexG < 0 || indexB < 0) {
        error = "OpenEXR file needs R, G and B channels: " + path;
        return false;
    }

    const int chunkRows = compression == 3 ? 16 : 1;
    const int chunkCount = (height + chunkRows - 1) / chunkRows;
    if (pos + static_cast<size_t>(chunkCount) * 8 > data.size()) {
        error = "damaged OpenEXR offset table: " + path;
        return false;
    }
    out.width = width;
    out.height = height;
    out.pixels.assign(static_cast<size_t>(width) * height * 3, 0.0f);

    size_t rowBytesTotal = 0;
    for (const Channel& channel : channels) {
        rowBytesTotal += static_cast<size_t>(width) * (channel.type == 1 ? 2 : 4);
    }

    for (int chunk = 0; chunk < chunkCount; ++chunk) {
        const uint64_t offset = readU64le(data, pos + static_cast<size_t>(chunk) * 8);
        if (offset + 8 > data.size()) {
            error = "damaged OpenEXR chunk: " + path;
            return false;
        }
        const int y0 = static_cast<int>(readU32le(data, static_cast<size_t>(offset)));
        const uint32_t packedSize = readU32le(data, static_cast<size_t>(offset) + 4);
        if (y0 < 0 || y0 >= height) {
            error = "damaged OpenEXR chunk row: " + path;
            return false;
        }
        const size_t chunkStart = static_cast<size_t>(offset) + 8;
        if (chunkStart + packedSize > data.size()) {
            error = "damaged OpenEXR pixel data: " + path;
            return false;
        }

        const int rows = std::min(chunkRows, height - y0);
        const size_t rawSize = static_cast<size_t>(rows) * rowBytesTotal;
        std::vector<unsigned char> inflated;
        const unsigned char* raw = &data[chunkStart];
        if (compression == 3 && packedSize != rawSize) {
            int decodedLength = 0;
            char* decoded = stbi_zlib_decode_malloc_guesssize_headerflag(
                reinterpret_cast<const char*>(raw), static_cast<int>(packedSize),
                static_cast<int>(rawSize), &decodedLength, 1);
            if (decoded == nullptr || decodedLength < 0 ||
                static_cast<size_t>(decodedLength) < rawSize) {
                std::free(decoded);
                error = "damaged OpenEXR ZIP block: " + path;
                return false;
            }
            inflated.assign(decoded, decoded + decodedLength);
            std::free(decoded);
            exrZipUnpredict(inflated);
            exrZipMerge(inflated);
            raw = inflated.data();
        }

        size_t rowOffset = 0;
        for (int r = 0; r < rows; ++r) {
            const int y = y0 + r;
            size_t channelOffset = rowOffset;
            for (size_t c = 0; c < channels.size(); ++c) {
                const int bytes = channels[c].type == 1 ? 2 : 4;
                const size_t channelBytes = static_cast<size_t>(width) * bytes;
                if (channelOffset + channelBytes > rawSize) {
                    error = "damaged OpenEXR pixel data: " + path;
                    return false;
                }
                int target = -1;
                if (static_cast<int>(c) == indexR) target = 0;
                if (static_cast<int>(c) == indexG) target = 1;
                if (static_cast<int>(c) == indexB) target = 2;
                if (target >= 0) {
                    for (int x = 0; x < width; ++x) {
                        const float value =
                            bytes == 2
                                ? halfToFloat(static_cast<uint16_t>(
                                      raw[channelOffset + static_cast<size_t>(x) * 2] |
                                      (raw[channelOffset + static_cast<size_t>(x) * 2 + 1] << 8)))
                                : readFloatLeAt(raw, channelOffset + static_cast<size_t>(x) * 4);
                        out.pixels[(static_cast<size_t>(y) * width + x) * 3 + target] = value;
                    }
                }
                channelOffset += channelBytes;
            }
            rowOffset += rowBytesTotal;
        }
    }
    return true;
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
    if (extensionOf(path) == ".exr") {
        if (!readExr(path, out, error)) return false;
        if (sourceBits != nullptr) *sourceBits = 32;
        return true;
    }

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

bool saveImage(const std::string& path, const Image& image, std::string& error, int bits,
               bool exrHalf, bool exrZip) {
    if (!image.valid()) {
        error = "refusing to write an empty image";
        return false;
    }
    if (extensionOf(path) == ".exr") {
        return writeExr(path, image, exrHalf, exrZip, error);
    }
    if (bits != 8 && bits != 16) {
        error = "PNG output supports 8 or 16 bits; use a .exr path for 32-bit float";
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

bool parseFloatTransfer(const std::string& name, FloatTransfer& out) {
    if (name == "srgb" || name == "sRGB" || name == "Srgb") {
        out = FloatTransfer::Srgb;
        return true;
    }
    if (name == "pq" || name == "PQ") {
        out = FloatTransfer::Pq;
        return true;
    }
    if (name == "hlg" || name == "HLG") {
        out = FloatTransfer::Hlg;
        return true;
    }
    return false;
}

void linearizeImage(Image& image, FloatTransfer transfer, double peakNits) {
    const double peak = peakNits > 0.0 ? peakNits : 1000.0;
    for (float& sample : image.pixels) {
        const double code = sample;
        double linear = 0.0;
        switch (transfer) {
            case FloatTransfer::Srgb:
                // Unclamped on purpose: a float delivery keeps super-whites.
                linear = code <= 0.04045 ? code / 12.92
                                         : std::pow((code + 0.055) / 1.055, 2.4);
                break;
            case FloatTransfer::Pq:
                linear = pqDecode(code) / peak;
                break;
            case FloatTransfer::Hlg:
                linear = hlgDecode(code);
                break;
        }
        sample = static_cast<float>(linear);
    }
}

Image makeTestChart(int width, int height) {    width = std::max(64, width);
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
