# SPDX-License-Identifier: GPL-3.0-or-later
# M2 demo: take a frame out of a Sony S-Log3 clip (FX30), bake the Portra 400
# LUT for S-Log3 input, and show "plain SDR decode" vs "shot on Portra 400".
#
# You do not need to understand this file. Double-click scripts\demo-m2.cmd,
# or run from the repo root:
#   .\scripts\demo-m2.ps1
#
# Everything lands in .\out\. The two PNGs it opens are the only thing you
# need to look at.

param(
    [switch]$NoOpen,
    [string]$Footage = "X:\agent\footage\C0113.MP4",
    [double]$Time = 1.5,
    [double]$Exposure = 0.7
)

[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$OutputEncoding = [Console]::OutputEncoding
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$exe = Join-Path $root "build\spektra-roll-cli.exe"
if (-not (Test-Path $exe)) {
    Write-Host "还没编译, 先编译一次 (大约 1 分钟)..." -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot "build.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$ffmpeg = "J:\ffmpeg\bin\ffmpeg.exe"
if (-not (Test-Path $ffmpeg)) {
    $found = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($null -eq $found) {
        Write-Host "找不到 ffmpeg. 请把 ffmpeg.exe 放到 J:\ffmpeg\bin 或者加进 PATH." -ForegroundColor Red
        exit 1
    }
    $ffmpeg = $found.Source
}

if (-not (Test-Path $Footage)) {
    Write-Host "找不到素材: $Footage" -ForegroundColor Red
    Write-Host "把 Sony FX30 的 S-Log3 素材放到 X:\agent\footage\, 然后用 -Footage 指定路径." -ForegroundColor Red
    exit 1
}

New-Item -ItemType Directory -Force -Path "out" | Out-Null

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll M2 演示" -ForegroundColor Cyan
Write-Host " Sony FX30 S-Log3 素材 -> 柯达 Portra 400 + Portra Endura" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/4  从素材里取一帧 (第 $Time 秒, 16-bit)..." -ForegroundColor Cyan
& $ffmpeg -v error -y -ss $Time -i $Footage -frames:v 1 `
    -vf "scale=in_color_matrix=bt709:in_range=auto:out_range=full,format=rgb48le" `
    "out\m2-frame-slog3.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "2/4  烘焙 S-Log3 输入的 Portra 400 LUT (33^3)..." -ForegroundColor Cyan
& $exe lut bake --input s-log3 --exposure $Exposure --out "out\m2-slog3.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "3/4  只做解码, 不上胶片 (对照)..." -ForegroundColor Cyan
& $exe convert --in "out\m2-frame-slog3.png" --out "out\m2-before.png" --from s-log3 --exposure $Exposure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "4/5  套上 Portra 400 (印片, 高对比)..." -ForegroundColor Cyan
& $exe lut apply --lut "out\m2-slog3.cube" --in "out\m2-frame-slog3.png" --out "out\m2-after.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "5/6  扫描负片 (默认反差 1.15)..." -ForegroundColor Cyan
& $exe lut bake --input s-log3 --exposure $Exposure --scan negative --out "out\m2-slog3-negative.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe lut apply --lut "out\m2-slog3-negative.cube" --in "out\m2-frame-slog3.png" --out "out\m2-negative.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "5b   负片, 不加反差 (留全部余量, 对比用)..." -ForegroundColor Cyan
& $exe lut bake --input s-log3 --exposure $Exposure --scan negative --contrast 1.0 --out "out\m2-slog3-negative-flat.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe lut apply --lut "out\m2-slog3-negative-flat.cube" --in "out\m2-frame-slog3.png" --out "out\m2-negative-flat.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "6/6  输出整段 HDR 视频 (PQ, 白色 1000 nits)..." -ForegroundColor Cyan
& $exe lut bake --input s-log3 --exposure $Exposure --scan negative --hdr pq --peak 1000 --out "out\m2-slog3-hdr.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe lut apply --lut "out\m2-slog3-hdr.cube" --in "out\m2-frame-slog3.png" --out "out\m2-hdr-pq.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# 整段：新命令直接吃 .mov，逐帧套 LUT；源有音频时一起带过去。
Write-Host "    解码整段 + 逐帧套 LUT + 编码 (4K50 约 1 分钟)..." -ForegroundColor Gray
& $exe video --lut "out\m2-slog3-hdr.cube" --in $Footage --out "out\m2-hdr-pq.mov" `
    --in-matrix bt709 --trc pq --ffmpeg $ffmpeg
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "看图重点:" -ForegroundColor Green
Write-Host "  * 天空和窗户: 高光是被压住了, 还是糊成一片白" -ForegroundColor Green
Write-Host "  * 暗部: 是不是偏暖, 有没有死黑" -ForegroundColor Green
Write-Host "  * 彩色招牌和墙面: 是不是过饱和或者串色" -ForegroundColor Green
Write-Host "  * 印片 vs 负片: 印片更像一张洗出来的照片, 负片更平但两头都留得住" -ForegroundColor Green
Write-Host ""
Write-Host "四张图已经生成:" -ForegroundColor Green
Write-Host "  $root\out\m2-before.png         (只解码, 对照)"
Write-Host "  $root\out\m2-after.png          (印片: Portra 400 + Endura)"
Write-Host "  $root\out\m2-negative.png       (扫描负片: 默认反差 1.15)"
Write-Host "  $root\out\m2-negative-flat.png  (扫描负片: 不加反差, 留余量)"
Write-Host ""
Write-Host "HDR 版本:" -ForegroundColor Green
Write-Host "  $root\out\m2-hdr-pq.png         (16-bit PQ 静帧, 白色 1000 nits)"
Write-Host "  $root\out\m2-hdr-pq.mov         (整段 HEVC 10-bit, PQ/BT.2020; 源有音频时会带上)"
Write-Host "  注意: MacBook Air 的屏幕是 SDR 的, 看这个文件会像普通视频;" -ForegroundColor Yellow
Write-Host "        用 AirDrop 传到 iPhone 16 Pro 上才能看到真正的高光。" -ForegroundColor Yellow
Write-Host ""

if (-not $NoOpen) {
    Start-Process (Join-Path $root "out\m2-negative.png")
    Start-Process (Join-Path $root "out\m2-negative-flat.png")
}
