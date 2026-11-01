@echo off
rem Double-click me to see the M3 grain + halation demo.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo-m3.ps1" %*
echo.
pause
