@echo off
REM WaitingTheLongest — Scheduled Backup Runner
REM Runs the backup system via npx tsx
REM Called by Windows Task Scheduler twice daily (6 AM + 6 PM)

cd /d "C:\Users\ianme\OneDrive\Desktop\WaitingTheLongestFinal"

REM Log start
echo [%date% %time%] Starting scheduled backup... >> backups\scheduler.log

REM Run backup
call npx tsx --tsconfig tsconfig.json agent/backup.ts >> backups\scheduler.log 2>&1

REM Log result
if %ERRORLEVEL% EQU 0 (
  echo [%date% %time%] Backup completed successfully >> backups\scheduler.log
) else (
  echo [%date% %time%] Backup FAILED with exit code %ERRORLEVEL% >> backups\scheduler.log
)
