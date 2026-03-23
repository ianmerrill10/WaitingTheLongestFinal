@echo off
:: RIGHT-CLICK THIS FILE AND SELECT "Run as administrator"
:: This sets up WaitingTheLongest agents to auto-start when you log in.

echo.
echo ============================================================
echo   Setting up WaitingTheLongest.com Auto-Start
echo ============================================================
echo.

:: Create scheduled task
schtasks /create /tn "WaitingTheLongest-Agents" /tr "powershell.exe -ExecutionPolicy Bypass -WindowStyle Minimized -File \"%~dp0start-agents.ps1\"" /sc onlogon /rl highest /f

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS! Agents will now auto-start when you log in.
    echo.
    echo To manually start:  npm run agent:all
    echo To remove auto-start:  schtasks /delete /tn "WaitingTheLongest-Agents" /f
) else (
    echo.
    echo FAILED - Make sure you ran this as Administrator.
    echo Right-click setup-autostart.bat and select "Run as administrator"
)

echo.
pause
