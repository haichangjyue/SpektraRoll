// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <array>
#include <string>
#include <vector>

namespace sr {

// One value per colour channel, in R/G/B (film) order. The density domain is
// actually CMY; spektrafilm stores the channels in R,G,B order and its
// `channel_density` matrix maps CMY -> spectral density, so we keep the same
// indexing and never rename anything.
using Triple = std::array<double, 3>;

struct ProfileInfo {
    std::string stock;
    std::string name;
    std::string type;               // "negative" | "positive"
    std::string support;            // "film" | "paper"
    std::string stage;              // "filming" | "printing"
    std::string referenceIlluminant;  // illuminant the sensitivity was measured under
    std::string viewingIlluminant;    // illuminant the print is viewed under
};

// Parametric density curves: a sum of normal CDFs per channel.
// `centers`, `amplitudes` and `sigmas` are channel-major: index(c, i).
struct DensityCurvesModel {
    int channels = 0;
    int layers = 0;
    std::vector<double> centers;
    std::vector<double> amplitudes;
    std::vector<double> sigmas;

    double center(int c, int i) const {
        return centers[static_cast<size_t>(c) * layers + i];
    }
    double amplitude(int c, int i) const {
        return amplitudes[static_cast<size_t>(c) * layers + i];
    }
    double sigma(int c, int i) const {
        return sigmas[static_cast<size_t>(c) * layers + i];
    }
    bool valid() const {
        return channels > 0 && layers > 0 &&
               centers.size() == static_cast<size_t>(channels) * layers &&
               amplitudes.size() == centers.size() && sigmas.size() == centers.size();
    }
};

// The subset of a spektrafilm profile that the M1 chain needs. NaN marks a
// wavelength with no measurement, exactly as the JSON `null` does upstream.
struct Profile {
    ProfileInfo info;
    std::vector<double> wavelengths;         // nm, ascending
    std::vector<Triple> logSensitivity;      // 10^x = relative sensitivity
    std::vector<Triple> channelDensity;      // CMY -> spectral density
    std::vector<double> baseDensity;         // per-wavelength base density
    std::vector<double> logExposure;         // characteristic-curve x axis
    std::vector<Triple> densityCurves;       // y values, may contain NaN
    DensityCurvesModel model;                // parametric curves (papers)
};

// Reads and validates a spektrafilm profile JSON. Requires 81 samples at
// 380..780 nm in 5 nm steps, matching SPECTRAL_SHAPE.
bool loadProfile(const std::string& path, Profile& out, std::string& error);

// Looks up `[paper][illuminant][film]` in spektrafilm's
// data/filters/neutral_print_filters.json (Kodak CC units, CMY order).
bool loadNeutralFilters(const std::string& path, const std::string& paper,
                        const std::string& film, const std::string& illuminant,
                        Triple& out, std::string& error);

}  // namespace sr
