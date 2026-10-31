# SPDX-License-Identifier: GPL-3.0-or-later
"""Cross-check: run the real upstream SimulationPipeline (lut_mode) and compare
it with the minimal chain in reference_model.py."""

from __future__ import annotations

import sys
import types
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
UPSTREAM = REPO.parent / "_upstream_spektrafilm" / "src"


def stub_upstream() -> None:
    for name in ("exiv2", "lensfunpy", "rawpy"):
        sys.modules.setdefault(name, types.ModuleType(name))
    oiio = types.ModuleType("OpenImageIO")
    for attr in ("ImageSpec", "DOUBLE", "FLOAT", "HALF", "UINT8", "UINT16",
                 "ImageInput", "ImageOutput", "TypeDesc"):
        setattr(oiio, attr, type(attr, (), {}))
    sys.modules.setdefault("OpenImageIO", oiio)


def main() -> int:
    stub_upstream()
    sys.path.insert(0, str(UPSTREAM))
    sys.path.insert(0, str(Path(__file__).resolve().parent))

    import numpy as np
    from spektrafilm.runtime.api import init_params, digest_params
    from spektrafilm.runtime.pipeline import SimulationPipeline
    from spektrafilm.utils.gamut_compression import (
        InputGamutCompressSpec,
        OutputGamutCompressSpec,
    )
    from reference_model import ColourChain

    params = init_params("kodak_portra_400", "kodak_portra_endura")
    params.settings.rgb_to_raw_method = "mallett2019"
    params.debug.lut_mode = True
    params.debug.deactivate_spatial_effects = True
    params.debug.deactivate_stochastic_effects = True
    params.camera.auto_exposure = False
    params.film_render.dir_couplers.active = False  # M1: no couplers
    params.io.input_color_space = "sRGB"
    params.io.input_cctf_decoding = True
    params.io.output_color_space = "sRGB"
    params.io.output_cctf_encoding = True
    params.io.input_gamut_compress = InputGamutCompressSpec(active=False)
    params.io.output_gamut_compress = OutputGamutCompressSpec(algorithm="off")

    pipeline = SimulationPipeline(digest_params(params))

    probes = np.array(
        [
            [0.0, 0.0, 0.0],
            [0.05, 0.05, 0.05],
            [0.184, 0.184, 0.184],
            [0.46, 0.46, 0.46],
            [0.5, 0.5, 0.5],
            [1.0, 1.0, 1.0],
            [0.8, 0.2, 0.2],
            [0.2, 0.6, 0.2],
            [0.1, 0.3, 0.9],
        ],
        dtype=float,
    )
    upstream = pipeline.process(probes.reshape(1, -1, 3)).reshape(-1, 3)

    chain = ColourChain("kodak_portra_400", "kodak_portra_endura")
    ours = chain.transform(probes)

    print("in                       upstream                          reference                    diff")
    worst = 0.0
    for p, u, o in zip(probes, upstream, ours):
        d = float(np.max(np.abs(u - o)))
        worst = max(worst, d)
        print(f"{np.round(p,3).tolist()!s:24} {np.round(u,5).tolist()!s:33} {np.round(o,5).tolist()!s:28} {d:.3e}")
    print(f"\nworst |diff| = {worst:.3e}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
