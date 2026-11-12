"""Finish explicit citations and list cross-report links for a final semantic audit."""
from pathlib import Path
import re

OUT=Path(__file__).resolve().parents[1]
p=OUT/'00_总览.md'
t=p.read_text(encoding='utf-8-sig').replace('scratch/survey.py:63-71','scratch/survey.py:53-60')
p.write_text(t,encoding='utf-8')
p=OUT/'C_时间敏感动作.md'
t=p.read_text(encoding='utf-8-sig')
for stem in ('force','motion'):
    t=t.replace(f'/{stem}.cs:',f'/{stem}.csv:')
assert '/motion.cs:' not in t
p.write_text(t,encoding='utf-8')
pattern=re.compile(r'(?P<name>[A-F]_[^`/:]+\.md):(?P<start>\d+)(?:[-–](?P<end>\d+))?')
for p in sorted(OUT.glob('*.md')):
    for line_no,line in enumerate(p.read_text(encoding='utf-8-sig').splitlines(),1):
        for m in pattern.finditer(line):
            target=OUT/m['name']
            if not target.is_file():
                continue
            lines=target.read_text(encoding='utf-8-sig').splitlines()
            start=int(m['start']);end=int(m['end'] or start)
            text=' / '.join(x[:170] for x in lines[start-1:min(end,start+1)])
            print(f'{p.name}:{line_no} -> {m[0]} => {text}')
