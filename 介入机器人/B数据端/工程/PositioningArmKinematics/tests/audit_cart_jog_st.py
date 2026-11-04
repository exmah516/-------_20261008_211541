"""离线语法与本轮接口约束自查；不调用TwinCAT、不连接PLC。"""
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[4]
plc = repo / "C从端/250902/Untitled2"


def check(ok, description):
    print(("PASS " if ok else "FAIL ") + description)
    assert ok, description


def st(name):
    return ET.parse(plc / ("POUs/" + name + ".TcPOU")).find(".//ST").text


for path in [plc / "GVLs/G.TcGVL", plc / "Untitled2.plcproj", plc / "PlcTask.TcTTO"] + [
    plc / ("POUs/" + name + ".TcPOU")
    for name in ["ArmCartJogGen", "ArmCartJog", "ArmCartesian", "ArmManual", "MAIN"]
]:
    root = ET.parse(path)
    check(True, "XML " + str(path.relative_to(repo)))
    for source in root.findall(".//ST"):
        body = re.sub(r"//[^\n]*|\(\*.*?\*\)", "", source.text, flags=re.S)
        stack = []
        pairs = {"END_IF": "IF", "END_FOR": "FOR", "END_WHILE": "WHILE", "END_CASE": "CASE"}
        for token in re.findall(r"\b(?:IF|END_IF|FOR|END_FOR|WHILE|END_WHILE|CASE|END_CASE)\b", body):
            if token in pairs:
                assert stack.pop() == pairs[token]
            else:
                stack.append(token)
        check(not stack, "ST block nesting " + path.stem)

gen = st("ArmCartJogGen")
wrapper = st("ArmCartJog")
home = st("ArmCartesian")
manual = st("ArmManual")
main = st("MAIN")
decl = ET.parse(plc / "POUs/ArmCartJogGen.TcPOU").find(".//Declaration").text
project = (plc / "Untitled2.plcproj").read_text(encoding="utf-8-sig")
check("arm_cartesian_();\narm_cart_jog_();" in main.replace("\r", ""), "MAIN call order unchanged")
for name in ["ArmCartJogGen", "ArmCartJog"]:
    check("POUs\\" + name + ".TcPOU" in project, "PLC registers " + name)
check("<CycleTime>1000</CycleTime>" in (plc / "PlcTask.TcTTO").read_text(encoding="utf-8-sig"), "PLC task 1000 us")
check(not re.search(r"\bG\.|MC_|AXIS_REF", gen + decl), "Gen has no global or axis I/O")
check("delta_sigma_des := 0.5*(path_v_previous+path_v)*dt;" in gen, "nominal-speed trapezoidal path increment")
check("delta_sigma := MIN(delta_sigma_des, JOINT_VCAP[j]*dt);" in gen, "single-axis increment cap")
check("BISECTION_COUNT : INT := 12;" in decl and "FOR i := 0 TO BISECTION_COUNT DO" in gen
      and "IF i = 0 THEN EXIT; END_IF" in gen, "full-step trial, otherwise exactly 12 bisections")
check("candidate_radius >= L1-L2" in gen and "candidate_radius <= L1+L2" in gen, "exact reachable annulus")
for i in [2, 3, 4]:
    check(f"ABS(candidate_q{i}-p[{i}]) <= JOINT_VCAP[{i}]*dt" in gen, f"axis {i} trial increment cap")
check("candidate_cosine := 1.0" in gen and "candidate_cosine := -1.0" in gen,
      "ACOS cosine has only prescribed [-1,1] clamp")
check("IF delta_sigma > 0 THEN" in gen and "p[2] := best_q2" in gen, "no accepted step preserves position")
check("INITIAL_BRANCH : INT := 1;" in decl and all(f"branch_q3 <> {q}" in gen for q in ["0.0", "180.0", "-180.0"]),
      "ambiguous folded/straight elbow retains prior branch; initial +1")
check("v[i] := (p[i]-position_before[i])/dt;" in gen
      and "a[i] := (v[i]-velocity_before[i])/dt;" in gen, "feed velocity/acceleration are finite differences")
check("path_speed := delta_sigma/dt;" in gen and "limited := delta_sigma < delta_sigma_des;" in gen,
      "executed path speed and limited flag")
check("[24,10,10,10,10]" in decl, "joint caps 24 mm/s, 10/10/10/10 deg/s")
check(not re.search(r"Q3_MARGIN|\bg\[|\bk\b|j11|j12|j21|j22|\bdet\b|isfinite|NaN", gen + decl),
      "old margin/Jacobian/k removed; no internal finite checks")
check("stage :=" not in gen, "mode/direction changes stay in wrapper stage 25")
ready = lambda source: source[source.index("drive_ready :="):source.index("END_FOR", source.index("drive_ready :="))]
check(re.sub(r"\s+", "", ready(wrapper)) == re.sub(r"\s+", "", ready(home)), "drive_ready identical to home")
for api in ["MC_ExtSetPointGenEnable", "MC_ExtSetPointGenDisable", "MC_Stop", "MC_ExtSetPointGenFeed"]:
    check(api in (plc / "POUs/ArmCartesian.TcPOU").read_text(encoding="utf-8-sig")
          and api in (plc / "POUs/ArmCartJog.TcPOU").read_text(encoding="utf-8-sig"), "existing Tc2_MC2 API " + api)
check("G.arm_cjog_owned AND (stage = 10 OR stage = 25)" in wrapper, "Feed only owned stages 10/25")
check("IF G.arm_cjog_owned THEN reject_error := 2001; END_IF" in home
      and "NOT G.arm_cjog_owned" in manual, "home/manual exclude jog ownership")
kin = repo / "B数据端/工程/PositioningArmKinematics"
source = (kin / "src/kinematics.cpp").read_text(encoding="utf-8-sig")
check("result.position << 0, 0, joints[0];" in source and "next[0] += command.translation.z();" in source,
      "kinematics z=+q1 retained")
check("base_offset_mm = Eigen::Vector3d::Zero()" in (kin / "include/positioning_arm/kinematics.h").read_text(encoding="utf-8-sig"),
      "base offset default zero retained")
result = subprocess.run(["rg", "-n", "TcUnit", str(plc), "-g", "*.TcPOU", "-g", "*.plcproj"], capture_output=True)
check(result.returncode == 1, "no TcUnit in PLC project; no framework added")
print("REVIEW target TwinCAT 3.1.4024: BOOL/INT/LREAL/fixed ARRAY, IF/FOR/WHILE/EXIT, array assignment, SIN/COS/ACOS/ATAN/SQRT/ABS/MIN")
print("NOT BUILT 未构建: no TwinCAT XAE/PLC compiler; static checks and ST translation do not compile or run Tc2_MC2")
