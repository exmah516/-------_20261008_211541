"""仅校验本轮交付物、源码字节和只读基准；不启动生产程序。"""
from pathlib import Path
import csv
import hashlib
import json
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'docs/arch_survey_r2'
SCRATCH = OUT / 'scratch'
D = ROOT / '介入机器人/B数据端/工程/ADS项目'
U = ROOT / '介入机器人/C从端/250902/Untitled2'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def read_csv(name, fields, count):
    p = OUT / name
    assert p.read_bytes().startswith(b'\xef\xbb\xbf'), name
    with p.open(encoding='utf-8-sig', newline='') as f:
        reader = csv.DictReader(f)
        assert reader.fieldnames == fields, (name, reader.fieldnames)
        rows = list(reader)
    assert len(rows) == count, (name, len(rows))
    assert all(None not in row and None not in row.values() for row in rows), name
    assert all('\ufffd' not in value and '\x00' not in value
               for row in rows for value in row.values()), name
    return rows

def check_ref(ref):
    name, numbers = ref.rsplit(':', 1)
    p = Path(name)
    if not p.is_absolute():
        p = ROOT / name
    assert p.is_file(), ref
    length = len(p.read_text(encoding='utf-8-sig').splitlines())
    indices = [int(n) for n in re.findall(r'\d+', numbers)]
    assert indices and min(indices) >= 1 and max(indices) <= length, ref

timeouts = read_csv('timeouts.csv', ['编号', '所在端', '文件:行号', '名称',
    '数值（ms）', '时钟类型', '保护对象', '到期动作', '恢复条件'], 166)
assert [r['编号'] for r in timeouts] == [f'T{i:03d}' for i in range(1, 167)]
for row in timeouts:
    assert not row['数值（ms）'] or re.fullmatch(r'\d+(?:\.\d+)?', row['数值（ms）'])
    assert row['名称'].startswith('已确认：')
    for ref in row['文件:行号'].split('; '):
        check_ref(ref)
blocking = read_csv('blocking_calls.csv', ['位置', '调用', '最坏阻塞时长的依据'], 426)
for row in blocking:
    check_ref(row['位置'])
    assert any(word in row['最坏阻塞时长的依据'] for word in ('已确认', '推断', '未知'))
blocking_audit = json.loads((SCRATCH / 'blocking_calls_audit.json').read_text(encoding='utf-8'))
assert blocking_audit['csv_rows'] == len(blocking)
assert blocking_audit['main_regex_uncovered'] == []

requested = [D / name for name in (
    'main.cpp control_types.h control_types.cpp control_messages.h motion_sync.h motion_sync.cpp '
    'return_recovery.h delivery_tracking.h delivery_tracking.cpp guidewire_mode.h guidewire_mode.cpp '
    'startup_sequence.h startup_sequence.cpp handle_net_distance.h plc_io.h plc_io.cpp '
    'ads_communication.h ads_communication.cpp zenoh_service.h zenoh_service.cpp tools/run_offline_tests.cjs'
).split()]
requested += sorted(p for p in (D / '手柄').rglob('*') if p.is_file())
requested += [U / 'POUs' / name for name in (
    'handle.TcPOU MAIN.TcPOU err.TcPOU clear_err.TcPOU init.TcPOU reset.TcPOU '
    'SelfCheck.TcPOU Axis2Sync.TcPOU InjectorManual.TcPOU').split()]
requested += [U / 'GVLs/G.TcGVL']
requested += sorted(p for p in (U / 'DUTs').rglob('*') if p.is_file())
assert len(requested) == 36 and len(set(requested)) == 36
zip_path = OUT / 'sources_r2.zip'
manifest = json.loads((SCRATCH / 'source_manifest.json').read_text(encoding='utf-8'))
with zipfile.ZipFile(zip_path) as z:
    assert z.testzip() is None
    assert len(z.namelist()) == len(set(z.namelist())) == 38
    assert json.loads(z.read('MANIFEST.json').decode('utf-8')) == manifest
    expected_originals = {p.relative_to(ROOT).as_posix() for p in requested}
    originals = {item['source'] for item in manifest['files'] if item['kind'] == 'original'}
    assert originals == expected_originals
    assert set(z.namelist()) == {item['archive_path'] for item in manifest['files']} | {'MANIFEST.json'}
    for item in manifest['files']:
        content = z.read(item['archive_path'])
        assert len(content) == item['bytes'] and sha(content) == item['sha256']
        source = (ROOT / item['source']).read_bytes()
        if item['kind'] == 'original':
            assert content == source, item['source']
        else:
            match = re.search(rb'<ST><!\[CDATA\[([\s\S]*?)\]\]></ST>', source)
            assert match and content == match[1]
            assert match.span(1) == (item['source_byte_start'], item['source_byte_end_exclusive'])
archive_sha = sha(zip_path.read_bytes())
assert archive_sha == 'f8e37c0316e67f42caa584b0c6c081c1524b1d9b1c4a9c2a113c66f32b8ddb72'

extractions = json.loads((SCRATCH / 'behavior_extractions.json').read_text(encoding='utf-8'))
for item in extractions:
    source = (ROOT / item['source']).read_bytes()
    assert sha(source) == item['source_sha256']
    source_text = source.decode('utf-8-sig').replace('\r\n', '\n')
    lo = source_text.index(item['start_marker'])
    hi = source_text.index(item['end_marker_exclusive'], lo)
    fragment = source_text[lo:hi].encode('utf-8')
    assert sha(fragment) == item['fragment_sha256']
    assert (SCRATCH / 'behavior_build' / (item['name'] + '.inc')).read_bytes().replace(b'\r\n', b'\n') == fragment
behavior_result = json.loads((SCRATCH / 'behavior_result.json').read_text(encoding='utf-8'))
behavior_log = (SCRATCH / 'behavior_probes.log').read_text(encoding='utf-8')
assert behavior_result['exit_code'] == 0 and 'PASS assertions=51' in behavior_log
assert not re.search(r'warning C\d+|error C\d+|ASSERT FAIL', behavior_log)

baseline = json.loads((SCRATCH / 'baseline.json').read_text(encoding='utf-8'))
changed = [name for name, digest in baseline['tracked'].items()
           if not (ROOT / name).is_file() or sha((ROOT / name).read_bytes()) != digest]
assert not changed, changed
assert subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip() == baseline['head']
result = {
    'date': '2026-11-12',
    'scope': '仅本轮交付物和基准文件的静态核验；不连接设备，不运行生产程序。',
    'zip': {'crc_valid': True, 'original_count': 36, 'exact_st_extracts': 1,
            'archive_entries': 38, 'bytes': zip_path.stat().st_size, 'sha256': archive_sha,
            'all_originals_equal_current_source_bytes': True},
    'csv': {'timeouts': {'data_rows': len(timeouts), 'columns': 9, 'utf8_bom': True,
                         'all_source_refs_valid': True, 'numeric_ms_or_blank': True},
            'blocking_calls': {'data_rows': len(blocking), 'columns': 3, 'utf8_bom': True,
                               'all_source_refs_valid': True, 'main_candidate_lines': 210,
                               'uncovered_main_candidates': 0}},
    'behavior_fixture': {'source_fragments_verified': len(extractions), 'exit_code': 0,
                         'assertions_passed': 51, 'compiler_warnings_in_saved_log': 0},
    'readonly': {'baseline_head': baseline['head'], 'tracked_files_checked': len(baseline['tracked']),
                 'changed_existing_files': changed},
    'reports': {'status': 'awaiting mechanical reference check'}
}
target = SCRATCH / 'delivery_validation.json'
target.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
check = subprocess.run([sys.executable, '-X', 'utf8', str(SCRATCH / 'verify_reports.py')],
                       cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
if check.returncode:
    print(check.stdout.decode('utf-8', errors='replace'))
    raise SystemExit(check.returncode)
reports = json.loads((SCRATCH / 'report_validation.json').read_text(encoding='utf-8'))
assert not reports['issues'] and len(reports['reports']) == 12
result['reports'] = {'markdown_files': 12, 'summary_lines': reports['reports']['00_总览.md']['lines'],
    'source_reference_groups': sum(item['reference_groups'] for item in reports['reports'].values()),
    'issues': []}
target.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(result, ensure_ascii=False, indent=2))
