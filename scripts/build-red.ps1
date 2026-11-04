# SPDX-License-Identifier: GPL-3.0-or-later
# D069: build the optional spektra-red plugin from a user-supplied RED SDK.
#
# The RED SDK is not bundled with SpektraRoll and is not part of any official
# binary. You must obtain it from RED, accept their EULA, and use the result
# for yourself. The plugin is built with MSVC because the Windows RED SDK
# libraries are MSVC-only; the GPL CLI loads it at runtime.
#
# Examples:
#   .\scripts\build-red.ps1 -Check -SdkRoot D:\RED_SDK
#   $env:SR_REDSDK_ROOT = "D:\RED_SDK"; .\scripts\build-red.ps1
#
# You do not need to understand this file.

param(
    [string]$SdkRoot = $env:SR_REDSDK_ROOT,
    [switch]$Check,
    [string]$BuildDir = "build-red"
)

[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$OutputEncoding = [Console]::OutputEncoding
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$toolRoot = "X:\agent\tools"
foreach ($dir in @("$toolRoot\mingw64\bin", "$toolRoot\ninja")) {
    if (Test-Path $dir) { $env:PATH = "$dir;$env:PATH" }
}

if (-not $SdkRoot) {
    Write-Host "没有指定 RED SDK 路径。" -ForegroundColor Red
    Write-Host "  .\scripts\build-red.ps1 -SdkRoot D:\RED_SDK" -ForegroundColor Yellow
    Write-Host "或者先设置环境变量 SR_REDSDK_ROOT。" -ForegroundColor Yellow
    exit 1
}
if (-not (Test-Path $SdkRoot)) {
    Write-Host "找不到 SDK 目录: $SdkRoot" -ForegroundColor Red
    exit 1
}

function Find-First([string[]]$paths, [string[]]$names) {
    foreach ($dir in $paths) {
        foreach ($name in $names) {
            $p = Join-Path $dir $name
            if (Test-Path $p) { return $p }
        }
    }
    return $null
}

$header = Find-First @("$SdkRoot\Include", "$SdkRoot\include", "$SdkRoot\Include\red") @("R3DSDK.h")
$library = Find-First @("$SdkRoot\Lib\win64", "$SdkRoot\lib\win64") @("R3DSDK-2017MD.lib")
$runtime = Find-First @("$SdkRoot\Redistributable\win") @("REDR3D-x64.dll")
$vcvars = Find-First @(
    "$toolRoot\vsbuild\VC\Auxiliary\Build",
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build"
) @("vcvars64.bat")

Write-Host ""
Write-Host "RED SDK / plugin check" -ForegroundColor Cyan
Write-Host "  root:    $SdkRoot"
Write-Host "  header:  $(if ($header) { $header } else { 'NOT FOUND' })"
Write-Host "  library: $(if ($library) { $library } else { 'NOT FOUND (need R3DSDK-2017MD.lib)' })"
Write-Host "  runtime: $(if ($runtime) { $runtime } else { 'NOT FOUND (need REDR3D-x64.dll)' })"
Write-Host "  MSVC:    $(if ($vcvars) { $vcvars } else { 'NOT FOUND (install VS Build Tools)' })"
Write-Host ""

if (-not $header -or -not $library -or -not $runtime -or -not $vcvars) {
    Write-Host "SDK 或 MSVC 不完整。" -ForegroundColor Red
    Write-Host "  * 需要 Include\R3DSDK.h、Lib\win64\R3DSDK-2017MD.lib、Redistributable\win\REDR3D-x64.dll" -ForegroundColor Yellow
    Write-Host "  * 需要 Visual Studio Build Tools 2022（C++ 工作负载）" -ForegroundColor Yellow
    Write-Host "  * 细节见 docs/red-sdk.md" -ForegroundColor Yellow
    exit 1
}

if ($Check) {
    Write-Host "检查通过（只检查文件，不启动 SDK）。" -ForegroundColor Green
    exit 0
}

$cli = Join-Path $root "build\spektra-roll-cli.exe"
if (-not (Test-Path $cli)) {
    Write-Host "先编译普通 CLI..." -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot "build.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
if (-not (Test-Path $cli)) {
    Write-Host "找不到 build\spektra-roll-cli.exe；先运行 scripts\build.ps1。" -ForegroundColor Red
    exit 1
}

$outDir = Join-Path $root $BuildDir
$objDir = Join-Path $outDir "obj"
New-Item -ItemType Directory -Force -Path $objDir | Out-Null

$plugin = Join-Path $outDir "spektra-red.dll"
$pluginSource = Join-Path $root "src\red-plugin\red_plugin_msvc.cpp"
$apiInclude = Join-Path $root "src\red-plugin"
$libDir = Split-Path -Parent $library
$libName = Split-Path -Leaf $library

$batch = @"
@echo off
call "$vcvars" >nul
cd /d "$root"
cl /nologo /EHsc /MD /O2 /LD /I "$(Split-Path -Parent $header)" /I "$apiInclude" "$pluginSource" /Fo"$objDir\\" /Fd"$objDir\spektra-red.pdb" /link /LIBPATH:"$libDir" $libName /IMPLIB:"$objDir\spektra-red.lib" /OUT:"$plugin"
"@
$batchPath = Join-Path $outDir "build-plugin.bat"
Set-Content -Path $batchPath -Value $batch -Encoding ASCII

Write-Host "编译 spektra-red.dll（仅本地使用）..." -ForegroundColor Cyan
& cmd.exe /c $batchPath
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $plugin)) {
    Write-Host "插件编译失败，看上面的 cl/link 输出。" -ForegroundColor Red
    exit 1
}

Write-Host "复制 CLI 与 RED 运行库到 $BuildDir ..." -ForegroundColor Cyan
Copy-Item $cli (Join-Path $outDir "spektra-roll-cli.exe") -Force
Copy-Item (Join-Path (Split-Path -Parent $runtime) "*.dll") $outDir -Force

$exe = Join-Path $outDir "spektra-roll-cli.exe"
Write-Host ""
Write-Host "完成: $exe" -ForegroundColor Green
Write-Host "自检:" -ForegroundColor Green
Write-Host "  $exe red-info --json"
Write-Host "  $exe red-decode --in <clip.R3D> --frame 0 --scale half --format srgb --out frame.png"
Write-Host ""
Write-Host "要怎么看:" -ForegroundColor Green
Write-Host "  scripts\demo-red.cmd"
Write-Host ""
Write-Host "提醒:" -ForegroundColor Yellow
Write-Host "  * 这是本地自用构建；不要把它当作官方包分发。"
Write-Host "  * RED SDK 的头文件/静态库没有复制进仓库；只有 Redistributable 的 DLL 在 build-red（git 忽略）。"
Write-Host "  * 换 SDK 或更新 CLI 后重新跑一次本脚本。"
Write-Host ""
