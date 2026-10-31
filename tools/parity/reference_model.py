# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 SpektraRoll contributors
"""Parity reference for the SpektraRoll M1 colour chain.

This script does NOT re-implement spektrafilm's maths. It imports the upstream
model modules and composes the *minimal* chain that M1 promises:

    sRGB (display encoded)
      -> Mallett 2019 spectral upsampling
      -> spectral sensitivity integration (film)
      -> characteristic curves  (no couplers, no grain)
      -> CMY -> spectral density
      -> enlarger + print paper
      -> characteristic curves (parametric CDF model)
      -> CMY -> spectral density
      -> scan under D50 -> XYZ -> linear sRGB -> sRGB encoded

Everything spatial / stochastic / auto-exposure is off, and output gamut
compression is 'off', matching `spektra-roll-cli lut bake`.

Usage (from the repository root):

    X:\\agent\\tools\\python\\python.exe tools\\parity\\reference_model.py \
        --out out\\reference.cube --size 33

Requires the upstream checkout and a Python environment with numpy, scipy and
colour-science. Upstream: https://github.com/andreavolpato/spektrafilm
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import types
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
UPSTREAM = Path(os.environ.get("SPEKTRAFILM_SRC", REPO.parent / "_upstream_spektrafilm" / "src"))

# The upstream package pulls in RAW-file helpers (exiv2, rawpy, OpenImageIO)
# at import time. We only need the colorimetry modules, so stub them out.
for _name in ("exiv2", "lensfunpy", "rawpy"):
    sys.modules.setdefault(_name, types.ModuleType(_name))
_oiio = types.ModuleType("OpenImageIO")
for _attr in ("ImageSpec", "DOUBLE", "FLOAT", "HALF", "UINT8", "UINT16",
              "ImageInput", "ImageOutput", "TypeDesc"):
    setattr(_oiio, _attr, type(_attr, (), {}))
sys.modules.setdefault("OpenImageIO", _oiio)

_root = str(UPSTREAM / "spektrafilm")
if "spektrafilm" not in sys.modules:
    _pkg = types.ModuleType("spektrafilm")
    _pkg.__path__ = [_root]
    sys.modules["spektrafilm"] = _pkg
    _utils = types.ModuleType("spektrafilm.utils")
    _utils.__path__ = [str(Path(_root) / "utils")]
    sys.modules["spektrafilm.utils"] = _utils
    # `params_schema` is imported by the coupler module, and the runtime
    # package __init__ pulls in the full simulator (skimage & friends).
    _runtime = types.ModuleType("spektrafilm.runtime")
    _runtime.__path__ = [str(Path(_root) / "runtime")]
    sys.modules["spektrafilm.runtime"] = _runtime

import numpy as np  # noqa: E402
import colour  # noqa: E402
from opt_einsum import contract  # noqa: E402

from spektrafilm.config import SPECTRAL_SHAPE, STANDARD_OBSERVER_CMFS  # noqa: E402
from spektrafilm.model.color_filters import color_enlarger  # noqa: E402
from spektrafilm.model.density_curves import interpolate_exposure_to_density  # noqa: E402
from spektrafilm.model.develop import compute_density_spectral, develop_simple  # noqa: E402
from spektrafilm.model.illuminants import standard_illuminant  # noqa: E402
from spektrafilm.profiles.io import load_profile  # noqa: E402
from spektrafilm.utils.conversions import density_to_light  # noqa: E402
from spektrafilm.utils.morph_curves import _evaluate_fitted_density  # noqa: E402
from spektrafilm.utils.spectral_upsampling import rgb_to_raw_mallett2019  # noqa: E402

MIDGRAY = 0.184
ENLARGER_ILLUMINANT = "TH-KG3"


def _neutral_print_filters(paper: str, film: str) -> tuple[float, float, float]:
    """Same database `digest_params` uses, keyed by paper/illuminant/film."""
    path = UPSTREAM / "spektrafilm" / "data" / "filters" / "neutral_print_filters.json"
    table = json.loads(path.read_text(encoding="utf-8"))
    values = table[paper][ENLARGER_ILLUMINANT][film]
    return tuple(float(v) for v in values)


def srgb_decode(x: np.ndarray) -> np.ndarray:
    x = np.clip(np.asarray(x, dtype=float), 0.0, 1.0)
    return np.where(x <= 0.04045, x / 12.92, ((x + 0.055) / 1.055) ** 2.4)


def srgb_encode(x: np.ndarray) -> np.ndarray:
    x = np.asarray(x, dtype=float)
    x = np.where(x <= 0.0031308, x * 12.92, 1.055 * np.maximum(x, 0.0) ** (1 / 2.4) - 0.055)
    return x


class ColourChain:
    def __init__(self, film_stock: str, paper_stock: str) -> None:
        self.film = load_profile(film_stock)
        self.paper = load_profile(paper_stock)
        self.wavelengths = np.asarray(self.film.data.wavelengths, dtype=float)

        self.film_sensitivity = np.nan_to_num(10 ** self.film.data.log_sensitivity)
        self.paper_sensitivity = np.nan_to_num(10 ** self.paper.data.log_sensitivity)

        self.reference_illuminant = standard_illuminant(self.film.info.reference_illuminant)
        self.print_illuminant_raw = standard_illuminant(ENLARGER_ILLUMINANT)
        self.enlarger_filters = _neutral_print_filters(paper_stock, film_stock)
        self.print_illuminant = color_enlarger(
            self.print_illuminant_raw, filter_cc_values=self.enlarger_filters
        )
        self.viewing_illuminant = standard_illuminant(self.paper.info.viewing_illuminant)

        self.cmf = np.asarray(STANDARD_OBSERVER_CMFS[:], dtype=float)
        self.scan_normalization = float(np.sum(self.viewing_illuminant * self.cmf[:, 1]))

        # Print exposure factor, exactly as PrintingStage._compute_exposure_factor_midgray
        # does it for lut_mode (print_exposure_compensation=False,
        # normalize_print_exposure=True).
        density_spectral_midgray = self._simple_rgb_to_density_spectral(MIDGRAY)
        light = density_to_light(density_spectral_midgray, self.print_illuminant)
        raw_midgray = contract("ijk,kl->ijl", light, self.paper_sensitivity)
        raw_midgray = np.fmax(raw_midgray, 1e-10)
        geomean = np.exp(np.mean(np.log(raw_midgray), axis=2, keepdims=True))
        self.print_exposure_factor = float(1.0 / geomean.squeeze())

        self.normalized_film_curves = (
            self.film.data.density_curves - np.nanmin(self.film.data.density_curves, axis=0)
        )

        illuminant_xyz = contract("k,kl->l", self.viewing_illuminant, self.cmf) / self.scan_normalization
        self.viewing_xy = colour.XYZ_to_xy(illuminant_xyz)

    # -- helpers ---------------------------------------------------------
    def _film_raw(self, linear_srgb: np.ndarray) -> np.ndarray:
        raw = rgb_to_raw_mallett2019(
            linear_srgb,
            self.film_sensitivity,
            color_space="sRGB",
            apply_cctf_decoding=False,
            reference_illuminant=self.film.info.reference_illuminant,
        )
        return np.nan_to_num(raw)

    def _simple_rgb_to_density_spectral(self, value: float) -> np.ndarray:
        rgb = np.array([[[value, value, value]]])
        raw = self._film_raw(rgb)
        log_raw = np.log10(raw + 1e-10)
        cmy = develop_simple(
            log_raw,
            self.film.data.log_exposure,
            self.film.data.density_curves,
            gamma_factor=1.0,
        )
        return compute_density_spectral(
            self.film.data.channel_density, cmy, base_density=self.film.data.base_density
        )

    # -- the chain -------------------------------------------------------
    def transform(self, srgb: np.ndarray) -> np.ndarray:
        srgb = np.asarray(srgb, dtype=float).reshape(-1, 3)
        linear = srgb_decode(srgb)[None, :, :]

        raw = self._film_raw(linear)
        log_raw = np.log10(np.fmax(raw, 0.0) + 1e-10)
        cmy_film = interpolate_exposure_to_density(
            log_raw, self.normalized_film_curves, self.film.data.log_exposure, 1.0
        )

        density_spectral = compute_density_spectral(
            self.film.data.channel_density, cmy_film, base_density=self.film.data.base_density
        )
        light = density_to_light(density_spectral, self.print_illuminant)
        raw_print = contract("ijk,kl->ijl", light, self.paper_sensitivity)
        raw_print *= self.print_exposure_factor
        log_raw_print = np.log10(np.fmax(raw_print, 0.0) + 1e-10)

        paper_curves = _evaluate_fitted_density(
            self.paper.data.log_exposure, self.paper.data.density_curves_model, "negative"
        )
        cmy_paper = interpolate_exposure_to_density(
            log_raw_print, paper_curves, self.paper.data.log_exposure, 1.0
        )

        paper_density = compute_density_spectral(
            self.paper.data.channel_density, cmy_paper, base_density=self.paper.data.base_density
        )
        scan_light = density_to_light(paper_density, self.viewing_illuminant)
        xyz = contract("ijk,kl->ijl", scan_light, self.cmf) / self.scan_normalization

        rgb = colour.XYZ_to_RGB(
            xyz,
            colourspace="sRGB",
            apply_cctf_encoding=False,
            illuminant=self.viewing_xy,
        )
        return srgb_encode(rgb).reshape(-1, 3)


def bake(chain: ColourChain, size: int) -> np.ndarray:
    axis = np.linspace(0.0, 1.0, size)
    b, g, r = np.meshgrid(axis, axis, axis, indexing="ij")
    grid = np.stack((r, g, b), axis=-1).reshape(-1, 3)
    out = chain.transform(grid)
    return out.reshape(size, size, size, 3)


def write_cube(path: Path, title: str, data: np.ndarray) -> None:
    size = data.shape[0]
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(f'TITLE "{title}"\n')
        handle.write(f"LUT_3D_SIZE {size}\n")
        handle.write("DOMAIN_MIN 0.0 0.0 0.0\n")
        handle.write("DOMAIN_MAX 1.0 1.0 1.0\n")
        for b in range(size):
            for g in range(size):
                for r in range(size):
                    v = data[b, g, r]
                    handle.write(f"{v[0]:.8f} {v[1]:.8f} {v[2]:.8f}\n")


def dump_tables(chain: ColourChain, path: Path) -> None:
    """Write the numeric tables the C++ engine embeds, as C++ source."""
    path.parent.mkdir(parents=True, exist_ok=True)

    def arr(values, per_line=6):
        flat = np.asarray(values, dtype=float).reshape(-1)
        # C++ has no `0f`: a float literal needs a decimal point or exponent.
        def literal(v):
            text = f"{v:.10g}"
            if "e" not in text and "E" not in text and "." not in text:
                text += ".0"
            return text + "f"

        lines = []
        for i in range(0, flat.size, per_line):
            chunk = flat[i:i + per_line]
            lines.append("    " + ", ".join(literal(v) for v in chunk) + ",")
        return "\n".join(lines)

    basis = np.asarray(
        colour.recovery.MSDS_BASIS_FUNCTIONS_sRGB_MALLETT2019.copy().align(SPECTRAL_SHAPE)[:],
        dtype=float,
    )
    # Pre-computed 3x3 matrix, so the C++ side does not need colour-science.
    # XYZ (D50 viewing illuminant) -> linear sRGB, CAT02 chromatic adaptation
    # (colour-science's default for XYZ_to_RGB). Sampling the transform at the
    # three XYZ basis vectors recovers it exactly, because it is linear.
    matrix = np.zeros((3, 3))
    for i in range(3):
        basis_xyz = np.zeros((1, 1, 3))
        basis_xyz[0, 0, i] = 1.0
        matrix[:, i] = colour.XYZ_to_RGB(
            basis_xyz,
            colourspace="sRGB",
            apply_cctf_encoding=False,
            illuminant=chain.viewing_xy,
        ).reshape(3)
    check = colour.XYZ_to_RGB(
        np.array([[[0.2, 0.3, 0.4]], [[0.05, 0.9, 0.1]]]),
        colourspace="sRGB",
        apply_cctf_encoding=False,
        illuminant=chain.viewing_xy,
    )
    err = np.max(np.abs(check.reshape(-1, 3) - np.einsum("ij,kj->ki", matrix, np.array([[0.2, 0.3, 0.4], [0.05, 0.9, 0.1]]))))
    assert err < 1e-10, f"XYZ->sRGB matrix mismatch: {err}"

    with path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("// SPDX-License-Identifier: GPL-3.0-or-later\n")
        handle.write("// Copyright (C) 2026 SpektraRoll contributors\n")
        handle.write("//\n")
        handle.write("// GENERATED by tools/parity/reference_model.py -- do not edit.\n")
        handle.write("// Numeric data derived from colour-science (BSD-3-Clause) and\n")
        handle.write("// spektrafilm profiles by Andrea Volpato (CC BY-SA 4.0).\n")
        handle.write('#include "sr/spectral_data.hpp"\n\n')
        handle.write("namespace sr::data {\n\n")

        handle.write("extern const float kXyzD50ToSrgb[9] = {\n")
        handle.write(arr(matrix.flatten(), 3))
        handle.write("\n};\n\n")

        for name, vec in (
            ("kCmfs", chain.cmf),
            ("kIlluminantD55", chain.reference_illuminant),
            ("kIlluminantD50", chain.viewing_illuminant),
            ("kIlluminantThKg3", chain.print_illuminant_raw),
            ("kMallettBasis", basis),
        ):
            handle.write(f"extern const float {name}[] = {{\n")
            handle.write(arr(vec))
            handle.write("\n};\n\n")

        handle.write("}  // namespace sr::data\n")

    meta = {
        "film": chain.film.info.stock,
        "paper": chain.paper.info.stock,
        "print_exposure_factor": chain.print_exposure_factor,
        "enlarger_filters": list(chain.enlarger_filters),
        "viewing_xy": chain.viewing_xy.tolist(),
        "scan_normalization": chain.scan_normalization,
    }
    (path.parent / "reference_meta.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=str(REPO / "out" / "reference_portra400_endura.cube"))
    parser.add_argument("--size", type=int, default=33)
    parser.add_argument("--film", default="kodak_portra_400")
    parser.add_argument("--paper", default="kodak_portra_endura")
    parser.add_argument("--dump-tables", action="store_true")
    args = parser.parse_args()

    chain = ColourChain(args.film, args.paper)
    print(f"print_exposure_factor = {chain.print_exposure_factor:.12f}")

    # Sanity probes.
    probes = np.array([[0.0, 0.0, 0.0], [0.184, 0.184, 0.184], [0.5, 0.5, 0.5],
                       [1.0, 1.0, 1.0], [0.8, 0.2, 0.2], [0.2, 0.6, 0.2]])
    print("probe in -> out")
    for p in probes:
        print("  ", np.round(p, 3).tolist(), "->", np.round(chain.transform(p)[0], 5).tolist())

    if args.dump_tables:
        dump_tables(chain, REPO / "src" / "core" / "src" / "spectral_tables.cpp")
        print("wrote src/core/src/spectral_tables.cpp")

    data = bake(chain, args.size)
    write_cube(Path(args.out), f"SpektraRoll reference {args.film} + {args.paper}", data)
    print(f"wrote {args.out} ({args.size}^3)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
