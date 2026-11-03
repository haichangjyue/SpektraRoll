// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// OpenColorIO integration (D062). The test is skipped (exit 77) when the build
// has no OCIO, like the GPU parity tests. It uses the vendored ACES 2.0 studio
// config, so it needs no network.
#include <cmath>
#include <cstdio>
#include <string>

#include "sr/ocio.hpp"
#include "sr/transfer.hpp"

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

}  // namespace

int main() {
    if (!sr::ocioAvailable()) {
        std::printf("skipped: this build has no OpenColorIO\n");
        return 77;
    }
    const std::string configPath =
        std::string(SR_SOURCE_DIR) + "/data/ocio/studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio";

    sr::OcioConfig config;
    std::string error;
    if (!sr::openOcioConfig(configPath, config, error)) {
        std::fprintf(stderr, "FAIL: %s\n", error.c_str());
        return 1;
    }
    check(config.valid(), "config loads");
    check(!config.name().empty(), "config has a name");
    check(!sr::ocioVersion().empty(), "version is reported");

    // The film model always wants linear Rec.709; the default pick must find it.
    std::string linear;
    check(sr::pickOcioLinearSpace(config, "", linear, error), "auto-pick a linear space");
    check(linear == "Linear Rec.709 (sRGB)", "auto-pick lands on Linear Rec.709 (sRGB)");

    sr::OcioSummary summary;
    check(sr::describeOcioConfig(config, "", summary, error), "describe the config");
    check(summary.colorSpaces.size() >= 20, "config lists its colour spaces");
    check(!summary.displays.empty(), "config lists its displays");
    bool sawView = false;
    for (const auto& entry : summary.views) {
        if (entry.first == "sRGB - Display" && !entry.second.empty()) sawView = true;
    }
    check(sawView, "sRGB - Display has views");

    // sRGB encoded -> linear Rec.709 must equal the built-in sRGB decode.
    {
        sr::OcioProcessor input;
        check(sr::makeOcioInputProcessor(config, "sRGB Encoded Rec.709 (sRGB)", linear, input,
                                         error),
              "build the sRGB input processor");
        for (double code : {0.0, 0.02, 0.18, 0.5, 0.75, 1.0}) {
            const double in[3] = {code, code, code};
            double out[3];
            input.apply(in, out);
            checkNear(out[0], sr::srgbDecode(code), 5e-5, "sRGB encoded matches srgbDecode");
            checkNear(out[0], out[1], 1e-6, "sRGB stays neutral (g)");
            checkNear(out[0], out[2], 1e-6, "sRGB stays neutral (b)");
        }
    }

    // ARRI LogC3: 18% grey lands on 0.18 linear, through the ACES transforms.
    {
        sr::OcioProcessor input;
        check(sr::makeOcioInputProcessor(config, "ARRI LogC3 (EI800)", linear, input, error),
              "build the ARRI LogC3 processor");
        const double grey[3] = {0.391006832034, 0.391006832034, 0.391006832034};
        double out[3];
        input.apply(grey, out);
        checkNear(out[0], 0.18, 2e-3, "ARRI LogC3 18% grey is 0.18");
        checkNear(out[0], out[1], 1e-4, "ARRI LogC3 grey stays neutral (g)");
        checkNear(out[0], out[2], 1e-4, "ARRI LogC3 grey stays neutral (b)");
    }

    // A display/view transform for the graded result.
    {
        sr::OcioProcessor display;
        check(sr::makeOcioDisplayProcessor(config, linear, "sRGB - Display",
                                           "ACES 2.0 - SDR 100 nits (Rec.709)", display, error),
              "build the sRGB display processor");
        const double grey[3] = {0.18, 0.18, 0.18};
        double shown[3];
        display.apply(grey, shown);
        check(shown[0] > 0.3 && shown[0] < 0.5, "0.18 linear lands in the display midtones");
    }

    // Mistakes must be reported, not silently ignored.
    {
        sr::OcioProcessor proc;
        check(!sr::makeOcioInputProcessor(config, "No Such Space", linear, proc, error),
              "unknown colour space is rejected");
        check(!sr::makeOcioDisplayProcessor(config, linear, "sRGB - Display", "", proc, error),
              "a display transform needs a view");
        std::string bogus;
        check(!sr::pickOcioLinearSpace(config, "No Such Space", bogus, error),
              "unknown linear space is rejected");
        sr::OcioConfig missing;
        check(!sr::openOcioConfig(configPath + ".missing", missing, error),
              "a missing config file is rejected");
    }

    if (g_failures == 0) {
        std::printf("all OCIO checks passed (OpenColorIO %s)\n", sr::ocioVersion().c_str());
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
