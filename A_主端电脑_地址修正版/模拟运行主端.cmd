@echo off
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0工程\build-and-run.ps1" -Sim
set "result=%errorlevel%"
if not "%result%"=="0" pause
exit /b %result%
