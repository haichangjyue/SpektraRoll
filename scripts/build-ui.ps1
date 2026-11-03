# SPDX-License-Identifier: GPL-3.0-or-later
# Build the Qt 6 desktop UI (D009) and deploy the Qt runtime next to the exe.
# Double-click scripts\build-ui.cmd. Everything lives outside the repo:
#   X:\agent\tools\qt\6.8.3\mingw_64          Qt 6.8.3 (MinGW build)
#   X:\agent\tools\qt\Tools\mingw1310_64      matching compiler

param(
    [switch]$NoLaunch
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$cmake = "C:\Program Files\CMake\bin\cmake.exe"
$ninja = "X:\agent\tools\ninja\ninja.exe"
$qt = "X:\agent\tools\qt\6.8.3\mingw_64"
$mingw = "X:\agent\tools\qt\Tools\mingw1310_64"

# cc1plus lives in libexec and finds its runtime DLLs through PATH, so the
# compiler's bin directory must come first. Qt's DLLs are needed at run time and
# by windeployqt.
$env:PATH = "$mingw\bin;$qt\bin;$env:PATH"

foreach ($path in @($cmake, $ninja, "$qt\bin\windeployqt.exe", "$mingw\bin\g++.exe")) {
    if (-not (Test-Path $path)) {
        Write-Host "Missing: $path" -ForegroundColor Red
        Write-Host "Install Qt 6.8.3 + MinGW 13.1 under X:\agent\tools\qt (see progress.md)." -ForegroundColor Yellow
        exit 1
    }
}

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll - Qt 6 UI build" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/3  Configure..." -ForegroundColor Cyan
# The UI compiler is Qt's MinGW 13.1, while the OCIO static library is built
# with the main toolchain (GCC 16.2); mixing the two libstdc++ ABIs does not
# link. The UI never calls OCIO itself (it drives the CLI), so the UI build
# leaves it out until the UI exposes OCIO (D062).
& $cmake -S . -B build-ui -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$ninja" `
    "-DCMAKE_CXX_COMPILER=$mingw\bin\g++.exe" `
    "-DCMAKE_PREFIX_PATH=$qt" `
    -DSR_BUILD_QT_UI=ON -DSR_BUILD_TESTS=OFF -DSR_WITH_OCIO=OFF -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "2/3  Build..." -ForegroundColor Cyan
& $cmake --build build-ui
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "3/3  Deploy Qt runtime..." -ForegroundColor Cyan
& "$qt\bin\windeployqt.exe" --no-translations --compiler-runtime "$root\build-ui\spektra-roll-ui.exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = Join-Path $root "build-ui\spektra-roll-ui.exe"
if (-not (Test-Path $exe)) {
    Write-Host "The UI executable was not produced." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Done: $exe" -ForegroundColor Green
Write-Host "The matching CLI is build-ui\spektra-roll-cli.exe." -ForegroundColor Green
Write-Host ""

if (-not $NoLaunch) {
    Start-Process $exe
}
