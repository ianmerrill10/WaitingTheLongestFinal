@echo off
REM ============================================================================
REM WaitingTheLongest.com - Run Script
REM ============================================================================
REM
REM This script runs the WaitingTheLongest application.
REM
REM Usage:
REM   run.bat [--config=path/to/config.json]
REM
REM ============================================================================

setlocal

REM Check if build exists
if not exist "build\Release\WaitingTheLongest.exe" (
    if not exist "build\Debug\WaitingTheLongest.exe" (
        echo.
        echo ERROR: Application not built yet!
        echo Please run build.bat first.
        exit /b 1
    ) else (
        set EXE_PATH=build\Debug\WaitingTheLongest.exe
    )
) else (
    set EXE_PATH=build\Release\WaitingTheLongest.exe
)

REM Create required directories
if not exist "logs" mkdir logs
if not exist "uploads" mkdir uploads
if not exist "uploads\media" mkdir uploads\media
if not exist "uploads\temp" mkdir uploads\temp

echo.
echo ============================================
echo   Starting WaitingTheLongest.com
echo ============================================
echo.

REM Run with any passed arguments
%EXE_PATH% %*
