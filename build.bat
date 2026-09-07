@echo off
setlocal EnableDelayedExpansion
echo.
echo ========================================================
echo   ASCII Art OFX Plugin - Windows Build
echo ========================================================
echo.

set "VCVARS="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
)

if "%VCVARS%"=="" (
    echo [ERROR] Could not find Visual Studio 2022 MSVC compiler.
    pause
    exit /b 1
)

echo [1/3] Initializing MSVC x64 build environment...
call "%VCVARS%" x64 >nul 2>&1

echo [2/3] Compiling src\ascii_art_ofx.cpp...
if not exist "AsciiArt.ofx.bundle\Contents\Win64" mkdir "AsciiArt.ofx.bundle\Contents\Win64" 2>nul
if not exist "build" mkdir "build" 2>nul

cl.exe /nologo /LD /MT /O2 /openmp /EHsc /std:c++17 ^
    /I "include" /I "src" ^
    /D "WIN32" /D "_WINDOWS" /D "NDEBUG" ^
    /W3 /wd4996 ^
    "src\ascii_art_ofx.cpp" ^
    /Fe:"AsciiArt.ofx.bundle\Contents\Win64\AsciiArt.ofx" ^
    /Fo:"build\\" ^
    /link /DLL user32.lib shell32.lib

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

del /f /q "AsciiArt.ofx.bundle\Contents\Win64\AsciiArt.exp" 2>nul
del /f /q "AsciiArt.ofx.bundle\Contents\Win64\AsciiArt.lib" 2>nul

echo [3/3] Build completed successfully!
echo Output: AsciiArt.ofx.bundle\Contents\Win64\AsciiArt.ofx
echo.
pause
