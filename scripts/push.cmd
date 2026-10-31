@echo off
rem First push to GitHub. Double-click this once.
rem A browser window will open: sign in as haichangjyue and approve.

cd /d "%~dp0.."

echo ============================================
echo  SpektraRoll - first push to GitHub
echo ============================================
echo.
echo Step 1: GitHub sign-in (a browser window opens)
echo.

git credential-manager github login
if errorlevel 1 (
    echo.
    echo Sign-in failed or was cancelled. Nothing was pushed.
    pause
    exit /b 1
)

echo.
echo Step 2: pushing main branch...
echo.

git push -u origin main
if errorlevel 1 (
    echo.
    echo Push failed. Copy the message above and send it to the team.
    pause
    exit /b 1
)

echo.
echo Done. Open https://github.com/haichangjyue/SpektraRoll
echo.
pause
