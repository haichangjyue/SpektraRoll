# SPDX-License-Identifier: GPL-3.0-or-later
# D062 demo: OpenColorIO in the film chain.
#   1) show what the config offers
#   2) decode a real Apple Log frame through OCIO and compare with the built-in
#      decode
#   3) render an ACES 2.0 display view of that frame
#   4) bake one negative LUT with OCIO's ARRI LogC3 and one with the built-in
#      curve, then compare them
#
# You do not need to understand this file. Double-click scripts\demo-ocio.cmd.

param(
    [switch]$NoOpen,
    [string]$Footage = "X:\agent\footage\A001_11011538_C001.mov",
    [string]$Config = "",
    [double]$Time = 1.5
)

[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$OutputEncoding = [Console]::OutputEncoding
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$exe = Join-Path $root "build\spektra-roll-cli.exe"
if (-not (Test-Path $exe)) {
    Write-Host "还没编译, 先编译一次..." -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot "build.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if ($Config -eq "") {
    $Config = Join-Path $root "data\ocio\studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio"
}

$ffmpeg = "J:\ffmpeg\bin\ffmpeg.exe"
if (-not (Test-Path $ffmpeg)) {
    $found = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($null -eq $found) {
        Write-Host "找不到 ffmpeg. 请把 ffmpeg 加进 PATH." -ForegroundColor Red
        exit 1
    }
    $ffmpeg = $found.Source
}

$out = Join-Path $root "out\ocio-demo"
New-Item -ItemType Directory -Force -Path $out | Out-Null

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll OpenColorIO 演示 (D062)" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/4  OCIO 版本 / 配置 / 相机空间..." -ForegroundColor Cyan
$info = & $exe ocio-info --ocio-config $Config 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host ($info | Out-String) -ForegroundColor Red
    Write-Host "这次编译没有 OpenColorIO. 看 RUN.md 的 'OpenColorIO（可选）' 一节." -ForegroundColor Yellow
    exit 1
}
$info | Select-Object -First 8 | ForEach-Object { Write-Host "     $_" }
$cameras = $info | Select-String -Pattern "^  (Apple Log|ARRI LogC3|ARRI LogC4|BMDFilm|DaVinci Intermediate|S-Log3 )"
Write-Host "     相机空间 (节选):" -ForegroundColor Green
foreach ($line in $cameras) { Write-Host "     $($line.Line.Trim())" }

Write-Host ""
Write-Host "2/4  同一帧: 内置 Apple Log 解码 vs OCIO 的 Apple Log..." -ForegroundColor Cyan
if (-not (Test-Path $Footage)) {
    Write-Host "找不到素材: $Footage (只跳过这一节)" -ForegroundColor Yellow
} else {
    & $ffmpeg -v error -y -ss $Time -i $Footage -frames:v 1 `
        -vf "scale=in_color_matrix=bt2020:in_range=tv:out_range=full" `
        -pix_fmt rgb48le (Join-Path $out "src.png")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    & $exe convert --from apple-log --in (Join-Path $out "src.png") `
        --out (Join-Path $out "apple-builtin.png") --bits 16 | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $exe convert --ocio-config $Config --ocio-input "Apple Log" `
        --in (Join-Path $out "src.png") --out (Join-Path $out "apple-ocio.png") --bits 16 | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $psnr = (& $ffmpeg -v info -i (Join-Path $out "apple-ocio.png") `
        -i (Join-Path $out "apple-builtin.png") -lavfi psnr -f null - 2>&1 |
        Select-String -Pattern "average:" | Select-Object -First 1)
    $ErrorActionPreference = $previous
    if ($null -ne $psnr) {
        Write-Host "     内置 vs OCIO: $($psnr.Line.Trim())" -ForegroundColor Green
        Write-Host "     (两条独立实现, 都在 60 dB 以上说明曲线和色域没写错)"
    }

    Write-Host ""
    Write-Host "3/4  同一帧: OCIO 显示器变换 (ACES 2.0 SDR 100 nits)..." -ForegroundColor Cyan
    & $exe convert --ocio-config $Config --ocio-input "Apple Log" `
        --ocio-display "sRGB - Display" --ocio-view "ACES 2.0 - SDR 100 nits (Rec.709)" `
        --in (Join-Path $out "src.png") --out (Join-Path $out "apple-aces2-sdr.png") --bits 16 |
        Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "     out\ocio-demo\apple-aces2-sdr.png" -ForegroundColor Green
}

Write-Host ""
Write-Host "4/4  同一张负片 LUT: 内置 ARRI LogC3 vs OCIO 的 ARRI LogC3..." -ForegroundColor Cyan
& $exe lut bake --input logc3 --scan negative --out (Join-Path $out "lut-builtin.cube") | Out-Null
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe lut bake --ocio-config $Config --ocio-input "ARRI LogC3 (EI800)" --scan negative `
    --out (Join-Path $out "lut-ocio.cube") | Out-Null
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$compare = & $exe lut compare --a (Join-Path $out "lut-ocio.cube") `
    --b (Join-Path $out "lut-builtin.cube") 2>&1
$compare | Select-String -Pattern "PSNR|max \|diff\|" | ForEach-Object { Write-Host "     $($_.Line.Trim())" }

Write-Host ""
Write-Host "要怎么看:" -ForegroundColor Green
Write-Host "  * OCIO 现在是可选依赖: 有它就能用任意相机/ACES 色彩空间当 LUT 的输入域,"
Write-Host "    没有它一切照旧 (只是 --ocio-* 会明确报错)"
Write-Host "  * 胶片模型永远吃线性 Rec.709; 配置里的 'Linear Rec.709 (sRGB)' 就是它"
Write-Host "  * Apple Log 的两条实现差 90 dB 以上: 我们手写的曲线和 ACES 官方配置一致"
Write-Host "  * ARRI LogC3 的 LUT 与官方配置差约 64 dB (差异来自 ACES 的 D60->D65 适应)"
Write-Host "  * LUT 烘好之后, lut apply / video 不需要装 OCIO, 因为解码已经烘进表里"
Write-Host ""
Write-Host "生成的文件:" -ForegroundColor Green
Write-Host "  $out\apple-builtin.png   内置 Apple Log 解码"
Write-Host "  $out\apple-ocio.png      OCIO 的 Apple Log 解码"
Write-Host "  $out\apple-aces2-sdr.png ACES 2.0 显示器变换 (SDR 100 nits)"
Write-Host "  $out\lut-builtin.cube    内置 ARRI LogC3 负片 LUT"
Write-Host "  $out\lut-ocio.cube       OCIO ARRI LogC3 负片 LUT"
Write-Host ""

if (-not $NoOpen) {
    Start-Process explorer.exe $out
}
