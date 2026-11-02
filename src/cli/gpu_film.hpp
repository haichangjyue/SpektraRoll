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

    // Share a D3D11 device owned by someone else (D049). `device` is an
    // ID3D11Device*; the caller keeps ownership and must outlive this object.
    // Off Windows, or with a null handle, this returns false.
    bool attach(void* device, std::string& error);

    // The raw device handle (ID3D11Device*), for attaching another backend.
    void* deviceHandle() const;

    // `rgb` and `out` are interleaved float RGB, `pixels * 3` values. They may
    // alias (in-place). `strength` blends the film result with the encoded
    // input, matching sr::mix. Returns false and fills `error` on device
    // failure.
    bool process(const float* rgb, std::size_t pixels, float* out, float strength,
                 std::string& error);

    // Split form used by the fused chain (D049): upload + dispatch, leaving the
    // result in resultBuffer(); readback() then copies it out. The two calls
    // must be paired before the next processToDevice().
    bool processToDevice(const float* rgb, std::size_t pixels, float strength,
                         std::string& error);
    void* resultBuffer() const;
    bool readback(float* out, std::size_t pixels, std::string& error);

    double lastMilliseconds() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace sr
