@echo off
chcp 65001 >nul
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\验证记录\库构建\can4wpf"
cl /nologo /c /MD /O2 /DNDEBUG /DWIN32 /utf-8 /EHsc /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\A_主端电脑\源码备份\本地库源码\eigen3" "D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\A_主端电脑\源码备份\本地库源码\can4wpf\copleycan.cpp"
if errorlevel 1 exit /b 1
cl /nologo /clr /LD /MD /O2 /DNDEBUG /DWIN32 /utf-8 /I"D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\A_主端电脑\源码备份\本地库源码\eigen3" "D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\A_主端电脑\源码备份\本地库源码\can4wpf\can_clr.cpp" copleycan.obj /Fe:can4wpf.dll /link /MACHINE:X64
exit /b %errorlevel%
