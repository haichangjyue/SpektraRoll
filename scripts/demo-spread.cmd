@echo off
rem Double-click me to compare the old box blur with the physical point spread.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo-spread.ps1" %*
echo.
pause
