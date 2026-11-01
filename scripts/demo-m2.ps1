# SPDX-License-Identifier: GPL-3.0-or-later
# M2 demo: take a frame out of an Apple Log clip, bake the Portra 400 LUT for
# Apple Log input, and show "plain SDR decode" vs "shot on Portra 400".
#
# You do not need to understand this file. Double-click scripts\demo-m2.cmd,
# or run from the repo root:
#   .\scripts\demo-m2.ps1
#
# Everything lands in .\out\. The two PNGs it opens are the only thing you
# need to look at.

param(
    [switch]$NoOpen,
    [string]$Footage = "X:\agent\footage\A001_11011538_C001.mov",
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
    Write-Host "把 iPhone 拍的 .mov 放到 X:\agent\footage\, 然后用 -Footage 指定路径." -ForegroundColor Red
    exit 1
}

New-Item -ItemType Directory -Force -Path "out" | Out-Null

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll M2 演示" -ForegroundColor Cyan
Write-Host " Apple Log 素材 -> 柯达 Portra 400 + Portra Endura" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/4  从素材里取一帧 (第 $Time 秒, 16-bit)..." -ForegroundColor Cyan
& $ffmpeg -v error -y -ss $Time -i $Footage -frames:v 1 `
    -vf "scale=in_color_matrix=bt2020:in_range=tv:out_range=full,format=rgb48le" `
    "out\m2-frame-applelog.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "2/4  烘焙 Apple Log 输入的 Portra 400 LUT (33^3)..." -ForegroundColor Cyan
& $exe lut bake --input apple-log --exposure $Exposure --out "out\m2-applelog.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "3/4  只做解码, 不上胶片 (对照)..." -ForegroundColor Cyan
& $exe convert --in "out\m2-frame-applelog.png" --out "out\m2-before.png" --from apple-log --exposure $Exposure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "4/4  套上 Portra 400..." -ForegroundColor Cyan
& $exe lut apply --lut "out\m2-applelog.cube" --in "out\m2-frame-applelog.png" --out "out\m2-after.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "看图重点:" -ForegroundColor Green
Write-Host "  * 天空和窗户: 高光是被压住了, 还是糊成一片白" -ForegroundColor Green
Write-Host "  * 暗部: 是不是偏暖, 有没有死黑" -ForegroundColor Green
Write-Host "  * 彩色招牌和墙面: 是不是过饱和或者串色" -ForegroundColor Green
Write-Host "  * 整体: 像不像胶片拍的, 还是像手机直出" -ForegroundColor Green
Write-Host ""
Write-Host "两张图已经打开, 可以并排看:" -ForegroundColor Green
Write-Host "  $root\out\m2-before.png   (只解码, 对照)"
Write-Host "  $root\out\m2-after.png    (过 Portra 400)"
Write-Host ""

if (-not $NoOpen) {
    Start-Process (Join-Path $root "out\m2-before.png")
    Start-Process (Join-Path $root "out\m2-after.png")
}
