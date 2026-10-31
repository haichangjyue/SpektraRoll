# SPDX-License-Identifier: GPL-3.0-or-later
# M1 acceptance check: build, unit tests, bake, numeric parity.
#
# Double-click scripts\verify-m1.cmd, or run from the repo root:
#   .\scripts\verify-m1.ps1
#
# This is the "does it still work?" button. It prints PASS or FAIL and exits
# with a non-zero code on failure.

[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$OutputEncoding = [Console]::OutputEncoding
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host ""
Write-Host "=== 1/3  编译 + 单元测试 ===" -ForegroundColor Cyan
& (Join-Path $PSScriptRoot "build.ps1")
if ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL: 编译或单元测试没有通过" -ForegroundColor Red
    exit 1
}

$exe = Join-Path $root "build\spektra-roll-cli.exe"
New-Item -ItemType Directory -Force -Path "out" | Out-Null

Write-Host ""
Write-Host "=== 2/3  烘焙 M1 LUT ===" -ForegroundColor Cyan
& $exe lut bake --out "out\m1-portra400-endura.cube"
if ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL: LUT 烘焙失败" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "=== 3/3  与参考实现逐点对比 ===" -ForegroundColor Cyan
& $exe lut compare --a "tests\reference\portra400_endura_33.cube" --b "out\m1-portra400-endura.cube"
if ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL: 数值对比没有通过" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "PASS: M1 全部检查通过." -ForegroundColor Green
Write-Host "双击 scripts\demo-m1.cmd 可以直接看画面." -ForegroundColor Green
