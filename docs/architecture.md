# Architecture

Status: M0. This describes the target shape; only the parts marked *(built)*
exist today.

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

## Layers

```
core/        C++20, no UI, no policy. C ABI on the outside.
  lut3d      .cube parse + tetrahedral/trilinear        (built)
  image      decode/encode + test chart                 (built)
  model/     spectral pipeline, curves, couplers        (next)
  bake/      LUT bake + OCIO config                     (next)
  grain/     temporal grain, µm-scaled, Philox seeds     (later)
  spatial/   halation, diffusion, glare                  (later)
  io/        clip readers/writers, colour metadata       (later)

gpu/
  metal/     MSL kernels
  d3d12/     HLSL kernels (or Slang, one source → both)

app_mac/     SwiftUI shell + AppKit + Metal canvas
app_win/     WinUI 3 (C++/WinRT) + SwapChainPanel + D3D12
ofx/         OpenFX plugin (second phase)
cli/         headless renders, frame ranges, resume
```

## Why one engine and two UIs

SpektraLab (the reference product) does exactly this: SwiftUI on macOS, Qt on
Windows, one C++ engine between them. SwiftUI does not exist on Windows, and
`SwiftCrossUI`-style shims cannot carry colour management, HDR or a GPU canvas.
Sharing the engine is cheap; sharing the UI is not.

## Determinism rules

- Colour transform: deterministic, frame independent.
- Grain: `seed = f(timecode, layer, sublayer)` — reproducible, parallel-safe,
  and "boiling" like real film.
- Auto exposure: measured once per clip, smoothed; never per frame.
- Tiled rendering: overlap ≥ blur radius, otherwise halation shows seams.

## Roadmap

| Phase | Deliverable |
|---|---|
| M0 *(now)* | CLI applies a LUT, tests, CI |
| M1 | spektrafilm model ported, first real film LUT, parity harness |
| M2 | GPU colour + spatial, 16-bit I/O |
| M3 | macOS app |
| M4 | Windows app |
| M5 | OFX plugin + render-farm CLI |
