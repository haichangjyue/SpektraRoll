@echo off
rem Double-click me to decode an R3D NE frame with your own RED SDK and grade it.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo-red.ps1" %*
echo.
pause
