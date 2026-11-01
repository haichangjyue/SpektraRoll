@echo off
rem Double-click me to build and launch the Qt 6 desktop UI.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-ui.ps1" %*
echo.
pause
