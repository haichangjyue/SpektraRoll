# SPDX-License-Identifier: GPL-3.0-or-later
# Sets up a working C++ toolchain without admin rights.
#   .\scripts\dev-install.ps1
#
# What it does:
#   * checks for CMake (winget or C:\Program Files\CMake)
#   * downloads a portable Ninja into X:\agent\tools\ninja
#   * downloads portable MinGW-w64 GCC into X:\agent\tools\mingw64
#
# Everything stays in user space. Nothing is installed system-wide.

$ErrorActionPreference = "Stop"
$toolRoot = "X:\agent\tools"
New-Item -ItemType Directory -Force -Path $toolRoot | Out-Null

function Write-Step($text) { Write-Host "`n== $text" -ForegroundColor Cyan }

function Ensure-CMake {
    $candidates = @(
        "C:\Program Files\CMake\bin\cmake.exe",
        "$env:LOCALAPPDATA\Programs\CMake\bin\cmake.exe"
    )
    foreach ($c in $candidates) { if (Test-Path $c) { return $c } }
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    Write-Step "Installing CMake with winget"
    winget install --id Kitware.CMake -e --source winget `
        --accept-package-agreements --accept-source-agreements
    if (Test-Path $candidates[0]) { return $candidates[0] }
    throw "CMake install failed. Install it from cmake.org and re-run."
}

function Ensure-Ninja {
    $exe = Join-Path $toolRoot "ninja\ninja.exe"
    if (Test-Path $exe) { return $exe }

    Write-Step "Downloading portable Ninja"
    $zip = Join-Path $toolRoot "ninja-win.zip"
    curl.exe -L --fail --retry 3 --max-time 300 `
        "https://github.com/ninja-build/ninja/releases/latest/download/ninja-win.zip" -o $zip
    Expand-Archive -Path $zip -DestinationPath (Join-Path $toolRoot "ninja") -Force
    Remove-Item $zip
    return $exe
}

function Ensure-MinGW {
    $gxx = Join-Path $toolRoot "mingw64\bin\g++.exe"
    if (Test-Path $gxx) { return $gxx }

    Write-Step "Downloading MinGW-w64 (about 260 MB, one time)"
    $version = "16.2.0posix-23.1.3-14.0.0-ucrt-r3"
    $file = "winlibs-x86_64-posix-seh-gcc-16.2.0-mingw-w64ucrt-14.0.0-r3.zip"
    $url = "https://github.com/brechtsanders/winlibs_mingw/releases/download/$version/$file"
    $zip = Join-Path $toolRoot "winlibs-mingw.zip"
    curl.exe -L --fail --retry 3 --max-time 900 $url -o $zip
    Expand-Archive -Path $zip -DestinationPath $toolRoot -Force
    Remove-Item $zip
    return $gxx
}

$cmake = Ensure-CMake
$ninja = Ensure-Ninja
$gxx = Ensure-MinGW

Write-Host ""
Write-Host "Toolchain ready:" -ForegroundColor Green
Write-Host "  cmake : $cmake"
Write-Host "  ninja : $ninja"
Write-Host "  g++   : $gxx"
& $gxx --version | Select-Object -First 1
Write-Host ""
Write-Host "Next: .\scripts\build.ps1" -ForegroundColor Green
