// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Stub used when the build has no OpenColorIO (D062). Every entry point fails
// with the same sentence, so the CLI can explain why --ocio-* did nothing
// instead of pretending the transform ran.
#include "sr/ocio.hpp"

namespace sr {
namespace {

const char* kNoOcio =
    "this build was compiled without OpenColorIO; rebuild with -DSR_WITH_OCIO=ON and a "
    "config (see RUN.md, D062)";

void fail(std::string& error) { error = kNoOcio; }

}  // namespace

bool ocioAvailable() { return false; }

std::string ocioVersion() { return "not built"; }

std::string OcioConfig::source() const { return {}; }

std::string OcioConfig::name() const { return {}; }

bool openOcioConfig(const std::string&, OcioConfig&, std::string& error) {
    fail(error);
    return false;
}

bool pickOcioLinearSpace(const OcioConfig&, const std::string&, std::string&, std::string& error) {
    fail(error);
    return false;
}

bool describeOcioConfig(const OcioConfig&, const std::string&, OcioSummary&, std::string& error) {
    fail(error);
    return false;
}

bool makeOcioInputProcessor(const OcioConfig&, const std::string&, const std::string&,
                            OcioProcessor&, std::string& error) {
    fail(error);
    return false;
}

bool makeOcioDisplayProcessor(const OcioConfig&, const std::string&, const std::string&,
                              const std::string&, OcioProcessor&, std::string& error) {
    fail(error);
    return false;
}

void OcioProcessor::apply(const double in[3], double out[3]) const {
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
}

void OcioProcessor::apply(float*, long) const {}

}  // namespace sr
