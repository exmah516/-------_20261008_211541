@echo off
chcp 65001 >nul
call "D:\Work_software\VS2022\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\docs\arch_survey_r2\scratch\behavior_build" "D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\docs\arch_survey_r2\scratch\behavior_probes.cpp" /Fe:behavior_probes.exe
if errorlevel 1 exit /b 1
behavior_probes.exe
exit /b %errorlevel%
