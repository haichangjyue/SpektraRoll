# Licensing notes

This is engineering documentation, not legal advice. If money or a company is
involved, have a lawyer read it.

## Our code

`GPL-3.0-or-later`. Every distributed binary must ship with the corresponding
source, and derivative works must stay GPL.

Consequences:

- **No Mac App Store.** Apple's terms add restrictions GPLv3 forbids.
  Distribute a signed, notarised DMG from the website.
- **The OFX plugin is GPL too.** That is normal in the OpenFX ecosystem; the
  host (DaVinci Resolve) stays closed and loads our .ofx binary.
- **Microsoft Store**: technically friendlier, still reviewed case by case.
  Plan on a direct download first.

## Upstream material

| Asset | Licence | What it means |
|---|---|---|
| spektrafilm code | GPL-3.0 | We may reuse it; our code stays GPL |
| film / paper profiles (JSON) | CC BY-SA 4.0 | Commercial use OK; modified profiles stay CC BY-SA |
| baked LUT files | custom: commercial use, free share, **no resale** | Never sell LUTs as a product |
| upstream README assertion | "derivative includes anything directly inspired by its methods" | Broader than copyright law; respect it, or ask the author |

If we ever want a closed-source build, ask Andrea Volpato for a dual licence
before writing the code, not after.

## Trademarks

Kodak, Fujifilm, Ilford and the stock names are trademarks of their owners.
We use them **only to identify the measured material**, with a visible
disclaimer of non-affiliation, and never in the product name, logo or domain.

## Third-party dependencies

| Dependency | Licence | Notes |
|---|---|---|
| stb_image / stb_image_write | public domain / MIT | M0 only |
| OpenColorIO (planned) | BSD-3-Clause | check LICENCE at the pinned tag |
| OpenImageIO (planned) | Apache-2.0 | |
| FFmpeg (planned) | LGPL-2.1+ by default | avoid `--enable-gpl` builds if we can |
| LibRaw (planned) | LGPL-2.1 / CDDL | |
| Metal-cpp / D3D12 headers | Apache-2.0 / MIT | |

**Codec patents** are separate from copyright. Prefer OS decoders
(VideoToolbox, Media Foundation) and royalty-free codecs (ProRes, DNxHR) over
shipping our own H.264/HEVC.

## Attribution we must keep

- Andrea Volpato — spektrafilm, the film process model and the measured profiles.
- Any contributor whose profile or measured data we ship.
