"""R2-4 只读盘点：列出 main.cpp 中 control_active 的直接赋值与 VisState 镜像。"""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[3]
MAIN = ROOT / "介入机器人/B数据端/工程/ADS项目/main.cpp"
OUT = Path(__file__).parent / "control_active_inventory.json"

lines = MAIN.read_text(encoding="utf-8-sig").splitlines()
items = []
for no, line in enumerate(lines, 1):
    if re.search(r"\bcontrol_active\s*=", line):
        items.append({"line": no, "text": line.strip(), "mirror": "vs.control_active" in line})

data = {
    "source": str(MAIN.relative_to(ROOT)).replace("\\", "/"),
    "direct_assignment_or_initialization_count": sum(not x["mirror"] for x in items),
    "mirror_count": sum(x["mirror"] for x in items),
    "items": items,
}
OUT.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(data, ensure_ascii=False, indent=2))
