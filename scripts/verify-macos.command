#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Double-click me on macOS to build, test and check the film LUT.
# Needs Xcode command line tools and cmake (brew install cmake).

set -e
cd "$(dirname "$0")/.."

echo "=============================================="
echo " SpektraRoll macOS 验收"
echo "=============================================="
echo

if ! xcode-select -p >/dev/null 2>&1; then
    echo "缺少 Xcode 命令行工具。"
    echo "请先在终端运行: xcode-select --install"
    echo "装完再双击本文件。"
    read -r -p "按回车退出..." _
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "缺少 cmake。安装方法: brew install cmake"
    read -r -p "按回车退出..." _
    exit 1
fi

echo "1/4  编译 (第一次大约 1 分钟)..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

echo
echo "2/4  单元测试..."
ctest --test-dir build --output-on-failure

echo
echo "3/4  烘 Portra 400 LUT 并和参考对比..."
mkdir -p out
./build/spektra-roll-cli version
./build/spektra-roll-cli lut bake --out out/mac-m1.cube
./build/spektra-roll-cli lut compare --a tests/reference/portra400_endura_33.cube --b out/mac-m1.cube

echo
echo "4/4  视频路径..."
if command -v ffmpeg >/dev/null 2>&1; then
    ffmpeg -y -v error -f lavfi -i testsrc2=size=320x180:rate=24 -t 0.5 -pix_fmt yuv420p out/mac-src.mov
    ./build/spektra-roll-cli video --lut out/mac-m1.cube --in out/mac-src.mov \
        --out out/mac-video.mov --trc srgb --in-matrix bt709
    echo "视频路径 OK: out/mac-video.mov"
else
    echo "没有安装 ffmpeg，跳过视频测试。"
    echo "需要的话运行: brew install ffmpeg"
fi

echo
echo "全部检查通过。"
read -r -p "按回车关闭窗口..." _
