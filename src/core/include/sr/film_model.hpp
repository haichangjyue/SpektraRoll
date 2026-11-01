// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <array>
#include <string>
#include <vector>

#include "sr/lut3d.hpp"
#include "sr/profile.hpp"
#include "sr/transfer.hpp"
#include "sr/gamut.hpp"

namespace sr {

// Which end of the chain produces the picture.
//
//   Print     negative -> paper -> scan, the M1 chain (a printed photograph)
//   Negative  scan the negative itself, inverted and balanced against the
//             film base: much more latitude, flatter look, meant for grading
enum class ScanMode { Print, Negative };

struct BakeOptions {
    InputTransfer input = InputTransfer::Srgb;
    // Exposure in stops, applied in linear light before the film model. Only
    // meaningful for scene-referred inputs such as Apple Log, where the code
    // range can hold several stops above diffuse white.
    double exposureEv = 0.0;
    ScanMode scan = ScanMode::Print;
    // Tone contrast around 18% grey, applied after a negative scan: 1.0 leaves
    // the film's own curve alone, >1 gives a punchier image, <1 flattens it.
    double contrast = 1.0;
    // Map out-of-gamut linear colour back into the sRGB cube (OkLCh chroma
    // reduction). Off for the print path so M1 parity is untouched (D027).
    bool gamutCompress = false;
};

struct FilmModelOptions {
    // DIR couplers (interlayer inhibition during development). Off for the M1
    // reference so it stays byte-identical, on for Apple Log bakes (D028).
    bool couplers = false;
};

// The M1 colour chain: display-encoded sRGB in, display-encoded sRGB out.
//
//   sRGB -> Mallett 2019 spectral upsampling -> film sensitivity
//        -> film characteristic curves -> CMY -> spectral density
//        -> enlarger light -> print paper sensitivity
//        -> paper characteristic curves -> CMY -> spectral density
//        -> scan under the viewing illuminant -> XYZ -> sRGB
//
// Couplers, grain, halation, diffusion and output gamut compression are off,
// matching spektrafilm's `lut_mode` with `algorithm="off"`. Keeping this a
// pure function of one RGB triple is what makes the LUT bake reproducible.
class FilmModel {
public:
    // `neutralFilters` are Kodak CC units in CMY order, looked up from
    // spektrafilm's neutral_print_filters.json for this film/paper pair.
    bool configure(const Profile& film, const Profile& paper,
                   const std::array<double, 3>& neutralFilters, std::string& error,
                   const FilmModelOptions& options = {});

    bool valid() const { return valid_; }

    // `srgb` is display-encoded (0..1 is the useful range, values outside are
    // clamped on input). The result is display-encoded sRGB.
    Rgb transform(Rgb srgb) const;

    // Same chain, but the input is decoded according to `options.input`.
    // Apple Log input is decoded to linear BT.2020, converted to linear sRGB,
    // clamped to >= 0 and exposed; gamut mapping proper is still M2 work.
    Rgb transformFrom(const BakeOptions& options, Rgb value) const;

    // Bake a size^3 LUT over the input cube. Entries are ordered r fastest,
    // then g, then b, as .cube expects.
    Lut3D bake(int size, const std::string& title) const;
    Lut3D bake(int size, const std::string& title, const BakeOptions& options) const;

    double printExposureFactor() const { return printExposureFactor_; }
    // Neutral print-filter values used, for reporting.
    std::array<double, 3> neutralFilters() const { return neutralFilters_; }

private:
    Rgb transformLinear(const double linear[3], const BakeOptions& options) const;
    void filmCmy(const double linear[3], double cmy[3]) const;
    void scanPositiveDye(const double cmy[3], double linearSrgb[3]) const;
    void filmRaw(const double rgb[3], double raw[3]) const;
    void simpleRgbToDensitySpectral(double value, double density[81]) const;
    double interpolate(double x, const std::vector<double>& axis,
                       const std::vector<Triple>& curves, int channel) const;
    double paperCurve(int sample, int channel) const;

    bool valid_ = false;
    ProfileInfo filmInfo_;
    ProfileInfo paperInfo_;

    std::vector<Triple> filmSensitivity_;
    std::vector<Triple> paperSensitivity_;
    std::vector<Triple> filmChannelDensity_;
    std::vector<Triple> paperChannelDensity_;
    std::vector<double> filmBaseDensity_;
    std::vector<double> paperBaseDensity_;

    std::vector<double> filmLogExposure_;
    std::vector<Triple> filmCurves_;   // normalized to a zero toe
    std::vector<Triple> filmCurvesRaw_;
    // With couplers on: the curve before interlayer inhibition, plus the
    // donor/receiver matrix and the shifted exposure axis (D028).
    std::vector<Triple> filmCurvesCoupler_;
    double couplersMatrix_[3][3] = {};
    bool couplersActive_ = false;
    bool filmIsPositive_ = false;

    std::vector<double> paperLogExposure_;
    std::vector<Triple> paperCurves_;

    std::vector<double> printIlluminant_;
    std::vector<double> viewingIlluminant_;

    // raw[m] = sum_k rgb[k] * filmMatrix_[k][m], then divided by the green
    // mid-grey reference (spektrafilm normalises the sensor response there).
    double filmMatrix_[3][3] = {};
    double filmMidgrayGreen_ = 1.0;

    double printExposureFactor_ = 1.0;
    double scanNormalization_ = 1.0;
    // Maximum film CMY densities, used to invert a negative scan.
    double filmMaxCmy_[3] = {0.0, 0.0, 0.0};
    // Linear sRGB of the positive scan at no dye (white) and at maximum dye
    // (black); the scan is mapped between them so the black end is neutral.
    double negativeWhite_[3] = {1.0, 1.0, 1.0};
    double negativeBlack_[3] = {0.0, 0.0, 0.0};
    // Per-channel power that neutralises mid-grey while keeping black and
    // white fixed (dye crosstalk correction).
    double negativeGamma_[3] = {1.0, 1.0, 1.0};
    // Luminance of the scan's 18% grey, used as the contrast pivot.
    double negativeGreyLuma_ = 0.18;
    std::array<double, 3> neutralFilters_{0.0, 0.0, 0.0};
};

}  // namespace sr
