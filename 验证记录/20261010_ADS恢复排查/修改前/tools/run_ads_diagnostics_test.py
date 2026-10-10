"""编译真实通信服务和离线替身，无 ADS DLL / 无设备连接。"""
from pathlib import Path
import os
import subprocess
import tempfile

project = Path(__file__).resolve().parents[1]
vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                              'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'],
                             text=True).strip()
build = Path(tempfile.mkdtemp(prefix='ads-offline-diagnostics-'))
script = build / 'build.cmd'
script.write_text('@echo off\nchcp 65001 >nul\n'
                  f'call "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
                  'if errorlevel 1 exit /b 1\n'
                  f'cl /nologo /std:c++17 /EHsc /utf-8 /Gy /MD /I"{project}" /I"{project / "ADS/Include"}" '
                  f'/I"{project / "手柄"}" /I"{project / "手柄/include"}" '
                  f'"{project / "tools/ads_diagnostics_test.cpp"}" '
                  f'"{project / "plc_io.cpp"}" /Fe:ads_diagnostics_test.exe /link /OPT:REF\n'
                  'if errorlevel 1 exit /b 1\nads_diagnostics_test.exe\nexit /b %errorlevel%\n', encoding='utf-8')
result = subprocess.run(['cmd.exe', '/d', '/c', str(script)], cwd=build)
print('Offline test artifacts:', build)
raise SystemExit(result.returncode)
