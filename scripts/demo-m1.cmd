@echo off
rem Double-click me to see the M1 Portra 400 demo. No programming needed.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo-m1.ps1" %*
echo.
pause
