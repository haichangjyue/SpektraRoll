# SpektraRoll

[![build](https://github.com/haichangjyue/SpektraRoll/actions/workflows/build.yml/badge.svg)](https://github.com/haichangjyue/SpektraRoll/actions/workflows/build.yml)

Open-source spectral film simulation for **motion picture**.

Status: **M1 complete** — the spectral colour chain is ported to C++ and bakes a
real **Kodak Portra 400 + Portra Endura** LUT, with parity against the upstream
Python model at **PSNR 88 dB** (ΔE76 max 0.0065).

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
.\scripts\demo.ps1          # synthetic LUT smoke test
.\scripts\demo-m1.ps1       # the real film LUT, before/after images
```

Or double-click `scripts\dev-install.cmd`, `scripts\build.cmd`,
`scripts\demo-m1.cmd`. `scripts\verify-m1.cmd` is the one-button acceptance
check (build + tests + bake + parity).

macOS / Linux:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
# CI also bakes the M1 LUT and compares it to the frozen reference here;
# re-running the full parity against the upstream Python model needs the
# upstream checkout and Python, see tools/parity/README.md
```

## Use

```
# bake the default Kodak Portra 400 + Portra Endura LUT
spektra-roll-cli lut bake --out portra400_endura.cube --size 33

# apply it to a 16-bit PNG (output stays 16-bit)
spektra-roll-cli gen --out chart.png --bits 16
spektra-roll-cli lut apply --lut portra400_endura.cube --in chart.png --out graded.png

# compare against a reference bake
spektra-roll-cli lut compare --a reference.cube --b portra400_endura.cube

# M0 commands still work
spektra-roll-cli lut gen --out warm.cube --size 33 --preset warm
spektra-roll-cli lut info --lut warm.cube
```

`lut bake` defaults to `data/profiles/kodak_portra_400.json` and
`data/profiles/kodak_portra_endura.json`, and looks the neutral enlarger
filters up in `data/profiles/neutral_print_filters.json`. Override with
`--film`, `--paper`, `--filters`, or `--cc C,M,Y`.

LUT domain and range are **display-encoded sRGB**, so `lut apply` works
directly on ordinary PNGs. The LUT is not linear-light; that conversion belongs
to the M2 colour work (D021).

## Layout

```
src/core/       C++20 library, no UI, no I/O policy
  lut3d         .cube parse / write + tetrahedral/trilinear interpolation
  image         stb decode + 8/16-bit PNG encode + test chart
  profile       spektrafilm profile JSON -> spectral data
  film_model    M1 spectral chain: upsampling, curves, print, scan
  metrics       PSNR and CIE76 delta E for parity checks
src/cli/        headless entry point (render-farm friendly later)
tests/          assert-based tests, no framework; frozen reference LUT
data/profiles/  upstream CC BY-SA 4.0 film / paper data + NOTICE
tools/parity/   Python reference chain and upstream cross-check
scripts/        build / install / demo / verify for non-developers
.github/        Windows + macOS CI
docs/           architecture and licensing notes
```

## Known limitations (on purpose, for now)

- No couplers, grain, halation, diffusion or glare yet (M4 for the spatial ones,
  M2 for couplers).
- SDR only today: input is display-encoded sRGB and output is SDR. Apple Log
  input and HDR (PQ/HLG) output are the M2 colour work (D021).
- No video file I/O yet: stills only until the M3 FFmpeg/ProRes step.
- Output gamut compression is off. Extreme colours can produce out-of-range
  LUT values, which are clamped when writing an image.
- 16-bit PNG only; no EXR or OpenImageIO yet.
- Mallett 2019 spectral upsampling only; hanatos2025 is a later option.
- `.cube` only; `.3dl` and OCIO configs come later.
- Windows UI does not exist; the CLI is the first deliverable.

## Licence

GPL-3.0-or-later. Film and paper profiles derived from spektrafilm stay
CC BY-SA 4.0; see `data/profiles/NOTICE.md`, `LICENSE` and `docs/licensing.md`.

Spektrafilm is by Andrea Volpato. This project is independent and not
affiliated with Kodak, Fujifilm or Ilford; stock names are used only to
identify measured materials.
