// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "sr/film_model.hpp"

namespace sr {

// D3D11 implementation of the per-pixel physical chain (D046). Windows-only;
// on other platforms available() returns false and the caller keeps the CPU
// spectral or LUT path. Not thread-safe: one instance drives one dispatch at a
// time.
class GpuFilm {
public:
    GpuFilm();
    ~GpuFilm();
    GpuFilm(const GpuFilm&) = delete;
    GpuFilm& operator=(const GpuFilm&) = delete;

    static bool available(std::string& error);

    bool configure(const FilmModel& model, const BakeOptions& options, std::string& error);

    // `rgb` and `out` are interleaved float RGB, `pixels * 3` values. They may
    // alias (in-place). `strength` blends the film result with the encoded
    // input, matching sr::mix. Returns false and fills `error` on device
    // failure.
    bool process(const float* rgb, std::size_t pixels, float* out, float strength,
                 std::string& error);

    double lastMilliseconds() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace sr
