# Architecture

Status: M1. Parts marked *(built)* exist today; the rest is the target shape.

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

Deliberately off in M1 (each has a decision record):

- output gamut compression (D014)
- dir couplers (D015)
- grain, halation, diffusion, glare (M2)
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
| M2 | gamut compression, couplers, GPU spatial effects, SIMD/multithreading, EXR |
| M3 | macOS app |
| M4 | Windows app |
| M5 | OFX plugin + render-farm CLI |
