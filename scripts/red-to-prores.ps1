# SPDX-License-Identifier: GPL-3.0-or-later
# D069 bridge: R3D NE -> Log3G10/RWG PNG sequence -> film LUT -> ProRes.
#
# This is not `video --in clip.R3D` yet; it uses red-decode + lut apply --in-dir
# and then ffmpeg. Audio is not carried (the SDK sample has no audio).
#
# You do not need to understand this file.

param(
    [string]$Footage = "",
    [string]$SdkRoot = $env:SR_REDSDK_ROOT,
    [int]$Start = 0,
    [int]$Frames = 12,
    [string]$Scale = "half",
    [double]$Exposure = 0.7,
    [string]$Out = "out\red-clip.mov"
)

[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$OutputEncoding = [Console]::OutputEncoding
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not $Footage) {
    $known = "X:\agent\footage\ZR_RED_Color_Science_Sunflower.R3D"
    if (Test-Path $known) { $Footage = $known }
}
if (-not $Footage -or -not (Test-Path $Footage)) {
    Write-Host "找不到 R3D 样片。用 -Footage <clip.R3D> 指定。" -ForegroundColor Red
    exit 1
}
if (-not $SdkRoot) {
    $knownSdk = "X:\agent\footage\R3DSDKv9_2_1"
    if (Test-Path $knownSdk) { $SdkRoot = $knownSdk }
}

$exe = Join-Path $root "build-red\spektra-roll-cli.exe"
if (-not (Test-Path $exe)) {
    & (Join-Path $PSScriptRoot "build-red.ps1") -SdkRoot $SdkRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$ffmpeg = "J:\ffmpeg\bin\ffmpeg.exe"
if (-not (Test-Path $ffmpeg)) {
    $found = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($null -eq $found) {
        Write-Host "找不到 ffmpeg。" -ForegroundColor Red
        exit 1
    }
    $ffmpeg = $found.Source
}

New-Item -ItemType Directory -Force -Path "out" | Out-Null
$ocio = "data\ocio\studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio"

Write-Host "读取 R3D 元数据..." -ForegroundColor Cyan
$info = & $exe red-info --in $Footage --json | ConvertFrom-Json
if (-not $info.available) { Write-Host $info.message -ForegroundColor Red; exit 1 }
$fps = [double]$info.clip.fps
$tc = [string]$info.clip.timecode
Write-Host "  $($info.clip.width)x$($info.clip.height) · $fps fps · $($info.clip.frames) 帧 · TC $tc"

$seq = "out\red-seq"
$graded = "out\red-seq-graded"
if (Test-Path $seq) { Remove-Item -Recurse -Force $seq }
if (Test-Path $graded) { Remove-Item -Recurse -Force $graded }

Write-Host "1/4  解码 $Frames 帧 ($Scale, Log3G10/RWG)..." -ForegroundColor Cyan
& $exe red-decode --in $Footage --out-dir $seq --start $Start --frames $Frames `
    --scale $Scale --format log3g10 --out-ext png
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "2/4  烘 Vision3 250D + 2383 LUT..." -ForegroundColor Cyan
& $exe lut bake --ocio-config $ocio --ocio-input "Log3G10 REDWideGamutRGB" `
    --film data\profiles\kodak_vision3_250d.json --paper data\profiles\kodak_2383.json `
    --scan print --exposure $Exposure --out "out\red-vision3-2383.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "3/4  套 LUT..." -ForegroundColor Cyan
& $exe lut apply --lut "out\red-vision3-2383.cube" --in-dir $seq `
    --out-dir $graded --out-ext png
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "4/4  ffmpeg 编码 ProRes..." -ForegroundColor Cyan
$first = Get-ChildItem $graded -Filter *.png | Sort-Object Name | Select-Object -First 1
if (-not $first) { Write-Host "没有输出帧" -ForegroundColor Red; exit 1 }
$pattern = Join-Path $graded (($first.BaseName -replace '\d+$', '%04d') + ".png")

$args = @(
    "-v", "error", "-y",
    "-framerate", "$fps",
    "-start_number", "$Start",
    "-i", $pattern,
    "-c:v", "prores_ks", "-profile:v", "3", "-pix_fmt", "yuv422p10le"
)
if ($tc) { $args += @("-timecode", $tc) }
$args += $Out
& $ffmpeg @args
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "完成: $Out" -ForegroundColor Green
Write-Host "  $Frames 帧 · $Scale · ProRes 422 HQ · 无音频（R3D 音频封装未接）"
Write-Host ""
