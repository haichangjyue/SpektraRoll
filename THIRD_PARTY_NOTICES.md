# Third-party notices

SpektraRoll itself is licensed under **GPL-3.0-or-later** (see `LICENSE`).
The macOS preview DMG bundles the components below. Copyright remains with the
respective authors.

## Bundled executables

### FFmpeg 9.0.2 (`ffmpeg`, `ffprobe`)

The macOS app bundle contains a static FFmpeg 9.0.2 build with GPL components
enabled. FFmpeg is licensed under the **GPL-2.0-or-later** for this build
(LGPL-2.1-or-later for builds without `--enable-gpl`).

- Project: <https://ffmpeg.org/>
- Source for the exact GPL build: <https://ffmpeg.org/download.html>
  (FFmpeg 9.0.2) — build configuration and patches are published with each
  release at <https://github.com/haichangjyue/SpektraRoll/releases>.
- Written offer: the complete corresponding source for the bundled FFmpeg
  binary is available on request via
  <https://github.com/haichangjyue/SpektraRoll/issues> and from the upstream
  project above.

FFmpeg is invoked as a separate process for decoding and encoding; it is not
linked into SpektraRoll.

### OpenColorIO 2.6.0

Statically linked (Apache-2.0). <https://opencolorio.org/>

Its static dependencies are also included in the binary:

| Component | Licence |
|---|---|
| Imath | BSD-3-Clause |
| yaml-cpp | MIT |
| pystring | BSD-3-Clause |
| minizip-ng | zlib |
| expat | MIT |
| zlib | zlib |

## Bundled data

### Film and paper profiles (`data/profiles/`)

Measured profiles and neutral print-filter values from
[spektrafilm](https://github.com/andreavolpato/spektrafilm) by **Andrea Volpato**,
licensed **CC BY-SA 4.0**. Modified versions (including baked LUTs produced
from this data) remain CC BY-SA 4.0. See `data/profiles/NOTICE.md`.

### ACES 2.0 Studio config (`data/ocio/`)

Unmodified copy of `studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio` from
[OpenColorIO-Config-ACES](https://github.com/AcademySoftwareFoundation/OpenColorIO-Config-ACES),
**Apache-2.0**. See `data/ocio/NOTICE.md`. ACES is a trademark of the Academy of
Motion Picture Arts and Sciences; this project is not affiliated with AMPAS.

### Fonts

The app bundle includes **Noto Sans CJK SC** (Regular/Bold), the Google OFL
release of the Source Han Sans design, licensed under the **SIL Open Font
License 1.1**. The OFL text is included in the app bundle when the fonts are.

## Code and numeric tables

| Component | Licence | Notes |
|---|---|---|
| stb_image / stb_image_write | public domain / MIT | image I/O |
| colour-science numeric tables | BSD-3-Clause | CMFs, illuminants, colour matrices embedded in source |
| Metal / D3D11 system headers | Apache-2.0 / MIT | platform SDKs |

## Not included

- **RED SDK / R3D decoding**: the official package ships only a loader and does
  not contain RED intellectual property. RED support requires a local
  BYO-SDK build (see `docs/red-sdk.md`).
- **Qt / Windows UI**: not part of the macOS preview.

## Trademarks

Kodak, Fujifilm, Ilford, Sony, ARRI, Blackmagic Design, Canon, Panasonic and the
film stock names are trademarks of their respective owners. They are used only
to identify the measured material being simulated. SpektraRoll is not affiliated
with or endorsed by any of these companies.
