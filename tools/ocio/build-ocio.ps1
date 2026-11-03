# SPDX-License-Identifier: GPL-3.0-or-later
# D062: build OpenColorIO for the Windows MinGW toolchain and install it into a
# small self-contained prefix that CMakeLists.txt picks up automatically.
#
# Why a script instead of a package manager: there is no MinGW build of OCIO on
# Windows, and MSVC is not installed (D005). OCIO 2.4.2 builds cleanly with GCC
# once two MinGW issues are patched:
#   1. tools/ocio/mingw.patch — OCIO's Windows monitor code assumes MSVC's wide
#      tstring; the patch keeps both the UNICODE and the narrow builds working.
#   2. the fetched yaml-cpp 0.7.0 misses #include <cstdint>, which GCC 13+
#      rejects. The script adds it after OCIO's configure step has downloaded it.
#
# The result is prefix/lib/libOpenColorIO.a plus its static dependencies; that is
# exactly what the CMake option SR_WITH_OCIO expects.

param(
    [string]$Version = "v2.4.2",
    [string]$Source = "X:\agent\tools\ocio-src",
    [string]$Build = "X:\agent\tools\ocio-build",
    [string]$Prefix = "X:\agent\tools\ocio",
    [string]$CMake = "C:\Program Files\CMake\bin\cmake.exe",
    [string]$Ninja = "X:\agent\tools\ninja\ninja.exe",
    [string]$Gcc = "X:\agent\tools\mingw64\bin\g++.exe"
)

$ErrorActionPreference = "Stop"
$gccDir = Split-Path -Parent $Gcc

Write-Host "OpenColorIO $Version -> $Prefix" -ForegroundColor Cyan

if (-not (Test-Path (Join-Path $Source "CMakeLists.txt"))) {
    Write-Host "cloning OCIO $Version..." -ForegroundColor Cyan
    & git clone --depth 1 --branch $Version https://github.com/AcademySoftwareFoundation/OpenColorIO.git $Source
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Apply the MinGW patch once (git apply is a no-op if it is already in).
Push-Location $Source
$applied = & git apply --check (Join-Path $PSScriptRoot "mingw.patch") 2>&1
if ($LASTEXITCODE -eq 0) {
    & git apply (Join-Path $PSScriptRoot "mingw.patch")
    if ($LASTEXITCODE -ne 0) { Pop-Location; exit $LASTEXITCODE }
    Write-Host "applied mingw.patch" -ForegroundColor Green
} else {
    Write-Host "mingw.patch already applied" -ForegroundColor Green
}
Pop-Location

# The nested external projects configure themselves with -GNinja but inherit
# nothing else, so ninja and the compiler have to be on PATH.
$env:PATH = (Split-Path -Parent $Ninja) + ";" + $gccDir + ";" + $env:PATH
$env:CC = Join-Path $gccDir "gcc.exe"
$env:CXX = $Gcc
# CMake 4 refuses the old cmake_minimum_required() in zlib/expat otherwise.
$env:CMAKE_POLICY_VERSION_MINIMUM = "3.5"

Write-Host "configuring..." -ForegroundColor Cyan
& $CMake -S $Source -B $Build -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$Ninja" `
    "-DCMAKE_C_COMPILER=$(Join-Path $gccDir 'gcc.exe')" `
    "-DCMAKE_CXX_COMPILER=$Gcc" `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_INSTALL_PREFIX=$Prefix" `
    -DOCIO_INSTALL_EXT_PACKAGES=ALL `
    -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_TESTS=OFF -DOCIO_BUILD_GPU_TESTS=OFF `
    -DOCIO_BUILD_PYTHON=OFF -DOCIO_BUILD_DOCS=OFF -DOCIO_BUILD_OPENFX=OFF `
    -DOCIO_USE_WINDOWS_UNICODE=OFF `
    -DBUILD_SHARED_LIBS=OFF
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# yaml-cpp 0.7.0 predates GCC 13's stricter transitive includes.
$yaml = Join-Path $Build "ext/build/yaml-cpp/src/yaml-cpp_install/src/emitterutils.cpp"
if (Test-Path $yaml) {
    $text = [System.IO.File]::ReadAllText($yaml)
    if ($text -notmatch '#include <cstdint>') {
        $text = $text.Replace("#include <algorithm>`n", "#include <algorithm>`n#include <cstdint>`n")
        [System.IO.File]::WriteAllText($yaml, $text)
        Write-Host "patched yaml-cpp emitterutils.cpp (<cstdint>)" -ForegroundColor Green
    }
}

Write-Host "building (several minutes)..." -ForegroundColor Cyan
& $CMake --build $Build --parallel 12
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $CMake --install $Build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Make the prefix self-contained: the static OCIO archive does not carry its
# dependencies, so the CMake target links them from the same directory.
$ext = Join-Path $Build "ext/dist"
New-Item -ItemType Directory -Force -Path (Join-Path $Prefix "lib") | Out-Null
foreach ($lib in @("libexpat.a", "libImath-3_1.a", "libminizip-ng.a", "libpystring.a",
                   "libyaml-cpp.a", "libzlibstatic.a")) {
    $from = Join-Path $ext "lib/$lib"
    if (Test-Path $from) { Copy-Item -LiteralPath $from -Destination (Join-Path $Prefix "lib") -Force }
}
Copy-Item -Path (Join-Path $ext "include/*") -Destination (Join-Path $Prefix "include") -Recurse -Force

Get-ChildItem (Join-Path $Prefix "lib") -Filter *.a |
    ForEach-Object { Write-Host ("  " + $_.Name) }
Write-Host "done: configure SpektraRoll with -DSR_OCIO_ROOT=$Prefix (the default already looks there)" -ForegroundColor Green
