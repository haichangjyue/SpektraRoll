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

    // The M2 entry point must be a no-op for sRGB input at 0 EV (D013/D021).
    sr::BakeOptions options;
    checkRgb(model.transformFrom(options, {0.5f, 0.5f, 0.5f}), model.transform({0.5f, 0.5f, 0.5f}),
             1e-9, "transformFrom(sRGB) matches M1 transform");
    checkRgb(model.transformFrom(options, {0.8f, 0.2f, 0.2f}), model.transform({0.8f, 0.2f, 0.2f}),
             1e-9, "transformFrom(sRGB) matches M1 transform (colour)");

    // Apple Log input: 18% grey in BT.2020 (0.48827 per channel) must land on
    // the same print as 18% grey in sRGB (0.18 linear -> 0.46135 encoded).
    // The BT.2020 -> sRGB matrix rounds at ~1.6e-4, so grey is not bit-exact.
    sr::BakeOptions appleLog;
    appleLog.input = sr::InputTransfer::AppleLog;
    const float grey = static_cast<float>(sr::appleLogEncode(0.18));
    checkRgb(model.transformFrom(appleLog, {grey, grey, grey}),
             model.transform({static_cast<float>(sr::srgbEncode(0.18)),
                              static_cast<float>(sr::srgbEncode(0.18)),
                              static_cast<float>(sr::srgbEncode(0.18))}),
             5e-4, "18% grey matches between sRGB and Apple Log input");

    // +1 EV doubles scene light, which must print brighter than 0 EV.
    sr::BakeOptions brighter = appleLog;
    brighter.exposureEv = 1.0;
    const sr::Rgb base = model.transformFrom(appleLog, {0.4f, 0.4f, 0.4f});
    const sr::Rgb up = model.transformFrom(brighter, {0.4f, 0.4f, 0.4f});
    check(up.r > base.r, "positive exposure brightens the print");

    // Scanning the negative keeps more latitude: black stays at 0, white gets
    // close to 1, 18% grey is neutral, and the grey ramp is monotonic.
    sr::BakeOptions negative = appleLog;
    negative.scan = sr::ScanMode::Negative;
    negative.exposureEv = 0.7;  // same default as scripts\demo-m2.ps1
    checkRgb(model.transformFrom(negative, {0.0f, 0.0f, 0.0f}), {0.0f, 0.0f, 0.0f}, 1e-6,
             "negative scan black");
    const sr::Rgb negativeWhite = model.transformFrom(negative, {1.0f, 1.0f, 1.0f});
    check(negativeWhite.r > 0.8f && negativeWhite.g > 0.8f && negativeWhite.b > 0.8f,
          "negative scan reaches white");
    const sr::Rgb negativeGrey = model.transformFrom(negative, {grey, grey, grey});
    check(std::fabs(negativeGrey.r - negativeGrey.g) < 0.02f &&
              std::fabs(negativeGrey.g - negativeGrey.b) < 0.02f,
          "negative scan grey is neutral");
    float previous = -1.0f;
    for (float code = 0.0f; code <= 0.9f; code += 0.05f) {
        const float value = model.transformFrom(negative, {code, code, code}).g;
        check(value >= previous, "negative scan is monotonic");
        previous = value;
    }

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
