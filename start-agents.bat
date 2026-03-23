@echo off
title WaitingTheLongest.com - All Agents
echo.
echo ============================================================
echo   WaitingTheLongest.com - Starting All Agents
echo   Data (3847) + Scraper (3849) + Improvement (3848)
echo ============================================================
echo.

cd /d "%~dp0"
npm run agent:all

pause
