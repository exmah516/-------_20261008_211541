@echo off
chcp 65001 >nul
call "D:\Work_software\VS2022\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /utf-8 /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\docs\arch_survey_r1\scratch\tests\temp\dual-control-regression-4omy4ahh" "D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\B数据端\工程\ADS项目\tools\dual_control_test.cpp" /Fe:dual_control_test.exe
if errorlevel 1 exit /b 1
dual_control_test.exe
exit /b %errorlevel%
