# GPU probe — D3D11 compute on the Arc A750

This is a throwaway feasibility probe, not part of the product build. It answers
one question: if the 81-sample spectral scan moves to the GPU, is the win large
enough to justify porting the whole `FilmModel` chain?

It dispatches one compute shader over a 2160×1214 buffer. Each thread runs the
same 81-iteration structure as `FilmModel::scanPositiveDye`: dot(CMY, dye
density), `exp2(-density)`, multiply by the viewing illuminant and the CIE CMFs.
The data is synthetic; this measures compute throughput, not colour accuracy.

## Build and run

```powershell
$env:PATH = "X:\agent\tools\qt\Tools\mingw1310_64\bin;$env:PATH"
g++ -O2 -std=c++20 tools\gpu-probe\gpu_probe.cpp -o out\gpu_probe.exe `
    -ld3d11 -ldxgi -ld3dcompiler
.\out\gpu_probe.exe
```

## Result (2026-11-02)

```
adapter: Intel(R) Arc(TM) A750 Graphics
feature level: 0xb000          # D3D_FEATURE_LEVEL_11_0
pixels: 2622240, groups: 40973
gpu query readback avg: 0.632 ms
```

- 2.6 MP × 81 samples: **0.63 ms** per dispatch (wall clock around submit and
  GPU completion, 5 runs after warm-up).
- CPU reference: the full per-pixel spectral chain is **1081 ms** for the same
  frame. The scan loop is one of its heavier stages, so the GPU headroom is
  roughly two to three orders of magnitude.

## What it does not prove

- It is one stage with dummy densities, not the complete film/paper/gamut chain.
- Double precision on the CPU vs float on the GPU will change the last bits;
  parity must be measured (target: ≥60 dB against the CPU spectral reference).
- D3D11 is Windows-only. macOS keeps the CPU spectral path.
- Upload/readback and texture formats are not included; the real path should
  keep pixels in a texture and avoid a staging copy.

## Next

Port the full `FilmModel::transformFrom` to HLSL as a D3D11 compute shader,
validate it against the CPU spectral engine on a grid and a real frame, then
expose `--engine spectral-gpu` with CPU fallback.
