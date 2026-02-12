@echo off
REM ============================================================================
REM WaitingTheLongest.com - Windows Build Script
REM ============================================================================
REM
REM This script builds the WaitingTheLongest application on Windows.
REM
REM Prerequisites:
REM   - CMake 3.16 or higher
REM   - Visual Studio 2019/2022 with C++ workload
REM   - Drogon framework installed
REM   - PostgreSQL development libraries (pqxx)
REM   - OpenSSL development libraries
REM
REM Usage:
REM   build.bat [Release|Debug]
REM
REM ============================================================================

setlocal enabledelayedexpansion

REM Default to Release build
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

echo.
echo ============================================
echo   WaitingTheLongest.com Build Script
echo ============================================
echo.
echo Build Type: %BUILD_TYPE%
echo.

REM Create build directory
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

cd build

REM Run CMake configuration
echo.
echo Running CMake configuration...
cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: CMake configuration failed!
    echo Please check that all dependencies are installed.
    exit /b 1
)

REM Build the project
echo.
echo Building project...
cmake --build . --config %BUILD_TYPE%

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Build failed!
    exit /b 1
)

echo.
echo ============================================
echo   Build completed successfully!
echo ============================================
echo.
echo Executable: build\%BUILD_TYPE%\WaitingTheLongest.exe
echo.
echo To run: cd build && %BUILD_TYPE%\WaitingTheLongest.exe
echo.

cd ..
exit /b 0
