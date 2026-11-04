@echo off
rem Double-click me to verify the D072 engine contract (capabilities / catalog / preview / progress).
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0verify-d072.ps1" %*
echo.
pause
