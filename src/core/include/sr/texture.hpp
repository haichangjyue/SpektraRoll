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

// D054: how the spatial effects are sized.
// `Physical` spreads the light by a distance in micrometres on the negative,
// converted with the frame width, and shapes the kernel like a real point
// spread function (a sharp core plus a wide skirt). `Box` is the pre-D054
// chain (fraction-of-width radius, three box blurs) and stays available so old
// looks and segment signatures still reproduce.
enum class SpreadModel { Physical, Box };

struct SpreadContext {
    SpreadModel model = SpreadModel::Physical;
    // Width of the negative the frame represents. 36 mm is the 35 mm still
    // frame; a Super 35 / Academy crop would use ~24.9 mm, which makes every
    // spread the same fraction smaller in micrometres per pixel.
    float frameWidthMm = 36.0f;
};

// One effect's kernel sizing, independent of where the parameters came from.
struct SpreadSpec {
    SpreadModel model = SpreadModel::Physical;
    float frameWidthMm = 36.0f;
    float microns = 0.0f;         // physical: effective sigma on the negative
    float radiusFraction = 0.0f;  // box: radius as a fraction of the width
};

// A symmetric separable kernel; `taps[radius]` is the centre and the taps sum
// to one. The GPU backend uploads exactly these numbers instead of rebuilding
// them in HLSL, so the two backends convolve with the same weights.
struct SpreadKernel {
    std::vector<float> taps;
    int radius = 0;
};

enum class SpreadKind { Halation, Glare, Diffusion };

// Micrometres on the negative -> pixels of this frame.
float spreadSigmaPixels(float microns, float frameWidthMm, int width);

// Kernel radius for an effect; the PSF is truncated at three sigma of its
// widest component (the skirt reaches further than three effective sigma).
int spreadKernelRadius(SpreadKind kind, float sigmaPx);

// Downsample factor that keeps the convolution cheap without changing the
// physical size: a 40 px sigma is filtered at a fraction of the resolution,
// because it carries no detail at that scale anyway.
int spreadDownsampleFactor(SpreadKind kind, float sigmaPx);

// Builds the effect's point spread function at this sigma. Mixtures are
// normalised so the result keeps the requested effective sigma and only
// changes the shape (far more energy in the skirt than a plain Gaussian).
SpreadKernel buildSpreadKernel(SpreadKind kind, float sigmaPx);

struct HalationParams {
    // 0 = off.
    float amount = 0.0f;
    // Box model: blur radius as a fraction of the image width.
    float radius = 0.006f;
    // Physical model: scatter distance on the negative, in micrometres. This
    // is the effective sigma of the kernel, calibrated to match the D034 look.
    float spreadMicrons = 225.0f;
    // Highlights above this luminance glow.
    float threshold = 0.72f;
};

struct GlareParams {
    // 0 = off. Fraction of the veiling light added back (screen blend), which
    // lifts the shadows and lowers contrast, like a lens with haze.
    float amount = 0.0f;
    // Box model: blur radius as a fraction of the image width.
    float radius = 0.02f;
    // Physical model: effective sigma on the negative, in micrometres.
    float spreadMicrons = 725.0f;
};

struct DiffusionParams {
    // 0 = off. Mix toward the blurred image, like a Pro-Mist style filter.
    float amount = 0.0f;
    // Box model: blur radius as a fraction of the image width.
    float radius = 0.004f;
    // Physical model: effective sigma on the negative, in micrometres.
    float spreadMicrons = 122.0f;
};

// The four spatial passes in the order the render loop applies them (D033):
// halation, veiling glare, diffusion, grain. The GPU backend mirrors this exact
// order, so keep the chain in one place instead of repeating it per caller.
struct TextureParams {
    GrainParams grain;
    HalationParams halation;
    GlareParams glare;
    DiffusionParams diffusion;
    SpreadContext spread;
};

// Interleaved float RGB, `width * height * 3` values in 0..1.
void applyHalation(float* pixels, int width, int height, const HalationParams& params,
                   const SpreadContext& spread = {});
void applyGlare(float* pixels, int width, int height, const GlareParams& params,
                const SpreadContext& spread = {});
void applyDiffusion(float* pixels, int width, int height, const DiffusionParams& params,
                    const SpreadContext& spread = {});

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
