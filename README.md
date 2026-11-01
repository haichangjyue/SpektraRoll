# SpektraRoll

[![build](https://github.com/haichangjyue/SpektraRoll/actions/workflows/build.yml/badge.svg)](https://github.com/haichangjyue/SpektraRoll/actions/workflows/build.yml)

Open-source spectral film simulation for **motion picture**.

Status: **M5 in progress** — the M1 spectral chain bakes a real **Kodak Portra 400
+ Portra Endura** LUT with parity against the upstream Python model at
**PSNR 88 dB** (ΔE76 max 0.0065), accepts **Apple Log** footage (BT.2020,
log-encoded) as input, can either print it (`--scan print`) or scan the
negative (`--scan negative`, much more latitude), writes SDR or **HDR
(PQ/HLG)**, and renders clips in resumable segments while keeping the camera
timecode. A **Qt 6 desktop UI** wraps the same pipeline.

Repo: https://github.com/haichangjyue/SpektraRoll

![SpektraRoll Qt UI](docs/ui-preview.png)

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
the spatial parts (grain, halation, diffusion, glare) run per frame and stay
out of the LUT. That split is what makes video tractable.

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

## Desktop UI (Qt 6)

The Windows shell is Qt 6 Widgets (D009); the CLI keeps working without it.
It only drives `spektra-roll-cli` — preview extracts one frame with ffmpeg and
grades it with `lut apply`, Render streams the CLI log and progress — so the UI
cannot drift from the pipeline CI tests.

```powershell
.\scripts\build-ui.cmd          # configure + build + windeployqt + launch
```

Qt 6.8.3 and MinGW 13.1 are expected under `X:\agent\tools\qt` (installed with
`aqtinstall`, not vendored). Qt is dynamically linked (LGPL-3.0/GPL-3.0);
`windeployqt` copies the runtime next to the executable.

Renders can be queued: **加入队列** freezes the current arguments and copies the
LUT, outputs that would collide get a `-2`, `-3` suffix, and the queue runs the
jobs one after another with per-job progress bars, elapsed/remaining time, and
status. The queue is saved to `out/ui-queue/queue.json` and restored on the next
launch; dropping several clips at once (or **批量加入**) enqueues one job per clip.

![Render queue](docs/ui-queue.png)

## Use

```
# bake the default Kodak Portra 400 + Portra Endura LUT
spektra-roll-cli lut bake --out portra400_endura.cube --size 33

# apply it to a 16-bit PNG (output stays 16-bit)
spektra-roll-cli gen --out chart.png --bits 16
spektra-roll-cli lut apply --lut portra400_endura.cube --in chart.png --out graded.png

# compare against a reference bake
spektra-roll-cli lut compare --a reference.cube --b portra400_endura.cube

# Apple Log input: the LUT domain is Apple Log code values (BT.2020 primaries)
spektra-roll-cli lut bake --input apple-log --exposure 0.7 --out applelog.cube
spektra-roll-cli convert --in frame.png --out frame_sdr.png --from apple-log --exposure 0.7
spektra-roll-cli lut apply --lut applelog.cube --in frame.png --out frame_portra.png

# scan the negative instead of printing it: more latitude, flatter look
spektra-roll-cli lut bake --input apple-log --exposure 0.7 --scan negative --out neg.cube
spektra-roll-cli lut apply --lut neg.cube --in frame.png --out frame_negative.png
# negative scans default to --contrast 1.15; pass 1.0 for the flat, full-latitude version
spektra-roll-cli lut bake --input apple-log --exposure 0.7 --scan negative --contrast 1.0 --out neg_flat.cube

# HDR delivery: PQ code values with white at 1000 nits (or --hdr hlg)
spektra-roll-cli lut bake --input apple-log --exposure 0.7 --scan negative --hdr pq --peak 1000 --out hdr.cube

# grade a whole clip (ffmpeg decodes, the LUT runs in-process, audio is copied)
spektra-roll-cli video --lut hdr.cube --in clip.mov --out graded.mov --trc pq

# only one second in the middle, as ProRes 422 HQ for the edit
spektra-roll-cli video --lut hdr.cube --in clip.mov --out trimmed.mov --trc pq \
    --start 00:00:12.5 --frames 24 --codec prores

# film texture: grain and halation are spatial, so they run per frame
spektra-roll-cli video --lut film.cube --in clip.mov --out textured.mov --trc srgb \
    --grain 0.35 --halation 0.6

# encoder knobs: smaller crf / slower preset = better quality, slower render
spektra-roll-cli video --lut film.cube --in clip.mov --out fast.mov --trc srgb \
    --crf 22 --preset fast

# hardware encode on Intel GPUs (--encoder qsv); ProRes always uses the software encoder
spektra-roll-cli video --lut film.cube --in clip.mov --out gpu.mov --trc pq --encoder qsv

# long render: 10 s chunks in <out>.parts; rerun the same command to resume after
# a crash (finished chunks are skipped) — the source timecode is copied along
spektra-roll-cli video --lut film.cube --in clip.mov --out graded.mov --trc srgb \
    --segment 10 --resume

# M0 commands still work
spektra-roll-cli lut gen --out warm.cube --size 33 --preset warm
spektra-roll-cli lut info --lut warm.cube
```

Apple Log frames have to be extracted from the clip first; the demo uses
`ffmpeg` and keeps the log code values intact (`scale=in_color_matrix=bt2020`).
`--exposure` shifts scene exposure in stops before the film model, which is
what decides how bright the shaded scene prints.

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
  transfer      sRGB / Apple Log OETF, BT.2020 -> sRGB (M2)
src/cli/        headless entry point (render-farm friendly later)
tests/          assert-based tests, no framework; frozen reference LUT
data/profiles/  upstream CC BY-SA 4.0 film / paper data + NOTICE
tools/parity/   Python reference chain and upstream cross-check
scripts/        build / install / demo / verify for non-developers
.github/        Windows + macOS CI
docs/           architecture and licensing notes
```

## Known limitations (on purpose, for now)

- No per-layer grain and no physical point-spread functions: glare, diffusion
  and halation are display-space blurs (D033/D035).
- Output gamut compression is on for Apple Log input (`--gamut oklch`, D027) and
  off by default for sRGB input so the M1 reference stays bit-identical.
- Apple Log input works, with two ways out: print (`--scan print`, the M1
  chain, contrasty) and scanned negative (`--scan negative`, more latitude,
  flatter). The negative path has no upstream parity and its grey balance is a
  power law rather than a full dye-correction matrix, so colours are close but
  not final.
- SDR output is the default; HDR (PQ/HLG, `--hdr`, D029) works and the demo
  writes an HEVC Main10 HDR clip, but seeing it needs an HDR display.
- No video file I/O in the library yet: `video` drives `ffmpeg` as a child process
  (D022), so a clip can be graded, trimmed, rendered as ProRes or HDR, and
  resumed from completed segments in one command.
- Output gamut compression is off by default for sRGB input; extreme colours can
  produce out-of-range LUT values, which are clamped when writing an image.
- Source timecode is copied and offset by `--start`; drop-frame sources are
  offset in nominal frames, so they can drift by a few frames per hour.
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
