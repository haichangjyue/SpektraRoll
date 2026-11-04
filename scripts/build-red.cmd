@echo off
rem D068: build a local RED R3D reader from your own SDK. The SDK is NOT bundled.
rem Example: scripts\build-red.cmd -Check -SdkRoot D:\RED_SDK
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-red.ps1" %*
echo.
pause
