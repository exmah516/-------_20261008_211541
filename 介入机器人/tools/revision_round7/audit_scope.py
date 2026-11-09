"""核对极简基线、停项边界、未改路径以及NC配置中的显式速度参数。"""
from pathlib import Path
from xml.etree import ElementTree as ET
import hashlib
import json

repo = Path(__file__).resolve().parents[2]
out = repo / "docs/revision_round7_20261109"
baseline = out / "baseline"
results = {}


def check(label, condition):
    assert condition, label
    results[label] = True
    print("PASS", label)


def text(path):
    return path.read_text(encoding="utf-8-sig")


gen = "C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU"
reference = "B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h"
check("PLC生产算法仅修改JOINT_VCAP", text(repo / gen).replace("[24,30,30,30,30]", "[24,10,10,10,10]") == text(baseline / gen))
check("C++生产参考仅修改JOINT_VCAP", text(repo / reference).replace("{{24,30,30,30,30}}", "{{24,10,10,10,10}}") == text(baseline / reference))
for name in ["ArmCartJog", "ArmCartesian"]:
    file = "C从端/250902/Untitled2/POUs/" + name + ".TcPOU"
    check(name + "与本轮前逐字节相同", (repo / file).read_bytes() == (baseline / file).read_bytes())
file = "B数据端/工程/PositioningArmKinematics/src/kinematics.cpp"
check("运动学与本轮前逐字节相同", (repo / file).read_bytes() == (baseline / file).read_bytes())
check("极简零位已在修改前源码中", "const double tool_yaw = joints[1] + joints[2] + joints[3];" in text(baseline / file))
panel = ET.parse(baseline / "A主端/工程/master/VI.Module.PositioningArm/PositioningArmPanel.xaml")
named = {e.get("{http://schemas.microsoft.com/winfx/2006/xaml}Name"): e for e in panel.iter()}
for name, direction in [("Cartesian3Forward", "-1"), ("Cartesian3Reverse", "1")]:
    e = named[name]
    check("极简Z轴基线 " + name, e.get("Tag") == "1" and e.get("CommandParameter") == direction and e.get("PreviewMouseLeftButtonDown") == "ArmJogButton_Down")
code = text(baseline / "A主端/工程/master/VI.Module.PositioningArm/PositioningArmPanel.xaml.cs")
check("极简总使能及五轴复位基线", "if (enabled) await _viewModel.SetArmManualEnableAsync(true);" in code
    and "await _viewModel.SetArmAxisEnableAsync(axis, enabled);" in code
    and "if (!enabled) await _viewModel.SetArmManualEnableAsync(false);" in code
    and "await _viewModel.ResetArmAxisAsync(axis);" in code)
for tier in ["Small", "Medium", "Large"]:
    check("极简辅助列定宽基线 " + tier, "AuxStatusWidth" in text(baseline / ("A主端/工程/master/VI.Control/Themes/Sizes" + tier + ".xaml")))

tree = ET.parse(repo / "C从端/250902/250902.tsproj")
parents = {child: parent for parent in tree.iter() for child in parent}
nc = {}
for axis in tree.iter("Axis"):
    number = int(axis.get("Id"))
    if number in range(1, 6):
        velocity = next(axis.iter("Velo"), None)
        nc[number] = dict(velocity.attrib) if velocity is not None else {}
links = [dict(owner=parents[e].get("Name"), **e.attrib) for e in tree.iter("Link") if "arm_axis[" in str(e.attrib)]
results["nc_velocity_parameters"] = nc
results["nc_links"] = links
results["source_hashes"] = {file: hashlib.sha256((repo / file).read_bytes()).hexdigest() for file in [gen, reference]}
(out / "static_audit.json").write_text(json.dumps(results, indent=2, ensure_ascii=False), encoding="utf-8")
