# SPDX-License-Identifier: GPL-3.0-or-later
# D069 demo: R3D NE -> REDWideGamutRGB/Log3G10 -> film simulation.
#
# This uses the locally built spektra-red plugin and your own RED SDK. Nothing
# here is bundled with SpektraRoll or an official binary.
#
# Steps:
#   1) decode one R3D NE frame twice: viewable sRGB and Log3G10/RWG
#   2) bake a Vision3 250D + 2383 LUT whose input domain is Log3G10/RWG
#   3) apply it to the log frame and open both images
#
# You do not need to understand this file. Double-click scripts\demo-red.cmd.

param(
    [switch]$NoOpen,
    [string]$Footage = "",
    [string]$SdkRoot = $env:SR_REDSDK_ROOT,
    [long]$Frame = 0,
    [string]$Scale = "half",
    [double]$Exposure = 0.7
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
if (-not $SdkRoot -or -not (Test-Path $SdkRoot)) {
    Write-Host "找不到 RED SDK。用 -SdkRoot <path> 指定，或设置 SR_REDSDK_ROOT。" -ForegroundColor Red
    exit 1
}

$exe = Join-Path $root "build-red\spektra-roll-cli.exe"
if (-not (Test-Path $exe)) {
    Write-Host "还没有 RED 插件构建，先编译一次..." -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot "build-red.ps1") -SdkRoot $SdkRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

New-Item -ItemType Directory -Force -Path "out" | Out-Null
$ocio = "data\ocio\studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio"

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll R3D NE (D069)" -ForegroundColor Cyan
Write-Host " REDWideGamutRGB + Log3G10 -> Vision3 250D + 2383" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/4  解码第 $Frame 帧（可看的 sRGB）..." -ForegroundColor Cyan
& $exe red-decode --in $Footage --frame $Frame --scale $Scale --format srgb `
    --out "out\red-original.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "2/4  解码同一帧为 REDWideGamutRGB + Log3G10（LUT 的输入域）..." -ForegroundColor Cyan
& $exe red-decode --in $Footage --frame $Frame --scale $Scale --format log3g10 `
    --out "out\red-log3g10.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "3/4  烘 Vision3 250D + 2383 的 Log3G10/RWG LUT..." -ForegroundColor Cyan
& $exe lut bake --ocio-config $ocio --ocio-input "Log3G10 REDWideGamutRGB" `
    --film data\profiles\kodak_vision3_250d.json --paper data\profiles\kodak_2383.json `
    --scan print --exposure $Exposure --out "out\red-vision3-2383.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "4/4  把 LUT 套到 R3D 帧上..." -ForegroundColor Cyan
& $exe lut apply --lut "out\red-vision3-2383.cube" --in "out\red-log3g10.png" `
    --out "out\red-graded.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "要怎么看:" -ForegroundColor Green
Write-Host "  * red-original.png 是 RED SDK 解码的 sRGB，作为对照"
Write-Host "  * red-log3g10.png 是 Log3G10/RWG，直接看会偏灰，这是正常的中间格式"
Write-Host "  * red-graded.png 是 Vision3 250D + 2383 的胶片/印片模拟"
Write-Host "  * 现在走的是 red-decode + lut apply；video/预览直连是下一阶段"
Write-Host "  * 想直接出一小段 ProRes：scripts\red-to-prores.cmd -Frames 12"
Write-Host "  * 解码是 CPU（6K 全分辨率较慢），--scale half 更快"
Write-Host ""
Write-Host "生成的文件:" -ForegroundColor Green
Write-Host "  $root\out\red-original.png"
Write-Host "  $root\out\red-log3g10.png"
Write-Host "  $root\out\red-graded.png"
Write-Host ""

if (-not $NoOpen) {
    Start-Process (Join-Path $root "out\red-original.png")
    Start-Process (Join-Path $root "out\red-graded.png")
}
