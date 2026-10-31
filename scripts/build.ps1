# SPDX-License-Identifier: GPL-3.0-or-later
# Build, test, and report where the binary landed.
#
# You do not need to understand this file. Run it from the repo root:
#   .\scripts\build.ps1

param(
    [switch]$Clean,
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$toolRoot = "X:\agent\tools"
$wanted = @(
    "$toolRoot\mingw64\bin",
    "$toolRoot\ninja"
)
foreach ($dir in $wanted) {
    if (Test-Path $dir) { $env:PATH = "$dir;$env:PATH" }
}

function Find-Tool([string]$name, [string[]]$fallbacks) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($p in $fallbacks) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

$cmake = $null
foreach ($p in @("C:\Program Files\CMake\bin\cmake.exe", "$toolRoot\cmake\bin\cmake.exe")) {
    if (Test-Path $p) { $cmake = $p; break }
}
if (-not $cmake) {
    $cmd = Get-Command "cmake" -ErrorAction SilentlyContinue
    if ($cmd) { $cmake = $cmd.Source }
}
if (-not $cmake) {
    Write-Host "CMake not found. Run scripts/dev-install.ps1 first." -ForegroundColor Red
    exit 1
}

$ninja = Find-Tool "ninja" @(
    "$toolRoot\ninja\ninja.exe"
)
$gxx = Find-Tool "g++" @(
    "$toolRoot\mingw64\bin\g++.exe"
)
$cl = Find-Tool "cl" @()

$compilerArgs = @()
$compilerName = "default"
if ($cl) {
    $compilerName = "MSVC"
} elseif ($gxx) {
    $compilerName = "MinGW GCC"
    $compilerArgs = @("-DCMAKE_CXX_COMPILER=$gxx")
}

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item -Recurse -Force $BuildDir
}

$generator = @()
if ($ninja) {
    $generator = @("-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=$ninja")
} else {
    Write-Host "Ninja not found, falling back to the default generator (slower)." -ForegroundColor Yellow
}

Write-Host "Compiler: $compilerName" -ForegroundColor Cyan
Write-Host "Configuring..." -ForegroundColor Cyan
& $cmake -S . -B $BuildDir @generator @compilerArgs -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Building..." -ForegroundColor Cyan
& $cmake --build $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Testing..." -ForegroundColor Cyan
& $cmake --build $BuildDir --target test
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = Join-Path $BuildDir "spektravideo-cli.exe"
if (-not (Test-Path $exe)) { $exe = Join-Path $BuildDir "spektravideo-cli" }

Write-Host ""
Write-Host "OK. Binary:" -ForegroundColor Green
Write-Host "  $exe"
Write-Host ""
Write-Host "Try the demo:" -ForegroundColor Green
Write-Host "  .\scripts\demo.ps1"
