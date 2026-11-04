@echo off
rem Double-click me to try the reversal (slide) films: Ektachrome / Velvia / Provia / Kodachrome.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo-slide.ps1" %*
echo.
pause
