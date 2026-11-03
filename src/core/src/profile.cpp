// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include "sr/profile.hpp"

#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

#include "sr/json.hpp"

namespace sr {
namespace {

constexpr int kExpectedWavelengths = 81;

bool readFile(const std::string& path, std::string& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open profile: " + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

const JsonValue* member(const JsonValue& object, const std::string& key, const std::string& where,
                        std::string& error) {
    const JsonValue* value = object.find(key);
    if (value == nullptr) {
        error = "profile is missing " + where + "." + key;
    }
    return value;
}

bool readNumber(const JsonValue& value, double& out) {
    if (value.isNumber()) {
        out = value.number();
        return true;
    }
    if (value.isNull()) {  // spektrafilm uses `null` for "no measurement"
        out = std::numeric_limits<double>::quiet_NaN();
        return true;
    }
    return false;
}

bool readVector(const JsonValue& value, const std::string& where, std::vector<double>& out,
                std::string& error) {
    if (!value.isArray()) {
        error = "profile field " + where + " must be an array";
        return false;
    }
    out.clear();
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        double number = 0.0;
        if (!readNumber(value.items()[i], number)) {
            error = "profile field " + where + "[" + std::to_string(i) + "] is not a number";
            return false;
        }
        out.push_back(number);
    }
    return true;
}

bool readTriples(const JsonValue& value, const std::string& where, std::vector<Triple>& out,
                 std::string& error) {
    if (!value.isArray()) {
        error = "profile field " + where + " must be an array";
        return false;
    }
    out.clear();
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        const JsonValue& row = value.items()[i];
        if (!row.isArray() || row.size() != 3) {
            error = "profile field " + where + "[" + std::to_string(i) + "] must have 3 values";
            return false;
        }
        Triple triple{};
        for (int c = 0; c < 3; ++c) {
            if (!readNumber(row.items()[static_cast<size_t>(c)], triple[c])) {
                error = "profile field " + where + "[" + std::to_string(i) + "][" +
                        std::to_string(c) + "] is not a number";
                return false;
            }
        }
        out.push_back(triple);
    }
    return true;
}

bool readString(const JsonValue& object, const std::string& key, std::string& out) {
    const JsonValue* value = object.find(key);
    if (value == nullptr || !value->isString()) return false;
    out = value->string();
    return true;
}

bool readModel(const JsonValue& value, DensityCurvesModel& out, std::string& error) {
    if (!value.isObject()) {
        error = "profile field data.density_curves_model must be an object";
        return false;
    }
    const auto readMatrix = [&](const char* key, std::vector<double>& target) {
        const JsonValue* matrix = value.find(key);
        if (matrix == nullptr || !matrix->isArray() || matrix->size() == 0) {
            error = std::string("profile field data.density_curves_model.") + key +
                    " must be a non-empty array";
            return false;
        }
        target.clear();
        const int channels = static_cast<int>(matrix->size());
        int layers = -1;
        for (int c = 0; c < channels; ++c) {
            const JsonValue& row = matrix->items()[static_cast<size_t>(c)];
            if (!row.isArray()) {
                error = std::string("profile field data.density_curves_model.") + key +
                        " must be an array of arrays";
                return false;
            }
            if (layers < 0) {
                layers = static_cast<int>(row.size());
            } else if (static_cast<int>(row.size()) != layers) {
                error = std::string("profile field data.density_curves_model.") + key +
                        " has inconsistent row lengths";
                return false;
            }
            for (size_t i = 0; i < row.size(); ++i) {
                double number = 0.0;
                if (!readNumber(row.items()[i], number)) {
                    error = std::string("profile field data.density_curves_model.") + key +
                            " contains a non-number";
                    return false;
                }
                target.push_back(number);
            }
        }
        if (out.channels == 0) {
            out.channels = channels;
            out.layers = layers;
        } else if (out.channels != channels || out.layers != layers) {
            error = std::string("profile field data.density_curves_model.") + key +
                    " does not match the other matrices";
            return false;
        }
        return true;
    };

    if (!readMatrix("centers", out.centers)) return false;
    if (!readMatrix("amplitudes", out.amplitudes)) return false;
    if (!readMatrix("sigmas", out.sigmas)) return false;
    if (!out.valid()) {
        error = "profile field data.density_curves_model is not a valid 3-channel model";
        return false;
    }
    return true;
}

bool checkShape(const Profile& profile, const std::string& path, std::string& error) {
    if (profile.wavelengths.size() != kExpectedWavelengths) {
        error = path + ": expected " + std::to_string(kExpectedWavelengths) +
                " spectral samples, found " + std::to_string(profile.wavelengths.size());
        return false;
    }
    for (int i = 0; i < kExpectedWavelengths; ++i) {
        const double expected = 380.0 + 5.0 * i;
        if (std::abs(profile.wavelengths[static_cast<size_t>(i)] - expected) > 1e-6) {
            error = path + ": wavelength " + std::to_string(i) + " is " +
                    std::to_string(profile.wavelengths[static_cast<size_t>(i)]) +
                    " nm, expected " + std::to_string(expected) + " nm";
            return false;
        }
    }
    const size_t n = kExpectedWavelengths;
    if (profile.logSensitivity.size() != n || profile.channelDensity.size() != n ||
        profile.baseDensity.size() != n) {
        error = path + ": spectral arrays must all have " + std::to_string(n) + " rows";
        return false;
    }
    if (profile.logExposure.size() < 2 || profile.densityCurves.size() != profile.logExposure.size()) {
        error = path + ": log_exposure and density_curves must be equally sized (>= 2)";
        return false;
    }
    return true;
}

}  // namespace

bool loadProfile(const std::string& path, Profile& out, std::string& error) {
    std::string text;
    if (!readFile(path, text, error)) return false;

    JsonValue root;
    if (!JsonValue::parse(text, root, error)) {
        error = path + ": " + error;
        return false;
    }

    const JsonValue* info = member(root, "info", "root", error);
    if (info == nullptr) return false;
    const JsonValue* data = member(root, "data", "root", error);
    if (data == nullptr) return false;
    if (!info->isObject() || !data->isObject()) {
        error = path + ": info and data must be objects";
        return false;
    }

    out = Profile{};
    readString(*info, "stock", out.info.stock);
    readString(*info, "name", out.info.name);
    readString(*info, "type", out.info.type);
    readString(*info, "support", out.info.support);
    readString(*info, "stage", out.info.stage);
    readString(*info, "reference_illuminant", out.info.referenceIlluminant);
    readString(*info, "viewing_illuminant", out.info.viewingIlluminant);

    if (out.info.stock.empty()) {
        error = path + ": profile has no info.stock";
        return false;
    }
    if (out.info.type != "negative" && out.info.type != "positive") {
        error = path + ": unsupported info.type '" + out.info.type + "'";
        return false;
    }

    const auto field = [&](const char* key) -> const JsonValue* {
        const JsonValue* value = data->find(key);
        if (value == nullptr) {
            error = path + ": profile is missing data." + key;
        }
        return value;
    };

    const JsonValue* wavelengths = field("wavelengths");
    if (wavelengths == nullptr) return false;
    if (!readVector(*wavelengths, "data.wavelengths", out.wavelengths, error)) return false;

    const JsonValue* sensitivity = field("log_sensitivity");
    if (sensitivity == nullptr) return false;
    if (!readTriples(*sensitivity, "data.log_sensitivity", out.logSensitivity, error)) return false;

    const JsonValue* channelDensity = field("channel_density");
    if (channelDensity == nullptr) return false;
    if (!readTriples(*channelDensity, "data.channel_density", out.channelDensity, error))
        return false;

    const JsonValue* baseDensity = field("base_density");
    if (baseDensity == nullptr) return false;
    if (!readVector(*baseDensity, "data.base_density", out.baseDensity, error)) return false;

    const JsonValue* logExposure = field("log_exposure");
    if (logExposure == nullptr) return false;
    if (!readVector(*logExposure, "data.log_exposure", out.logExposure, error)) return false;

    const JsonValue* curves = field("density_curves");
    if (curves == nullptr) return false;
    if (!readTriples(*curves, "data.density_curves", out.densityCurves, error)) return false;

    if (const JsonValue* model = data->find("density_curves_model")) {
        if (!readModel(*model, out.model, error)) return false;
    }

    if (!checkShape(out, path, error)) return false;
    return true;
}

bool loadNeutralFilters(const std::string& path, const std::string& paper,
                        const std::string& film, const std::string& illuminant,
                        Triple& out, std::string& error) {
    std::string text;
    if (!readFile(path, text, error)) return false;

    JsonValue root;
    if (!JsonValue::parse(text, root, error)) {
        error = path + ": " + error;
        return false;
    }

    const JsonValue* paperNode = root.find(paper);
    if (paperNode == nullptr) {
        error = "neutral print filters: no entry for paper '" + paper + "'";
        return false;
    }
    const JsonValue* illuminantNode = paperNode->find(illuminant);
    if (illuminantNode == nullptr) {
        error = "neutral print filters: no entry for paper '" + paper + "' under '" + illuminant + "'";
        return false;
    }
    const JsonValue* filmNode = illuminantNode->find(film);
    if (filmNode == nullptr) {
        error = "neutral print filters: no entry for film '" + film + "'";
        return false;
    }
    if (!filmNode->isArray() || filmNode->size() != 3) {
        error = "neutral print filters: entry must be an array of 3 CC values";
        return false;
    }
    for (size_t i = 0; i < 3; ++i) {
        if (!filmNode->items()[i].isNumber()) {
            error = "neutral print filters: entry contains a non-number";
            return false;
        }
        out[i] = filmNode->items()[i].number();
    }
    return true;
}

}  // namespace sr
