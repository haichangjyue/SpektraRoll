# SPDX-License-Identifier: GPL-3.0-or-later
# M1 demo: bake the real Kodak Portra 400 + Portra Endura LUT and apply it.
#
# You do not need to understand this file. Double-click scripts\demo-m1.cmd,
# or run from the repo root:
#   .\scripts\demo-m1.ps1
#
# Everything lands in .\out\. The two PNGs it opens are the only thing you
# need to look at.

param(
    [switch]$NoOpen
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

New-Item -ItemType Directory -Force -Path "out" | Out-Null

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll M1 演示" -ForegroundColor Cyan
Write-Host " 柯达 Portra 400 负片 + Portra Endura 相纸" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/4  烘焙真正的胶片 LUT (33^3)..." -ForegroundColor Cyan
& $exe lut bake --out "out\m1-portra400-endura.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "2/4  生成 16-bit 测试图..." -ForegroundColor Cyan
& $exe gen --out "out\m1-before.png" --width 1280 --height 720 --bits 16
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "3/4  把 LUT 套到测试图上..." -ForegroundColor Cyan
& $exe lut apply --lut "out\m1-portra400-endura.cube" --in "out\m1-before.png" --out "out\m1-after.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "4/4  和 Python 参考实现逐点对比 (质量检查)..." -ForegroundColor Cyan
& $exe lut compare --a "tests\reference\portra400_endura_33.cube" --b "out\m1-portra400-endura.cube"
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "注意: 对比没有通过, 请把窗口截图发给我." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "看图重点:" -ForegroundColor Green
Write-Host "  * 上半部的灰阶: 暗部是不是偏暖, 亮部是不是发灰" -ForegroundColor Green
Write-Host "  * 第二排左起第 1 块 (肤色): 是不是自然, 还是发黄或发红" -ForegroundColor Green
Write-Host "  * 红色和蓝色色块: 有没有过饱和或者串色" -ForegroundColor Green
Write-Host ""
Write-Host "两张图已经打开, 可以并排看:" -ForegroundColor Green
Write-Host "  $root\out\m1-before.png   (原始)"
Write-Host "  $root\out\m1-after.png    (过 Portra 400)"
Write-Host ""

if (-not $NoOpen) {
    Start-Process (Join-Path $root "out\m1-before.png")
    Start-Process (Join-Path $root "out\m1-after.png")
}
