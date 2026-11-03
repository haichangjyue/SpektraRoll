@echo off
rem Double-click me to bake one negative LUT per camera log and compare CPU vs GPU.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo-log-inputs.ps1" %*
echo.
pause
