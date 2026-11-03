@echo off
rem Double-click me to see OpenColorIO decoding a real frame and baking a LUT.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo-ocio.ps1" %*
echo.
pause
