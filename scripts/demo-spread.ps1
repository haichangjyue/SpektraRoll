# SPDX-License-Identifier: GPL-3.0-or-later
# D054 demo: physical point spread vs the old box blur.
# Renders the same frame and the same 1 s clip with both models, prints the
# milliseconds per frame and the PSNR between the two stills, and lays the two
# stills side by side so the shape of the glow can be compared at 100%.
#
# You do not need to understand this file. Double-click scripts\demo-spread.cmd.

param(
    [switch]$NoOpen,
    [string]$Footage = "X:\agent\footage\C0113.MP4",
    [double]$Time = 1.5,
    [double]$Exposure = 0.7,
    [double]$Grain = 0.4,
    [double]$Halation = 0.45,
    [double]$Glare = 0.2,
    [double]$Diffusion = 0.1,
    [double]$HalationSpread = 225,
    [double]$GlareSpread = 725,
    [double]$DiffusionSpread = 122
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

$ffmpeg = "J:\ffmpeg\bin\ffmpeg.exe"
if (-not (Test-Path $ffmpeg)) {
    $found = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($null -eq $found) {
        Write-Host "找不到 ffmpeg. 请把 ffmpeg 加进 PATH." -ForegroundColor Red
        exit 1
    }
    $ffmpeg = $found.Source
}

if (-not (Test-Path $Footage)) {
    Write-Host "找不到素材: $Footage" -ForegroundColor Red
    exit 1
}

New-Item -ItemType Directory -Force -Path "out" | Out-Null

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll 空间质感演示: 盒状模糊 vs 物理核" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/5  烘焙 SDR 扫描负片 LUT..." -ForegroundColor Cyan
& $exe lut bake --input s-log3 --exposure $Exposure --scan negative --out "out\spread-lut.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "2/5  取一帧..." -ForegroundColor Cyan
& $ffmpeg -v error -y -ss $Time -i $Footage -frames:v 1 `
    -vf "scale=in_color_matrix=bt709:in_range=auto:out_range=full" `
    -pix_fmt rgb48le "out\spread-frame.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$common = @(
    "--grain", "$Grain", "--halation", "$Halation",
    "--glare", "$Glare", "--diffusion", "$Diffusion"
)

Write-Host ""
Write-Host "3/5  同一帧: 旧盒状模糊 vs 新物理核..." -ForegroundColor Cyan
& $exe lut apply --lut "out\spread-lut.cube" --in "out\spread-frame.png" `
    --out "out\spread-box.png" --spread-model box @common
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe lut apply --lut "out\spread-lut.cube" --in "out\spread-frame.png" `
    --out "out\spread-physical.png" --spread-model physical `
    --halation-spread $HalationSpread --glare-spread $GlareSpread `
    --diffusion-spread $DiffusionSpread @common
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "4/5  两张图并排 (各缩到 1080 宽)..." -ForegroundColor Cyan
& $ffmpeg -v error -y -i "out\spread-box.png" -i "out\spread-physical.png" `
    -filter_complex "[0:v]scale=1080:-1[a];[1:v]scale=1080:-1[b];[a][b]hstack=inputs=2" `
    -frames:v 1 "out\spread-compare.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$previous = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$psnr = (& $ffmpeg -v info -i "out\spread-box.png" -i "out\spread-physical.png" `
    -lavfi psnr -f null - 2>&1 | Select-String -Pattern "average:" | Select-Object -First 1)
$ErrorActionPreference = $previous
if ($null -ne $psnr) {
    Write-Host "  两张图差异: $($psnr.Line.Trim())" -ForegroundColor Green
}

Write-Host ""
Write-Host "5/5  同一段 1 秒视频, 两个模型各渲一次..." -ForegroundColor Cyan
& $exe video --lut "out\spread-lut.cube" --in $Footage --out "out\spread-box.mov" `
    --in-matrix bt709 --duration 1 --codec prores --spread-model box @common --ffmpeg $ffmpeg
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe video --lut "out\spread-lut.cube" --in $Footage --out "out\spread-physical.mov" `
    --in-matrix bt709 --duration 1 --codec prores --spread-model physical `
    --halation-spread $HalationSpread --glare-spread $GlareSpread `
    --diffusion-spread $DiffusionSpread @common --ffmpeg $ffmpeg
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "看图重点 (建议放大到 100% 看):" -ForegroundColor Green
Write-Host "  * 左半 = 旧盒状模糊, 右半 = 新物理核"
Write-Host "  * 高光边缘: 右边应该是一层贴边的暖色芯 + 很淡很宽的外晕"
Write-Host "  * 暗部: 眩光把黑位轻轻抬起, 但不应该发灰发脏"
Write-Host "  * 尺度: 扩散距离按底片微米算, 1080p / 4K 渲染应为同样物理大小"
Write-Host ""
Write-Host "生成的文件:" -ForegroundColor Green
Write-Host "  $root\out\spread-compare.png     (左旧 / 右新)"
Write-Host "  $root\out\spread-box.png         (旧盒状模糊)"
Write-Host "  $root\out\spread-physical.png    (新物理核)"
Write-Host "  $root\out\spread-box.mov         (1 秒, 旧)"
Write-Host "  $root\out\spread-physical.mov    (1 秒, 新)"
Write-Host ""

if (-not $NoOpen) {
    Start-Process (Join-Path $root "out\spread-compare.png")
}
