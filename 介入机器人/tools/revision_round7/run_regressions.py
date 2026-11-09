"""第七轮ADS、Zenoh、本体、定位臂既有回归；原有黄金文件保持不变。"""
from pathlib import Path
repo = Path(__file__).resolve().parents[2]
script = repo / "tools/revision_u_v3/run_regressions.py"
source = script.read_text(encoding="utf-8").replace("docs/revision_u_v3_20261109", "docs/revision_round7_20261109/final_results")
(repo / "docs/revision_round7_20261109/final_results").mkdir(parents=True, exist_ok=True)
exec(compile(source, str(script), "exec"), {"__file__": str(script)})
