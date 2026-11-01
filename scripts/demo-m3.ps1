# SPDX-License-Identifier: GPL-3.0-or-later
# M3 demo: film texture. Bakes the SDR negative LUT, then adds grain and
# halation per frame and renders the whole clip.
#
# You do not need to understand this file. Double-click scripts\demo-m3.cmd.

param(
    [switch]$NoOpen,
    [string]$Footage = "X:\agent\footage\A001_11011538_C001.mov",
    [double]$Time = 1.5,
    [double]$Exposure = 0.7,
    [double]$Grain = 0.4,
    [double]$Halation = 0.45
)

[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$OutputEncoding = [Console]::OutputEncoding
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$exe = Join-Path $root "build\spektra-roll-cli.exe"
if (-not (Test-Path $exe)) {
    Write-Host "还没编译, 先编译一次..." -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot "build.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$ffmpeg = "J:\ffmpeg\bin\ffmpeg.exe"
if (-not (Test-Path $ffmpeg)) {
    $found = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($null -eq $found) {
        Write-Host "找不到 ffmpeg. 请把 ffmpeg 加进 PATH 或用 -Footage 之外的参数指定." -ForegroundColor Red
        exit 1
    }
    $ffmpeg = $found.Source
}

if (-not (Test-Path $Footage)) {
    Write-Host "找不到素材: $Footage" -ForegroundColor Red
    exit 1
}

New-Item -ItemType Directory -Force -Path "out" | Out-Null

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll M3 演示: 颗粒 + 光晕" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/4  烘焙 SDR 扫描负片 LUT..." -ForegroundColor Cyan
& $exe lut bake --input apple-log --exposure $Exposure --scan negative --out "out\m3-applelog-sdr.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "2/4  取一帧..." -ForegroundColor Cyan
& $ffmpeg -v error -y -ss $Time -i $Footage -frames:v 1 `
    -vf "scale=in_color_matrix=bt2020:in_range=tv:out_range=full" `
    -pix_fmt rgb48le "out\m3-frame.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "3/4  同一帧: 干净版 vs 颗粒+光晕版..." -ForegroundColor Cyan
& $exe lut apply --lut "out\m3-applelog-sdr.cube" --in "out\m3-frame.png" --out "out\m3-clean.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe lut apply --lut "out\m3-applelog-sdr.cube" --in "out\m3-frame.png" --out "out\m3-texture.png" `
    --grain $Grain --halation $Halation
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "4/4  整段渲染 (带颗粒和光晕)..." -ForegroundColor Cyan
& $exe video --lut "out\m3-applelog-sdr.cube" --in $Footage --out "out\m3-texture.mov" `
    --trc srgb --grain $Grain --halation $Halation --ffmpeg $ffmpeg
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "看图重点 (建议放大到 100% 看):" -ForegroundColor Green
Write-Host "  * 颗粒: 中灰区域的细颗粒, 暗部和亮部应该更少" -ForegroundColor Green
Write-Host "  * 颗粒在动: 播放视频, 颗粒应该逐帧变化, 不是一张贴图" -ForegroundColor Green
Write-Host "  * 光晕: 高光边缘有没有一点点暖色的晕开" -ForegroundColor Green
Write-Host ""
Write-Host "生成的文件:" -ForegroundColor Green
Write-Host "  $root\out\m3-clean.png     (不带质感)"
Write-Host "  $root\out\m3-texture.png   (颗粒 $Grain, 光晕 $Halation)"
Write-Host "  $root\out\m3-texture.mov   (整段, 带声音)"
Write-Host ""

if (-not $NoOpen) {
    Start-Process (Join-Path $root "out\m3-texture.png")
}
