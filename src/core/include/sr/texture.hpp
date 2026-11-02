// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sr {

// Film grain and halation are spatial effects, so they cannot live in the 3D
// LUT; they run per frame after the colour transform (D033).

struct GrainParams {
    // 0 = off. 1 is a strong print-grain look; the demo uses ~0.35.
    float amount = 0.0f;
    // Grain radius in pixels (1-2 reads as film grain; 0 is raw per-pixel
    // noise). Real grain scales with the scanned resolution, so this is
    // deliberately resolution dependent.
    float size = 1.4f;
    unsigned seed = 1;
    // Frame index: changing it gives a new, independent grain field.
    int frame = 0;
    // How many frames a field takes to cross-fade into the next one. 1 gives a
    // completely new field every frame (digital "boiling"), 3-4 morphs the
    // grain instead, which is what keeps video from strobing (D036).
    int rate = 3;
};

struct HalationParams {
    // 0 = off.
    float amount = 0.0f;
    // Blur radius as a fraction of the image width (film halation is measured
    // in micrometres, but for now this is the practical knob).
    float radius = 0.006f;
    // Highlights above this luminance glow.
    float threshold = 0.72f;
};

struct GlareParams {
    // 0 = off. Fraction of the veiling light added back (screen blend), which
    // lifts the shadows and lowers contrast, like a lens with haze.
    float amount = 0.0f;
    // Blur radius as a fraction of the image width. This is a very wide blur.
    float radius = 0.02f;
};

struct DiffusionParams {
    // 0 = off. Mix toward the blurred image, like a Pro-Mist style filter.
    float amount = 0.0f;
    // Blur radius as a fraction of the image width.
    float radius = 0.004f;
};

// The four spatial passes in the order the render loop applies them (D033):
// halation, veiling glare, diffusion, grain. The GPU backend mirrors this exact
// order, so keep the chain in one place instead of repeating it per caller.
struct TextureParams {
    GrainParams grain;
    HalationParams halation;
    GlareParams glare;
    DiffusionParams diffusion;
};

// Interleaved float RGB, `width * height * 3` values in 0..1.
void applyHalation(float* pixels, int width, int height, const HalationParams& params);
void applyGlare(float* pixels, int width, int height, const GlareParams& params);
void applyDiffusion(float* pixels, int width, int height, const DiffusionParams& params);

// Reuses the generated grain fields between frames. With the default
// `GrainParams::rate` of 3, a field is on screen for three frames, so without a
// cache two full noise fields plus four blurs are rebuilt for every frame even
// though only one field actually changed (D039).
//
// Keep one cache alive for the whole sequence that is rendered frame by frame:
// the two most recently used fields are kept (an LRU pair), which is exactly
// what the cross-fade needs. It is deliberately not thread-safe; the render
// loop is sequential per frame.
class GrainFieldCache {
public:
    // Returns the blurred noise field for `index`, generating it on a miss.
    // The reference stays valid until the next call with a different index.
    const std::vector<float>& field(unsigned seed, int width, int height, int radius,
                                    std::uint32_t index);

    // Number of fields generated so far (diagnostics and tests).
    std::size_t builds() const { return builds_; }

private:
    struct Entry {
        bool valid = false;
        unsigned seed = 0;
        int width = 0;
        int height = 0;
        int radius = -1;
        std::uint32_t index = 0;
        std::uint64_t lastUse = 0;
        std::vector<float> data;
    };
    Entry entries_[2];
    std::uint64_t clock_ = 0;
    std::size_t builds_ = 0;
};

// `cache` is optional: pass the same instance for every frame of a sequence to
// reuse grain fields; omit it for one-off images. Results are identical either
// way, because the cache stores exactly the fields the uncached path builds.
void applyGrain(float* pixels, int width, int height, const GrainParams& params,
                GrainFieldCache* cache = nullptr);

// Applies the whole chain in the reference order. This is the CPU reference the
// D3D11 texture backend is checked against (D047).
void applyTexture(float* pixels, int width, int height, const TextureParams& params, int frame,
                  GrainFieldCache* cache = nullptr);

}  // namespace sr
