# SPDX-License-Identifier: GPL-3.0-or-later
# D055 demo: floating point delivery. Grades one real frame to 32-bit float
# EXR and to 16-bit PNG, reads the EXR back through the same CLI, and shows an
# over-exposed conversion where the PNG clips and the EXR does not.
#
# You do not need to understand this file. Double-click scripts\demo-exr.cmd.

param(
    [switch]$NoOpen,
    [string]$Footage = "X:\agent\footage\A001_11011538_C001.mov",
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

function Size-Mb([string]$path) {
    if (-not (Test-Path $path)) { return "?" }
    return ("{0:N1} MB" -f ((Get-Item $path).Length / 1MB))
}

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll 浮点交付演示: 16-bit PNG vs float EXR" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/5  烘焙 SDR 扫描负片 LUT..." -ForegroundColor Cyan
& $exe lut bake --input apple-log --exposure $Exposure --scan negative --out "out\exr-lut.cube"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "2/5  取一帧..." -ForegroundColor Cyan
& $ffmpeg -v error -y -ss $Time -i $Footage -frames:v 1 `
    -vf "scale=in_color_matrix=bt2020:in_range=tv:out_range=full" `
    -pix_fmt rgb48le "out\exr-frame-src.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "3/5  同一帧: 16-bit PNG / float EXR / half EXR..." -ForegroundColor Cyan
& $exe lut apply --lut "out\exr-lut.cube" --in "out\exr-frame-src.png" `
    --out "out\exr-frame.png" --bits 16 `
    --grain 0.4 --halation 0.45 --glare 0.2 --diffusion 0.1
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe lut apply --lut "out\exr-lut.cube" --in "out\exr-frame-src.png" `
    --out "out\exr-frame.exr" `
    --grain 0.4 --halation 0.45 --glare 0.2 --diffusion 0.1
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe lut apply --lut "out\exr-lut.cube" --in "out\exr-frame-src.png" `
    --out "out\exr-frame-half.exr" --exr-half `
    --grain 0.4 --halation 0.45 --glare 0.2 --diffusion 0.1
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe lut apply --lut "out\exr-lut.cube" --in "out\exr-frame-src.png" `
    --out "out\exr-frame-linear.exr" --exr-linear `
    --grain 0.4 --halation 0.45 --glare 0.2 --diffusion 0.1
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "     16-bit PNG: $(Size-Mb 'out\exr-frame.png')" -ForegroundColor Green
Write-Host "     float EXR : $(Size-Mb 'out\exr-frame.exr')" -ForegroundColor Green
Write-Host "     half EXR  : $(Size-Mb 'out\exr-frame-half.exr')" -ForegroundColor Green
Write-Host "     linear EXR: $(Size-Mb 'out\exr-frame-linear.exr')" -ForegroundColor Green

Write-Host ""
Write-Host "4/5  读回 EXR (同一套 CLI), 和 16-bit PNG 比较..." -ForegroundColor Cyan
& $exe lut gen --preset identity --out "out\exr-identity.cube"
& $exe lut apply --lut "out\exr-identity.cube" --in "out\exr-frame.exr" `
    --out "out\exr-roundtrip.png" --bits 16
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$previous = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$psnr = (& $ffmpeg -v info -i "out\exr-roundtrip.png" -i "out\exr-frame.png" `
    -lavfi psnr -f null - 2>&1 | Select-String -Pattern "average:" | Select-Object -First 1)
$ErrorActionPreference = $previous
if ($null -ne $psnr) {
    Write-Host "     EXR 读回 vs 16-bit PNG: $($psnr.Line.Trim())" -ForegroundColor Green
}

Write-Host ""
Write-Host "5/5  曝光 +3 EV: PNG 会切掉高光, EXR 保留..." -ForegroundColor Cyan
& $exe convert --from apple-log --exposure 3 --in "out\exr-frame-src.png" `
    --out "out\exr-over.png" --bits 16
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $exe convert --from apple-log --exposure 3 --in "out\exr-frame-src.png" `
    --out "out\exr-over.exr"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "     (EXR 那一行会打印范围: 超过 1 的部分没有丢, PNG 那行没有这个提示)" -ForegroundColor Green

Write-Host ""
Write-Host "要怎么看:" -ForegroundColor Green
Write-Host "  * 正常曝光这一帧, EXR 和 16-bit PNG 读回来应该几乎逐位相同"
Write-Host "  * linear EXR 是同一帧的线性光: 0.5 灰会变成约 0.21, 1.0 仍是最白"
Write-Host "  * +3 EV 的 EXR 里约 1/3 的像素大于 1 (最亮约 5.3), 在 Resolve/Nuke 里还能拉回来"
Write-Host "  * 把 linear EXR 拖进 DaVinci Resolve: 它按线性解读, 不要再套 sRGB 输入变换"
Write-Host "  * 普通 EXR 是显示编码值, 进调色软件要设 sRGB (或 PQ) 输入变换"
Write-Host "  * EXR 目前是不压缩 scanline; half 体积约为 float 的一半"
Write-Host ""
Write-Host "生成的文件:" -ForegroundColor Green
Write-Host "  $root\out\exr-frame.exr        (float, 带质感)"
Write-Host "  $root\out\exr-frame-half.exr   (half)"
Write-Host "  $root\out\exr-frame-linear.exr (float, 线性光)"
Write-Host "  $root\out\exr-frame.png        (16-bit 对照)"
Write-Host "  $root\out\exr-roundtrip.png    (EXR 读回后的图)"
Write-Host "  $root\out\exr-over.exr         (+3 EV, 未切高光)"
Write-Host "  $root\out\exr-over.png         (+3 EV, 高光被切)"
Write-Host ""

if (-not $NoOpen) {
    Start-Process (Join-Path $root "out\exr-frame.png")
}
