"""仅替换阻尼函数后的第七轮完整复验；单独保存输出，不覆盖原停项证据。"""
from pathlib import Path
import json
import os
import subprocess
import sys

repo = Path(__file__).resolve().parents[2]
previous = "docs/revision_round7_20261109"
current = "docs/revision_round7_smooth_20261109"
out = repo / current
out.mkdir(parents=True, exist_ok=True)
result_file = out / "runner_results.json"
results = json.loads(result_file.read_text(encoding="utf-8")) if result_file.exists() else {}
failed = False
env = dict(os.environ, PYTHONUTF8="1", PYTHONIOENCODING="utf-8", VSLANG="1033", DOTNET_CLI_UI_LANGUAGE="en")

for name in (sys.argv[1:] or ["dls", "e11", "home_chain", "managed", "regressions"]):
    script = repo / ("tools/revision_round7/run_smooth_e11.py" if name == "e11" else "tools/revision_round7/run_" + name + ".py")
    source = script.read_text(encoding="utf-8")
    if name != "e11":
        source = source.replace(previous, current)
    source = source.replace("build/Round7Release/bin", "build/Round7SmoothRelease/bin")
    if name == "home_chain":
        source = source.replace('setup = setup.replace(',
            'setup = setup.replace(' + repr(previous) + ', ' + repr(current) + ')\nsetup = setup.replace(', 1)
    code = "exec(compile(" + repr(source) + ", " + repr(str(script)) + ", 'exec'), {'__file__': " + repr(str(script)) + "})"
    with (out / (name + "_runner.log")).open("wb") as log:
        result = subprocess.run([sys.executable, "-c", code], cwd=repo, env=env,
            stdout=log, stderr=subprocess.STDOUT)
    results[name] = result.returncode
    failed = failed or result.returncode != 0
    print(name, "exit=" + str(result.returncode), str(out / (name + "_runner.log")), flush=True)
result_file.write_text(json.dumps(results, indent=2), encoding="utf-8")
raise SystemExit(1 if failed else 0)
