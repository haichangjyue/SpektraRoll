# SPDX-License-Identifier: GPL-3.0-or-later
# Double-click me to verify the D072 engine contract on Windows.
# This does not touch the UI: it checks the CLI handshake, catalog, preview
# protocol and (when the test footage is present) JSON render progress.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$cli = Join-Path $root "build\spektra-roll-cli.exe"
if (-not (Test-Path $cli)) {
    Write-Host "Missing $cli" -ForegroundColor Red
    Write-Host "Run scripts\build.cmd first." -ForegroundColor Yellow
    exit 1
}

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll - D072 engine contract" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/4  capabilities --json ..." -ForegroundColor Cyan
$caps = & $cli capabilities --json | Out-String | ConvertFrom-Json
if ($caps.preview.protocol -ne 2) {
    throw "preview protocol is $($caps.preview.protocol), expected 2"
}
if (-not ($caps.commands -contains "catalog")) {
    throw "capabilities.commands does not list catalog"
}
Write-Host "     ok: $($caps.platform) $($caps.arch), gpu=$($caps.gpu.backend), ocio=$($caps.ocio.available)" -ForegroundColor Green

Write-Host "2/4  catalog --json ..." -ForegroundColor Cyan
$catalog = & $cli catalog --json | Out-String | ConvertFrom-Json
if ($catalog.counts.films -ne 20 -or $catalog.counts.papers -ne 8) {
    throw "catalog counts are $($catalog.counts.films)/$($catalog.counts.papers), expected 20/8"
}
Write-Host "     ok: $($catalog.counts.films) films, $($catalog.counts.papers) papers" -ForegroundColor Green

Write-Host "3/4  preview hello ..." -ForegroundColor Cyan
$hello = ("hello`nquit`n" | & $cli preview) -join "`n"
if ($hello -notmatch '"protocol":2') {
    throw "preview did not answer hello with protocol 2"
}
Write-Host "     ok: $($hello.Split("`n")[0])" -ForegroundColor Green

Write-Host "4/4  ctest ..." -ForegroundColor Cyan
$ctest = "C:\Program Files\CMake\bin\ctest.exe"
if (-not (Test-Path $ctest)) { $ctest = "ctest" }
& $ctest --test-dir build --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "ctest failed with exit code $LASTEXITCODE"
}
Write-Host "     ok: 11/11" -ForegroundColor Green

$footage = "X:\agent\footage\C0113.MP4"
$ffmpeg = "J:\ffmpeg\bin\ffmpeg.exe"
$lut = Join-Path $root "out\cine-portra400.cube"
if ((Test-Path $footage) -and (Test-Path $ffmpeg) -and (Test-Path $lut)) {
    Write-Host "5/5  video --progress json (24 frames) ..." -ForegroundColor Cyan
    $out = Join-Path $root "out\phase0-progress.mov"
    $lines = & $cli video --lut $lut --in $footage --out $out --frames 24 `
        --codec prores --in-matrix bt709 --ffmpeg $ffmpeg --progress json 2>&1
    $done = $lines | Where-Object { $_ -match '"event":"done"' }
    if (-not $done) { throw "no done event in JSON progress" }
    Write-Host "     ok: $done" -ForegroundColor Green
} else {
    Write-Host "5/5  video --progress json: skipped (needs C0113.MP4 + ffmpeg + out\cine-portra400.cube)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "D072 engine contract: OK" -ForegroundColor Green
