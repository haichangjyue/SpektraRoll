@echo off
rem Double-click me to check that M1 still works.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0verify-m1.ps1" %*
echo.
pause
