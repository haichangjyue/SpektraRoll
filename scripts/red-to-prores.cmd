@echo off
rem Double-click me to render a short R3D NE clip through the film LUT to ProRes.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0red-to-prores.ps1" %*
echo.
pause
