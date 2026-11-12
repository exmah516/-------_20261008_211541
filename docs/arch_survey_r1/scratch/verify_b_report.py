from pathlib import Path
import csv
import json
import re

ROOT = Path(__file__).resolve().parents[3]
DOC = ROOT / 'docs/arch_survey_r1/B_main解剖.md'
ADS = ROOT / '介入机器人/B数据端/工程/ADS项目'
text = DOC.read_text(encoding='utf-8-sig')
pattern = re.compile(r'(?P<path>[A-Za-z0-9_\u4e00-\u9fff./\\-]+\.(?:cpp|h|TcPOU)):(?P<nums>\d+(?:[-–]\d+)?(?:[,、]\d+(?:[-–]\d+)?)*)')
issues = []
checked = []
for m in pattern.finditer(text):
    name = m['path']
    p = ROOT / name
    if not p.exists():
        p = ADS / name
    if not p.exists():
        issues.append({'reference': m[0], 'issue': 'file_not_found'})
        continue
    line_count = len(p.read_text(encoding='utf-8-sig').splitlines())
    refs = list(map(int, re.findall(r'\d+', m['nums'])))
    if any(n < 1 or n > line_count for n in refs):
        issues.append({'reference': m[0], 'issue': 'line_out_of_range', 'actual_lines': line_count})
    checked.append(m[0])
inventory = list(csv.DictReader((DOC.parent / 'scratch/main_outer_inventory.tsv').open(encoding='utf-8-sig'), delimiter='\t'))
missing_names = [r['name'] for r in inventory if not re.search(r'\b'+re.escape(r['name'])+r'\b', text)]
result = {'checked_reference_groups': len(checked), 'issues': issues, 'outer_declarations': len(inventory), 'names_not_mentioned': missing_names}
(DOC.parent / 'scratch/B_reference_validation.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print(json.dumps(result, ensure_ascii=False, indent=2))
