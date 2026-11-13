"""从Git暂存内容导出离线测试输入，检查未跟踪文件不会掩盖打包遗漏。"""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'docs/refactor_stage1/logs/clean_checkout'
OUT.mkdir(parents=True, exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='handoff-clean-'))
prefixes = ('tools/refactor_baseline/', 'tools/handoff_tests/',
            '介入机器人/B数据端/工程/ADS项目/',
            '介入机器人/C从端/250902/Untitled2/',
            '介入机器人/tools/reliability_round5/')
suffixes = ('.h', '.hpp', '.cpp', '.py', '.cjs', '.inc', '.TcPOU', '.TcGVL', '.csv', '.json')
names = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode('utf-8').split('\0')
names = [name for name in names if name.startswith(prefixes) and name.endswith(suffixes)]
subprocess.run(['git', 'checkout-index', '-z', '--stdin', '--prefix=' + str(work) + os.sep],
               input=('\0'.join(names) + '\0').encode('utf-8'), cwd=ROOT, check=True)
env = dict(os.environ, PYTHONIOENCODING='utf-8',
           GIT_DIR=subprocess.check_output(['git', 'rev-parse', '--absolute-git-dir'], cwd=ROOT).decode().strip(),
           GIT_WORK_TREE=str(work))
commands = [('baseline', [sys.executable, '-X', 'utf8', str(work/'tools/refactor_baseline/run.py'),
                          '--events', '--output', str(OUT/'baseline')]),
            ('terminal', [sys.executable, '-X', 'utf8', str(work/'tools/handoff_tests/compare_terminal.py')])]
results = []
for name, command in commands:
    with (OUT/(name + '.log')).open('wb') as log:
        result = subprocess.run(command, cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT)
    results.append({'name': name, 'exit': result.returncode})
    print(name, result.returncode, flush=True)
    if result.returncode:
        print((OUT/(name + '.log')).read_text(encoding='utf-8', errors='replace'))
        raise SystemExit(result.returncode)
shutil.copy2(work/'docs/refactor_stage1/logs/terminal/result.json', OUT/'terminal_result.json')
(OUT/'result.json').write_text(json.dumps({'status': 'pass', 'workspace': str(work),
                                        'exported_from_index': len(names), 'commands': results},
                                       ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('PASS clean staged input reproduction', len(names), 'files')
