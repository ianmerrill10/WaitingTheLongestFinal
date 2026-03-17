@echo off
REM ============================================================================
REM WaitingTheLongest.com - Database Migration Script
REM ============================================================================
REM
REM This script runs all SQL migrations in order.
REM
REM Prerequisites:
REM   - PostgreSQL installed and psql in PATH
REM   - Database created (waitingthelongest)
REM   - User configured in config/config.json
REM
REM Usage:
REM   migrate.bat [postgres_user] [postgres_password]
REM
REM ============================================================================

setlocal enabledelayedexpansion

set PGUSER=%1
set PGPASSWORD=%2
set PGDATABASE=waitingthelongest

if "%PGUSER%"=="" set PGUSER=postgres
if "%PGPASSWORD%"=="" set PGPASSWORD=postgres

echo.
echo ============================================
echo   Running Database Migrations
echo ============================================
echo.
echo Database: %PGDATABASE%
echo User: %PGUSER%
echo.

REM Core migrations (in order)
echo Running core migrations...

for %%f in (sql\core\001_extensions.sql sql\core\002_states.sql sql\core\003_shelters.sql sql\core\004_dogs.sql sql\core\005_users.sql sql\core\006_sessions.sql sql\core\007_favorites.sql sql\core\008_foster_homes.sql sql\core\009_foster_placements.sql sql\core\010_debug_schema.sql sql\core\011_indexes.sql sql\core\012_views.sql sql\core\013_functions.sql) do (
    if exist "%%f" (
        echo   - %%f
        psql -U %PGUSER% -d %PGDATABASE% -f "%%f" -q
        if !ERRORLEVEL! neq 0 (
            echo ERROR: Failed to run %%f
            exit /b 1
        )
    )
)

REM Module migrations
echo.
echo Running module migrations...

for %%f in (sql\modules\admin_schema.sql sql\modules\analytics_schema.sql sql\modules\aggregator_schema.sql sql\modules\blog_schema.sql sql\modules\media_schema.sql sql\modules\notifications_schema.sql sql\modules\scheduler_schema.sql sql\modules\social_schema.sql sql\modules\templates_schema.sql sql\modules\tiktok_schema.sql) do (
    if exist "%%f" (
        echo   - %%f
        psql -U %PGUSER% -d %PGDATABASE% -f "%%f" -q
        if !ERRORLEVEL! neq 0 (
            echo ERROR: Failed to run %%f
            exit /b 1
        )
    )
)

REM Seed data
echo.
echo Running seed data...

if exist "sql\core\seed_states.sql" (
    echo   - sql\core\seed_states.sql
    psql -U %PGUSER% -d %PGDATABASE% -f "sql\core\seed_states.sql" -q
)

echo.
echo ============================================
echo   Migrations completed successfully!
echo ============================================
echo.
