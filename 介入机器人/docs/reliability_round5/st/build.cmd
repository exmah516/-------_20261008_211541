@echo off
chcp 65001 >nul
call "D:\Work_software\VS2022\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\docs\reliability_round5\st" /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\tools\reliability_round5" "D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\tools\reliability_round5\st_tests.cpp" /Fe:st_tests.exe
if errorlevel 1 exit /b 1
st_tests.exe
exit /b %errorlevel%
