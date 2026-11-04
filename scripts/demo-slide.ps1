# SPDX-License-Identifier: GPL-3.0-or-later
# D067 demo: reversal (slide) films scanned directly.
#   1) take one frame from the Sony FX30 S-Log3 clip
#   2) bake Ektachrome 100 / Velvia 100 / Provia 100F / Kodachrome 64
#   3) apply all four to the same frame and open them
#
# A reversal film is already a positive image after development, so there is
# no paper and no inversion: `--scan slide` scans the developed dyes directly
# and balances the scan between clear base (white) and maximum dye (black).
#
# You do not need to understand this file. Double-click scripts\demo-slide.cmd.

param(
    [switch]$NoOpen,
    [string]$Footage = "X:\agent\footage\C0113.MP4",
    [double]$Time = 1.5,
    [double]$Exposure = 0.7
)

[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$OutputEncoding = [Console]::OutputEncoding
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$exe = Join-Path $root "build\spektra-roll-cli.exe"
if (-not (Test-Path $exe)) {
    Write-Host "还没有 CLI, 先编译一次..." -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot "build.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$ffmpeg = "J:\ffmpeg\bin\ffmpeg.exe"
if (-not (Test-Path $ffmpeg)) {
    $found = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($null -eq $found) {
        Write-Host "找不到 ffmpeg. 请把 ffmpeg.exe 放到 J:\ffmpeg\bin 或者加进 PATH." -ForegroundColor Red
        exit 1
    }
    $ffmpeg = $found.Source
}

New-Item -ItemType Directory -Force -Path "out" | Out-Null

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll 反转片直扫 (D067)" -ForegroundColor Cyan
Write-Host " Ektachrome / Velvia / Provia / Kodachrome" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/5  从素材取一帧 (Sony FX30 S-Log3, 第 $Time 秒, 16-bit)..." -ForegroundColor Cyan
if (Test-Path $Footage) {
    # in_range=auto follows the clip's color_range tag (this FX30 file is full range).
    & $ffmpeg -v error -y -ss $Time -i $Footage -frames:v 1 `
        -vf "scale=in_color_matrix=bt709:in_range=auto:out_range=full,format=rgb48le" `
        "out\slide-frame.png"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host "找不到素材: $Footage (用内置测试图代替)" -ForegroundColor Yellow
    & $exe gen --out "out\slide-frame.png" --bits 16 | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$stocks = @(
    @{ Id = "kodak_ektachrome_100"; Short = "ektachrome" },
    @{ Id = "fujifilm_velvia_100"; Short = "velvia" },
    @{ Id = "fujifilm_provia_100f"; Short = "provia" },
    @{ Id = "kodak_kodachrome_64"; Short = "kodachrome" }
)

$index = 2
foreach ($stock in $stocks) {
    $name = "out\slide-$($stock.Short)"
    Write-Host ""
    Write-Host "$index/5  $($stock.Id) -> 直扫反转片..." -ForegroundColor Cyan
    & $exe lut bake --film "data\profiles\$($stock.Id).json" --scan slide `
        --input s-log3 --exposure $Exposure --out "$name.cube"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $exe lut apply --lut "$name.cube" --in "out\slide-frame.png" --out "$name.png"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $index++
}

Write-Host ""
Write-Host "要怎么看:" -ForegroundColor Green
Write-Host "  * 反转片直扫不经过相纸: 显影后的染料本身就是正像, 不需要倒相"
Write-Host "  * Ektachrome 中性、Velvia 高饱和、Provia 均衡、Kodachrome 偏暖/复古"
Write-Host "  * 扫描按'清澈片基 -> 最深染料'做黑白平衡, 中灰保持中性"
Write-Host "  * 想更平/更硬就调 --contrast (反转片默认 1.0)"
Write-Host ""
Write-Host "生成的文件:" -ForegroundColor Green
Write-Host "  $root\out\slide-frame.png        (S-Log3 原帧)"
Write-Host "  $root\out\slide-ektachrome.png   (Ektachrome 100)"
Write-Host "  $root\out\slide-velvia.png       (Velvia 100)"
Write-Host "  $root\out\slide-provia.png       (Provia 100F)"
Write-Host "  $root\out\slide-kodachrome.png   (Kodachrome 64)"
Write-Host ""

if (-not $NoOpen) {
    Start-Process (Join-Path $root "out\slide-ektachrome.png")
    Start-Process (Join-Path $root "out\slide-velvia.png")
    Start-Process (Join-Path $root "out\slide-provia.png")
    Start-Process (Join-Path $root "out\slide-kodachrome.png")
}
