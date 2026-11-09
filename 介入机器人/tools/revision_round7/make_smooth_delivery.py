"""汇总平滑阻尼复验及未满足的合入条件，不修改生产代码或既有验收阈值。"""
from pathlib import Path
import csv
import difflib
import hashlib
import json
import subprocess

repo = Path(__file__).resolve().parents[2]
previous = repo / "docs/revision_round7_20261109"
out = repo / "docs/revision_round7_smooth_20261109"
candidate = out / "dls_candidate"


def read(path):
    return path.read_text(encoding="utf-8")


def link(path, line=None):
    target = path.as_posix() + (":" + str(line) if line else "")
    return "[" + path.name + (":" + str(line) if line else "") + "](<" + target + ">)"


def fields(line):
    return dict(token.split("=", 1) for token in line.split()[1:] if "=" in token)


log = read(out / "dls_acceptance.log")
rows = list(csv.DictReader((out / "dls_cases.csv").open(encoding="utf-8", newline="")))
metrics = {name: fields(next(line for line in log.splitlines() if line.startswith(name + " ")))
    for name in ("E1", "E6", "E8", "E9", "E8_WITNESS") if name != "E6"}
e6 = fields(next(line for line in log.splitlines() if line.startswith("E6 crossed=")))
e8 = metrics["E8"]
witness = metrics["E8_WITNESS"]
e11_log = read(out / "e11_candidate.log")
t2 = [fields(line) for line in e11_log.splitlines() if line.startswith("T2_CASE ")]
assert len(t2) == 18
worst_t2 = max(t2, key=lambda row: float(row["line_mm"]))
far = [row for row in rows if row["id"] == "E2" and row["damped"] == "0"]
e2 = [row for row in rows if row["id"] == "E2"]
e3 = [row for row in rows if row["id"] == "E3"]
status = {"E" + str(i): "PASS" if "PASS E" + str(i) + "\n" in log else "FAIL" for i in range(1, 10)}
home = json.loads(read(out / "home_chain_results.json"))
status["E10"] = "PASS" if not any(home.values()) else "FAIL"
status["E11"] = "FAIL" if "FAIL T2" in e11_log else "PASS"
assert all(status["E" + str(i)] == "PASS" for i in range(1, 11))
assert status["E11"] == "FAIL"

manifest = json.loads(read(previous / "change_manifest.json"))
unchanged = {item["path"]: hashlib.sha256((repo.parent / item["path"]).read_bytes()).hexdigest() == item["sha256"]
    for item in manifest}
assert all(unchanged.values()), unchanged
diff = []
for name in ("ArmCartJogGen.TcPOU", "arm_cart_jog_reference.h", "dls_acceptance.cpp"):
    before = previous / "dls_candidate" / name
    after = candidate / name
    diff.extend(difflib.unified_diff(read(before).splitlines(True), read(after).splitlines(True),
        fromfile="original_candidate/" + name, tofile="smooth_candidate/" + name))
(out / "candidate.diff").write_text("".join(diff), encoding="utf-8")
production_check = subprocess.run(["git", "apply", "--reverse", "--check", str(previous / "round7_full.diff")],
    cwd=repo, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(out / "production_unchanged.log").write_bytes(production_check.stdout)
assert production_check.returncode == 0
home_compare = subprocess.run([str(out / "final_results/arm_build/home_regression.exe"), "--verify",
    str(previous / "home_round7_baseline.txt")], cwd=repo, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(out / "home_compare.log").write_bytes(home_compare.stdout)
assert home_compare.returncode == 0
builds = ["master_build", "wpf_build", "protocol_build", "handlehost_build", "ads_build"]
for name in builds:
    build_log = read(out / "final_results" / (name + ".log"))
    assert "0 Warning(s)" in build_log and "0 Error(s)" in build_log, name
summary = dict(acceptance=status, merged=False, E8=e8, E8_witness=witness, E6=e6,
    E11_T2=worst_t2, previous_delivery_unchanged=unchanged, builds_zero_warnings_errors=builds)
(out / "acceptance_summary.json").write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding="utf-8")

table = "\n".join("| " + ("X" if row["mode"] == "1" else "Y") + ("+" if row["dir"] == "1" else "−")
    + " | " + row["speed"] + " | " + format(float(row["displacement_mm"]), ".6f")
    + " | " + format(float(row["displacement_mm"])/float(row["nominal_integral"])*100, ".6f") + "%"
    + " | " + format(float(row["yaw_deg"]), "+.6f") + " |" for row in e3)
seed = "[" + ", ".join(witness["seed" + str(i)] for i in range(1, 6)) + "]"
before = "[" + ", ".join(witness["before" + str(i)] for i in range(1, 6)) + "]"
after = "[" + ", ".join(witness["after" + str(i)] for i in range(1, 6)) + "]"
report = f"""# 第七轮补充：平滑DLS阻尼完整复验（2026-11-09）

**E1–E10通过，E11中原T2未通过，因此未合入生产代码。** 本次没有修改生产PLC、生产C++参考模型、界面提示或NC配置，也没有删除当前运行的旧算法。用户规定“全部通过后再合入”，合入条件尚未满足。

候选ST和C++只修改原阻尼函数：`ratio`就是规格中的`u`，取`1−s/DLS_S0`，`lambda=DLS_LAMBDA_MAX*ratio*ratio*(3−2*ratio)`；`s>=DLS_S0`时仍为0。其余算法、曲线、参数和限速完全不变。验收程序只增加E8峰值位置记录，未改变E1–E9输入、阈值或判定。原候选和原报告保留。

修改位置：{link(candidate / "ArmCartJogGen.TcPOU", 102)}、{link(candidate / "arm_cart_jog_reference.h", 81)}。候选算法及观测改动的完整diff：{link(out / "candidate.diff")}。

**E8最大关节加速度及发生位置**

最大绝对值 **{e8['max_acc']}°/s²**，较原阻尼的357.43785821296115°/s²降低，满足300°/s²阈值。方向为负加速度，出现在E2随机姿态的Y+快档，轴3。全部E2–E8共{e8['cycles']}周期；新增E11指定原场景另68018周期，最大36.6859622147°/s²，未超过本峰值。

| 量 | 实测值 |
|---|---|
| 初始位置，mm/° | `{seed}` |
| 指令 | Y+，30 mm/s；按住3s，随后松开0.5s |
| 周期、时刻 | 第{e8['cycle']}周期，{witness['time_s']}s，dt=0.001s |
| 峰值发生前的位置，mm/° | `{before}` |
| 峰值发生后的位置，mm/° | `{after}` |
| s / lambda | {witness['s']} / {witness['lambda']} mm |
| 轴3上一周期速度 | {witness['previous_v3']}°/s |
| 轴3本周期速度 | {witness['v3']}°/s |
| 轴3加速度 | −{e8['max_acc']}°/s² |
| 最大关节速度 | {e8['max_velocity']}°/s，满足30·(1+1e−9) |
| 前馈速度与Δq/dt最大差 | {e8['difference']} |
| ST/C++逐周期最大差 | {metrics['E9']['max_difference']} |

原始记录：{link(out / "dls_acceptance.log", 53)}，`E8_WITNESS`记录全部五轴的初值、峰值前后位置及速度；{link(out / "dls_cases.csv")}保留全部逐场景指标。

**E1–E11全部执行结果**

| 项目 | 结果与实测值 |
|---|---|
| E1 | 通过，1000姿态解析J/中心差分最大差{metrics['E1']['max_error']}。 |
| E2 | 通过。1152场景，其中{len(far)}条全程在阻尼区外，最大直线偏差0.011078875468251148mm、最大偏航变化2.1884716261411086e−12°；{len(e2)-len(far)}条途中进入阻尼区，按第3节允许让步并继续纳入E8。 |
| E3 | 通过，零位8条方向/档位组合，位移比例80.892251%–98.228083%，详见下表。 |
| E4 | 通过，q3=175°向外、切向及反向均无NaN/Inf、关节不超速；各方向位移见CSV。 |
| E5 | 通过，q3=90°偏航点动TCP最大漂移0.0076243105512920984mm；零位最大漂移7.463826030093025mm，均无NaN/Inf。 |
| E6 | 通过，从实际q3=40°出发，4条基本场景，另144组q2/q4网格、576条扫描；扫描中进入阻尼区144条，最大加速度{e6['peak_acc_deg_s2']}°/s²，低于200。 |
| E7 | 通过，原T3/T5/T6/T7/T8的输入时序和物理阈值保持：加速1000拍、减速500拍，换档、换模式、重按、保活丢失均通过。 |
| E8 | 通过，{e8['cycles']}周期，最大加速度{e8['max_acc']}°/s²；速度、位置差分和非有限值检查通过。 |
| E9 | 通过，{e8['cycles']}周期数值最大差0、离散状态一致；独立Eigen LDLT/闭式求逆最大qdot差2.8483881919783016e−12rad/s。 |
| E10 | 通过，正常归零、已在零位、规划失败1002、PLC拒绝2003、点动占用1008五场景，实际WPF→Zenoh→生产服务/规划器→内存PLC替身→WPF链路全部通过。 |
| E11 | **未通过**。WPF、Zenoh、ADS、本体全部通过；候选运行原T1/T10(Z及俯仰)/T12通过，**原T2失败**，详见下文。全部原有运行代码回归亦已执行，既有历史黄金/旧算法诊断失败仍保留。 |

| 零位方向 | 速度mm/s | 沿指令位移mm | 位移/名义速度积分 | 偏航变化° |
|---|---:|---:|---:|---:|
{table}

分母是原平滑曲线的名义速度积分；没有改用让步后的`path_speed`积分来提高比例。全部表格数据对应本次平滑函数重跑。

**未合入的具体原因：E11要求保留的原T2与阻尼区让步行为冲突**

原T2：{link(repo / "B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_tests.cpp", 214)}，第231行要求全程直线偏差≤0.01mm，`invariants`≤1e−6，其中包含以度计的偏航变化。原18个输入和全部数值阈值保留，候选结果为直线偏差 **0.115715968843mm**、最大偏航变化 **0.115961795262°**。这与E2在远离阻尼区采用0.05mm/0.01°、第3节在阻尼区允许让步的验收定义不同。

一个最大直线偏差场景：初始`[100, -14.1920347265, -150, 37, 12]`（mm/°），X+、30mm/s，按住3s再松开501拍。`min(s)=0.32834676004<0.5`，确实进入阻尼区，偏航让步0.115961795261°。对应Y+输入的偏航最大值为0.115961795262°。原T2的q3=−90°共6个场景均保持在阻尼区外，最大直线偏差约0.004399493mm、偏航变化≤5.32e−13°，通过其原阈值。

另外原T2中q3=−30°的快档直线偏差为0.0108088722756mm，也高于其旧0.01mm阈值；本次没有改变Euler积分或原T2阈值。所有18条明细见{link(out / "e11_candidate.log")}。

运行方法：从原`arm_cart_jog_tests.cpp`直接提取T1、T2、T10、T12的输入和断言。候选已删除内部`wx/wy/branch`缓存，测试中的缓存读取改为原腕点公式和独立逆解分支计算；没有给候选增加旧字段。生成源码完整保存在{link(out / "e11_build/original_e11_cases.cpp")}。原测试文件未修改、未删除、未跳过。本轮增加的候选验收覆盖68018周期，ST/C++最大差0。

因此合入前仍需统一“保留原T2阈值”与“阻尼区允许让步”的要求；本次没有擅自豁免场景、放宽阈值、改变算法或增加控制逻辑。第3.4节删除生产旧算法及第3.5节更新生产提示文字，均随合入条件保持未执行。候选本身已无闭式逆解、二分、环带、分支、TCP0/d0锚定等旧逻辑。

**构建、完整回归与未改路径**

- MasterConsole、VI.Control.Tests、协议测试、VI.HandleHost及ADS.sln Release/x64：全部0警告、0错误。
- WPF：11621断言通过；C++ Zenoh：84通过；C# Zenoh网络：182通过；非网络142、SDK替身160通过。
- ADS及本体11个原生测试、handle_net_distance、第五轮恢复和完整PLC模型回归全部通过；PLC模型11049周期最大差0，普通err435周期、四轴回退108周期，SelfCheck完整流程保持覆盖。
- 原归零100组、1700个double及状态逐位比较差0：{link(out / "home_compare.log")}。
- 完整历史定位臂runner未忽略失败：原home黄金274个差异、历史点动黄金不匹配、旧算法Y1/S4及round4诊断等仍失败，和补充前一致；未覆盖旧黄金制造通过。候选的新增T2失败单独列出，未混为历史失败。
- 补充前交付清单的{len(unchanged)}个文件SHA256全部一致，原完整diff反向检查通过，证明此次未合入生产改动。详见{link(out / "acceptance_summary.json")}。

构建和回归原始日志/逐项退出码：{link(out / "final_results/managed_results.json")}、{link(out / "final_results/regression_results.json")}、{link(out / "home_chain_results.json")}、{link(out / "e11_results.json")}。完整历史定位臂失败项：{link(out / "final_results/arm/round4/results.json")}。

**常量**

| 名称 | ST与C++候选同值 |
|---|---|
| JOINT_VCAP | [24,30,30,30,30]，mm/s、°/s |
| DLS_PSI_WEIGHT | 200mm/rad |
| DLS_S0 | 0.5 |
| DLS_LAMBDA_MAX | 30mm |
| lambda | s<0.5：u=1−s/0.5，30u²(3−2u)；否则0 |
| T_ACC / T_DEC / dt | 1.0 / 0.5 / 0.001s |
| L1 / L2 | 200 / 180mm |
| 工具偏移 / 工具长度 | 456.3 / 554.5mm |

**复现命令**（在仓库根目录，不连接PLC或手柄硬件）

```powershell
& 'D:/Study_software/Anaconda/python.exe' -X utf8 tools/revision_round7/run_smooth.py
& 'D:/Study_software/Anaconda/python.exe' -X utf8 tools/revision_round7/make_smooth_delivery.py
```

首条命令依次运行候选E1–E9、候选E11指定原场景、E10全链路、托管构建/测试、全部原生/历史回归；即使某项失败也继续余项。最终退出1，失败保留在`runner_results.json`。各阶段：DLS=0、E10=0、managed=0、regressions=1、候选E11=1。第二条命令退出0仅表示报告/证据一致性核对通过，不代表E11通过。

PLC未使用TwinCAT XAE构建，未下载或激活，未执行现场运动。生产算法保持补充前状态；NC Axis2–5 Maximum源码仍未显式保存，30°/s是否允许仍需工程机确认。只有满足完整合入条件后，才进行同步部署、PLC编译下载激活及原第七轮现场验证。
"""
(out / "REPORT.md").write_text(report, encoding="utf-8")
print("E1-E10 PASS; E11 T2 FAIL; production unchanged; report ready")
