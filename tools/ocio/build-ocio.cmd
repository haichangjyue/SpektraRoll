@echo off
rem Build OpenColorIO for MinGW and install it into X:\agent\tools\ocio (D062).
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-ocio.ps1" %*
echo.
pause
