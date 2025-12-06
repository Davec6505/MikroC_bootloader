@echo off
REM Windows build script for MikroC bootloader
REM Ensures MinGW64 bin directory is in PATH

SET MINGW64_BIN=C:\mingw64\bin
SET PATH=%MINGW64_BIN%;%PATH%

echo Building MikroC bootloader...
make clean
make

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful! Executable: bins\mikro_hb.exe
) else (
    echo.
    echo Build failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)
