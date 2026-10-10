@echo off
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0工程\tools\send_vtk_motion.ps1" -MasterAddress 192.168.50.1
set "result=%errorlevel%"
if not "%result%"=="0" pause
exit /b %result%
