@echo off
setlocal EnableDelayedExpansion
title ASCII OFX Plugin - Installer (v1.3 Build 11)

:: Request Administrator privileges if needed
net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -Command "Start-Process cmd -ArgumentList '/c \"\"%~dp0Install.bat\"\"' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"
set "OFX_DIR=%CommonProgramFiles%\OFX\Plugins"
set "OFX_DEST=%OFX_DIR%\AsciiArt.ofx.bundle"

echo.
echo ========================================================
echo   ASCII OFX Plugin - Installer (v1.3 Build 11)
echo ========================================================
echo.
echo   What's New in v1.3 (Build 11):
echo   * Curated Style Presets (Goliath Cyber Lime, Classic Matrix, Cyberpunk, etc.)
echo   * Bundled Consolas Monospace font with automatic cross-platform fallback
echo   * Expanded Color Modes (CGA, ZX Spectrum, Matrix Green, Cyberpunk Neon, etc.)
echo   * Renamed effect to clean "ASCII" inside DaVinci Resolve
echo   * Inspector GitHub Link button: https://github.com/SKBwastaken/ASCIICreatorOFX
echo   * Update Every N Frames (Frame Hold) & zero-allocation RAM Cache
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
    echo [FAILED] Plugin files missing. Please extract the ZIP folder or run build.bat before running.
    echo.
    pause
    exit /b 1
)

if not exist "%OFX_DIR%" (
    mkdir "%OFX_DIR%" 2>nul
)

:: Backup existing installation if present
if exist "%OFX_DEST%" (
    for /f "tokens=*" %%a in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "TIMESTAMP=%%a"
    set "BACKUP_DEST=%OFX_DIR%\AsciiArt.ofx.bundle.backup_%TIMESTAMP%"
    echo [INFO] Existing installation detected. Creating backup...
    ren "%OFX_DEST%" "AsciiArt.ofx.bundle.backup_%TIMESTAMP%" >nul 2>&1
    if errorlevel 1 (
        xcopy /E /I /Y "%OFX_DEST%" "%BACKUP_DEST%\" >nul 2>&1
        rmdir /S /Q "%OFX_DEST%" >nul 2>&1
    )
    echo [INFO] Backup saved to: %BACKUP_DEST%
)

echo [INFO] Installing ASCII OFX Plugin bundle to %OFX_DEST%...
xcopy /E /I /Y "AsciiArt.ofx.bundle" "%OFX_DEST%\" >nul 2>&1

if %errorLevel% neq 0 (
    echo [FAILED] Could not copy files to %OFX_DEST%. Please ensure DaVinci Resolve is closed.
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================================
echo [SUCCESS] ASCII OFX Plugin (v1.3 Build 11) installed successfully!
echo   Effect appears as "ASCII" under Stylize in DaVinci Resolve.
echo ========================================================
echo.
pause
