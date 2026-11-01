# SPDX-License-Identifier: GPL-3.0-or-later
# M4 demo: segmented render + resume + source timecode (D040).
#
# Renders 2 seconds in 1-second chunks, deletes the last chunk to simulate a
# crash, then runs the same command with --resume. The second run must skip the
# finished chunk, render only the missing one, and keep the clip's timecode.
#
# Double-click scripts\demo-m4.cmd.

param(
    [switch]$NoOpen,
    [string]$Footage = "X:\agent\footage\A001_11011538_C001.mov",
    [double]$Time = 1.5,
    [double]$Exposure = 0.7,
    [double]$Seconds = 2,
    [double]$Segment = 1
)

[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$OutputEncoding = [Console]::OutputEncoding
$ErrorActionPreference = "Stop"

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
$ffprobe = Join-Path (Split-Path $ffmpeg) "ffprobe.exe"
if (-not (Test-Path $ffprobe)) {
    Write-Host "ffprobe not found next to ffmpeg." -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $Footage)) {
    Write-Host "Footage not found: $Footage" -ForegroundColor Red
    exit 1
}

New-Item -ItemType Directory -Force -Path "out" | Out-Null
$out = "out\m4-resume.mov"
$parts = "$out.parts"
if (Test-Path $parts) { Remove-Item $parts -Recurse -Force }

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll M4: segmented render + resume" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/4  Bake the SDR negative LUT..." -ForegroundColor Cyan
& $exe lut bake --input apple-log --exposure $Exposure --scan negative --out "out\m4-applelog-sdr.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "2/4  Render $Seconds s in $Segment s chunks..." -ForegroundColor Cyan
& $exe video --lut "out\m4-applelog-sdr.cube" --in $Footage --out $out `
    --trc srgb --duration $Seconds --segment $Segment --keep-parts --ffmpeg $ffmpeg
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "3/4  Simulate a crash: delete the last chunk..." -ForegroundColor Cyan
$victims = Get-ChildItem $parts -Filter "part-00001.mov*" -ErrorAction SilentlyContinue
if ($victims) {
    $victims | Remove-Item -Force
    Write-Host "  removed $($victims.Name -join ', ')"
} else {
    Write-Host "  no second chunk (increase -Seconds to see the resume)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "4/4  Resume the same command..." -ForegroundColor Cyan
Write-Host "     watch for 'part 1/2: already done'" -ForegroundColor DarkGray
& $exe video --lut "out\m4-applelog-sdr.cube" --in $Footage --out $out `
    --trc srgb --duration $Seconds --segment $Segment --resume --ffmpeg $ffmpeg
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$frames = (& $ffprobe -v error -select_streams v:0 -show_entries stream=nb_frames -of default=nw=1:nk=1 $out) -join ""
$timecode = @(& $ffprobe -v error -show_entries stream_tags=timecode:format_tags=timecode -of default=nw=1:nk=1 $out |
    Where-Object { $_ -ne "" })[0]
$audio = (& $ffprobe -v error -select_streams a:0 -show_entries stream=codec_name -of default=nw=1:nk=1 $out) -join ""

Write-Host ""
Write-Host "Result:" -ForegroundColor Green
Write-Host "  file:      $root\$out"
Write-Host "  frames:    $frames"
Write-Host "  timecode:  $timecode"
Write-Host "  audio:     $audio"
Write-Host ""
Write-Host "What to look for:" -ForegroundColor Green
Write-Host "  * the resumed run says 'part 1/2: already done' and renders only the rest"
Write-Host "  * the final clip still has the camera timecode (15:38:35:00 on this footage)"
Write-Host "  * the clip plays through the chunk boundary without a flash or a gap"
Write-Host ""

if (-not $NoOpen) {
    Start-Process (Join-Path $root $out)
}
