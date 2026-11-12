"""只读统计仓库内已有记录；不把记录采样间隔当成 main 周期。"""
from pathlib import Path
import collections
import csv
import hashlib
import json
import math
import re
import statistics

root = Path(__file__).resolve().parents[3]
out = Path(__file__).resolve().parent
session_roots = [
    root / "介入机器人/B数据端/工程/ADS项目/records",
    root / "介入机器人/B数据端/工程/ADS项目/x64/Release/records",
]


def rel(path):
    return path.relative_to(root).as_posix()


def distribution(values):
    if not values:
        return None
    ordered = sorted(values)
    def nearest(p):
        return ordered[max(0, math.ceil(len(ordered) * p) - 1)]
    return dict(n=len(values), minimum=min(values), median=statistics.median(values),
                p95=nearest(.95), p99=nearest(.99), maximum=max(values),
                mean=statistics.mean(values), nonpositive=sum(v <= 0 for v in values))


sessions = []
for folder in session_roots:
    for path in sorted(folder.glob("**/session.json")):
        info = json.loads(path.read_text(encoding="utf-8-sig"))
        entry = dict(path=rel(path), metadata=info, csv=[])
        for csv_path in sorted(path.parent.glob("*.csv")):
            with csv_path.open(encoding="utf-8-sig", newline="") as stream:
                reader = csv.DictReader(stream)
                fields = reader.fieldnames or []
                rows = list(reader)
            elapsed = [int(r['elapsed_us']) for r in rows if r.get('elapsed_us')]
            validity = {
                field: dict(collections.Counter(r[field] for r in rows))
                for field in fields if field.endswith('_valid') or field == 'position_valid'
            }
            floats = {}
            for field in fields:
                if not any(v in field for v in ('mm', 'newton', 'force', 'mv', 'velocity', '_raw_v', '_zero_v', '_torque', 'ff_')):
                    continue
                numeric = []
                for row in rows:
                    try:
                        numeric.append(float(row[field]))
                    except (ValueError, TypeError):
                        pass
                if numeric:
                    finite = [v for v in numeric if math.isfinite(v)]
                    floats[field] = dict(total=len(numeric), finite=len(finite),
                                         nonzero=sum(v != 0 for v in finite))
            events = dict(collections.Counter(r['event_code'] for r in rows)) if 'event_code' in fields else None
            entry['csv'].append(dict(path=rel(csv_path), fields=fields, row_count=len(rows),
                                     last_line=len(rows) + 1, first_elapsed_us=elapsed[0] if elapsed else None,
                                     last_elapsed_us=elapsed[-1] if elapsed else None,
                                     interval_us=distribution([b-a for a,b in zip(elapsed, elapsed[1:])]),
                                     validity=validity, numeric=floats, events=events))
        sessions.append(entry)

logs = []
for path in [root / '介入机器人/B数据端/工程/ADS项目/x64/Release/ads_fault_diagnostics.log',
             root / '介入机器人/B数据端/工程/ADS项目/x64/Debug/ads_fault_diagnostics.log',
             root / 'ADS项目/ADS项目/x64/Debug/ads_fault_diagnostics.log']:
    data = path.read_bytes()
    lines = data.decode('utf-8-sig').splitlines()
    logs.append(dict(path=rel(path), lines=len(lines), sha256=hashlib.sha256(data).hexdigest(),
                     connect=sum('CONNECT:' in line for line in lines),
                     port_open_failure=sum('AdsPortOpenEx failed' in line for line in lines),
                     read_state_6=sum('AdsSyncReadStateReqEx: 6 ' in line for line in lines),
                     read_state_1861=sum('AdsSyncReadStateReqEx: 1861 ' in line for line in lines),
                     rtt_fields=sum(bool(re.search(r'\brtt', line, re.I)) for line in lines),
                     other_lines=[dict(line=i, text=line) for i,line in enumerate(lines, 1) if 'CONNECT:' not in line]))

result = dict(quantiles='nearest rank; median is conventional middle/mean of middle pair',
              sessions=sessions, logs=logs)
(out / 'existing_records_stats.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
assert len(sessions) == 4
for session in sessions:
    print(session['path'])
    for item in session['csv']:
        print(' ', Path(item['path']).name, item['row_count'], item['interval_us'], item['validity'], item['events'])
for log in logs:
    print(log['path'], {k:v for k,v in log.items() if k not in ['path','other_lines','sha256']})
