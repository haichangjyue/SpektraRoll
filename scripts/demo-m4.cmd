@echo off
rem Double-click me to see the M4 segmented render + resume demo.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo-m4.ps1" %*
echo.
pause
