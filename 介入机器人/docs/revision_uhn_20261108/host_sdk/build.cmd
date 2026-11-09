@echo off
chcp 65001 >nul
call "D:\Work_software\VS2022\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /LD "D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\tools\reliability_round5\fake_handle_sdk.cpp" /Fe:FakeFLCatheter.dll
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /LD /DVI_NO_JOINTS "D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\tools\reliability_round5\fake_handle_sdk.cpp" /Fe:MissingEntry.dll
exit /b %errorlevel%
