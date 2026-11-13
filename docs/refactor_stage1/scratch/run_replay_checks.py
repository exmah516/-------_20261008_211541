from pathlib import Path
import json
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
out = root / 'docs/refactor_stage1/logs/replay_final'
out.mkdir(parents=True, exist_ok=True)
results = {}
for kind in ('handle', 'network'):
    runner = root / '介入机器人/tools/revision_uhn' / ('run_' + kind + '.py')
    source = runner.read_text(encoding='utf-8-sig')
    if kind == 'handle':
        source = source.replace('out = repo / "docs/revision_uhn_20261108/handle" / ("baseline" if baseline else "fixed")',
                                'out = Path(' + repr(str(out / kind)) + ')')
    else:
        source = source.replace('out = repo / "docs/revision_uhn_20261108/network"',
                                'out = Path(' + repr(str(out / kind)) + ')')
    with (out / (kind + '.log')).open('wb') as log:
        result = subprocess.run([sys.executable, '-c', '__file__=' + repr(str(runner)) + '\n' + source],
                                cwd=root, stdout=log, stderr=subprocess.STDOUT)
    results[kind] = result.returncode
    print(kind, result.returncode, flush=True)
    (out / 'results.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
raise SystemExit(any(results.values()))
