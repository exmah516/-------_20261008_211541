@echo off
chcp 65001 >nul
title VI.Control - 联机运行
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0工程\build-and-run.ps1" 
set "result=%errorlevel%"
if not "%result%"=="0" pause
exit /b %result%
