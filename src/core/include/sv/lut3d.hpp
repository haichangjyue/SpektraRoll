// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 spektravideo contributors
#pragma once

#include <string>
#include <vector>

namespace sv {

struct Rgb {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

enum class Interp {
    Tetrahedral,
    Trilinear,
};

// A 3D colour lookup table as produced by spektrafilm's LUT baker
// (.cube / .3dl). The MVP only reads .cube; .3dl is a later addition.
class Lut3D {
public:
    // Returns false and fills `error` when the file cannot be parsed.
    bool loadCube(const std::string& path, std::string& error);

    static Lut3D identity(int size);

    int size() const { return size_; }
    bool empty() const { return data_.empty(); }
    Rgb domainMin() const { return domainMin_; }
    Rgb domainMax() const { return domainMax_; }
    const std::string& title() const { return title_; }

    // `in` is in the LUT's domain (normally 0..1). Values outside the domain
    // are clamped.
    Rgb apply(Rgb in, Interp mode = Interp::Tetrahedral) const;

private:
    Rgb at(int r, int g, int b) const;
    int index(int r, int g, int b) const { return (b * size_ + g) * size_ + r; }

    int size_ = 0;
    Rgb domainMin_{0.0f, 0.0f, 0.0f};
    Rgb domainMax_{1.0f, 1.0f, 1.0f};
    std::string title_;
    std::vector<Rgb> data_;  // r varies fastest, then g, then b
};

// Blend between the input and the LUT result. strength 0 = input, 1 = full LUT.
Rgb mix(Rgb a, Rgb b, float t);

}  // namespace sv
