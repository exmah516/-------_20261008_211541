"""正式 R2-8 行为特征测试：按标记抽取当前生产片段，仅在本目录编译并执行内存测试。"""
from pathlib import Path
import hashlib
import json
import os
import subprocess

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[1]
ADS = ROOT / '介入机器人/B数据端/工程/ADS项目'
import tempfile
BUILD = Path(tempfile.mkdtemp(prefix='behavior-probes-'))
BUILD.mkdir(exist_ok=True)
manifest = []

def extract(name, filename, start, end):
    p = ADS / filename
    original = p.read_bytes()
    text = original.decode('utf-8-sig').replace('\r\n', '\n')
    lo = text.index(start)
    hi = text.index(end, lo)
    fragment = text[lo:hi]
    target = BUILD / (name + '.inc')
    target.write_text(fragment, encoding='utf-8')
    manifest.append(dict(name=name, source=p.relative_to(ROOT).as_posix(),
                         start_line=text.count('\n', 0, lo)+1,
                         end_line=text.count('\n', 0, hi-1)+1,
                         start_marker=start, end_marker_exclusive=end,
                         source_sha256=hashlib.sha256(original).hexdigest(),
                         fragment_sha256=hashlib.sha256(fragment.encode()).hexdigest()))

extract('types', 'control_types.h', 'struct ControlConfig', '// 启动准备是在')
extract('mapping_helpers', 'main.cpp', '\tauto clear_axis1_delivery_mapping =', '\tauto apply_axis1_mirror_from_abs =')
extract('connection', 'main.cpp', '\t\tconst bool connection_hold_active =', '\t\tif (!initial_sync_done)')
extract('button', 'main.cpp', '\t\tconst bool spacing_recovery_button_pressed =', '\t\t// 换手/屈曲恢复期间冻结方向')
extract('save', 'main.cpp', '\t\tif (spacing_recovery_button_press_edge)', '\t\t// 该映射只属于')
extract('mapping_guard', 'main.cpp', '\t\tif (axis1_reverse_pressed ||', '\t\t// 恢复键只登记请求')
extract('entry', 'main.cpp', '\t\t\tauto calculate_spacing_recovery_remaining =', '\t\t\tif (axis6_soft_limit_hold && !motion_startup_active)')
extract('action', 'main.cpp', '\t\t\telse if (spacing_recovery.active())', '\t\t\telse if (motion_startup_active)')
extract('exit', 'main.cpp', '\t\tconst DWORD spacing_recovery_exit_elapsed_ms =', '\t\tconst bool axis4_manual_error_now =')
extract('dual_mapping', 'main.cpp', '\t\t\t\t// g约束始终使用轴3/5的名义增量', '\t\t\t\tconst double g_now =')
(BUILD/'behavior_extractions.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')

vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
vs = Path(subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip())
cmd = ('@echo off\r\nchcp 65001 >nul\r\ncall "'+str(vs/'VC/Auxiliary/Build/vcvars64.bat')+'" >nul\r\n'
       'if errorlevel 1 exit /b 1\r\ncl /nologo /std:c++17 /EHsc /utf-8 /W4 /I"'+str(BUILD)+'" "'+str(OUT/'behavior_probes.cpp')+'" /Fe:behavior_probes.exe\r\n'
       'if errorlevel 1 exit /b 1\r\nbehavior_probes.exe\r\nexit /b %errorlevel%\r\n')
(BUILD/'build.cmd').write_text(cmd, encoding='utf-8')
result = subprocess.run(['cmd.exe', '/d', '/c', 'build.cmd'], cwd=BUILD, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
log = result.stdout.decode('utf-8', errors='replace')
(BUILD/'behavior_probes.log').write_text(log, encoding='utf-8')
(BUILD/'behavior_result.json').write_text(json.dumps(dict(exit_code=result.returncode,
    build_cmd='behavior_build/build.cmd', log='behavior_probes.log', extractions='behavior_extractions.json'), indent=2), encoding='utf-8')
print(log)
raise SystemExit(result.returncode)
