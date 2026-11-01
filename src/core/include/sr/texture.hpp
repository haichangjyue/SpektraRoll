// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <cstddef>

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

// Interleaved float RGB, `width * height * 3` values in 0..1.
void applyHalation(float* pixels, int width, int height, const HalationParams& params);
void applyGlare(float* pixels, int width, int height, const GlareParams& params);
void applyDiffusion(float* pixels, int width, int height, const DiffusionParams& params);
void applyGrain(float* pixels, int width, int height, const GrainParams& params);

}  // namespace sr
