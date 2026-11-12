@echo off
chcp 65001 >nul
call "D:\Work_software\VS2022\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /utf-8 /DNOMINMAX /DMSGPACK_NO_BOOST /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\zenoh\sdk\msgpack-cxx-9.0.0\include" /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\zenoh\sdk\zenoh-c-1.10.1-msvc\include" /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\B数据端\工程\ADS项目"  "D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\B数据端\工程\ADS项目\tests\zenoh\service_tests.cpp" "D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人\B数据端\工程\ADS项目\zenoh_service.cpp" /Fe:service_tests.exe /link /LIBPATH:"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\zenoh\sdk\zenoh-c-1.10.1-msvc\lib" zenohc.dll.lib bcrypt.lib ws2_32.lib
exit /b %errorlevel%
