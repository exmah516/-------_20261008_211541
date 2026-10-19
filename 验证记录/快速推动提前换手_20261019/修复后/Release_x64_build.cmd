@echo off
chcp 65001 >nul
call "D:\Work_software\VS2022\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
MSBuild "D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\B数据端\工程\ADS项目\ADS.vcxproj" /m /v:minimal /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:OutDir="C:/Users/Sui/AppData/Local/Temp/handoff-release-c31loE/out/" /p:IntDir="C:/Users/Sui/AppData/Local/Temp/handoff-release-c31loE/obj/" > "C:\Users\Sui\AppData\Local\Temp\handoff-release-c31loE\build.log" 2>&1
exit /b %errorlevel%
