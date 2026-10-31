# SPDX-License-Identifier: GPL-3.0-or-later
# End-to-end smoke test you can see with your own eyes.
#   .\scripts\demo.ps1
#
# It creates a test chart, bakes a synthetic LUT, applies it, and opens the
# output folder. Everything lands in .\out\

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$exe = Join-Path $root "build\spektravideo-cli.exe"
if (-not (Test-Path $exe)) {
    Write-Host "Binary not found, building first..." -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot "build.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

New-Item -ItemType Directory -Force -Path "out" | Out-Null

Write-Host "1/4  generating test chart" -ForegroundColor Cyan
& $exe gen --out "out\chart.png" --width 1600 --height 900

Write-Host "2/4  baking a synthetic LUT" -ForegroundColor Cyan
& $exe lut gen --out "out\warm.cube" --size 33 --preset warm

Write-Host "3/4  applying the LUT" -ForegroundColor Cyan
& $exe lut apply --lut "out\warm.cube" --in "out\chart.png" --out "out\chart_warm.png"

Write-Host "4/4  done" -ForegroundColor Cyan
Write-Host ""
Write-Host "Compare these two files:" -ForegroundColor Green
Write-Host "  $root\out\chart.png"
Write-Host "  $root\out\chart_warm.png"
Write-Host ""
Start-Process explorer.exe (Join-Path $root "out")
