// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Parity anchors for the M1 chain. The expected numbers were produced by
// tools/parity/reference_model.py, which composes the upstream spektrafilm
// functions directly, and cross-checked against the real upstream
// SimulationPipeline in lut_mode (worst deviation 1.3e-4).
#include <cmath>
#include <cstdio>
#include <string>

#include "sr/film_model.hpp"
#include "sr/profile.hpp"

namespace {

int g_failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

void checkNear(double actual, double expected, double tolerance, const char* what) {
    if (!(std::fabs(actual - expected) <= tolerance)) {
        std::fprintf(stderr, "FAIL: %s (expected %.9f, got %.9f)\n", what, expected, actual);
        ++g_failures;
    }
}

void checkRgb(sr::Rgb actual, sr::Rgb expected, double tolerance, const char* what) {
    if (!(std::fabs(actual.r - expected.r) <= tolerance &&
          std::fabs(actual.g - expected.g) <= tolerance &&
          std::fabs(actual.b - expected.b) <= tolerance)) {
        std::fprintf(stderr, "FAIL: %s (expected %.5f %.5f %.5f, got %.5f %.5f %.5f)\n", what,
                     expected.r, expected.g, expected.b, actual.r, actual.g, actual.b);
        ++g_failures;
    }
}

std::string sourceDir() { return SR_SOURCE_DIR; }

}  // namespace

int main() {
    const std::string root = sourceDir();
    sr::Profile film;
    sr::Profile paper;
    std::string error;

    check(sr::loadProfile(root + "/data/profiles/kodak_portra_400.json", film, error),
          "Kodak Portra 400 profile loads");
    check(sr::loadProfile(root + "/data/profiles/kodak_portra_endura.json", paper, error),
          "Kodak Portra Endura profile loads");
    if (g_failures != 0) return 1;

    check(film.info.stock == "kodak_portra_400", "film stock name");
    check(paper.info.stock == "kodak_portra_endura", "paper stock name");

    sr::Triple filters{0.0, 0.0, 0.0};
    check(sr::loadNeutralFilters(root + "/data/profiles/neutral_print_filters.json",
                                 paper.info.stock, film.info.stock, "TH-KG3", filters, error),
          "neutral print filters load");
    checkNear(filters[0], 0.0, 1e-12, "neutral filter C");
    checkNear(filters[1], 51.56801468495496, 1e-12, "neutral filter M");
    checkNear(filters[2], 52.53400422349596, 1e-12, "neutral filter Y");

    sr::FilmModel model;
    check(model.configure(film, paper, filters, error), "M1 chain configures");

    // Upstream PrintingStage._compute_exposure_factor_midgray for lut_mode.
    // We skip colour-science's no-op sRGB->sRGB round trip, whose matrix
    // round-off shifts the mid-grey input by ~1e-5; see docs/parity.md.
    checkNear(model.printExposureFactor(), 0.837787256300, 5e-5, "print exposure factor");

    // Probes from the reference chain (display-encoded sRGB in and out).
    checkRgb(model.transform({0.0f, 0.0f, 0.0f}), {0.04959f, 0.05322f, 0.06565f}, 5e-4,
             "probe 0.0");
    checkRgb(model.transform({0.5f, 0.5f, 0.5f}), {0.49472f, 0.50497f, 0.51136f}, 5e-4,
             "probe 0.5");
    checkRgb(model.transform({1.0f, 1.0f, 1.0f}), {0.84345f, 0.84896f, 0.85185f}, 5e-4,
             "probe 1.0");
    checkRgb(model.transform({0.8f, 0.2f, 0.2f}), {0.63112f, 0.18985f, 0.21619f}, 5e-4,
             "probe 0.8 0.2 0.2");

    // A LUT bake must reproduce the per-pixel transform at its grid points.
    const sr::Lut3D lut = model.bake(9, "test");
    check(lut.size() == 9, "baked LUT size");
    const sr::Rgb fromLut = lut.apply({0.5f, 0.5f, 0.5f});
    checkRgb(fromLut, model.transform({0.5f, 0.5f, 0.5f}), 1e-6, "LUT matches transform");

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
