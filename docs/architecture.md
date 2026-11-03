# Architecture

Status: M2 in progress. Parts marked *(built)* exist today; the rest is the target shape.

## The split that makes video possible

Running 81 wavelengths per pixel per frame is not real-time and never will be.
The pipeline is therefore cut in two:

```
colour (per pixel, position independent)   → bake to 3D LUT, evaluate per frame
spatial (neighbourhood dependent)          → GPU passes, every frame
```

- **Colour**: spectral sensitivity integration, characteristic curves, couplers,
  print exposure, scan → a 3D LUT. A 33³ float LUT with tetrahedral
  interpolation is visually lossless for this class of transform.
- **Spatial**: grain, halation, diffusion, glare. These cannot live in a LUT,
  and they must be recomputed per frame because they depend on the image.

Anything that changes per frame (exposure, white balance) must be **locked for
the clip**, otherwise the picture breathes.

## The M1 colour chain *(built)*

`src/core/src/film_model.cpp` implements exactly this, in double precision,
and bakes it with `FilmModel::bake()`:

```
display-encoded sRGB
  → decode sRGB CCTF → linear sRGB
  → Mallett 2019 spectral upsampling (81 × 3 basis)
  → film spectral sensitivity integration, normalised at 0.184 mid-grey
  → film characteristic curves (per channel, no couplers)
  → CMY → spectral density (channel dye densities + base density)
  → enlarger: 3400 K + KG3 + neutral dichroic filters
  → paper spectral sensitivity integration
  → print exposure factor (geometric mean of mid-grey response = 1)
  → paper characteristic curves (sum of normal CDFs)
  → CMY → spectral density
  → scan under D50 → XYZ → CAT02 → linear sRGB → encode sRGB CCTF
```

Everything is a pure function of one RGB triple. That is what makes the bake
deterministic and the LUT meaningful.

`--engine spectral` is the escape hatch from the table: it calls the same
`FilmModel::transformFrom` for every pixel at run time. It is still one RGB
triple in, one out; the difference is sampling error versus a 33³ lookup.
Measured on the i5-10600KF: 1.08 s for a 2.6 MP still and 1.26 s per video frame
with 12 threads, so it is the offline/final-render path, not the interactive
one. The LUT remains the default.

Two input domains are supported (D023). The LUT domain is whichever one you
bake:

```
display-encoded sRGB            (M1 default)
Apple Log code values, BT.2020  → Apple Log OETF decode → linear BT.2020
                                → linear sRGB → the chain above
```

Apple Log decodes with the constants from Apple's white paper; a negative
linear value below black is kept until the final sRGB clamp so the hue near
black stays right. `--exposure` scales linear light before the film model.

`--scan negative` (D024) replaces paper + print with: invert the film's dye
densities (`Dmax - D`), drop the base density so the orange mask disappears,
scan that virtual positive under D50, map no-dye to white and maximum dye to
black, then neutralise mid-grey with a per-channel power (a first-order stand-in
for a dye-correction matrix). That path has much more latitude, looks flatter
and has no upstream counterpart to compare against. `--contrast` (D025) pivots
on the scan's own 18% grey and scales all channels together, so it adds punch
without shifting hue; negative scans default to 1.15 (D026), and 1.0 keeps the
flat film curve for grading later.

Out-of-gamut output is rolled back in with an OkLCh chroma knee (D027). The
print path from Apple Log can produce negative linear values; the negative scan
is bounded by construction, so the compression is a no-op there. It defaults on
for Apple Log input and off for sRGB input, which keeps the M1 reference
byte-identical.

DIR couplers (D028) add interlayer inhibition during development: the film's
published curves are the end result, so the bake pre-computes the curves before
inhibition and subtracts the inhibitor amount from exposure before looking them
up. This is per-pixel chemistry only; the spatial diffusion of inhibitors stays
in the M4 GPU pass. It matches the upstream reference bake to PSNR 80.75 dB and
defaults on for Apple Log input.

HDR delivery (D029) only changes the output encoding: `--hdr pq` maps the
image's white to `--peak` nits (default 1000) and writes ST 2084 code values,
`--hdr hlg` writes BT.2100 HLG. The film model is untouched, so SDR and HDR
renders share every earlier stage.

Delivery (D055) is 8/16-bit PNG or OpenEXR. The EXR writer emits uncompressed
scanline RGB with FLOAT or HALF samples, so nothing is clamped to 0..1 and a
super-white HDR frame survives. It carries display-encoded values (sRGB or
PQ/HLG code values), not scene-linear light, and the reader accepts the same
subset. Compression (ZIP/PIZ) and an explicit linear option are deliberately
still open.

The per-pixel chain has two GPU implementations behind one `GpuFilm` interface:
a D3D11 compute shader on Windows (D046) and a Metal kernel on macOS (D056).
The Metal kernel is compiled from source at runtime and uses shared buffers, so
a frame never leaves unified memory; it must be built with `fastMathEnabled =
NO`, because the spectral tables contain NaN densities that the CPU chain skips
with `density != density` and fast math would delete that guard. Both backends
fall back to the CPU spectral engine when no device exists. Spatial texture is
still CPU on macOS (D057).

Video (D022) is a child-process integration: `spektra-roll-cli video` runs
ffprobe once for geometry and rotation, then pipes raw RGB48 frames from an
ffmpeg decoder through the LUT into an ffmpeg encoder, mapping the source audio
straight through. No intermediate files, no frame sequence on disk. `--start`
/`--frames`/`--duration` seek both the video pipe and the audio input so they
stay in sync, and `--codec prores` swaps the encoder for ProRes 422 HQ (D031).

Grain and halation (D033) run after the LUT, per frame, because they are
spatial: grain is a seeded hash field blurred into clumps and weighted toward
mid-tones, halation is a thresholded highlight mask spread by the halation PSF
and tinted warm. The seed and frame index make the field reproducible.

Glare and diffusion (D035) follow: glare spreads the whole luminance very widely
and screens it back (veiling light, so blacks lift and highlights stay), while
diffusion adds the spread-minus-original luminance back to all three channels,
softening detail without shifting hue. Order is halation, glare, diffusion,
grain.

Since D054 the three spreads are measured on the negative instead of on the
frame: `spreadMicrons` is the effective sigma of a two-Gaussian kernel (a tight
core plus a wide skirt) and `frameWidthMm` converts it to pixels, so 1080p and
4K renders of the same shot carry the same physical glow. The kernel is
truncated at three sigma of its widest component, normalised to sum to one, and
generated on the CPU (`buildSpreadKernel`) for both backends; D3D11 uploads the
same taps through a structured buffer instead of recomputing them in HLSL, and
each thread walks `-r..+r` in the same order as the CPU convolution. Kernels
wider than a few pixels are evaluated at 1/2..1/8 resolution and bilinearly
upsampled, which keeps the cost close to the old box blur. `--spread-model box`
still selects the pre-D054 fraction-of-width box chain.

Grain is cross-faded across frames (D036): two independent fields are mixed over
`--grain-rate` frames instead of replacing the field every frame, because a
fresh pattern at 24 fps reads as digital boiling rather than film. Those fields
are cached between frames (D039): `GrainFieldCache` keeps the two most recently
used fields keyed by seed, geometry, radius and field index, so a 3-frame
cross-fade generates one new field per three frames instead of two per frame.
The pixel code is shared with the uncached path, so output is bit-identical.

Long renders are split into video-only parts (D040): `--segment` writes
`<out>.parts/part-NNNNN.mov`, `--resume` skips parts with a matching `.done`
marker, and every part carries the frame index of the whole range so grain stays
continuous across the cut. Audio is not encoded per part; once the parts are
concatenated (`-c copy`) the source audio is copied in one go, which avoids AAC
seams and A/V drift. The source timecode is read once and stamped on the final
file, offset by `--start`.

Deliberately off so far (each has a decision record):

- output gamut compression (D014) — M2
- dir couplers (D015) — M2
- HDR (PQ/HLG) output — M2 (D021); Apple Log input is now built (D023)
- hanatos2025 upsampling (D012)

## Layers

```
core/        C++20, no UI, no policy. C ABI on the outside.
  lut3d      .cube parse / write + tetrahedral/trilinear    (built)
  image      8/16-bit PNG + float/half EXR, test chart    (built)
  json       minimal JSON reader for profile files          (built)
  profile    profile JSON -> spectral arrays                (built)
  film_model M1 spectral chain + LUT bake                   (built)
  metrics    PSNR / CIE76 for parity                        (built)
  transfer   sRGB / Apple Log OETF, BT.2020 -> sRGB         (built)
  bake/      OCIO config, .3dl, other LUT flavours          (next)
  grain/     temporal grain, µm-scaled, Philox seeds         (later)
  spatial/   halation, diffusion, glare                      (later)
  io/        clip readers/writers, colour metadata           (later)

gpu/
  metal/     MSL kernels
  d3d12/     HLSL kernels (or Slang, one source → both)

ui-qt/       Qt 6 Widgets shell (D009): drives the CLI, no pipeline code
             — ships on Windows now, same source can build on macOS
ofx/         OpenFX plugin (second phase)
cli/         headless renders, frame ranges, segment/resume, timecode
```

## Determinism rules

- Colour transform: deterministic, frame independent.
- LUT bake: pure double-precision function of the input triple; the same
  profile data always produces the same table.
- Grain: `seed = f(timecode, layer, sublayer)` — reproducible, parallel-safe,
  and "boiling" like real film.
- Auto exposure: measured once per clip, smoothed; never per frame.
- Tiled rendering: overlap ≥ blur radius, otherwise halation shows seams.

## Parity

The C++ engine is not trusted on its own. `tools/parity/reference_model.py`
composes the same chain from upstream functions and bakes a reference `.cube`;
`tools/parity/check_pipeline.py` checks that reference against the real
upstream `SimulationPipeline` in `lut_mode`. `lut compare` then reports PSNR
and CIE76 ΔE over all 33³ entries. Current numbers are in
`tools/parity/README.md`.

## Roadmap

| Phase | Deliverable |
|---|---|
| M0 *(done)* | CLI applies a LUT, tests, CI |
| M1 *(done)* | spektrafilm model ported, real Portra 400 + Endura LUT, parity harness, 16-bit PNG |
| M2 | colour correctness: real-photo sample, gamut compression (done), couplers (done), Apple Log input (done), scanned negative (done), HDR PQ/HLG output (done) |
| M3 | video I/O: FFmpeg / ProRes, time dimension, frame ranges; decide standalone vs OFX (D022) |
| M4 | texture and speed: grain / halation / diffusion / glare, deterministic seeds, threads, GPU colour (done, D046), GPU texture (done, D047), float / EXR |
| M5 | UI: Qt 6 Widgets (D009), macOS build, OFX plugin, render-farm CLI |
