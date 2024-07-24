@echo off
cd %~dp0
start "" "%~dp0Locate_Code.exe"
timeout /t 1 /nobreak >nul
start "" "%~dp0Process_Analyzer.exe"

rem Run the PowerShell script to keep the windows on top
powershell -File "%~dp0keep_windows_on_top.ps1"
