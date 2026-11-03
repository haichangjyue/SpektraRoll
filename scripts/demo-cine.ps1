# SPDX-License-Identifier: GPL-3.0-or-later
# D065 demo: cinema stocks in SpektraRoll.
#   1) take one frame from the Sony FX30 S-Log3 clip
#   2) bake Vision3 500T printed on 2383 (the cinema print film) and the same
#      negative scanned directly
#   3) bake a Portra 400 still-photo look on the same frame, for comparison
#   4) open the three images side by side
#
# Vision3 500T is measured under T (tungsten); the 2383 print is viewed under
# K75P (a cinema projector). Both are handled by the model, not hard-coded.
#
# You do not need to understand this file. Double-click scripts\demo-cine.cmd.

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
    Write-Host "还没有 CLI, 先编译一次..." -ForegroundColor Yellow
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

New-Item -ItemType Directory -Force -Path "out" | Out-Null

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll 电影片库 (D065)" -ForegroundColor Cyan
Write-Host " Vision3 500T -> 2383 印片 / 负片扫描 / Portra 400 对照" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/4  从素材取一帧 (Sony FX30 S-Log3, 第 $Time 秒, 16-bit)..." -ForegroundColor Cyan
if (Test-Path $Footage) {
    # in_range=auto follows the clip's color_range tag (this FX30 file is full range).
    & $ffmpeg -v error -y -ss $Time -i $Footage -frames:v 1 `
        -vf "scale=in_color_matrix=bt709:in_range=auto:out_range=full,format=rgb48le" `
        "out\cine-frame.png"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host "找不到素材: $Footage (用内置测试图代替)" -ForegroundColor Yellow
    & $exe gen --out "out\cine-frame.png" --bits 16 | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host ""
Write-Host "2/4  Vision3 500T + 2383 印片 (T 型灯 + K75P 放映机)..." -ForegroundColor Cyan
& $exe lut bake --film data\profiles\kodak_vision3_500t.json `
    --paper data\profiles\kodak_2383.json --scan print --input s-log3 `
    --exposure $Exposure --out "out\cine-vision3-2383.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe lut apply --lut "out\cine-vision3-2383.cube" --in "out\cine-frame.png" `
    --out "out\cine-vision3-2383.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "3/4  同一种片直接扫负片 (跳过印片, 留更多余量)..." -ForegroundColor Cyan
& $exe lut bake --film data\profiles\kodak_vision3_500t.json `
    --paper data\profiles\kodak_2383.json --scan negative --input s-log3 `
    --exposure $Exposure --contrast 1.15 --out "out\cine-vision3-negative.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe lut apply --lut "out\cine-vision3-negative.cube" --in "out\cine-frame.png" `
    --out "out\cine-vision3-negative.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "4/4  同一帧的 Portra 400 负片 (静态照片口味, 对照)..." -ForegroundColor Cyan
& $exe lut bake --film data\profiles\kodak_portra_400.json `
    --paper data\profiles\kodak_portra_endura.json --scan negative --input s-log3 `
    --exposure $Exposure --contrast 1.15 --out "out\cine-portra400.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe lut apply --lut "out\cine-portra400.cube" --in "out\cine-frame.png" `
    --out "out\cine-portra400.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "要怎么看:" -ForegroundColor Green
Write-Host "  * Vision3 500T 是电影负片, 2383 是电影印片; 对比静态 Portra 400:"
Write-Host "    电影负片更中性、宽容度更高, Portra 400 更暖、更“照片”"
Write-Host "  * 2383 的观看光源是 K75P (放映机), 不是相纸的 D50; 颜色会略暖"
Write-Host "  * 负片扫描跳过印片, 高光/暗部留得更多, 适合再调色"
Write-Host ""
Write-Host "生成的文件:" -ForegroundColor Green
Write-Host "  $root\out\cine-frame.png            (S-Log3 原帧)"
Write-Host "  $root\out\cine-vision3-2383.png     (Vision3 500T + 2383 印片)"
Write-Host "  $root\out\cine-vision3-negative.png (Vision3 500T 负片扫描)"
Write-Host "  $root\out\cine-portra400.png        (Portra 400 + Endura 负片, 对照)"
Write-Host ""

if (-not $NoOpen) {
    Start-Process (Join-Path $root "out\cine-vision3-2383.png")
    Start-Process (Join-Path $root "out\cine-vision3-negative.png")
    Start-Process (Join-Path $root "out\cine-portra400.png")
}
