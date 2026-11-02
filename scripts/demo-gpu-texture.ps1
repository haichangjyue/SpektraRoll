# SPDX-License-Identifier: GPL-3.0-or-later
# GPU texture demo (D047).
#
# Renders the same 2 second clip twice: once with the reference CPU texture
# chain and once with grain/halation/glare/diffusion on the D3D11 GPU. Prints
# both timings, the speed-up and a PSNR between the two encoded clips, then
# leaves the GPU clip open for inspection.
#
# Double-click scripts\demo-gpu-texture.cmd.

param(
    [switch]$NoOpen,
    [string]$Footage = "X:\agent\footage\A001_11011538_C001.mov",
    [double]$Seconds = 2,
    [double]$Grain = 0.4,
    [double]$Halation = 0.45,
    [double]$Glare = 0.2,
    [double]$Diffusion = 0.1
)

[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$OutputEncoding = [Console]::OutputEncoding
# ffmpeg reports progress on stderr; with "Stop" that becomes a terminating
# NativeCommandError, so keep going and check $LASTEXITCODE where it matters.
$ErrorActionPreference = "Continue"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$exe = Join-Path $root "build\spektra-roll-cli.exe"
if (-not (Test-Path $exe)) {
    Write-Host "Building first..." -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot "build.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$ffmpeg = "J:\ffmpeg\bin\ffmpeg.exe"
if (-not (Test-Path $ffmpeg)) {
    $found = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($null -eq $found) {
        Write-Host "ffmpeg not found. Put it on PATH or pass -Footage..." -ForegroundColor Red
        exit 1
    }
    $ffmpeg = $found.Source
}

if (-not (Test-Path $Footage)) {
    Write-Host "Footage not found: $Footage" -ForegroundColor Red
    exit 1
}

New-Item -ItemType Directory -Force -Path "out" | Out-Null

$shared = @(
    "video",
    "--engine", "spectral-gpu",
    "--input", "apple-log",
    "--scan", "negative",
    "--contrast", "1.15",
    "--couplers", "on",
    "--gamut", "oklch",
    "--exposure", "0.7",
    "--grain", $Grain,
    "--halation", $Halation,
    "--glare", $Glare,
    "--diffusion", $Diffusion,
    "--in", $Footage,
    "--duration", $Seconds,
    "--codec", "prores",
    "--ffmpeg", $ffmpeg
)

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll: texture chain on the GPU (D047)" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

function Measure-Render([string]$label, [string[]]$extra, [string]$out) {
    Write-Host $label -ForegroundColor Yellow
    $text = & $exe @shared @extra --out $out 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        Write-Host $text.Trim() -ForegroundColor Red
        Write-Host "Render failed." -ForegroundColor Red
        exit 1
    }
    $text.Trim() | Write-Host
    $match = [regex]::Match($text, "([0-9.]+) ms/frame")
    if (-not $match.Success) {
        Write-Host "Could not read the timing from the render output." -ForegroundColor Red
        exit 1
    }
    return [double]$match.Groups[1].Value
}

$cpuMs = Measure-Render "1/2  CPU texture chain (reference)" @("--texture-engine", "cpu") "out\gpu-texture-cpu.mov"
Write-Host ""
$gpuMs = Measure-Render "2/2  GPU texture chain (D3D11)" @() "out\gpu-texture-gpu.mov"

Write-Host ""
Write-Host "----------------------------------------------" -ForegroundColor DarkGray
Write-Host (" CPU texture: {0,7:N1} ms/frame" -f $cpuMs)
Write-Host (" GPU texture: {0,7:N1} ms/frame" -f $gpuMs)
if ($gpuMs -gt 0) {
    Write-Host (" Speed-up:    {0,7:N2}x" -f ($cpuMs / $gpuMs)) -ForegroundColor Green
}
Write-Host "----------------------------------------------" -ForegroundColor DarkGray

# The encoded clips differ by the encoder as well, so this is a sanity check
# rather than the parity number (tests\test_gpu_texture.cpp does the raw one).
$psnrLog = "out\gpu-texture-psnr.log"
Start-Process -FilePath $ffmpeg -NoNewWindow -Wait `
    -ArgumentList @("-hide_banner", "-i", "out\gpu-texture-cpu.mov", "-i", "out\gpu-texture-gpu.mov",
                   "-lavfi", "psnr", "-f", "null", "NUL") `
    -RedirectStandardError $psnrLog -RedirectStandardOutput "out\gpu-texture-psnr-out.log" | Out-Null
$psnr = $null
if (Test-Path $psnrLog) {
    $psnr = (Get-Content $psnrLog | Select-String "average:") | Select-Object -First 1
}
if ($psnr) {
    Write-Host ""
    Write-Host "Encoded-clip PSNR (CPU vs GPU texture):" -ForegroundColor Cyan
    Write-Host ("  " + $psnr.Line.Trim())
}

Write-Host ""
Write-Host "Files:" -ForegroundColor Cyan
Write-Host "  out\gpu-texture-cpu.mov   (reference chain)"
Write-Host "  out\gpu-texture-gpu.mov   (GPU chain)"
Write-Host ""
Write-Host "Raw parity (no encoder) is checked by:" -ForegroundColor Cyan
Write-Host "  ctest --test-dir build -R gpu_texture --output-on-failure" -ForegroundColor Gray
Write-Host ""

if (-not $NoOpen) {
    Start-Process "out\gpu-texture-gpu.mov"
}
