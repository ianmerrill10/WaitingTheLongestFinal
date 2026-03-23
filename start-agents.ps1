# WaitingTheLongest.com — Auto-start all agents
# Run this script to launch all agents, or add to Windows Task Scheduler for boot startup.
#
# Task Scheduler setup (run at login):
#   Action: Start a program
#   Program: powershell.exe
#   Arguments: -ExecutionPolicy Bypass -File "C:\Users\ianme\OneDrive\Desktop\WaitingTheLongestFinal\start-agents.ps1"
#   Start in: C:\Users\ianme\OneDrive\Desktop\WaitingTheLongestFinal

$ErrorActionPreference = "Continue"
$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "  WaitingTheLongest.com - Starting All Agents" -ForegroundColor Green
Write-Host "  Data (3847) + Scraper (3849) + Improvement (3848)" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
Write-Host ""

Set-Location $ProjectDir

# Start the master launcher
npm run agent:all
