@echo off
setlocal EnableDelayedExpansion
title ASCII Art OFX Plugin - Installer (Build 10)

:: Request Administrator privileges if needed
net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -Command "Start-Process cmd -ArgumentList '/c \"\"%~dp0Install.bat\"\"' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"
set "OFX_DEST=%CommonProgramFiles%\OFX\Plugins\AsciiArt.ofx.bundle"

echo.
echo ========================================================
echo   ASCII Art OFX Plugin - Build 10
echo ========================================================
echo.
echo   What's New in Build 10:
echo   * Direct Website Button: Click in inspector opens https://therealskb.carrd.co/
echo   * Minimalist Installer & Clean UI
echo   * Multi-Core CPU Hardware Acceleration (OpenMP)
echo   * Update Every N Frames (Frame Hold 1-12) & RAM Cache
echo   * Skip Black Areas & Alpha Cutoff thresholds
echo   * Uniform Character Spacing slider (up to 200px)
echo   * Goliath Encrypted Font (bundled & auto-detected)
echo   * Cyber Lime (#C2FD04 on Black) Color Presets
echo ========================================================
echo.

tasklist /FI "IMAGENAME eq Resolve.exe" 2>nul | find /I /N "Resolve.exe" >nul
if "%ERRORLEVEL%"=="0" (
    echo [FAILED] DaVinci Resolve is currently running.
    echo Please close DaVinci Resolve and run Install.bat again.
    echo.
    pause
    exit /b 1
)

if not exist "AsciiArt.ofx.bundle\Contents\Win64\AsciiArt.ofx" (
    echo [FAILED] Plugin files missing. Please extract the ZIP folder before running.
    echo.
    pause
    exit /b 1
)

if not exist "%CommonProgramFiles%\OFX\Plugins" (
    mkdir "%CommonProgramFiles%\OFX\Plugins" 2>nul
)

xcopy /E /I /Y "AsciiArt.ofx.bundle" "%OFX_DEST%\" >nul 2>&1

if %errorLevel% neq 0 (
    echo [FAILED] Could not copy files to %OFX_DEST%. Please ensure DaVinci Resolve is closed.
    echo.
    pause
    exit /b 1
)

echo [SUCCESS] ASCII Art OFX Plugin (Build 10) installed successfully!
echo.
pause
