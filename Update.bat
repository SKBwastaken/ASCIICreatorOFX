@echo off
setlocal EnableDelayedExpansion
title ASCII OFX Plugin - Updater (v1.3 Build 11)

:: Request Administrator privileges if needed
net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -Command "Start-Process cmd -ArgumentList '/c \"\"%~dp0Update.bat\"\"' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"
set "OFX_DIR=%CommonProgramFiles%\OFX\Plugins"
set "OFX_DEST=%OFX_DIR%\AsciiArt.ofx.bundle"

echo.
echo ========================================================
echo   ASCII OFX Plugin - Updater (v1.3 Build 11)
echo ========================================================
echo.

tasklist /FI "IMAGENAME eq Resolve.exe" 2>nul | find /I /N "Resolve.exe" >nul
if "%ERRORLEVEL%"=="0" (
    echo [FAILED] DaVinci Resolve is currently running.
    echo Please close DaVinci Resolve before updating to prevent file locking.
    echo.
    pause
    exit /b 1
)

if not exist "AsciiArt.ofx.bundle\Contents\Win64\AsciiArt.ofx" (
    echo [FAILED] New plugin binary missing. Run build.bat first.
    echo.
    pause
    exit /b 1
)

if not exist "%OFX_DIR%" (
    mkdir "%OFX_DIR%" 2>nul
)

:: Generate safe timestamp for backup
for /f "tokens=*" %%a in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "TIMESTAMP=%%a"
set "BACKUP_DEST=%OFX_DIR%\AsciiArt.ofx.bundle.backup_%TIMESTAMP%"

if exist "%OFX_DEST%" (
    echo [1/2] Backing up current installation...
    echo       Source: %OFX_DEST%
    echo       Backup: %BACKUP_DEST%
    ren "%OFX_DEST%" "AsciiArt.ofx.bundle.backup_%TIMESTAMP%" >nul 2>&1
    if errorlevel 1 (
        echo [WARNING] Could not rename in place, attempting folder copy backup...
        xcopy /E /I /Y "%OFX_DEST%" "%BACKUP_DEST%\" >nul 2>&1
        rmdir /S /Q "%OFX_DEST%" >nul 2>&1
    )
    echo       [OK] Backup created.
) else (
    echo [1/2] No existing installation found. Proceeding with fresh installation...
)

echo [2/2] Installing new ASCII OFX Plugin v1.3 (Build 11)...
xcopy /E /I /Y "AsciiArt.ofx.bundle" "%OFX_DEST%\" >nul 2>&1

if %errorLevel% neq 0 (
    echo [FAILED] Could not copy files to %OFX_DEST%.
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================================
echo [SUCCESS] ASCII OFX Plugin (v1.3 Build 11) updated!
echo   * Plugin Name: ASCII
echo   * Destination: %OFX_DEST%
if exist "%BACKUP_DEST%" (
    echo   * Previous version preserved at: %BACKUP_DEST%
)
echo ========================================================
echo.
pause
