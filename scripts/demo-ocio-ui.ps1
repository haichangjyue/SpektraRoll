# SPDX-License-Identifier: GPL-3.0-or-later
# D063 demo: OpenColorIO in the Qt UI.
#   1) the UI reads the bundled ACES 2.0 config and lists every colour space
#   2) the Bake button path bakes a Portra 400 negative LUT with "Apple Log"
#   3) the same table is baked from the CLI and compared (should be identical)
#   4) a real frame is rendered with the UI-baked LUT
#
# The UI does not link OCIO itself: it asks `spektra-roll-cli ocio-info --json`
# for the names and passes --ocio-input to the same CLI build that renders.
#
# You do not need to understand this file. Double-click scripts\demo-ocio-ui.cmd.

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

$cli = Join-Path $root "build\spektra-roll-cli.exe"
$ui = Join-Path $root "build-ui\spektra-roll-ui.exe"

if (-not (Test-Path $cli)) {
    Write-Host "还没有 CLI, 先编译一次..." -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot "build.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
if (-not (Test-Path $ui)) {
    Write-Host "还没有 Qt 界面, 先编译一次..." -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot "build-ui.ps1") -NoLaunch
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if ($Config -eq "") {
    $Config = Join-Path $root "data\ocio\studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio"
}
if (-not (Test-Path $Config)) {
    Write-Host "找不到 OCIO 配置: $Config" -ForegroundColor Red
    exit 1
}

$out = Join-Path $root "out\ocio-ui"
New-Item -ItemType Directory -Force -Path $out | Out-Null

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll Qt 界面 + OpenColorIO (D063)" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/4  界面读取配置: 列出所有色彩空间并烘一张 Apple Log LUT..." -ForegroundColor Cyan
# A GUI-subsystem exe is not waited for by `&`, so use Start-Process -Wait with
# redirected output (same reason the preview selftest lives in a script).
$selftestOut = Join-Path $out "selftest.out.txt"
$selftestErr = Join-Path $out "selftest.err.txt"
$proc = Start-Process -FilePath $ui -ArgumentList "--ocio-selftest" -WorkingDirectory $root `
    -Wait -PassThru -NoNewWindow -RedirectStandardOutput $selftestOut `
    -RedirectStandardError $selftestErr
$uiExit = $proc.ExitCode
$selftest = @()
foreach ($file in @($selftestOut, $selftestErr)) {
    if (Test-Path $file) {
        $selftest += Get-Content $file | Where-Object { $_.Trim() -ne "" }
    }
}
$uiLut = Join-Path $root "out\ui-ocio-selftest.cube"
$selftest | ForEach-Object { Write-Host "     $_" }
if ($uiExit -ne 0 -or -not (Test-Path $uiLut)) {
    Write-Host "界面自检失败. 如果 CLI 没有 OpenColorIO, 先看 RUN.md 的 OCIO 一节." -ForegroundColor Red
    exit 1
}
Copy-Item -Force $uiLut (Join-Path $out "lut-ui.cube")
$uiLut = Join-Path $out "lut-ui.cube"

Write-Host ""
Write-Host "2/4  同一个参数用 CLI 再烘一张, 两张表逐点比较..." -ForegroundColor Cyan
& $cli lut bake --ocio-config $Config --ocio-input "Apple Log" --scan negative `
    --contrast 1.15 --couplers on --gamut oklch --exposure 0.70 `
    --out (Join-Path $out "lut-cli.cube") | Out-Null
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$compare = & $cli lut compare --a $uiLut --b (Join-Path $out "lut-cli.cube") 2>&1
$compare | Select-String -Pattern "PSNR|max \|diff\|" | ForEach-Object {
    Write-Host "     $($_.Line.Trim())"
}
if ($LASTEXITCODE -ne 0) {
    Write-Host "两张 LUT 不一致: 界面和 CLI 的参数没有对齐." -ForegroundColor Red
    exit 1
}
Write-Host "     (inf = 界面烘出来的表和 CLI 逐位相同)" -ForegroundColor Green

Write-Host ""
Write-Host "3/4  用这张 UI 烘的 LUT 渲染一帧真实素材..." -ForegroundColor Cyan
$src = Join-Path $out "src.png"
$decoded = Join-Path $out "decoded-ocio.png"
$graded = Join-Path $out "graded-ui-lut.png"
if (Test-Path $Footage) {
    $ffmpeg = "J:\ffmpeg\bin\ffmpeg.exe"
    if (-not (Test-Path $ffmpeg)) {
        $found = Get-Command ffmpeg -ErrorAction SilentlyContinue
        if ($null -eq $found) {
            Write-Host "找不到 ffmpeg, 跳过真实素材 (用内置测试图代替)" -ForegroundColor Yellow
            & $cli gen --out $src --bits 16 | Out-Null
        } else {
            $ffmpeg = $found.Source
        }
    }
    if (Test-Path $ffmpeg) {
        & $ffmpeg -v error -y -ss $Time -i $Footage -frames:v 1 `
            -vf "scale=in_color_matrix=bt2020:in_range=tv:out_range=full" `
            -pix_fmt rgb48le $src
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
} else {
    Write-Host "找不到素材: $Footage (用内置测试图代替)" -ForegroundColor Yellow
    & $cli gen --out $src --bits 16 | Out-Null
}
& $cli convert --ocio-config $Config --ocio-input "Apple Log" --in $src --out $decoded --bits 16 |
    Out-Null
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cli lut apply --lut $uiLut --in $src --out $graded --bits 16 | Out-Null
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "     $decoded  (只做 OCIO 解码)" -ForegroundColor Green
Write-Host "     $graded  (UI 烘的 LUT: 解码 + Portra 400 负片)" -ForegroundColor Green

Write-Host ""
Write-Host "4/4  以后在界面里怎么用..." -ForegroundColor Cyan
Write-Host "     左栏 '色彩管理（OCIO）' -> 勾选 -> 配置文件默认就是仓库里的 ACES 2.0"
Write-Host "     点 '读取' -> 在 '色彩空间' 里选 Apple Log / ARRI LogC3 / ACEScg ..."
Write-Host "     '画面类型' 会自动变灰; 烘焙 LUT 时用的是 OCIO 色彩空间"
Write-Host "     (光谱 GPU 不支持 OCIO, 会像 CLI 一样自动退回 CPU 光谱)"

Write-Host ""
Write-Host "要怎么看:" -ForegroundColor Green
Write-Host "  * 界面的下拉不是硬编码: 名字来自 'ocio-info --json', 配置里加空间就会多一项"
Write-Host "  * UI 烘的 LUT 和 CLI 逐位相同, 说明界面没有自己的第二条颜色管线"
Write-Host "  * 解码烘进 LUT 之后, lut apply / video 都不需要装 OCIO"
Write-Host ""

if (-not $NoOpen) {
    Start-Process $graded
    Start-Process explorer.exe $out
}
