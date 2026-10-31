// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include "sr/lut3d.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

namespace sr {
namespace {

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

float clamp01(float v) { return std::min(1.0f, std::max(0.0f, v)); }

}  // namespace

Lut3D Lut3D::identity(int size) {
    Lut3D lut;
    lut.size_ = std::max(2, size);
    lut.data_.resize(static_cast<size_t>(lut.size_) * lut.size_ * lut.size_);
    const float scale = 1.0f / static_cast<float>(lut.size_ - 1);
    for (int b = 0; b < lut.size_; ++b) {
        for (int g = 0; g < lut.size_; ++g) {
            for (int r = 0; r < lut.size_; ++r) {
                lut.data_[lut.index(r, g, b)] = {r * scale, g * scale, b * scale};
            }
        }
    }
    return lut;
}

Lut3D Lut3D::fromData(int size, std::vector<Rgb> data, std::string title) {
    Lut3D lut;
    lut.size_ = std::max(2, size);
    const size_t expected = static_cast<size_t>(lut.size_) * lut.size_ * lut.size_;
    if (data.size() != expected) return {};
    lut.data_ = std::move(data);
    lut.title_ = std::move(title);
    return lut;
}

bool Lut3D::saveCube(const std::string& path, std::string& error) const {
    if (empty()) {
        error = "refusing to write an empty LUT";
        return false;
    }
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        error = "cannot write LUT file: " + path;
        return false;
    }
    std::fprintf(file, "TITLE \"%s\"\n", title_.empty() ? "SpektraRoll LUT" : title_.c_str());
    std::fprintf(file, "LUT_3D_SIZE %d\n", size_);
    std::fprintf(file, "DOMAIN_MIN %.8f %.8f %.8f\n", domainMin_.r, domainMin_.g, domainMin_.b);
    std::fprintf(file, "DOMAIN_MAX %.8f %.8f %.8f\n", domainMax_.r, domainMax_.g, domainMax_.b);
    for (int b = 0; b < size_; ++b) {
        for (int g = 0; g < size_; ++g) {
            for (int r = 0; r < size_; ++r) {
                const Rgb value = at(r, g, b);
                std::fprintf(file, "%.8f %.8f %.8f\n", value.r, value.g, value.b);
            }
        }
    }
    const bool ok = std::fclose(file) == 0;
    if (!ok) error = "cannot finish writing LUT file: " + path;
    return ok;
}

bool Lut3D::loadCube(const std::string& path, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "cannot open LUT file: " + path;
        return false;
    }

    std::vector<Rgb> values;
    int declaredSize = 0;
    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        std::istringstream stream(trimmed);
        std::string token;
        stream >> token;
        const std::string key = lower(token);

        if (key == "title") {
            std::string value = trim(trimmed.substr(token.size()));
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            title_ = value;
            continue;
        }
        if (key == "lut_3d_size") {
            stream >> declaredSize;
            continue;
        }
        if (key == "lut_1d_size") {
            error = "1D LUTs are not supported yet (line " + std::to_string(lineNumber) + ")";
            return false;
        }
        if (key == "domain_min") {
            stream >> domainMin_.r >> domainMin_.g >> domainMin_.b;
            continue;
        }
        if (key == "domain_max") {
            stream >> domainMax_.r >> domainMax_.g >> domainMax_.b;
            continue;
        }

        // Otherwise the line must be a triplet of table values. The first one
        // has already been consumed as `token`.
        char* end = nullptr;
        Rgb value;
        value.r = std::strtof(token.c_str(), &end);
        if (end == token.c_str() || !(stream >> value.g >> value.b)) {
            error = "malformed row at line " + std::to_string(lineNumber);
            return false;
        }
        values.push_back(value);
    }

    if (declaredSize < 2) {
        error = "missing or invalid LUT_3D_SIZE";
        return false;
    }

    const size_t expected =
        static_cast<size_t>(declaredSize) * declaredSize * declaredSize;
    if (values.size() != expected) {
        error = "LUT_3D_SIZE is " + std::to_string(declaredSize) + " (" +
                std::to_string(expected) + " entries) but the file has " +
                std::to_string(values.size());
        return false;
    }

    size_ = declaredSize;
    data_ = std::move(values);
    return true;
}

Rgb Lut3D::at(int r, int g, int b) const { return data_[index(r, g, b)]; }

Rgb Lut3D::apply(Rgb in, Interp mode) const {
    if (empty()) return in;

    const float spanR = domainMax_.r - domainMin_.r;
    const float spanG = domainMax_.g - domainMin_.g;
    const float spanB = domainMax_.b - domainMin_.b;

    const float nr = spanR != 0.0f ? clamp01((in.r - domainMin_.r) / spanR) : 0.0f;
    const float ng = spanG != 0.0f ? clamp01((in.g - domainMin_.g) / spanG) : 0.0f;
    const float nb = spanB != 0.0f ? clamp01((in.b - domainMin_.b) / spanB) : 0.0f;

    const int n = size_;
    const float x = nr * static_cast<float>(n - 1);
    const float y = ng * static_cast<float>(n - 1);
    const float z = nb * static_cast<float>(n - 1);

    const int r0 = std::min(static_cast<int>(std::floor(x)), n - 2);
    const int g0 = std::min(static_cast<int>(std::floor(y)), n - 2);
    const int b0 = std::min(static_cast<int>(std::floor(z)), n - 2);

    const float fx = x - static_cast<float>(r0);
    const float fy = y - static_cast<float>(g0);
    const float fz = z - static_cast<float>(b0);

    const Rgb c000 = at(r0, g0, b0);
    const Rgb c100 = at(r0 + 1, g0, b0);
    const Rgb c010 = at(r0, g0 + 1, b0);
    const Rgb c001 = at(r0, g0, b0 + 1);
    const Rgb c110 = at(r0 + 1, g0 + 1, b0);
    const Rgb c101 = at(r0 + 1, g0, b0 + 1);
    const Rgb c011 = at(r0, g0 + 1, b0 + 1);
    const Rgb c111 = at(r0 + 1, g0 + 1, b0 + 1);

    if (mode == Interp::Trilinear) {
        const auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
        const auto mix3 = [&](const Rgb& a, const Rgb& b, float t) {
            return Rgb{lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t)};
        };
        const Rgb c00 = mix3(c000, c100, fx);
        const Rgb c10 = mix3(c010, c110, fx);
        const Rgb c01 = mix3(c001, c101, fx);
        const Rgb c11 = mix3(c011, c111, fx);
        const Rgb c0 = mix3(c00, c10, fy);
        const Rgb c1 = mix3(c01, c11, fy);
        return mix3(c0, c1, fz);
    }

    // Tetrahedral interpolation: fewer samples on the diagonal, no visible
    // banding on saturated colours, and the standard for film LUTs.
    const auto add = [](const Rgb& a, const Rgb& b) {
        return Rgb{a.r + b.r, a.g + b.g, a.b + b.b};
    };
    const auto sub = [](const Rgb& a, const Rgb& b) {
        return Rgb{a.r - b.r, a.g - b.g, a.b - b.b};
    };
    const auto mul = [](const Rgb& a, float s) {
        return Rgb{a.r * s, a.g * s, a.b * s};
    };

    Rgb result;
    if (fx > fy) {
        if (fy > fz) {
            result = add(c000, add(mul(sub(c100, c000), fx),
                                   add(mul(sub(c110, c100), fy), mul(sub(c111, c110), fz))));
        } else if (fx > fz) {
            result = add(c000, add(mul(sub(c100, c000), fx),
                                   add(mul(sub(c101, c100), fz), mul(sub(c111, c101), fy))));
        } else {
            result = add(c000, add(mul(sub(c001, c000), fz),
                                   add(mul(sub(c101, c001), fx), mul(sub(c111, c101), fy))));
        }
    } else {
        if (fz > fy) {
            result = add(c000, add(mul(sub(c001, c000), fz),
                                   add(mul(sub(c011, c001), fy), mul(sub(c111, c011), fx))));
        } else if (fz > fx) {
            result = add(c000, add(mul(sub(c010, c000), fy),
                                   add(mul(sub(c011, c010), fz), mul(sub(c111, c011), fx))));
        } else {
            result = add(c000, add(mul(sub(c010, c000), fy),
                                   add(mul(sub(c110, c010), fx), mul(sub(c111, c110), fz))));
        }
    }
    return result;
}

Rgb mix(Rgb a, Rgb b, float t) {
    const float s = clamp01(t);
    return Rgb{a.r + (b.r - a.r) * s, a.g + (b.g - a.g) * s, a.b + (b.b - a.b) * s};
}

}  // namespace sr
