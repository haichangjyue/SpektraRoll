@echo off
rem Double-click me to see grain/halation/glare/diffusion running on the GPU.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo-gpu-texture.ps1" %*
echo.
pause
