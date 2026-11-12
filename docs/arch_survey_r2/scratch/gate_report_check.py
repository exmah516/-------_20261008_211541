"""检查 R2-4 报告的引用路径、行范围、赋值覆盖及短代码块；不执行生产程序。"""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[3]
REPORT = ROOT / 'docs/arch_survey_r2/R2-4_控制门控与可见性.md'
text = REPORT.read_text(encoding='utf-8')
lines = text.splitlines()
aliases = {}
for line in lines:
    m = re.fullmatch(r'\| ([DM]/[A-Za-z_]+) \| `([^`]+)` \|', line)
    if m:
        aliases[m[1]] = ROOT / m[2]

errors = []
refs = []
for m in re.finditer(r'\b([DM]/[A-Za-z_]+):(\d+)(?:-(\d+))?', text):
    alias, first, last = m[1], int(m[2]), int(m[3] or m[2])
    path = aliases.get(alias)
    if path is None or not path.is_file():
        errors.append(f'undefined alias {alias}')
        continue
    length = len(path.read_text(encoding='utf-8-sig').splitlines())
    if not (1 <= first <= last <= length):
        errors.append(f'bad range {alias}:{first}-{last} of {length}')
    refs.append({'alias': alias, 'first': first, 'last': last})

fences = []
begin = None
for no, line in enumerate(lines, 1):
    if line.startswith('```'):
        if begin is None:
            begin = no
        else:
            fences.append({'first': begin, 'last': no, 'content_lines': no - begin - 1})
            if no - begin - 1 > 20:
                errors.append(f'long code block {begin}-{no}')
            begin = None
if begin is not None:
    errors.append(f'unclosed code block at {begin}')

inventory = json.loads((Path(__file__).parent / 'control_active_inventory.json').read_text(encoding='utf-8'))
for item in inventory['items']:
    if f'D/main:{item["line"]}' not in text:
        errors.append(f'missing control_active reference {item["line"]}')

result = {'report_lines': len(lines), 'alias_count': len(aliases), 'reference_count': len(refs),
          'direct_assignment_or_initialization_count': inventory['direct_assignment_or_initialization_count'],
          'fences': fences, 'errors': errors}
(Path(__file__).parent / 'gate_report_check.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(result, ensure_ascii=False, indent=2))
assert not errors
