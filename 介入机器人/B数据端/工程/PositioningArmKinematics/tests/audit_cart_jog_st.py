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
check("delta_sigma_des := 0.5*(path_v_previous+path_v)*cycle_dt;" in gen, "nominal-speed trapezoidal path increment")
check("delta_sigma := MIN(delta_sigma_des, JOINT_VCAP[j]*cycle_dt);" in gen, "single-axis increment cap")
check("BISECTION_COUNT : INT := 12;" in decl and "FOR i := 0 TO BISECTION_COUNT DO" in gen
      and "IF i = 0 THEN EXIT; END_IF" in gen, "full-step trial, otherwise exactly 12 bisections")
check("candidate_radius >= L1-L2" in gen and "candidate_radius <= L1+L2" in gen, "exact reachable annulus")
for i in [2, 3, 4]:
    check(f"ABS(candidate_q{i}-p[{i}]) <= JOINT_VCAP[{i}]*cycle_dt" in gen, f"axis {i} trial increment cap")
check("candidate_cosine := 1.0" in gen and "candidate_cosine := -1.0" in gen,
      "ACOS cosine has only prescribed [-1,1] clamp")
check("IF delta_sigma > 0 THEN" in gen and "p[2] := best_q2" in gen, "no accepted step preserves position")
check("INITIAL_BRANCH : INT := 1;" in decl and all(f"branch_q3 <> {q}" in gen for q in ["0.0", "180.0", "-180.0"]),
      "ambiguous folded/straight elbow retains prior branch; initial +1")
check("v[i] := (p[i]-position_before[i])/cycle_dt;" in gen
      and "a[i] := (v[i]-velocity_before[i])/cycle_dt;" in gen, "feed velocity/acceleration are finite differences")
check("path_speed := delta_sigma/cycle_dt;" in gen and "limited := delta_sigma < delta_sigma_des;" in gen,
      "executed path speed and limited flag")
check("[24,10,10,10,10]" in decl, "joint caps 24 mm/s, 10/10/10/10 deg/s")
check("TOOL_LATERAL_OFFSET_MM : LREAL := 456.3;" in decl
      and "TOOL_LENGTH_MM : LREAL := 554.5;" in decl, "TCP tool offsets match kinematics.h in millimetres")
check("IF latched_mode = 5 THEN" in gen
      and "candidate_psi := psi0+latched_dir*(sigma+trial_step);" in gen
      and "candidate_wx := tcp0x-d0x*COS(candidate_psi*RAD)+d0y*SIN(candidate_psi*RAD);" in gen
      and "candidate_wy := tcp0y-d0x*SIN(candidate_psi*RAD)-d0y*COS(candidate_psi*RAD);" in gen,
      "yaw trial recomputes wrist from anchored TCP and cumulative yaw")
check("d0y := -TOOL_LENGTH_MM*COS(anchor[5]*RAD);" in gen
      and "tcp0x := wx+d0x*COS(psi0*RAD)-d0y*SIN(psi0*RAD);" in gen
      and "tcp0y := wy+d0x*SIN(psi0*RAD)+d0y*COS(psi0*RAD);" in gen,
      "yaw anchor uses the q5-dependent horizontal tool offset")
check("candidate_q4 := anchor[4]+(candidate_psi-psi0)-((candidate_q2+candidate_q3)-(anchor[2]+anchor[3]));" in gen,
      "yaw inverse compensates axis 4 to track requested yaw")
check("IF latched_mode = 3 THEN j := 1;\n    ELSIF latched_mode = 4 THEN j := 5;" in gen
      and "ELSIF latched_mode = 5" not in gen,
      "mode 5 is no longer a single-axis jog")
check(not re.search(r"Q3_MARGIN|\bg\[|\bk\b|j11|j12|j21|j22|\bdet\b|isfinite|NaN", gen + decl),
      "old margin/Jacobian/k removed; no internal finite checks")
check("stage :=" not in gen, "mode/direction changes stay in wrapper stage 25")
ready = lambda source: source[source.index("drive_ready :="):source.index("END_FOR", source.index("drive_ready :="))]
check(re.sub(r"\s+", "", ready(wrapper)) == re.sub(r"\s+", "", ready(home)), "drive_ready identical to home")
for api in ["MC_ExtSetPointGenEnable", "MC_ExtSetPointGenDisable", "MC_Stop", "MC_ExtSetPointGenFeed"]:
    check(api in (plc / "POUs/ArmCartesian.TcPOU").read_text(encoding="utf-8-sig")
          and api in (plc / "POUs/ArmCartJog.TcPOU").read_text(encoding="utf-8-sig"), "existing Tc2_MC2 API " + api)
check("G.arm_cjog_owned AND (stage = 10 OR stage = 25)" in wrapper, "Feed only owned stages 10/25")
for name, body, error in [("ArmCartJog", wrapper, "arm_cjog_error"), ("ArmCartesian", home, "arm_curve_error")]:
    check("Execute:=(stage=50) AND NOT G.arm_axis[i].Status.Error" in body,
          name + " skips MC_Stop on NC error axis")
    check("all_disabled AND (fb_disable[i].Done OR fb_disable[i].Error OR G.arm_axis[i].Status.Error)" in body,
          name + " Disable Done/Error/axis error completes stage 40")
    check("all_stopped AND (fb_stop[i].Done OR fb_stop[i].Error OR G.arm_axis[i].Status.Error)" in body,
          name + " Stop Done/Error/axis error completes stage 50")
    check("all_released := all_released AND NOT fb_stop[i].Busy;" in body,
          name + " stage 60 waits only for Stop Busy to clear")
    check("IF G." + error + " = 0 THEN\n            IF fb_enable[i].Error" in body,
          name + " FB errors preserve first cause")
for name in ["ArmCartJogGen.TcPOU", "ArmManual.TcPOU"]:
    working = (plc / "POUs" / name).read_text(encoding="utf-8-sig").replace("\r\n", "\n")
    before = subprocess.check_output(["git", "show", "713d8830aaf04cd23b1547c3f0da03528f5c5d94:介入机器人/C从端/250902/Untitled2/POUs/" + name], cwd=repo).decode("utf-8-sig").replace("\r\n", "\n")
    check(working == before, name + " unchanged from pre-round4 (line endings normalized)")
check("IF G.arm_curve_owned OR G.arm_cjog_owned THEN" in manual
      and "reset_exec[i] := TRUE;" in manual
      and "Execute:=reset_exec[i] AND (axis_state[i] = 0)" in manual,
      "manual reset request resumes after ownership release; MC_Reset call retained")
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
