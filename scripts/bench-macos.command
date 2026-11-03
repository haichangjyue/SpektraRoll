#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Double-click me on macOS to time the GPU paths at 2.6 MP.
# Needs a build first: scripts/verify-macos.command does that.

set -e
cd "$(dirname "$0")/.."

EXE=./build/spektra-roll-cli
if [ ! -x "$EXE" ]; then
    echo "还没有编译。先双击 scripts/verify-macos.command，或手动:"
    echo "  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"
    read -r -p "按回车退出..." _
    exit 1
fi

mkdir -p out

echo "=============================================="
echo " SpektraRoll 2.6 MP 基准 (2160x1214)"
echo "=============================================="
echo

echo "== 设备 =="
"$EXE" gpu-info
echo

echo "== 生成 2160x1214 16-bit 测试帧 =="
"$EXE" gen --out out/bench-frame.png --width 2160 --height 1214 --bits 16
echo

echo "== 光谱颜色: CPU vs Metal =="
"$EXE" lut apply --engine spectral --scan negative --couplers on --gamut oklch \
    --in out/bench-frame.png --out out/bench-cpu.png | tail -n 2
"$EXE" lut apply --engine spectral-gpu --scan negative --couplers on --gamut oklch \
    --in out/bench-frame.png --out out/bench-gpu.png | tail -n 2
echo

echo "== 质感: CPU vs Metal (颜色都在 spectral-gpu) =="
"$EXE" lut apply --engine spectral-gpu --scan negative --couplers on --gamut oklch \
    --texture-engine cpu --grain 0.4 --halation 0.45 --glare 0.2 --diffusion 0.1 \
    --in out/bench-frame.png --out out/bench-tex-cpu.png | tail -n 2
"$EXE" lut apply --engine spectral-gpu --scan negative --couplers on --gamut oklch \
    --texture-engine gpu --grain 0.4 --halation 0.45 --glare 0.2 --diffusion 0.1 \
    --in out/bench-frame.png --out out/bench-tex-gpu.png | tail -n 2
echo

echo "注意: 这些时间包含 PNG 读写; 看每一对的差值, 不要看绝对值。"
echo "把这四行发回给工程侧, 就可以补进实测表。"
echo
read -r -p "按回车关闭窗口..." _
