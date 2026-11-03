@echo off
rem Double-click me to see the 16-bit PNG vs float EXR comparison.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo-exr.ps1" %*
echo.
pause
