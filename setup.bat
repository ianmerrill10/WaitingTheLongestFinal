@echo off
REM ============================================================================
REM WaitingTheLongest.com - Setup Script
REM ============================================================================
REM
REM This script sets up the development environment for WaitingTheLongest.
REM
REM It will:
REM   1. Create required directories
REM   2. Copy example config to config.json (if not exists)
REM   3. Display database setup instructions
REM
REM ============================================================================

echo.
echo ============================================
echo   WaitingTheLongest.com Setup
echo ============================================
echo.

REM Create directories
echo Creating directories...

if not exist "logs" mkdir logs
if not exist "uploads" mkdir uploads
if not exist "uploads\media" mkdir uploads\media
if not exist "uploads\temp" mkdir uploads\temp
if not exist "build" mkdir build

echo   - logs/
echo   - uploads/
echo   - uploads/media/
echo   - uploads/temp/
echo   - build/

REM Check config
echo.
echo Checking configuration...

if not exist "config\config.json" (
    echo   - Creating config\config.json from example...
    copy "config\config.example.json" "config\config.json" > nul
    echo   - IMPORTANT: Edit config\config.json with your database credentials!
) else (
    echo   - config\config.json exists
)

echo.
echo ============================================
echo   Database Setup Instructions
echo ============================================
echo.
echo 1. Install PostgreSQL 16 if not already installed
echo.
echo 2. Create the database:
echo    psql -U postgres -c "CREATE DATABASE waitingthelongest;"
echo.
echo 3. Create the user (optional, or use postgres):
echo    psql -U postgres -c "CREATE USER wtl_user WITH PASSWORD 'your_password';"
echo    psql -U postgres -c "GRANT ALL PRIVILEGES ON DATABASE waitingthelongest TO wtl_user;"
echo.
echo 4. Run the migrations:
echo    psql -U postgres -d waitingthelongest -f sql/core/001_extensions.sql
echo    psql -U postgres -d waitingthelongest -f sql/core/002_states.sql
echo    ... (run all sql/core/*.sql files in order)
echo    ... (then run all sql/modules/*.sql files)
echo.
echo 5. Seed the states data:
echo    psql -U postgres -d waitingthelongest -f sql/core/seed_states.sql
echo.
echo ============================================
echo   Build Instructions
echo ============================================
echo.
echo 1. Install dependencies:
echo    - CMake 3.16+
echo    - Visual Studio 2019/2022 with C++ workload
echo    - vcpkg (recommended for dependency management)
echo.
echo 2. Install required libraries via vcpkg:
echo    vcpkg install drogon:x64-windows
echo    vcpkg install libpqxx:x64-windows
echo    vcpkg install openssl:x64-windows
echo.
echo 3. Build the project:
echo    build.bat
echo.
echo 4. Run the application:
echo    run.bat
echo.
echo ============================================
echo   Setup complete!
echo ============================================
echo.
