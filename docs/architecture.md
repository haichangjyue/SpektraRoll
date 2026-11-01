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

Video (D022) is a child-process integration: `spektra-roll-cli video` runs
ffprobe once for geometry and rotation, then pipes raw RGB48 frames from an
ffmpeg decoder through the LUT into an ffmpeg encoder, mapping the source audio
straight through. No intermediate files, no frame sequence on disk.

Deliberately off so far (each has a decision record):

- output gamut compression (D014) — M2
- dir couplers (D015) — M2
- grain, halation, diffusion, glare — M4
- HDR (PQ/HLG) output — M2 (D021); Apple Log input is now built (D023)
- hanatos2025 upsampling (D012)

## Layers

```
core/        C++20, no UI, no policy. C ABI on the outside.
  lut3d      .cube parse / write + tetrahedral/trilinear    (built)
  image      8/16-bit PNG decode + encode, test chart       (built)
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

app_mac/     SwiftUI shell + AppKit + Metal canvas
app_win/     WinUI 3 (C++/WinRT) + SwapChainPanel + D3D12
ofx/         OpenFX plugin (second phase)
cli/         headless renders, frame ranges, resume
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
| M4 | texture and speed: grain / halation / diffusion / glare, deterministic seeds, SIMD / threads / GPU, float / EXR |
| M5 | UI: Windows WinUI 3, macOS SwiftUI, OFX plugin, render-farm CLI |
