"""编译迁移后的真实转发实现；仅使用本机回环替身。"""
from pathlib import Path
import os
import subprocess
import tempfile

project = Path(__file__).resolve().parents[1]
vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                              'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'],
                             text=True).strip()
build = Path(tempfile.mkdtemp(prefix='imaging-camera-test-'))
script = build / 'build.cmd'
script.write_text('@echo off\nchcp 65001 >nul\n'
                  f'call "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
                  'if errorlevel 1 exit /b 1\n'
                  f'cl /nologo /std:c++17 /EHsc /utf-8 /MT /I"{project}" '
                  f'"{project / "tools/hik_camera_relay_test.cpp"}" "{project / "hik_camera_relay.cpp"}" '
                  '/Fe:camera_relay_test.exe\n'
                  'if errorlevel 1 exit /b 1\ncamera_relay_test.exe\nexit /b %errorlevel%\n', encoding='utf-8')
result = subprocess.run(['cmd.exe', '/d', '/c', str(script)], cwd=build)
print('Loopback test artifacts:', build)
raise SystemExit(result.returncode)
