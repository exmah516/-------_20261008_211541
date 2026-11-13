"""翻译当前Handoff与handle全文，运行1ms离线事务测试；不加载ADS或设备SDK。"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
SUPPORT = ROOT / '介入机器人/tools/reliability_round5'
sys.path.insert(0, str(SUPPORT))
from translate_st import translate, declaration

PLC = ROOT / '介入机器人/C从端/250902/Untitled2'
OUT = ROOT / 'docs/refactor_stage1/logs/st'
OUT.mkdir(parents=True, exist_ok=True)
BUILD = Path(tempfile.mkdtemp(prefix='handoff-st-'))
generated = ['#pragma once']
for name in ['Handoff', 'handle']:
    generated.append(translate((PLC / ('POUs/' + name + '.TcPOU')).read_bytes(), 'PLC' + name))
_, init = declaration(ET.parse(PLC / 'GVLs/G.TcGVL').find('.//Declaration').text, 'G')
generated += ['inline void initPLC(Node& G) {'] + init + ['}']
(BUILD / 'plc_generated.h').write_text('\n'.join(generated), encoding='utf-8')
vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
command = '@echo off\nchcp 65001 >nul\ncall "' + vs + '\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
command += 'cl /nologo /std:c++17 /EHsc /utf-8 /W4 /WX /I"' + str(BUILD) + '" /I"' + str(SUPPORT) + '" "' + str(Path(__file__).with_name('st_tests.cpp')) + '" /Fe:st_tests.exe\nif errorlevel 1 exit /b 1\nst_tests.exe\nexit /b %errorlevel%\n'
(BUILD / 'build.cmd').write_text(command, encoding='utf-8')
r = subprocess.run(['cmd', '/d', '/c', 'build.cmd'], cwd=BUILD, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(OUT / 'result.log').write_bytes(r.stdout)
print(r.stdout.decode('utf-8', errors='replace'))
print('ST build:', BUILD)
raise SystemExit(r.returncode)
