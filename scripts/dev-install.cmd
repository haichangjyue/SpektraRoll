@echo off
rem Double-click me once to install the toolchain (no admin rights needed).
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\dev-install.ps1" %*
echo.
pause
