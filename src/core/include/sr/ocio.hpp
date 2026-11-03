// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sr {

// OpenColorIO support (D062).
//
// The library is optional: when SpektraRoll is built without it, every call
// below reports the same "not built with OpenColorIO" error and the rest of the
// engine is unchanged. The CLI turns that into a readable message instead of
// silently ignoring --ocio-* flags.
//
// The contract with the film model is narrow on purpose: OCIO only converts
// between colour spaces, and the film chain always receives *linear Rec.709*.
// A config therefore needs a linear Rec.709 space (the ACES CG config calls it
// "Linear Rec.709 (sRGB)"); `linearSpace` can name it explicitly when a config
// spells it differently.

bool ocioAvailable();
std::string ocioVersion();

// An open config. Cheap to copy (shared), safe to hold across threads.
class OcioConfig {
public:
    OcioConfig() = default;
    bool valid() const { return impl_ != nullptr; }
    // Absolute path the config was loaded from (empty when it came from $OCIO
    // or from the built-in raw config).
    std::string source() const;
    std::string name() const;
    // Opaque handle for the implementation; nullptr in stub builds.
    const std::shared_ptr<void>& handle() const { return impl_; }

private:
    friend bool openOcioConfig(const std::string&, OcioConfig&, std::string&);
    std::shared_ptr<void> impl_;
};

// `path` may be empty, in which case $OCIO is used. Loading errors include the
// OCIO exception text, which already names the offending file.
bool openOcioConfig(const std::string& path, OcioConfig& config, std::string& error);

struct OcioSummary {
    std::string version;
    std::string configName;
    std::string configFile;
    // The linear space the film model will receive (Rec.709 primaries).
    std::string linearSpace;
    std::vector<std::string> colorSpaces;
    std::vector<std::string> displays;
    std::vector<std::pair<std::string, std::vector<std::string>>> views;
};

bool describeOcioConfig(const OcioConfig& config, const std::string& linearHint,
                        OcioSummary& summary, std::string& error);

// The linear space used when --ocio-linear is not given. Tries the names the
// ACES and studio configs use, then falls back to anything whose name contains
// "Rec.709" and "Linear".
bool pickOcioLinearSpace(const OcioConfig& config, const std::string& hint, std::string& out,
                         std::string& error);

// A compiled transform. `toLinear` takes code values in the source space and
// returns linear Rec.709; `toDisplay` goes the other way into a display/view.
class OcioProcessor {
public:
    OcioProcessor() = default;
    bool valid() const { return impl_ != nullptr; }
    // Single sample (LUT baking); thread safe.
    void apply(const double in[3], double out[3]) const;
    // Whole buffer, interleaved RGB float, in place. Much faster than apply()
    // per pixel because OCIO can vectorise it.
    void apply(float* rgb, long pixels) const;
    const std::shared_ptr<void>& handle() const { return impl_; }

private:
    friend bool makeOcioInputProcessor(const OcioConfig&, const std::string&,
                                       const std::string&, OcioProcessor&, std::string&);
    friend bool makeOcioDisplayProcessor(const OcioConfig&, const std::string&,
                                         const std::string&, const std::string&,
                                         OcioProcessor&, std::string&);
    std::shared_ptr<void> impl_;
};

// source space -> linear Rec.709
bool makeOcioInputProcessor(const OcioConfig& config, const std::string& sourceSpace,
                            const std::string& linearSpace, OcioProcessor& out,
                            std::string& error);

// linear Rec.709 -> display/view
bool makeOcioDisplayProcessor(const OcioConfig& config, const std::string& linearSpace,
                              const std::string& display, const std::string& view,
                              OcioProcessor& out, std::string& error);

// Convenience: decode one triple from `sourceSpace` to linear Rec.709 using a
// processor made by makeOcioInputProcessor.
inline void ocioToLinear(const OcioProcessor& processor, const double encoded[3],
                         double linear[3]) {
    processor.apply(encoded, linear);
}

}  // namespace sr
