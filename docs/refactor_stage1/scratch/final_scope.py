"""核对已编译/已测试输入、未改黄金及原有工作区修改的提交边界。"""
from pathlib import Path
import hashlib
import json
import subprocess

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'docs/refactor_stage1/logs/final_scope.json'
BASE = 'd2bb5fc4de8e53aad750b2a048008b52fa488453'


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)


def paths(*args):
    return [s for s in git(*args).decode('utf-8').split('\0') if s]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(name):
    return json.loads((ROOT / name).read_text(encoding='utf-8-sig'))


initial = load('docs/refactor_stage1/scratch/baseline.json')
protected = [line[3:].strip('"') for line in initial['initial_status'].splitlines()]
committed = paths('diff', '--name-only', '-z', BASE, 'HEAD')
staged = paths('diff', '--cached', '--name-only', '-z')
conflicts = sorted({p for p in committed + staged
                    if any(p == old or old.endswith('/') and p.startswith(old) for old in protected)})
assert not conflicts, ('混入了开始前修改', conflicts)

script = 'tools/plc_build.ps1'
script_unchanged = sha(ROOT / script) == initial['files'][script]
assert script_unchanged, '本阶段改动了开始前的PLC脚本'

golden_paths = paths('ls-files', '-z', '--', 'tools/refactor_baseline/golden')
golden = []
for name in golden_paths:
    previous = git('show', BASE + ':' + name).replace(b'\r\n', b'\n')
    current = (ROOT / name).read_bytes().replace(b'\r\n', b'\n')
    assert previous == current, ('黄金发生变化', name)
    golden.append({'path': name, 'unchanged': True})

compiled = load('docs/refactor_stage1/logs/plc_final/sources.json')
build = load('docs/refactor_stage1/logs/plc_final/result.json')
plc = []
for name, expected in compiled.items():
    original = ROOT / name
    relative = original.relative_to(ROOT / '介入机器人/C从端')
    copied = Path(build['workspace']) / relative
    match = sha(original) == expected
    copy_match = sha(copied) == expected if copied.exists() else None
    assert match and copy_match is not False, ('PLC编译输入不匹配', name)
    plc.append({'path': original.relative_to(ROOT).as_posix(), 'sha256': expected,
                'current_matches': match, 'build_copy_matches': copy_match})
assert build['success'] and build['errors'] == 0 and build['warnings'] == 0

public = load('docs/refactor_stage1/logs/public_contract.json')['files']
for item in public:
    assert item['unchanged']
    assert git('diff', '--name-only', BASE, '--', item['path']) == b'', ('公共文件发生变化', item['path'])

tested = load('docs/refactor_stage1/logs/baseline/result.json')['sources']
tested_sources = []
for name, expected in tested.items():
    current = ROOT / '介入机器人/B数据端/工程/ADS项目' / name
    assert sha(current) == expected, ('黄金运行后生产输入已变化', name)
    tested_sources.append({'path': current.relative_to(ROOT).as_posix(), 'sha256': expected})

old_manifest = load('tools/handoff_tests/stage0_support/manifest.json')['sources']
recovered = []
for name in ('handle_velocity_recorder.h', 'homing_record.h'):
    path = ROOT / '介入机器人/B数据端/工程/ADS项目' / name
    value = path.read_bytes()
    variants = (value, value.replace(b'\r\n', b'\n'), value.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'))
    assert any(hashlib.sha256(v).hexdigest() == old_manifest[name] for v in variants)
    recovered.append({'path': path.relative_to(ROOT).as_posix(), 'stage0_sha256': old_manifest[name],
                      'current_sha256': sha(path), 'same_content_allowing_git_line_endings': True})

source_changes = [p for p in committed + staged
                  if p.startswith('介入机器人/C从端/250902/Untitled2/')
                  or p.startswith('介入机器人/B数据端/工程/ADS项目/') and '/tools/' not in p]
result = dict(status='pass', baseline_commit=BASE, head_before_section4=git('rev-parse', 'HEAD').decode().strip(),
              golden_unchanged=golden, compiled_plc_inputs=plc, plc_errors=0, plc_warnings=0,
              public_contract_unchanged_count=len(public), tested_sources=tested_sources,
              recovered_unchanged_headers=recovered, preexisting_paths_excluded=protected,
              preexisting_conflicts=conflicts, plc_build_script_matches_initial=script_unchanged,
              production_paths=sorted(set(source_changes)), section4_staged_paths=staged)
OUT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('PASS scope:', len(golden), 'golden files,', len(plc), 'PLC inputs,', len(public), 'public files,', len(tested_sources), 'tested inputs')
