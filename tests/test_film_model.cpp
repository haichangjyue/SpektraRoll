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

    // Contrast pivots at 18% grey: grey must not move, shadows must darken.
    sr::BakeOptions punchy = negative;
    punchy.contrast = 1.2;
    checkRgb(model.transformFrom(punchy, {grey, grey, grey}), negativeGrey, 0.01f,
             "contrast leaves 18% grey alone");
    const sr::Rgb darkBase = model.transformFrom(negative, {0.3f, 0.3f, 0.3f});
    const sr::Rgb darkPunchy = model.transformFrom(punchy, {0.3f, 0.3f, 0.3f});
    check(darkPunchy.g < darkBase.g, "contrast above 1 darkens shadows");

    // DIR couplers (D028): parity anchors from the upstream reference bake
    // (out/couplers_ref_33.cube, PSNR 80.75 dB against this implementation).
    sr::FilmModelOptions couplerOptions;
    couplerOptions.couplers = true;
    sr::FilmModel couplerModel;
    check(couplerModel.configure(film, paper, filters, error, couplerOptions),
          "coupler chain configures");
    checkRgb(couplerModel.transform({0.5f, 0.5f, 0.5f}), {0.501f, 0.507f, 0.498f}, 1e-3,
             "couplers probe 0.5");
    checkRgb(couplerModel.transform({0.8125f, 0.1875f, 0.1875f}), {0.664f, 0.073f, 0.107f},
             1e-3, "couplers probe warm red");
    checkRgb(couplerModel.transform({0.1875f, 0.5f, 0.1875f}), {0.014f, 0.41f, 0.109f}, 1e-3,
             "couplers probe green");
    check(model.transform({0.5f, 0.5f, 0.5f}).g != couplerModel.transform({0.5f, 0.5f, 0.5f}).g,
          "couplers change the result");

    // D065: cinema chain. Vision3 500T is measured under T and the 2383 print is
    // viewed under K75P. Anchors from tools/parity/reference_model.py (same 33^3
    // bake, PSNR 88.85 dB at 17^3).
    sr::Profile cineFilm;
    sr::Profile cinePrint;
    check(sr::loadProfile(root + "/data/profiles/kodak_vision3_500t.json", cineFilm, error),
          "Vision3 500T profile loads");
    check(sr::loadProfile(root + "/data/profiles/kodak_2383.json", cinePrint, error),
          "2383 print profile loads");
    check(cineFilm.info.stage == "filming" && cinePrint.info.stage == "printing",
          "cinema profile stages");
    sr::Triple cineFilters{0.0, 0.0, 0.0};
    check(sr::loadNeutralFilters(root + "/data/profiles/neutral_print_filters.json",
                                 cinePrint.info.stock, cineFilm.info.stock, "TH-KG3",
                                 cineFilters, error),
          "cinema neutral filters load");
    sr::FilmModel cine;
    check(cine.configure(cineFilm, cinePrint, cineFilters, error), "cinema chain configures");
    checkNear(cine.printExposureFactor(), 1.720784698861, 5e-5, "cinema print exposure factor");
    checkRgb(cine.transform({0.0f, 0.0f, 0.0f}), {0.00273f, 0.00372f, 0.00335f}, 5e-4,
             "cinema probe 0.0");
    checkRgb(cine.transform({0.184f, 0.184f, 0.184f}), {0.03846f, 0.06614f, 0.05668f}, 5e-4,
             "cinema probe 0.184");
    checkRgb(cine.transform({0.5f, 0.5f, 0.5f}), {0.49671f, 0.49776f, 0.49658f}, 5e-4,
             "cinema probe 0.5");
    checkRgb(cine.transform({1.0f, 1.0f, 1.0f}), {0.83715f, 0.82096f, 0.77999f}, 5e-4,
             "cinema probe 1.0");
    checkRgb(cine.transform({0.8f, 0.2f, 0.2f}), {0.60061f, 0.20366f, 0.16389f}, 5e-4,
             "cinema probe warm red");
    checkRgb(cine.transform({0.2f, 0.6f, 0.2f}), {0.09026f, 0.42742f, 0.26134f}, 5e-4,
             "cinema probe green");

    // The daylight Vision3 and the alternative print film take the same path.
    sr::Profile daylightFilm;
    sr::Profile premierPrint;
    check(sr::loadProfile(root + "/data/profiles/kodak_vision3_250d.json", daylightFilm, error),
          "Vision3 250D profile loads");
    check(sr::loadProfile(root + "/data/profiles/kodak_2393.json", premierPrint, error),
          "2393 print profile loads");
    sr::Triple daylightFilters{0.0, 0.0, 0.0};
    check(sr::loadNeutralFilters(root + "/data/profiles/neutral_print_filters.json",
                                 premierPrint.info.stock, daylightFilm.info.stock, "TH-KG3",
                                 daylightFilters, error),
          "250D / 2393 neutral filters load");
    sr::FilmModel daylight;
    check(daylight.configure(daylightFilm, premierPrint, daylightFilters, error),
          "250D / 2393 configures");
    const sr::Rgb daylightGrey = daylight.transform({0.5f, 0.5f, 0.5f});
    check(std::fabs(daylightGrey.r - daylightGrey.g) < 0.01f &&
              std::fabs(daylightGrey.g - daylightGrey.b) < 0.01f,
          "250D / 2393 mid-grey is neutral");

    // D067: reversal (slide) direct scan. A positive stock is scanned through
    // its own dyes; the scan is balanced between clear base (white) and maximum
    // dye (black), then mid-grey is neutralised like the negative scan.
    sr::Profile slideFilm;
    check(sr::loadProfile(root + "/data/profiles/kodak_ektachrome_100.json", slideFilm, error),
          "Ektachrome 100 profile loads");
    check(slideFilm.info.type == "positive", "Ektachrome is a positive stock");
    sr::Triple slideFilters{0.0, 0.0, 0.0};
    check(sr::loadNeutralFilters(root + "/data/profiles/neutral_print_filters.json",
                                 paper.info.stock, slideFilm.info.stock, "TH-KG3", slideFilters,
                                 error),
          "Ektachrome neutral filters load");
    sr::FilmModel slide;
    check(slide.configure(slideFilm, paper, slideFilters, error), "slide chain configures");

    sr::BakeOptions slideOptions;
    slideOptions.scan = sr::ScanMode::Positive;
    slideOptions.gamutCompress = true;
    checkRgb(slide.transformFrom(slideOptions, {0.0f, 0.0f, 0.0f}), {0.0f, 0.0f, 0.0f}, 1e-3,
             "slide scan black");
    const sr::Rgb slideWhite = slide.transformFrom(slideOptions, {1.0f, 1.0f, 1.0f});
    check(slideWhite.r > 0.9f && slideWhite.g > 0.9f && slideWhite.b > 0.9f,
          "slide scan reaches white");
    const sr::Rgb slideGrey =
        slide.transformFrom(slideOptions, {static_cast<float>(sr::srgbEncode(0.18)),
                                           static_cast<float>(sr::srgbEncode(0.18)),
                                           static_cast<float>(sr::srgbEncode(0.18))});
    check(std::fabs(slideGrey.r - slideGrey.g) < 0.02f &&
              std::fabs(slideGrey.g - slideGrey.b) < 0.02f,
          "slide scan grey is neutral");
    float previousSlide = -1.0f;
    for (float code = 0.0f; code <= 0.9f; code += 0.05f) {
        const float value = slide.transformFrom(slideOptions, {code, code, code}).g;
        check(value >= previousSlide, "slide scan is monotonic");
        previousSlide = value;
    }
    // Frozen anchors from the same arithmetic in tools/parity (float64).
    checkRgb(slide.transformFrom(slideOptions, {0.5f, 0.5f, 0.5f}),
             {0.56516f, 0.56440f, 0.56354f}, 5e-4, "slide probe 0.5");
    checkRgb(slide.transformFrom(slideOptions, {1.0f, 1.0f, 1.0f}),
             {0.92891f, 0.93354f, 0.92488f}, 5e-4, "slide probe 1.0");

    // The other three reversal stocks take the same path.
    const char* reversalStocks[] = {"kodak_kodachrome_64", "fujifilm_velvia_100",
                                    "fujifilm_provia_100f"};
    for (const char* stock : reversalStocks) {
        sr::Profile profile;
        check(sr::loadProfile(root + "/data/profiles/" + stock + ".json", profile, error),
              "reversal profile loads");
        check(profile.info.type == "positive", "reversal stock is positive");
        sr::Triple filters{0.0, 0.0, 0.0};
        check(sr::loadNeutralFilters(root + "/data/profiles/neutral_print_filters.json",
                                     paper.info.stock, profile.info.stock, "TH-KG3", filters,
                                     error),
              "reversal neutral filters load");
        sr::FilmModel model;
        check(model.configure(profile, paper, filters, error), "reversal chain configures");
        const sr::Rgb white = model.transformFrom(slideOptions, {1.0f, 1.0f, 1.0f});
        check(white.r > 0.85f && white.g > 0.85f && white.b > 0.85f,
              "reversal scan reaches white");
    }

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
