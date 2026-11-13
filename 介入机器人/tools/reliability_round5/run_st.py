from pathlib import Path
import os
import subprocess
import zipfile
from translate_st import translate, declaration
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[2]
out = repo.parent / 'docs/refactor_stage1/logs/st_regression'
out.mkdir(parents=True, exist_ok=True)
prefix = 'C从端/250902/Untitled2/'
baseline_ref = 'd2bb5fc4de8e53aad750b2a048008b52fa488453'
names = ['handle', 'err', 'clear_err', 'Axis2Sync', 'MAIN', 'SelfCheck']
locals_by_version = {}
for version in ('Old', 'New'):
    generated = ['#pragma once']
    for name in names:
        path = prefix + 'POUs/' + name + '.TcPOU'
        raw = subprocess.check_output(['git','show',baseline_ref+':介入机器人/'+path],cwd=repo.parent) if version == 'Old' else (repo / path).read_bytes()
        generated.append(translate(raw, version + name))
        declarations, _ = declaration(ET.fromstring(raw).find('.//Declaration').text)
        locals_by_version[version, name] = {line[5:-1] for line in declarations}
    path = prefix + 'GVLs/G.TcGVL'
    raw = subprocess.check_output(['git','show',baseline_ref+':介入机器人/'+path],cwd=repo.parent) if version == 'Old' else (repo / path).read_bytes()
    _, init = declaration(ET.fromstring(raw).find('.//Declaration').text, 'G')
    generated += ['inline void init' + version + '(Node& G) {'] + init + ['}']
    (out / (version + '.h')).write_text('\n'.join(generated), encoding='utf-8')
common = ['#pragma once', '#include <utility>']
for name in names:
    excluded = {'i', 'j', 'k'} | ({'all_powered','handoff','handoff_reinit','return_wait_state','return_enable','return_wait'} if name == 'handle' else set()) | ({'err_','handoff_called'} if name == 'MAIN' else set())
    fields = sorted(locals_by_version['Old', name] & locals_by_version['New', name] - excluded)
    common += ['inline std::pair<Node,Node> commonLocals(Old'+name+'& a, New'+name+'& b) { Node x,y;']
    common += ['x["'+field+'"] = a.'+field+'; y["'+field+'"] = b.'+field+';' for field in fields]
    common += ['return {x,y}; }']
(out/'Common.h').write_text('\n'.join(common), encoding='utf-8')
vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
cmd = out / 'build.cmd'
cmd.write_text('@echo off\nchcp 65001 >nul\ncall "' + vs + '\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
               'cl /nologo /std:c++17 /EHsc /utf-8 /W4 /I"' + str(out) + '" /I"' + str(Path(__file__).parent) + '" "' + str(Path(__file__).with_name('st_tests.cpp')) + '" /Fe:st_tests.exe\n'
               'if errorlevel 1 exit /b 1\nst_tests.exe\nexit /b %errorlevel%\n', encoding='utf-8')
r = subprocess.run(['cmd.exe', '/d', '/c', str(cmd)], cwd=out, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(out / 'results.log').write_bytes(r.stdout)
print(r.stdout.decode('utf-8', errors='replace'))
raise SystemExit(r.returncode)
