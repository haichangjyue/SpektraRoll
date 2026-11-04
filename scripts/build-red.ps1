# SPDX-License-Identifier: GPL-3.0-or-later
# D068: build a LOCAL, optional RED R3D reader from a user-supplied SDK.
#
# The RED SDK is not bundled with SpektraRoll and is not part of any official
# binary. You must obtain it from RED, accept their EULA, and use the result
# for yourself. See docs/red-sdk.md.
#
# Examples:
#   .\scripts\build-red.ps1 -Check -SdkRoot D:\RED_SDK
#   $env:SR_REDSDK_ROOT = "D:\RED_SDK"; .\scripts\build-red.ps1
#
# You do not need to understand this file.

param(
    [string]$SdkRoot = $env:SR_REDSDK_ROOT,
    [switch]$Check,
    [switch]$Clean,
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

$includeCandidates = @("$SdkRoot\include", "$SdkRoot\include\red")
$libCandidates = @(
    "$SdkRoot\lib", "$SdkRoot\lib\win64", "$SdkRoot\lib\macos",
    "$SdkRoot\bin"
)

function Find-First([string[]]$paths, [string[]]$names) {
    foreach ($dir in $paths) {
        foreach ($name in $names) {
            $p = Join-Path $dir $name
            if (Test-Path $p) { return $p }
        }
    }
    return $null
}

$header = Find-First $includeCandidates @("R3DSDK.h")
$library = Find-First $libCandidates @("R3DSDK.lib", "libR3DSDK.dylib", "libR3DSDK.so")
$runtime = Find-First $libCandidates @("R3DSDK.dll", "libR3DSDK.dylib", "libR3DSDK.so")

Write-Host ""
Write-Host "RED SDK check" -ForegroundColor Cyan
Write-Host "  root:    $SdkRoot"
Write-Host "  header:  $(if ($header) { $header } else { 'NOT FOUND' })"
Write-Host "  library: $(if ($library) { $library } else { 'NOT FOUND' })"
Write-Host "  runtime: $(if ($runtime) { $runtime } else { 'NOT FOUND' })"
Write-Host ""

if (-not $header -or -not $library) {
    Write-Host "SDK 不完整：需要 R3DSDK.h 和 R3DSDK 库。" -ForegroundColor Red
    Write-Host "如果你的 SDK 目录结构不同，看 docs/red-sdk.md。" -ForegroundColor Yellow
    exit 1
}

if ($Check) {
    Write-Host "检查通过（只检查文件，不启动 SDK）。" -ForegroundColor Green
    exit 0
}

function Find-Tool([string]$name, [string[]]$fallbacks) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($p in $fallbacks) { if (Test-Path $p) { return $p } }
    return $null
}

$cmake = Find-Tool "cmake" @("C:\Program Files\CMake\bin\cmake.exe", "$toolRoot\cmake\bin\cmake.exe")
$ninja = Find-Tool "ninja" @("$toolRoot\ninja\ninja.exe")
$gxx = Find-Tool "g++" @("$toolRoot\mingw64\bin\g++.exe")
if (-not $cmake -or -not $ninja -or -not $gxx) {
    Write-Host "找不到 cmake / ninja / g++，先运行 scripts\build.ps1。" -ForegroundColor Red
    exit 1
}

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item -Recurse -Force $BuildDir
}

Write-Host "配置本地 RED 构建（官方包不包含这个模块）..." -ForegroundColor Cyan
& $cmake -S . -B $BuildDir -G Ninja `
    "-DCMAKE_MAKE_PROGRAM=$ninja" `
    "-DCMAKE_CXX_COMPILER=$gxx" `
    -DCMAKE_BUILD_TYPE=Release `
    -DSR_WITH_REDSDK=ON `
    "-DSR_REDSDK_ROOT=$SdkRoot"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "编译..." -ForegroundColor Cyan
& $cmake --build $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = Join-Path $root "$BuildDir\spektra-roll-cli.exe"
if (-not (Test-Path $exe)) { $exe = Join-Path $root "$BuildDir\spektra-roll-cli" }

Write-Host ""
Write-Host "完成: $exe" -ForegroundColor Green
Write-Host "自检:" -ForegroundColor Green
Write-Host "  $exe red-info --json"
Write-Host ""
Write-Host "提醒:" -ForegroundColor Yellow
Write-Host "  * 这是本地自用构建；不要把它当作官方包分发。"
Write-Host "  * 如果 SDK 动态库不在 exe 旁边，设置 SPEKTRA_RED_SDK 指向 SDK 的 bin/lib 目录。"
Write-Host "  * 当前骨架只读元数据；渲染 R3D 还没接入，先用 REDCINE-X 导出 ProRes 4444。"
Write-Host ""
