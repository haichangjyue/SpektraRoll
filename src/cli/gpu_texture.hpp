// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "sr/texture.hpp"

namespace sr {

// D3D11 implementation of the spatial texture chain (D047): halation, veiling
// glare, diffusion and grain. The shaders are a pass-by-pass translation of
// sr::applyTexture's helpers, including the separable box blurs and the
// downsample/blur/upsample shortcut, so the CPU path stays the reference and the
// fallback. Windows-only; elsewhere process() returns false.
class GpuTexture {
public:
    GpuTexture();
    ~GpuTexture();
    GpuTexture(const GpuTexture&) = delete;
    GpuTexture& operator=(const GpuTexture&) = delete;

    static bool available(std::string& error);

    // Share a D3D11 device owned by someone else (D049); `device` is an
    // ID3D11Device* and the caller keeps ownership.
    bool attach(void* device, std::string& error);

    // Compiles the shaders up front so the first frame does not pay for it
    // inside its timing (a single-image run is dominated by that otherwise).
    bool prepare(std::string& error);

    // `rgb` and `out` are interleaved float RGB, `width * height * 3` values and
    // may alias (in-place). `frame` is the sequence frame index used by the grain
    // cross-fade. Returns false and fills `error` on device failure; the caller
    // then keeps using the CPU chain and the output buffer is untouched.
    bool process(const float* rgb, int width, int height, const TextureParams& params, int frame,
                 float* out, std::string& error);

    // Fused path (D049): the input is a float4 buffer already on the device
    // (the film pass output, `pixels` elements). No upload, no pack.
    bool processBuffer(void* input, int width, int height, const TextureParams& params, int frame,
                       float* out, std::string& error);

    double lastMilliseconds() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace sr
