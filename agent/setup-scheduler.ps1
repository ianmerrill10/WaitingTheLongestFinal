# WTL Backup — Windows Task Scheduler Setup
# Creates a scheduled task that runs backups at 6 AM and 6 PM daily

$taskName = "WTL-Backup"
$batPath = "C:\Users\ianme\OneDrive\Desktop\WaitingTheLongestFinal\agent\run-backup.bat"
$workDir = "C:\Users\ianme\OneDrive\Desktop\WaitingTheLongestFinal"

# Create action
$action = New-ScheduledTaskAction -Execute "cmd.exe" -Argument "/c `"$batPath`"" -WorkingDirectory $workDir

# Create triggers: 6 AM and 6 PM daily
$trigger1 = New-ScheduledTaskTrigger -Daily -At "6:00AM"
$trigger2 = New-ScheduledTaskTrigger -Daily -At "6:00PM"

# Settings: run even on battery, start if missed, requires network
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable -RunOnlyIfNetworkAvailable

# Register (overwrite if exists)
Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger1,$trigger2 -Settings $settings -Description "WaitingTheLongest.com - Automated Google Drive Backup with Redundant Versioning" -Force

Write-Host "`nTask registered successfully!" -ForegroundColor Green
Write-Host ""

# Show task info
Get-ScheduledTask -TaskName $taskName | Format-List TaskName, State, Description
Get-ScheduledTaskInfo -TaskName $taskName | Format-List LastRunTime, NextRunTime
