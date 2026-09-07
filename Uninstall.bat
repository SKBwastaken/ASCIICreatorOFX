@echo off
title ASCII Art OFX Plugin - Uninstaller

net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -Command "Start-Process cmd -ArgumentList '/c \"\"%~dp0Uninstall.bat\"\"' -Verb RunAs"
    exit /b
)

set "OFX_DEST=%CommonProgramFiles%\OFX\Plugins\AsciiArt.ofx.bundle"

if exist "%OFX_DEST%" (
    rmdir /S /Q "%OFX_DEST%"
    echo [SUCCESS] ASCII Art OFX Plugin has been removed.
) else (
    echo Plugin is not installed.
)

pause
