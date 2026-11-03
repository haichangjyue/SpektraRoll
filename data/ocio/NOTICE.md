# OpenColorIO configs

`studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio` is an unmodified copy of the
ACES 2.0 **Studio** config published by the
[OpenColorIO-Config-ACES](https://github.com/AcademySoftwareFoundation/OpenColorIO-Config-ACES)
project (release `v3.0.0`, asset
`studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio`).

- Licence: [Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0) (compatible
  with this repository's GPL-3.0-or-later code; it is data, not linked code).
- Copyright: Academy Software Foundation and the OpenColorIO-Config-ACES
  contributors.
- No modifications were made.

Why this one: it needs OpenColorIO **2.4** (the version SpektraRoll builds
against), it is a single self-contained `.ocio` file with no external LUTs, and
it carries the camera transforms the demo uses (`ARRI LogC3 (EI800)`,
`ARRI LogC4`, `BMDFilm WideGamut Gen5`, `DaVinci Intermediate WideGamut`,
`S-Log3 S-Gamut3`, `S-Log3 S-Gamut3.Cine`, `Apple Log`) plus
`Linear Rec.709 (sRGB)`, which is the space the film model is fed (D062).

Any other config works too: pass `--ocio-config <file.ocio>`, or set `$OCIO`.
Newer ACES 2.0 configs (`v4.x`, OCIO 2.5) need a matching OpenColorIO build.

The ACES name and logo are trademarks of the Academy of Motion Picture Arts and
Sciences. SpektraRoll is not affiliated with or endorsed by AMPAS.
