@echo off
rem Double-click me to try the cinema stocks: Vision3 500T + 2383 print film.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo-cine.ps1" %*
echo.
pause
