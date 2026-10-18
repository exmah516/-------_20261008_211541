"""抽取生产运动、方向和恢复分支离线测试；不加载ADS/手柄DLL，不连接设备。"""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile

project = Path(__file__).resolve().parents[1]
package = project.parents[2]
report = Path(sys.argv[1]) if len(sys.argv) > 1 else package / '验证记录/双手柄修正_20261011'
report.mkdir(parents=True, exist_ok=True)
source = (project / 'main.cpp').read_text(encoding='utf-8-sig')
header = (project / 'control_types.h').read_text(encoding='utf-8-sig')
sync = (project / 'motion_sync.cpp').read_text(encoding='utf-8-sig')

def between(text, start, end):
    lo = text.index(start)
    return text[lo:text.index(end, lo)]

parts = {
    'motion': between(source, '\t\t\t\tconst double tol = cfg.crawl_arrive_tol_mm;',
                      '\n\t\t\t}\n\t\t\telse\n\t\t\t{\n\t\t\t\t// 常规导管模式：'),
    'windows': between(sync, '\tdouble from_left_to_abs', '\tvoid lock_axis6_window_from_current'),
    'target_safety': between(source, '\t\t\tauto planned_return_targets_safe =', '\t\t\tauto rebase_planned_return ='),
    'recovery_type': between(header, 'enum class SpacingRecoveryPhase', '// 启动准备是在'),
    'mode': between(source, '\tbool vis_reverse_override_active =', '\n\tauto request_mode_selection'),
    'direction': between(source, '\t\t// 换手/屈曲恢复期间冻结方向', '\t\tconst bool startup_sequence_active'),
    'save': between(source, '\t\tif (spacing_recovery_button_press_edge)', '\t\t// 该映射只属于'),
    'exit': between(source, '\t\t\t\tconst ModeSelection restore_selection =', '\t\t\t\tstd::cout << "屈曲恢复退出后'),
    'switch': between(source, '\t\tif (!single_handle_mode && axis1_reverse_pressed != axis1_reverse_pressed_prev)', '\t\t// 单手柄调试保留'),
    'filter': between(header, 'struct HandleFilterState', 'struct ControlConfig'),
}
fields = set(re.findall(r'cfg(?:\.|->)(\w+)', parts['motion'] + parts['windows']))
fields |= {'linear_increment_noise_deadband_mm', 'axis3_delivery_stop_from_left_mm',
           'axis3_delivery_release_hysteresis_mm'}
cfg = '\n'.join('double ' + f + ' = ' + re.search(rf'double\s+{f}\s*=\s*([^;]+)', header)[1] + ';'
                for f in sorted(fields))
parts['config'] = cfg.replace('-Rad', '-1.0')
build = Path(tempfile.mkdtemp(prefix='dual-control-regression-'))
for name, code in parts.items():
    (build / (name + '.inc')).write_text(code, encoding='utf-8')
vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
script = build / 'build.cmd'
script.write_text('@echo off\nchcp 65001 >nul\n'
    f'call "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
    'if errorlevel 1 exit /b 1\n'
    f'cl /nologo /std:c++17 /EHsc /utf-8 /I"{build}" "{project / "tools/dual_control_test.cpp"}" /Fe:dual_control_test.exe\n'
    'if errorlevel 1 exit /b 1\ndual_control_test.exe\nexit /b %errorlevel%\n', encoding='utf-8')
result = subprocess.run(['cmd.exe', '/d', '/c', str(script)], cwd=build, capture_output=True)
output = result.stdout.decode('utf-8', errors='replace') + result.stderr.decode('utf-8', errors='replace')
print(output)
(report / 'dual_control_test.log').write_text(output, encoding='utf-8')
if (build / 'trajectories.csv').exists():
    (report / 'trajectories.csv').write_bytes((build / 'trajectories.csv').read_bytes())
(report / 'test_source_hashes.json').write_text(json.dumps({str(p.relative_to(package)):
    hashlib.sha256(p.read_bytes()).hexdigest() for p in
    [project/'main.cpp', project/'motion_sync.cpp', project/'control_types.h',
     project/'tools/dual_control_test.cpp', Path(__file__)]}, ensure_ascii=False, indent=2), encoding='utf-8')
print('Offline test artifacts:', build)
raise SystemExit(result.returncode)
