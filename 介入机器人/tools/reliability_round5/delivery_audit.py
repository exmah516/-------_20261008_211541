"""核对交付文件、原编码、PLC XML和基线；--normalize仅恢复原BOM/换行。"""
from pathlib import Path
import hashlib
import json
import re
import sys
import zipfile
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[2]
out = repo / "docs/reliability_round5"
baseline = zipfile.ZipFile(out / "baseline/source.zip")
normalizations = []
changed = []
for name in baseline.namelist():
    path = repo / name
    if not path.exists():
        continue
    old, new = baseline.read(name), path.read_bytes()
    if old == new:
        continue
    text = new.decode("utf-8-sig").replace("\r\n", "\n")
    if old.count(b"\n") == old.count(b"\r\n"):
        text = text.replace("\n", "\r\n")
    expected = (b"\xef\xbb\xbf" if old.startswith(b"\xef\xbb\xbf") else b"") + text.encode("utf-8")
    if new != expected:
        normalizations.append(name)
        if "--normalize" in sys.argv:
            path.write_bytes(expected)
        else:
            raise AssertionError("BOM/换行与原文件不同：" + name)
    changed.append(name)
if "--normalize" in sys.argv:
    (out / "encoding_restored.json").write_text(json.dumps(normalizations, ensure_ascii=False, indent=2), encoding="utf-8")

def section(text, start, end):
    begin=text.index(start)
    return text[begin:text.index(end,begin)]
main_path="B数据端/工程/ADS项目/main.cpp"
old_main=baseline.read(main_path).decode("utf-8-sig").replace("\r\n","\n")
new_main=(repo/main_path).read_text(encoding="utf-8-sig")
assert section(old_main,"\tauto lock_handle_mode =","\n\tif (axis1_handle_ready") == section(new_main,"\tauto lock_handle_mode =","\n\tif (axis1_handle_ready")
assert section(old_main,"\t\tif (ads_stats.reconnect_count !=","\t\t// 运动控制只依赖") == section(new_main,"\t\tif (ads_stats.reconnect_count !=","\t\t// 运动控制只依赖")
handle_path="C从端/250902/Untitled2/POUs/handle.TcPOU"
old_handle=baseline.read(handle_path).decode("utf-8-sig").replace("\r\n","\n")
new_handle=(repo/handle_path).read_text(encoding="utf-8-sig")
assert section(old_handle,"\t\t\tIF return_state[i] <> 30 THEN","\n\t\t\ttraj_vel[i]") == section(new_handle,"\t\t\tIF return_state[i] <> 30 THEN","\n\t\t\ttraj_vel[i]")

plc = repo / "C从端/250902/Untitled2"
plc_changed = [name for name in changed if name.endswith((".TcPOU", ".TcGVL"))]
for name in plc_changed:
    ET.parse(repo / name)
project = ET.parse(next(plc.glob("*.plcproj")))
references = {node.attrib["Include"].replace("\\", "/") for node in project.iter() if "Include" in node.attrib}
for name in plc_changed:
    relative = (repo / name).relative_to(plc).as_posix()
    assert relative in references, relative

untouched = []
for name in baseline.namelist():
    if name.startswith("B数据端/工程/master"):
        continue
    if ("PositioningArmKinematics" in name or re.search(r"/POUs/(Arm[^/]*|SelfCheck|clear_err)\.TcPOU$", name)):
        assert (repo / name).read_bytes() == baseline.read(name), name
        untouched.append(name)

sizes = {}
for row in (repo / "B数据端/工程/ADS项目/tools/ads_symbol_sizes.tsv").read_text(encoding="utf-8").splitlines():
    if row and not row.startswith("#"):
        name, size = row.split("\t")
        sizes[name] = int(size)
for name, size in {
    "G.err_backoff_active": 7, "G.err_backoff_failed": 7,
    "G.axis2_sync_error": 1, "G.axis2_sync_error_id": 4,
    "G.homing_restore_req": 1, "G.homing_restore_status": 4,
    "G.homing_restore_reject_reason": 4,
    "G.homing_restore_leftlimit": 56, "G.homing_restore_init_pos": 56,
}.items():
    assert sizes[name] == size, name
summary = {
    "changed_baseline_files": len(changed),
    "encoding": "PASS UTF-8；BOM及纯LF/CRLF保持原文件",
    "plc_xml_and_project_references": plc_changed,
    "plc_added_symbol_sizes": "PASS BOOL=1,DINT/UDINT=4,LREAL[7]=56,BOOL[7]=7",
    "unchanged_local_guards":"PASS B08模式锁、既有reconnect_count块、状态30恒假分支全文一致",
    "unchanged_source_files": untouched,
    "plc_build": "未构建；本机没有已配置的TwinCAT XAE构建工具",
    "sha256": {name: hashlib.sha256((repo/name).read_bytes()).hexdigest() for name in changed},
}
(out/"delivery_audit.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
print("PASS encoding, PLC XML/project references, added symbol sizes, unchanged Arm/SelfCheck/clear_err")
print("Changed baseline files:", len(changed), "unchanged source guards:", len(untouched))
