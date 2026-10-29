"""编译真实通信服务和离线替身，无 ADS DLL / 无设备连接。"""
from pathlib import Path
import os
import subprocess
import tempfile
import sys

project = Path(__file__).resolve().parents[1]
vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                              'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'],
                             text=True).strip()
build = Path(tempfile.mkdtemp(prefix='ads-offline-diagnostics-'))
test = sys.argv[1] if len(sys.argv) > 1 else 'ads_diagnostics_test'
if test not in ('ads_diagnostics_test', 'remote_recovery_test', 'main_recovery_test', 'handle_net_distance_test', 'net_distance_gateway_test', 'remote_handle_test'):
    raise SystemExit('Unknown offline test')
extra_source = f'"{project / "plc_io.cpp"}" ' if test == 'ads_diagnostics_test' else ''
if test == 'remote_handle_test':
    extra_source = f'"{project / "手柄/手柄.cpp"}" '
if test == 'handle_net_distance_test':
    source = (project / 'main.cpp').read_text(encoding='utf-8-sig')
    for name, start, end in (
        ('distance_sample', '\t\tauto handle_distance_follow_allowed', '\t\t// 9) 根据当前顶层模式'),
        ('distance_output', '\t\tconst bool distance_follow_output', '\t\tconst std::uint64_t output_generation')):
        lo = source.index(start)
        (build / (name + '.inc')).write_text(source[lo:source.index(end, lo)], encoding='utf-8')
if test == 'ads_diagnostics_test':
    rows = (project / 'tools/ads_symbol_sizes.tsv').read_text(encoding='utf-8').splitlines()
    entries = []
    for row in rows:
        if not row or row.startswith('#'):
            continue
        name, size = row.split('\t')
        entries.append('{"' + name + '", ' + str(int(size)) + '},')
    (build / 'ads_symbol_sizes.inc').write_text('\n'.join(entries), encoding='utf-8')
if test == 'main_recovery_test':
    source = (project / 'main.cpp').read_text(encoding='utf-8-sig')
    def between(start, end):
        begin = source.index(start)
        return source[begin:source.index(end, begin)]
    observe_start = ('\t\tconst bool ads_self_check_valid' if 'const bool ads_self_check_valid' in source
                     else '\t\tself_check_done = ads_events.self_check_done;')
    observe = between(observe_start, '\t\tif (ads_events.selfcheck_status')
    hold = between('\t\tif (!ads_motion_cycle_valid)', '\t\t\tif (!ads_soft_hold_active)') + '\n}\n'
    (build / 'main_observe.inc').write_text(observe + hold, encoding='utf-8')
    rebase = between('\t\telse if ((ads_soft_hold_active', '\t\tconst bool connection_hold_active')
    (build / 'main_rebase.inc').write_text(rebase.replace('else if', 'if', 1), encoding='utf-8')
    auto = between('\t\tif (!control_active && !return_ads_fault_hold', '\t\tunsigned short cylinder1_cmd')
    (build / 'main_auto_resume.inc').write_text(auto, encoding='utf-8')
    start = between('\t\t\t\tcase VisCommandType::SelectDirectControl:', '\t\t\t\t\tcase VisCommandType::SetGravityCompensation:')
    (build / 'main_start.inc').write_text(start.split('\n', 1)[1], encoding='utf-8')
script = build / 'build.cmd'
script.write_text('@echo off\nchcp 65001 >nul\n'
                  f'call "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
                  'if errorlevel 1 exit /b 1\n'
                  f'cl /nologo /std:c++17 /EHsc /utf-8 /Gy /MD /I"{build}" /I"{project}" /I"{project / "ADS/Include"}" '
                  f'/I"{project / "手柄"}" /I"{project / "protocol/cpp"}" '
                  f'"{project / ("tools/" + test + ".cpp")}" '
                  f'{extra_source}/Fe:ads_diagnostics_test.exe /link /OPT:REF\n'
                  'if errorlevel 1 exit /b 1\nads_diagnostics_test.exe\nexit /b %errorlevel%\n', encoding='utf-8')
result = subprocess.run(['cmd.exe', '/d', '/c', str(script)], cwd=build)
print('Offline test artifacts:', build)
raise SystemExit(result.returncode)
