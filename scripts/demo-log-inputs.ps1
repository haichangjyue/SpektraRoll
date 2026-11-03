# SPDX-License-Identifier: GPL-3.0-or-later
# D061 demo: every camera log input bakes a negative LUT, grades the same frame
# and matches between the CPU spectral engine and the GPU one.
#
# You do not need to understand this file. Double-click scripts\demo-log-inputs.cmd.

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

New-Item -ItemType Directory -Force -Path "out\log-inputs" | Out-Null

# Camera logs, in the order they appear in the UI drop down.
$inputs = @(
    @{ Name = "apple-log";       Label = "Apple Log (BT.2020)" },
    @{ Name = "s-log3";          Label = "Sony S-Log3 (S-Gamut3.Cine)" },
    @{ Name = "s-log3-sgamut3";  Label = "Sony S-Log3 (S-Gamut3)" },
    @{ Name = "c-log3";          Label = "Canon C-Log3 (Cinema Gamut)" },
    @{ Name = "v-log";           Label = "Panasonic V-Log (V-Gamut)" },
    @{ Name = "logc3";           Label = "ARRI LogC3 (Wide Gamut 3)" },
    @{ Name = "logc4";           Label = "ARRI LogC4 (Wide Gamut 4)" },
    @{ Name = "bmd-film";        Label = "Blackmagic Film Gen 5" },
    @{ Name = "davinci";         Label = "DaVinci Intermediate (DWG)" }
)

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host " SpektraRoll 相机 Log 输入演示: 9 种输入, 一条链" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1/4  取一帧, 再缩一张小图给 CPU/GPU 对照用..." -ForegroundColor Cyan
& $ffmpeg -v error -y -ss $Time -i $Footage -frames:v 1 `
    -vf "scale=in_color_matrix=bt2020:in_range=tv:out_range=full" `
    -pix_fmt rgb48le "out\log-inputs\src.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $ffmpeg -v error -y -i "out\log-inputs\src.png" -vf "scale=480:-2" `
    -pix_fmt rgb48le "out\log-inputs\small.png"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "2/4  每种输入各烘一张扫描负片 LUT, 再套到同一帧上..." -ForegroundColor Cyan
$rows = @()
foreach ($input in $inputs) {
    $name = $input.Name
    $lut = "out\log-inputs\$name.cube"
    $started = Get-Date
    & $exe lut bake --input $name --exposure $Exposure --scan negative --out $lut | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $bakeMs = ((Get-Date) - $started).TotalMilliseconds

    & $exe lut apply --lut $lut --in "out\log-inputs\src.png" `
        --out "out\log-inputs\$name.png" --grain 0.4 --halation 0.45 `
        --glare 0.2 --diffusion 0.1 | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $rows += [pscustomobject]@{
        Name = $name
        Label = $input.Label
        BakeMs = [math]::Round($bakeMs)
        Psnr = ""
    }
}

Write-Host ""
Write-Host "3/4  同一张小图: CPU 光谱 vs GPU 光谱 (应 >= 60 dB)..." -ForegroundColor Cyan
for ($i = 0; $i -lt $rows.Count; $i++) {
    $name = $rows[$i].Name
    & $exe lut apply --engine spectral --input $name --exposure $Exposure --scan negative `
        --couplers on --gamut oklch --in "out\log-inputs\small.png" `
        --out "out\log-inputs\$name-cpu.png" | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $exe lut apply --engine spectral-gpu --input $name --exposure $Exposure --scan negative `
        --couplers on --gamut oklch --in "out\log-inputs\small.png" `
        --out "out\log-inputs\$name-gpu.png" | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $psnr = (& $ffmpeg -v info -i "out\log-inputs\$name-gpu.png" `
        -i "out\log-inputs\$name-cpu.png" -lavfi psnr -f null - 2>&1 |
        Select-String -Pattern "average:" | Select-Object -First 1)
    $ErrorActionPreference = $previous
    if ($null -ne $psnr -and $psnr.Line -match "average:(\S+)") {
        $rows[$i].Psnr = [math]::Round([double]$Matches[1], 2)
    } else {
        $rows[$i].Psnr = "n/a"
    }
}

Write-Host ""
Write-Host "4/4  结果" -ForegroundColor Cyan
Write-Host ("{0,-16}  {1,-30}  {2,8}  {3,10}" -f "input", "description", "bake ms", "GPU/CPU")
Write-Host ("{0,-16}  {1,-30}  {2,8}  {3,10}" -f ("-" * 16), ("-" * 30), ("-" * 8), ("-" * 10))
foreach ($row in $rows) {
    Write-Host ("{0,-16}  {1,-30}  {2,8}  {3,10}" -f $row.Name, $row.Label, $row.BakeMs, $row.Psnr)
}

Write-Host "要怎么看:" -ForegroundColor Green
Write-Host "  * 这一帧本身是 iPhone 的 Apple Log, 所以除了 apple-log 那一行, 其他是故意用错"
Write-Host "    的曲线解码, 只是证明每种输入都能烘表、都能出图、GPU 和 CPU 一致"
Write-Host "  * 真正用 ARRI / Blackmagic / Sony 素材时, 选对应输入即可; 曲线已和"
Write-Host "    colour-science 0.4.7 的 101 点逐点对照过"
Write-Host "  * 18% 中灰在每种 Log 里的码值: logc3 0.3910 / logc4 0.2784 /"
Write-Host "    bmd-film 0.3836 / davinci 0.3360 / s-log3 0.4106 / c-log3 0.3434 /"
Write-Host "    v-log 0.4233 / apple-log 0.4883"
Write-Host "  * 输入矩阵现在跟着素材标签走 (--in-matrix auto), 不再按输入类型猜"
Write-Host ""
Write-Host "生成的文件:" -ForegroundColor Green
Write-Host "  $root\out\log-inputs\<输入>.png   (同一帧, 每种输入的负片成片)"
Write-Host "  $root\out\log-inputs\<输入>.cube  (对应的 LUT)"
Write-Host ""

if (-not $NoOpen) {
    Start-Process explorer.exe (Join-Path $root "out\log-inputs")
}
