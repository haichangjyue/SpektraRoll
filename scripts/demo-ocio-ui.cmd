@echo off
rem Double-click me to see the Qt UI's OCIO input: list spaces, bake a LUT,
rem compare it with the CLI, and render one frame.
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo-ocio-ui.ps1" %*
echo.
pause
