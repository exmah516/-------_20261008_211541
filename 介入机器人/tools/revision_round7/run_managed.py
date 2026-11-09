"""第七轮主端/宿主构建与全部既有托管测试；保留每项失败结果。"""
from pathlib import Path
repo = Path(__file__).resolve().parents[2]
script = repo / "tools/revision_u_v3/run_managed.py"
source = script.read_text(encoding="utf-8").replace("docs/revision_u_v3_20261109", "docs/revision_round7_20261109/final_results")
source = source.replace("build/UV3Release/bin", "build/Round7Release/bin")
source = source.replace("    assert result.returncode == 0, str(out / (name + \".log\"))", "    if result.returncode: print(str(out / (name + \".log\")), flush=True)")
scope = {"__file__": str(script)}
exec(compile(source, str(script), "exec"), scope)
raise SystemExit(1 if any(scope["results"].values()) else 0)
