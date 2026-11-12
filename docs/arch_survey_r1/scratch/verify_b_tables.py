from pathlib import Path
import json
import re

doc = Path(__file__).resolve().parents[1] / 'B_main解剖.md'
issues = []
tables = 0
rows = 0
expected = None
fenced = False
for number, line in enumerate(doc.read_text(encoding='utf-8-sig').splitlines(), 1):
    if line.lstrip().startswith('```'):
        fenced = not fenced
        expected = None
        continue
    if fenced or not line.startswith('|'):
        expected = None
        continue
    count = len(re.findall(r'(?<!\\)\|', line)) - 1
    if expected is None:
        tables += 1
        expected = count
    elif count != expected:
        issues.append({'line': number, 'expected_columns': expected, 'actual_columns': count, 'text': line})
    rows += 1
result = {'tables': tables, 'rows': rows, 'issues': issues}
(doc.parent / 'scratch/B_table_validation.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print(json.dumps(result, ensure_ascii=False, indent=2))
