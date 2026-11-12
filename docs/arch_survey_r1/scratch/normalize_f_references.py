"""将 F 报告内缩写证据路径展开为仓库根相对路径，并检查引用行号。"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[3]
report = root / 'docs/arch_survey_r1/F_测试现状.md'
ads = '介入机器人/B数据端/工程/ADS项目/'
kin = '介入机器人/B数据端/工程/PositioningArmKinematics/'
master = '介入机器人/A主端/工程/master/'
logs = 'docs/arch_survey_r1/scratch/tests/'
arm = logs + 'arm/round4/'
native = 'docs/arch_survey_r1/scratch/tests_native/'
mapping = {
    'Program.cs': master + 'VI.Control.Tests/Program.cs',
    'NetworkSmoke.cs': master + 'MasterConsole.Protocol.Tests/NetworkSmoke.cs',
    'HandleHostTests.cs': master + 'MasterConsole.Protocol.Tests/HandleHostTests.cs',
    'st_runtime.h': '介入机器人/tools/reliability_round5/st_runtime.h',
    'translate_st.py': '介入机器人/tools/reliability_round5/translate_st.py',
    'run_host_sdk.py': '介入机器人/tools/reliability_round5/run_host_sdk.py',
    'run_offline_tests.cjs': ads + 'tools/run_offline_tests.cjs',
    'main.cpp': ads + 'main.cpp',
    'C_logs_notes.md': 'docs/arch_survey_r1/scratch/C_logs_notes.md',
}
for path in (root / (ads + 'tools')).glob('*.cpp'):
    mapping[path.name] = ads + 'tools/' + path.name
for path in (root / (kin + 'tests')).glob('*.cpp'):
    mapping[path.name] = kin + 'tests/' + path.name
for path in (root / arm).glob('*.log'):
    mapping[path.name] = arm + path.name
for path in (root / logs).glob('*.log'):
    mapping[path.name] = logs + path.name
for path in (root / native).glob('*.log'):
    mapping[path.name] = native + path.name


def expand(path):
    if (root / path).is_file():
        return path
    if path in mapping:
        return mapping[path]
    if path.startswith(('tools/reliability_round5/', 'tools/revision_')):
        return '介入机器人/' + path
    if path.startswith(('tools/', 'tests/zenoh/')):
        return ads + path
    if path.startswith('MasterConsole.Protocol.Tests/'):
        return master + path
    if path.startswith('PositioningArmKinematics/'):
        return '介入机器人/B数据端/工程/' + path
    if path.startswith('tests/'):
        return kin + path
    if path.startswith('arm/round4/'):
        return logs + path
    if path.startswith(('revision_round7_', 'docs/revision_')):
        return '介入机器人/' + (path if path.startswith('docs/') else 'docs/' + path)
    return path


text = report.read_text(encoding='utf-8')
text = text.replace('；日志`:119`', '')
lines = []
for line in text.splitlines():
    last = None
    def replace(match):
        global last
        token = match[1]
        ref = re.fullmatch(r'(.*?):([0-9]+(?:-[0-9]+)?)', token)
        if not ref:
            return match[0]
        file, number = ref.groups()
        if file == '':
            file = last
        else:
            file = expand(file)
        if not file or not (root / file).is_file():
            return match[0]
        last = file
        return '`' + file + ':' + number + '`'
    line = re.sub(r'`([^`]+)`', replace, line)
    lines.append(line)
text = '\n'.join(lines) + '\n'
text = text.replace('arm_ads_tests.log:19', 'arm_ads_tests.log:8')
text = text.replace('st_static_audit_round4.log:61`', 'st_static_audit_round4.log:63`')
report.write_text(text, encoding='utf-8')

for n,line in enumerate(text.splitlines(), 1):
    for token in re.findall(r'`([^`]+)`', line):
        ref = re.fullmatch(r'(.*?):([0-9]+)(?:-([0-9]+))?', token)
        if not ref or ref[1].startswith(('tcp/', '127.0.0.1')):
            continue
        path = root / ref[1]
        if not path.is_file():
            print('UNRESOLVED', n, token)
            continue
        line_count = path.read_bytes().count(b'\n') + 1
        if int(ref[2]) > line_count or ref[3] and int(ref[3]) > line_count:
            print('LINE_OUT_OF_RANGE', n, token, line_count)
print('report lines', len(text.splitlines()))
