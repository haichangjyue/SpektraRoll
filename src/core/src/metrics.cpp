// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include "sr/metrics.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace sr {
namespace {

// IEC 61966-2-1 sRGB primaries with a D65 white, as used by colour-science.
constexpr double kSrgbToXyz[9] = {
    0.41239079926595934, 0.35758433938387800, 0.18048078840183430,
    0.21263900587151027, 0.71516867876775600, 0.07219231536073371,
    0.01933081871559182, 0.11919477979462598, 0.95053215224966070,
};
constexpr double kWhiteD65[3] = {0.9504559270516716, 1.0, 1.0890577507598784};

double srgbDecode(double value) {
    const double x = std::min(1.0, std::max(0.0, value));
    return x <= 0.04045 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4);
}

double labF(double t) {
    constexpr double delta = 6.0 / 29.0;
    return t > delta * delta * delta ? std::cbrt(t) : t / (3.0 * delta * delta) + 4.0 / 29.0;
}

void toLab(double r, double g, double b, double lab[3]) {
    const double linear[3] = {srgbDecode(r), srgbDecode(g), srgbDecode(b)};
    double xyz[3];
    for (int i = 0; i < 3; ++i) {
        xyz[i] = kSrgbToXyz[i * 3 + 0] * linear[0] + kSrgbToXyz[i * 3 + 1] * linear[1] +
                 kSrgbToXyz[i * 3 + 2] * linear[2];
    }
    const double fx = labF(xyz[0] / kWhiteD65[0]);
    const double fy = labF(xyz[1] / kWhiteD65[1]);
    const double fz = labF(xyz[2] / kWhiteD65[2]);
    lab[0] = 116.0 * fy - 16.0;
    lab[1] = 500.0 * (fx - fy);
    lab[2] = 200.0 * (fy - fz);
}

}  // namespace

bool compareLuts(const Lut3D& a, const Lut3D& b, LutComparison& out, std::string& error) {
    if (a.empty() || b.empty()) {
        error = "cannot compare an empty LUT";
        return false;
    }
    if (a.size() != b.size()) {
        error = "LUT sizes differ: " + std::to_string(a.size()) + " and " +
                std::to_string(b.size());
        return false;
    }

    const int size = a.size();
    const size_t count = static_cast<size_t>(size) * size * size;
    const double scale = 1.0 / static_cast<double>(size - 1);

    double squared = 0.0;
    double totalAbs = 0.0;
    double totalDeltaE = 0.0;
    double maxAbs = 0.0;
    double maxDeltaE = 0.0;
    Rgb worst{};
    std::vector<double> deltaEs;
    deltaEs.reserve(count);

    for (int bIndex = 0; bIndex < size; ++bIndex) {
        for (int gIndex = 0; gIndex < size; ++gIndex) {
            for (int rIndex = 0; rIndex < size; ++rIndex) {
                const Rgb in{static_cast<float>(rIndex * scale), static_cast<float>(gIndex * scale),
                             static_cast<float>(bIndex * scale)};
                const Rgb av = a.apply(in);
                const Rgb bv = b.apply(in);

                const double dr = static_cast<double>(av.r) - bv.r;
                const double dg = static_cast<double>(av.g) - bv.g;
                const double db = static_cast<double>(av.b) - bv.b;
                squared += dr * dr + dg * dg + db * db;

                const double absError = std::max({std::abs(dr), std::abs(dg), std::abs(db)});
                totalAbs += (std::abs(dr) + std::abs(dg) + std::abs(db)) / 3.0;
                if (absError > maxAbs) {
                    maxAbs = absError;
                    worst = in;
                }

                double labA[3];
                double labB[3];
                toLab(av.r, av.g, av.b, labA);
                toLab(bv.r, bv.g, bv.b, labB);
                const double dE = std::sqrt((labA[0] - labB[0]) * (labA[0] - labB[0]) +
                                            (labA[1] - labB[1]) * (labA[1] - labB[1]) +
                                            (labA[2] - labB[2]) * (labA[2] - labB[2]));
                totalDeltaE += dE;
                maxDeltaE = std::max(maxDeltaE, dE);
                deltaEs.push_back(dE);
            }
        }
    }

    out.samples = count;
    out.mse = squared / static_cast<double>(count * 3);
    out.psnr = out.mse > 0.0 ? 10.0 * std::log10(1.0 / out.mse) : INFINITY;
    out.maxAbs = maxAbs;
    out.meanAbs = totalAbs / static_cast<double>(count);
    out.meanDeltaE = totalDeltaE / static_cast<double>(count);
    out.maxDeltaE = maxDeltaE;
    out.worstInput = worst;

    std::sort(deltaEs.begin(), deltaEs.end());
    const size_t index = static_cast<size_t>(std::ceil(0.95 * static_cast<double>(count))) - 1;
    out.p95DeltaE = deltaEs[std::min(index, deltaEs.size() - 1)];
    return true;
}

}  // namespace sr
