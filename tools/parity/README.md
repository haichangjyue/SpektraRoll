# Parity harness

These scripts exist so the C++ engine can be checked against the upstream
Python implementation instead of against "it looks about right".

```
reference_model.py   composes the M1 chain from upstream spektrafilm functions
                     -> writes out/reference_portra400_endura.cube
                     -> writes src/core/src/spectral_tables.cpp with --dump-tables
check_pipeline.py    runs the real upstream SimulationPipeline in lut_mode and
                     compares it with reference_model.py (sanity check on the
                     reference itself)
```

## Requirements

- The upstream checkout at `..\_upstream_spektrafilm` (or set
  `SPEKTRAFILM_SRC` to its `src` directory).
- A Python environment with numpy, scipy, colour-science, opt_einsum.
  This machine uses the portable interpreter in `X:\agent\tools\python`.

## Run

```powershell
cd X:\agent\SpektraRoll

# 1. regenerate the reference LUT and the embedded numeric tables
X:\agent\tools\python\python.exe tools\parity\reference_model.py --size 33 --dump-tables

# 2. check the reference itself against the real upstream pipeline
X:\agent\tools\python\python.exe tools\parity\check_pipeline.py

# 3. bake with C++ and compare
.\build\spektra-roll-cli.exe lut bake --out out\m1-portra400-endura.cube
.\build\spektra-roll-cli.exe lut compare --a out\reference_portra400_endura.cube --b out\m1-portra400-endura.cube
```

## Current numbers

| Metric | Value |
|---|---|
| PSNR | 88.07 dB |
| max abs difference | 4.57e-4 |
| mean abs difference | 1.28e-5 |
| CIE76 delta E, mean / p95 / max | 0.0015 / 0.0035 / 0.0065 |

The C++ side skips one no-op `colour.RGB_to_RGB(sRGB, sRGB)` round trip that
the Python reference performs. That round trip is not an identity at the
1e-5 level; it shifts the print exposure factor from 0.837773014 (exact) to
0.837787256 (upstream). The resulting output difference is below the numbers
above. Replicating a matrix round-off bug would make the engine less accurate,
so we do not.

`tests/reference/portra400_endura_33.cube` is the frozen reference used by
`scripts\verify-m1.cmd`; it lets the acceptance check run without Python.
Regenerate it only when the model is intentionally changed.

## Camera log curves (D060 / D061)

Every input decode (`slog3Decode`, `arriLogC3Decode`, `arriLogC4Decode`,
`bmdFilmGen5Decode`, `daVinciIntermediateDecode`) is compared with
colour-science 0.4.7 over a 101-point sweep. `tests/test_transfer.cpp` keeps a
subset of those points permanently (18% grey, both sides of every piecewise
knot, white), so a regression fails `ctest` without Python.

To re-run the full sweep after touching a curve, build a throwaway probe:

```cpp
// out/log-probe.cpp
#include <cstdio>
#include "sr/transfer.hpp"
int main() {
    for (int i = 0; i <= 100; ++i) {
        const double x = i / 100.0;
        std::printf("logc3 %.4f %.17g\n", x, sr::arriLogC3Decode(x));
    }
    return 0;
}
```

```powershell
X:\agent\tools\mingw64\bin\g++.exe -std=c++20 -O2 -I src\core\include `
  out\log-probe.cpp build\libsr_core.a -o out\log-probe.exe -static
```

then compare those numbers with `colour.log_decoding_ARRILogC3` etc. The current
result is a relative difference of 0 for S-Log3 / LogC3 / Blackmagic and
`<= 2.3e-16` for LogC4 / DaVinci (last-bit rounding: `exp2` vs `2 **`).
