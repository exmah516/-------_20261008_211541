@echo off
chcp 65001 >nul
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0工程\tools\check_vtk_motion.ps1" -ListenAddress 192.168.50.1 -PeerAddress 192.168.50.2
set "result=%errorlevel%"
if not "%result%"=="0" pause
exit /b %result%
