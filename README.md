# SpektraRoll

Open-source spectral film simulation for **motion picture**.

Status: **M0 complete** — a working CLI that applies a baked 3D LUT to images,
with tests and CI. The spectral film model is not wired in yet.

Repo: https://github.com/haichangjyue/SpektraRoll

## What this will become

A film-and-print simulator for video: light traced through 81 wavelengths,
through a virtual negative, an enlarger and a print, then scanned — the same
physical model as [spektrafilm](https://github.com/andreavolpato/spektrafilm),
but built for clips instead of stills.

```
scene → [camera + film] → latent image → [development] → negative densities
      → [enlarger + paper] → latent print → [development] → print densities
      → [scan] → display RGB
```

The colour part of that chain is baked into a 3D LUT and evaluated per pixel;
the spatial parts (grain, halation, diffusion, glare) run as GPU passes and
stay out of the LUT. That split is what makes video tractable.

## Build

Windows:

```powershell
.\scripts\dev-install.ps1   # once, no admin rights needed
.\scripts\build.ps1
.\scripts\demo.ps1          # writes out\chart.png and out\chart_warm.png
```

Or double-click `scripts\dev-install.cmd`, `scripts\build.cmd`,
`scripts\demo.cmd`.

macOS / Linux:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Use

```
spektra-roll-cli gen --out chart.png --width 1600 --height 900
spektra-roll-cli lut gen --out warm.cube --size 33 --preset warm
spektra-roll-cli lut info --lut warm.cube
spektra-roll-cli lut apply --lut warm.cube --in chart.png --out chart_warm.png
```

## Layout

```
src/core/       C++20 library, no UI, no I/O policy
  lut3d         .cube parsing + tetrahedral/trilinear interpolation
  image         stb-based image I/O (8-bit for now) + test chart
src/cli/        headless entry point (render-farm friendly later)
tests/          assert-based tests, no framework
scripts/        build / install / demo for non-developers
.github/        Windows + macOS CI
docs/           architecture notes
```

## Known limitations (on purpose, for now)

- 8-bit in, 8-bit out. No colour management, no transfer functions.
- `.cube` only; `.3dl` and OCIO configs come with the bake pipeline.
- No spectral model yet — `lut gen` writes a synthetic curve, not film.
- Windows UI does not exist; the CLI is the first deliverable.

## Licence

GPL-3.0-or-later. Film and paper profiles derived from spektrafilm stay
CC BY-SA 4.0. See `LICENSE` and `docs/licensing.md`.

Spektrafilm is by Andrea Volpato. This project is independent and not
affiliated with Kodak, Fujifilm or Ilford; stock names are used only to
identify measured materials.
