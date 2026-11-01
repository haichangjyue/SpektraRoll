// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include "sr/film_model.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "sr/spectral_data.hpp"

namespace sr {
namespace {

constexpr int kN = data::kWavelengthCount;  // 81 samples, 380..780 nm

double nanToZero(double value) { return std::isnan(value) ? 0.0 : value; }

double normCdf(double z) { return 0.5 * std::erfc(-z / std::sqrt(2.0)); }

}  // namespace

bool FilmModel::configure(const Profile& film, const Profile& paper,
                          const std::array<double, 3>& neutralFilters, std::string& error) {
    valid_ = false;

    if (film.info.support != "film") {
        error = "expected a film profile, got support '" + film.info.support + "'";
        return false;
    }
    if (paper.info.support != "paper") {
        error = "expected a paper profile, got support '" + paper.info.support + "'";
        return false;
    }
    if (film.info.referenceIlluminant != "D55") {
        error = "M1 supports reference illuminant D55 only, profile asks for '" +
                film.info.referenceIlluminant + "'";
        return false;
    }
    if (paper.info.viewingIlluminant != "D50") {
        error = "M1 supports viewing illuminant D50 only, profile asks for '" +
                paper.info.viewingIlluminant + "'";
        return false;
    }
    if (!paper.model.valid()) {
        error = "paper profile has no usable density_curves_model";
        return false;
    }
    if (film.logSensitivity.size() != kN || film.channelDensity.size() != kN ||
        film.baseDensity.size() != kN || paper.logSensitivity.size() != kN ||
        paper.channelDensity.size() != kN || paper.baseDensity.size() != kN) {
        error = "profile spectral arrays must have 81 samples";
        return false;
    }

    filmInfo_ = film.info;
    paperInfo_ = paper.info;
    neutralFilters_ = neutralFilters;

    filmSensitivity_.resize(kN);
    paperSensitivity_.resize(kN);
    for (int l = 0; l < kN; ++l) {
        for (int c = 0; c < 3; ++c) {
            filmSensitivity_[l][c] =
                nanToZero(std::pow(10.0, film.logSensitivity[l][c]));
            paperSensitivity_[l][c] =
                nanToZero(std::pow(10.0, paper.logSensitivity[l][c]));
        }
    }
    filmChannelDensity_ = film.channelDensity;
    paperChannelDensity_ = paper.channelDensity;
    filmBaseDensity_ = film.baseDensity;
    paperBaseDensity_ = paper.baseDensity;

    filmLogExposure_ = film.logExposure;
    paperLogExposure_ = paper.logExposure;
    for (size_t i = 1; i < filmLogExposure_.size(); ++i) {
        if (filmLogExposure_[i] <= filmLogExposure_[i - 1]) {
            error = "film log_exposure must be strictly ascending";
            return false;
        }
    }
    for (size_t i = 1; i < paperLogExposure_.size(); ++i) {
        if (paperLogExposure_[i] <= paperLogExposure_[i - 1]) {
            error = "paper log_exposure must be strictly ascending";
            return false;
        }
    }

    // Film curves are shifted so the toe sits at density 0 (develop() does the
    // same with `density_curves - nanmin(..., axis=0)`). The unshifted curves
    // are still needed for the mid-grey print exposure factor.
    filmCurvesRaw_ = film.densityCurves;
    filmCurves_ = film.densityCurves;
    for (int c = 0; c < 3; ++c) {
        double minimum = std::numeric_limits<double>::infinity();
        for (const Triple& row : film.densityCurves) {
            const double value = row[static_cast<size_t>(c)];
            if (!std::isnan(value)) minimum = std::min(minimum, value);
        }
        if (!std::isfinite(minimum)) minimum = 0.0;
        for (Triple& row : filmCurves_) row[static_cast<size_t>(c)] -= minimum;
    }

    // Paper characteristic curves, evaluated at the profile's log exposure.
    // `profile_type` flips the CDF for positive stocks; M1 papers are negative.
    paperCurves_.resize(paperLogExposure_.size());
    const double sign = paper.info.type == "positive" ? -1.0 : 1.0;
    for (size_t i = 0; i < paperLogExposure_.size(); ++i) {
        for (int c = 0; c < 3; ++c) {
            double total = 0.0;
            for (int layer = 0; layer < paper.model.layers; ++layer) {
                const double sigma = paper.model.sigma(c, layer);
                const double z = (paperLogExposure_[i] - paper.model.center(c, layer)) / sigma;
                total += paper.model.amplitude(c, layer) * normCdf(sign * z);
            }
            paperCurves_[i][static_cast<size_t>(c)] = total;
        }
    }

    // Illuminants. The tables are already normalised to mean 1 over 380..780.
    const std::vector<double> referenceIlluminant(data::kIlluminantD55,
                                                 data::kIlluminantD55 + kN);
    viewingIlluminant_.assign(data::kIlluminantD50, data::kIlluminantD50 + kN);

    // Enlarger: 3400 K tungsten + KG3 heat filter, then the neutral CC
    // filters. `custom_dichroic_filters` uses these fixed edges/transitions.
    printIlluminant_.resize(kN);
    for (int l = 0; l < kN; ++l) {
        const double wl = 380.0 + 5.0 * l;
        const double c = 0.5 - 0.5 * std::erf((wl - 607.0) / 8.0);
        const double m = wl <= 550.0 ? 0.5 - 0.5 * std::erf((wl - 500.0) / 8.0)
                                     : 0.5 + 0.5 * std::erf((wl - 610.0) / 8.0);
        const double y = 0.5 + 0.5 * std::erf((wl - 516.0) / 12.0);
        const double filters[3] = {c, m, y};
        double total = 1.0;
        for (int channel = 0; channel < 3; ++channel) {
            const double transmittance =
                std::pow(10.0, -neutralFilters_[static_cast<size_t>(channel)] / 100.0);
            total *= 1.0 - (1.0 - filters[channel]) * (1.0 - transmittance);
        }
        printIlluminant_[static_cast<size_t>(l)] = data::kIlluminantThKg3[l] * total;
    }

    // film raw = rgb @ M, normalised by the green mid-grey response.
    for (int k = 0; k < 3; ++k) {
        for (int m = 0; m < 3; ++m) {
            double sum = 0.0;
            for (int l = 0; l < kN; ++l) {
                sum += data::kMallettBasis[l * 3 + k] * referenceIlluminant[static_cast<size_t>(l)] *
                       filmSensitivity_[static_cast<size_t>(l)][static_cast<size_t>(m)];
            }
            filmMatrix_[k][m] = sum;
        }
    }
    double midgrayGreen = 0.0;
    for (int l = 0; l < kN; ++l) {
        midgrayGreen += referenceIlluminant[static_cast<size_t>(l)] *
                        filmSensitivity_[static_cast<size_t>(l)][1];
    }
    filmMidgrayGreen_ = 0.184 * midgrayGreen;

    scanNormalization_ = 0.0;
    for (int l = 0; l < kN; ++l) {
        scanNormalization_ += viewingIlluminant_[static_cast<size_t>(l)] * data::kCmfs[l * 3 + 1];
    }

    // Maximum film CMY densities, so a negative scan can be inverted.
    for (int c = 0; c < 3; ++c) {
        double maximum = 0.0;
        for (const Triple& row : filmCurves_) {
            const double value = row[static_cast<size_t>(c)];
            if (!std::isnan(value)) maximum = std::max(maximum, value);
        }
        filmMaxCmy_[c] = maximum;
    }

    // Negative scan: invert the film's dye densities (no base density, so the
    // orange mask and the film base fall away) and scan the resulting virtual
    // positive transparency, mapped between "no dye" and "maximum dye" so the
    // deepest black is neutral.
    {
        const double noDye[3] = {0.0, 0.0, 0.0};
        const double maxDye[3] = {filmMaxCmy_[0], filmMaxCmy_[1], filmMaxCmy_[2]};
        scanPositiveDye(noDye, negativeWhite_);
        scanPositiveDye(maxDye, negativeBlack_);

        // Neutralise mid-grey on top of the black/white mapping: the film's
        // dyes overlap, so an un-corrected scan of the positive carries a warm
        // cast (the same reason a scanner needs a dye-correction matrix).
        const double greyLinear[3] = {0.184, 0.184, 0.184};
        double cmyGrey[3];
        filmCmy(greyLinear, cmyGrey);
        const double positiveGrey[3] = {filmMaxCmy_[0] - cmyGrey[0],
                                        filmMaxCmy_[1] - cmyGrey[1],
                                        filmMaxCmy_[2] - cmyGrey[2]};
        double rgbGrey[3];
        scanPositiveDye(positiveGrey, rgbGrey);
        for (int c = 0; c < 3; ++c) {
            const double span = negativeWhite_[c] - negativeBlack_[c];
            rgbGrey[c] = span > 1e-9 ? (rgbGrey[c] - negativeBlack_[c]) / span : 0.0;
        }
        const double greyLuma = 0.2126 * rgbGrey[0] + 0.7152 * rgbGrey[1] + 0.0722 * rgbGrey[2];
        negativeGreyLuma_ = std::min(1.0, std::max(1e-4, greyLuma));
        for (int c = 0; c < 3; ++c) {
            // A per-channel power keeps black at 0 and white at 1 while
            // neutralising mid-grey; a plain gain would tint the highlights.
            const double value = std::min(1.0, std::max(1e-4, rgbGrey[c]));
            const double target = std::min(1.0, std::max(1e-4, greyLuma));
            negativeGamma_[c] = std::log(target) / std::log(value);
        }
    }

    // Print exposure factor: expose mid-grey through the print illuminant and
    // normalise the geometric mean of the paper's raw response to 1.
    double midgrayDensity[kN];
    simpleRgbToDensitySpectral(0.184, midgrayDensity);
    double rawMidgray[3] = {0.0, 0.0, 0.0};
    for (int l = 0; l < kN; ++l) {
        const double density = midgrayDensity[l];
        const double light = std::isnan(density)
                                 ? 0.0
                                 : std::pow(10.0, -density) *
                                       printIlluminant_[static_cast<size_t>(l)];
        for (int c = 0; c < 3; ++c) {
            rawMidgray[c] += light * paperSensitivity_[static_cast<size_t>(l)][static_cast<size_t>(c)];
        }
    }
    double logMean = 0.0;
    for (int c = 0; c < 3; ++c) {
        // np.fmax ignores NaN and picks the other operand.
        const double value = std::isnan(rawMidgray[c]) ? 1e-10 : std::max(rawMidgray[c], 1e-10);
        logMean += std::log(value);
    }
    logMean /= 3.0;
    printExposureFactor_ = 1.0 / std::exp(logMean);

    valid_ = true;
    return true;
}

void FilmModel::filmRaw(const double rgb[3], double raw[3]) const {
    for (int m = 0; m < 3; ++m) {
        double value = 0.0;
        for (int k = 0; k < 3; ++k) value += rgb[k] * filmMatrix_[k][m];
        raw[m] = nanToZero(value) / filmMidgrayGreen_;
    }
}

void FilmModel::simpleRgbToDensitySpectral(double value, double density[kN]) const {
    const double rgb[3] = {value, value, value};
    double raw[3];
    filmRaw(rgb, raw);
    double logRaw[3];
    for (int c = 0; c < 3; ++c) logRaw[c] = std::log10(raw[c] + 1e-10);

    double cmy[3];
    for (int c = 0; c < 3; ++c) {
        cmy[c] = interpolate(logRaw[c], filmLogExposure_, filmCurvesRaw_, c);
    }
    for (int l = 0; l < kN; ++l) {
        double sum = filmBaseDensity_[static_cast<size_t>(l)];
        for (int c = 0; c < 3; ++c) {
            sum += cmy[c] * filmChannelDensity_[static_cast<size_t>(l)][static_cast<size_t>(c)];
        }
        density[l] = sum;
    }
}

double FilmModel::interpolate(double x, const std::vector<double>& axis,
                              const std::vector<Triple>& curves, int channel) const {
    if (!std::isfinite(x)) return 0.0;
    if (x <= axis.front()) return curves.front()[static_cast<size_t>(channel)];
    if (x >= axis.back()) return curves.back()[static_cast<size_t>(channel)];

    const size_t high =
        static_cast<size_t>(std::upper_bound(axis.begin(), axis.end(), x) - axis.begin());
    const size_t low = high - 1;
    const double span = axis[high] - axis[low];
    const double t = span != 0.0 ? (x - axis[low]) / span : 0.0;
    const double y0 = curves[low][static_cast<size_t>(channel)];
    const double y1 = curves[high][static_cast<size_t>(channel)];
    return y0 + t * (y1 - y0);
}

double FilmModel::paperCurve(int sample, int channel) const {
    return paperCurves_[static_cast<size_t>(sample)][static_cast<size_t>(channel)];
}

Rgb FilmModel::transform(Rgb srgb) const {
    double linear[3] = {srgbDecode(srgb.r), srgbDecode(srgb.g), srgbDecode(srgb.b)};
    return transformLinear(linear, ScanMode::Print, 1.0);
}

Rgb FilmModel::transformFrom(const BakeOptions& options, Rgb value) const {
    double linear[3];
    if (options.input == InputTransfer::AppleLog) {
        const double code[3] = {value.r, value.g, value.b};
        double lin2020[3];
        // No per-channel clamp before the matrix: below-black codes decode
        // negative and still contribute to the hue near black.
        for (int c = 0; c < 3; ++c) lin2020[c] = appleLogDecode(code[c]);
        bt2020ToSrgb(lin2020, linear);
        for (int c = 0; c < 3; ++c) linear[c] = std::max(0.0, linear[c]);
    } else {
        const double encoded[3] = {value.r, value.g, value.b};
        for (int c = 0; c < 3; ++c) linear[c] = srgbDecode(encoded[c]);
    }
    if (options.exposureEv != 0.0) {
        const double gain = std::exp2(options.exposureEv);
        for (int c = 0; c < 3; ++c) linear[c] *= gain;
    }
    return transformLinear(linear, options.scan, options.contrast);
}

void FilmModel::filmCmy(const double linear[3], double cmy[3]) const {
    double raw[3];
    filmRaw(linear, raw);
    for (int c = 0; c < 3; ++c) {
        const double logRaw = std::log10(std::max(raw[c], 0.0) + 1e-10);
        cmy[c] = interpolate(logRaw, filmLogExposure_, filmCurves_, c);
    }
}

void FilmModel::scanPositiveDye(const double cmy[3], double linearSrgb[3]) const {
    double xyz[3] = {0.0, 0.0, 0.0};
    for (int l = 0; l < kN; ++l) {
        double density = 0.0;
        for (int c = 0; c < 3; ++c) {
            density += cmy[c] * filmChannelDensity_[static_cast<size_t>(l)][static_cast<size_t>(c)];
        }
        const double light =
            std::isnan(density) ? 0.0
                                : std::pow(10.0, -density) *
                                      viewingIlluminant_[static_cast<size_t>(l)];
        for (int c = 0; c < 3; ++c) xyz[c] += light * data::kCmfs[l * 3 + c];
    }
    for (int c = 0; c < 3; ++c) xyz[c] /= scanNormalization_;
    for (int c = 0; c < 3; ++c) {
        linearSrgb[c] = data::kXyzD50ToSrgb[c * 3 + 0] * xyz[0] +
                        data::kXyzD50ToSrgb[c * 3 + 1] * xyz[1] +
                        data::kXyzD50ToSrgb[c * 3 + 2] * xyz[2];
    }
}

Rgb FilmModel::transformLinear(const double linear[3], ScanMode scan, double contrast) const {
    double cmyFilm[3];
    filmCmy(linear, cmyFilm);

    if (scan == ScanMode::Negative) {
        const double positive[3] = {filmMaxCmy_[0] - cmyFilm[0], filmMaxCmy_[1] - cmyFilm[1],
                                    filmMaxCmy_[2] - cmyFilm[2]};
        double rgb[3];
        scanPositiveDye(positive, rgb);
        for (int c = 0; c < 3; ++c) {
            const double span = negativeWhite_[c] - negativeBlack_[c];
            const double value = span > 1e-9 ? (rgb[c] - negativeBlack_[c]) / span : 0.0;
            const double clamped = std::min(1.0, std::max(0.0, value));
            rgb[c] = std::pow(clamped, negativeGamma_[c]);
        }
        if (contrast != 1.0) {
            // Pivot at the scan's own 18% grey and scale all channels together
            // so hue and saturation are left alone.
            const double luma = 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
            if (luma > 1e-9) {
                const double scale =
                    std::pow(luma / negativeGreyLuma_, contrast - 1.0);
                for (int c = 0; c < 3; ++c) rgb[c] = std::min(1.0, rgb[c] * scale);
            }
        }
        return Rgb{static_cast<float>(srgbEncode(rgb[0])), static_cast<float>(srgbEncode(rgb[1])),
                   static_cast<float>(srgbEncode(rgb[2]))};
    }

    double printRaw[3] = {0.0, 0.0, 0.0};
    for (int l = 0; l < kN; ++l) {
        double density = filmBaseDensity_[static_cast<size_t>(l)];
        for (int c = 0; c < 3; ++c) {
            density += cmyFilm[c] * filmChannelDensity_[static_cast<size_t>(l)][static_cast<size_t>(c)];
        }
        const double light = std::isnan(density)
                                 ? 0.0
                                 : std::pow(10.0, -density) * printIlluminant_[static_cast<size_t>(l)];
        for (int c = 0; c < 3; ++c) {
            printRaw[c] += light * paperSensitivity_[static_cast<size_t>(l)][static_cast<size_t>(c)];
        }
    }

    double cmyPaper[3];
    for (int c = 0; c < 3; ++c) {
        const double value = printRaw[c] * printExposureFactor_;
        const double logRaw = std::log10(std::max(value, 0.0) + 1e-10);
        cmyPaper[c] = interpolate(logRaw, paperLogExposure_, paperCurves_, c);
    }

    double xyz[3] = {0.0, 0.0, 0.0};
    for (int l = 0; l < kN; ++l) {
        double density = paperBaseDensity_[static_cast<size_t>(l)];
        for (int c = 0; c < 3; ++c) {
            density += cmyPaper[c] * paperChannelDensity_[static_cast<size_t>(l)][static_cast<size_t>(c)];
        }
        const double light =
            std::isnan(density)
                ? 0.0
                : std::pow(10.0, -density) * viewingIlluminant_[static_cast<size_t>(l)];
        for (int c = 0; c < 3; ++c) xyz[c] += light * data::kCmfs[l * 3 + c];
    }
    for (int c = 0; c < 3; ++c) xyz[c] /= scanNormalization_;

    double rgb[3];
    for (int c = 0; c < 3; ++c) {
        rgb[c] = data::kXyzD50ToSrgb[c * 3 + 0] * xyz[0] +
                 data::kXyzD50ToSrgb[c * 3 + 1] * xyz[1] +
                 data::kXyzD50ToSrgb[c * 3 + 2] * xyz[2];
    }

    return Rgb{static_cast<float>(srgbEncode(rgb[0])), static_cast<float>(srgbEncode(rgb[1])),
               static_cast<float>(srgbEncode(rgb[2]))};
}

Lut3D FilmModel::bake(int size, const std::string& title) const {
    return bake(size, title, BakeOptions{});
}

Lut3D FilmModel::bake(int size, const std::string& title, const BakeOptions& options) const {
    size = std::max(2, size);
    std::vector<Rgb> values(static_cast<size_t>(size) * size * size);
    const float scale = 1.0f / static_cast<float>(size - 1);
    for (int b = 0; b < size; ++b) {
        for (int g = 0; g < size; ++g) {
            for (int r = 0; r < size; ++r) {
                values[(static_cast<size_t>(b) * size + g) * size + r] =
                    transformFrom(options, Rgb{r * scale, g * scale, b * scale});
            }
        }
    }
    return Lut3D::fromData(size, std::move(values), title);
}

}  // namespace sr
