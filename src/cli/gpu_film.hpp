// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "sr/film_model.hpp"

namespace sr {

// GPU implementation of the per-pixel physical chain (D046, Metal since D056).
// D3D11 on Windows, Metal on macOS; elsewhere available() returns false and the
// caller keeps the CPU spectral or LUT path. Not thread-safe: one instance
// drives one dispatch at a time.
class GpuFilm {
public:
    GpuFilm();
    ~GpuFilm();
    GpuFilm(const GpuFilm&) = delete;
    GpuFilm& operator=(const GpuFilm&) = delete;

    static bool available(std::string& error);

    // Human-readable backend/device report for `spektra-roll-cli gpu-info`.
    // Includes every Metal device, not just the system default, so a support
    // report shows why a machine fell back to the CPU.
    static std::string deviceInfo();

    bool configure(const FilmModel& model, const BakeOptions& options, std::string& error);

    // Share a GPU device owned by someone else (D049). `device` is an
    // ID3D11Device* on Windows and an id<MTLDevice> on macOS; the caller keeps
    // ownership and must outlive this object. With a null handle this returns
    // false.
    bool attach(void* device, std::string& error);

    // The raw device handle (ID3D11Device* / id<MTLDevice>), for attaching
    // another backend.
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
