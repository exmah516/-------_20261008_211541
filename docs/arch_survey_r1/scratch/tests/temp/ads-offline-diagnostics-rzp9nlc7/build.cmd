@echo off
chcp 65001 >nul
call "D:\Work_software\VS2022\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /utf-8 /Gy /MD /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\docs\arch_survey_r1\scratch\tests\temp\ads-offline-diagnostics-rzp9nlc7" /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\B数据端\工程\ADS项目" /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\B数据端\工程\ADS项目\ADS\Include" /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\B数据端\工程\ADS项目\手柄" /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\B数据端\工程\ADS项目\protocol\cpp" "D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\B数据端\工程\ADS项目\tools\handle_net_distance_test.cpp" /Fe:ads_diagnostics_test.exe /link /OPT:REF
if errorlevel 1 exit /b 1
ads_diagnostics_test.exe
exit /b %errorlevel%
