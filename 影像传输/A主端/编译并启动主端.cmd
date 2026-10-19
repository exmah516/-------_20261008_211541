@echo off
chcp 65001 >nul
set "ROOT=%~dp0工程\ImagingMaster"
dotnet build "%ROOT%\ImagingMaster.sln" --configuration Release --nologo
if not "%errorlevel%"=="0" (
    pause
    exit /b %errorlevel%
)
start "" "%ROOT%\bin\Release\net472\ImagingMaster.exe"
