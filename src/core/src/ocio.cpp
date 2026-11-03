// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Thin wrapper around OpenColorIO (D062). Only this file includes OCIO headers,
// so the rest of the engine never has to care whether the library was built in.
#include "sr/ocio.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

#include <OpenColorIO/OpenColorIO.h>

namespace sr {
namespace {

namespace OCIO = OCIO_NAMESPACE;

struct ConfigBox {
    OCIO::ConstConfigRcPtr config;
    std::string source;
    std::string name;
};

struct ProcessorBox {
    OCIO::ConstCPUProcessorRcPtr cpu;
    // Keeps the config alive for as long as the processor is used.
    OCIO::ConstConfigRcPtr config;
};

std::string lower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool isLinearRec709(const std::string& name) {
    const std::string key = lower(name);
    if (key.find("linear") == std::string::npos) return false;
    return key.find("rec.709") != std::string::npos || key.find("rec709") != std::string::npos ||
           key.find("srgb") != std::string::npos;
}

// The names the ACES, studio and camera configs use for linear Rec.709.
const char* const kLinearCandidates[] = {
    "Linear Rec.709 (sRGB)",
    "Linear Rec.709",
    "Linear sRGB",
    "Utility - Linear - Rec.709",
    "lin_rec709",
};

}  // namespace

bool ocioAvailable() { return true; }

std::string ocioVersion() { return OCIO_VERSION_STR; }

std::string OcioConfig::source() const {
    if (!impl_) return {};
    return static_cast<const ConfigBox*>(impl_.get())->source;
}

std::string OcioConfig::name() const {
    if (!impl_) return {};
    return static_cast<const ConfigBox*>(impl_.get())->name;
}

bool openOcioConfig(const std::string& path, OcioConfig& config, std::string& error) {
    try {
        auto box = std::make_shared<ConfigBox>();
        if (path.empty()) {
            box->config = OCIO::GetCurrentConfig();
            box->source = "$OCIO";
        } else {
            box->config = OCIO::Config::CreateFromFile(path.c_str());
            box->source = path;
        }
        if (!box->config) {
            error = "OpenColorIO returned an empty config for " +
                    (path.empty() ? std::string("$OCIO") : path);
            return false;
        }
        box->name = box->config->getName();
        config.impl_ = box;
        return true;
    } catch (const OCIO::Exception& e) {
        error = std::string("OpenColorIO: ") + e.what();
        return false;
    } catch (const std::exception& e) {
        error = std::string("OpenColorIO: ") + e.what();
        return false;
    }
}

bool pickOcioLinearSpace(const OcioConfig& config, const std::string& hint, std::string& out,
                         std::string& error) {
    if (!config.valid()) {
        error = "no OpenColorIO config is loaded";
        return false;
    }
    const auto* box = static_cast<const ConfigBox*>(config.handle().get());
    try {
        if (!hint.empty()) {
            // getColorSpace also resolves roles, so "scene_linear" works too.
            if (box->config->getColorSpace(hint.c_str())) {
                out = hint;
                return true;
            }
            error = "OpenColorIO config has no colour space named '" + hint + "'";
            return false;
        }
        for (const char* candidate : kLinearCandidates) {
            if (box->config->getColorSpace(candidate)) {
                out = candidate;
                return true;
            }
        }
        // Fall back to the only colour space that looks like linear Rec.709.
        std::vector<std::string> matches;
        const int count = box->config->getNumColorSpaces();
        for (int i = 0; i < count; ++i) {
            const char* name = box->config->getColorSpaceNameByIndex(i);
            if (name != nullptr && isLinearRec709(name)) matches.emplace_back(name);
        }
        if (matches.size() == 1) {
            out = matches.front();
            return true;
        }
        if (matches.empty()) {
            error = "OpenColorIO config has no linear Rec.709 colour space; pass --ocio-linear "
                    "<name> (try `ocio-info` to list the spaces)";
            return false;
        }
        error = "several linear Rec.709 colour spaces match (" + matches.front() + ", ...); pass "
                "--ocio-linear <name>";
        return false;
    } catch (const OCIO::Exception& e) {
        error = std::string("OpenColorIO: ") + e.what();
        return false;
    }
}

bool describeOcioConfig(const OcioConfig& config, const std::string& linearHint,
                        OcioSummary& summary, std::string& error) {
    if (!config.valid()) {
        error = "no OpenColorIO config is loaded";
        return false;
    }
    const auto* box = static_cast<const ConfigBox*>(config.handle().get());
    try {
        summary.version = OCIO_VERSION_STR;
        summary.configName = box->name;
        summary.configFile = box->source;
        if (!pickOcioLinearSpace(config, linearHint, summary.linearSpace, error)) return false;
        const int colorSpaces = box->config->getNumColorSpaces();
        summary.colorSpaces.reserve(static_cast<size_t>(colorSpaces));
        for (int i = 0; i < colorSpaces; ++i) {
            const char* name = box->config->getColorSpaceNameByIndex(i);
            if (name != nullptr) summary.colorSpaces.emplace_back(name);
        }
        const int displays = box->config->getNumDisplays();
        for (int i = 0; i < displays; ++i) {
            const char* display = box->config->getDisplay(i);
            if (display == nullptr) continue;
            summary.displays.emplace_back(display);
            std::vector<std::string> views;
            const int viewCount = box->config->getNumViews(display);
            for (int v = 0; v < viewCount; ++v) {
                const char* view = box->config->getView(display, v);
                if (view != nullptr) views.emplace_back(view);
            }
            summary.views.emplace_back(display, std::move(views));
        }
        return true;
    } catch (const OCIO::Exception& e) {
        error = std::string("OpenColorIO: ") + e.what();
        return false;
    }
}

bool makeOcioInputProcessor(const OcioConfig& config, const std::string& sourceSpace,
                            const std::string& linearSpace, OcioProcessor& out,
                            std::string& error) {
    if (!config.valid()) {
        error = "no OpenColorIO config is loaded";
        return false;
    }
    if (sourceSpace.empty()) {
        error = "--ocio-input needs a colour space name";
        return false;
    }
    const auto* box = static_cast<const ConfigBox*>(config.handle().get());
    try {
        if (!box->config->getColorSpace(sourceSpace.c_str())) {
            error = "OpenColorIO config has no colour space named '" + sourceSpace + "'";
            return false;
        }
        auto processor = box->config->getProcessor(sourceSpace.c_str(), linearSpace.c_str());
        auto cpu = processor->getDefaultCPUProcessor();
        if (!cpu) {
            error = "OpenColorIO could not build a CPU processor for '" + sourceSpace +
                    "' -> '" + linearSpace + "'";
            return false;
        }
        auto impl = std::make_shared<ProcessorBox>();
        impl->cpu = cpu;
        impl->config = box->config;
        out = OcioProcessor();
        out.impl_ = impl;
        return true;
    } catch (const OCIO::Exception& e) {
        error = std::string("OpenColorIO: ") + e.what();
        return false;
    }
}

bool makeOcioDisplayProcessor(const OcioConfig& config, const std::string& linearSpace,
                              const std::string& display, const std::string& view,
                              OcioProcessor& out, std::string& error) {
    if (!config.valid()) {
        error = "no OpenColorIO config is loaded";
        return false;
    }
    if (display.empty() || view.empty()) {
        error = "a display transform needs both --ocio-display and --ocio-view";
        return false;
    }
    const auto* box = static_cast<const ConfigBox*>(config.handle().get());
    try {
        auto processor = box->config->getProcessor(
            linearSpace.c_str(), display.c_str(), view.c_str(),
            OCIO::TRANSFORM_DIR_FORWARD);
        auto cpu = processor->getDefaultCPUProcessor();
        if (!cpu) {
            error = "OpenColorIO could not build a CPU processor for '" + display + "/" + view +
                    "'";
            return false;
        }
        auto impl = std::make_shared<ProcessorBox>();
        impl->cpu = cpu;
        impl->config = box->config;
        out = OcioProcessor();
        out.impl_ = impl;
        return true;
    } catch (const OCIO::Exception& e) {
        error = std::string("OpenColorIO: ") + e.what();
        return false;
    }
}

void OcioProcessor::apply(const double in[3], double out[3]) const {
    if (!impl_) {
        out[0] = in[0];
        out[1] = in[1];
        out[2] = in[2];
        return;
    }
    float pixel[3] = {static_cast<float>(in[0]), static_cast<float>(in[1]),
                      static_cast<float>(in[2])};
    static_cast<const ProcessorBox*>(impl_.get())->cpu->applyRGB(pixel);
    out[0] = pixel[0];
    out[1] = pixel[1];
    out[2] = pixel[2];
}

void OcioProcessor::apply(float* rgb, long pixels) const {
    if (!impl_ || pixels <= 0) return;
    OCIO::PackedImageDesc image(rgb, pixels, 1, 3);
    static_cast<const ProcessorBox*>(impl_.get())->cpu->apply(image);
}

}  // namespace sr
