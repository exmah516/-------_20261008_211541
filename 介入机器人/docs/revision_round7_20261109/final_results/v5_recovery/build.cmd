@echo off
chcp 65001 >nul
call "D:\Work_software\VS2022\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /DNOMINMAX /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\B数据端\工程\ADS项目" "D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\tools\reliability_round5\recovery_tests.cpp" /Fe:recovery_tests.exe
if errorlevel 1 exit /b 1
recovery_tests.exe
exit /b %errorlevel%
