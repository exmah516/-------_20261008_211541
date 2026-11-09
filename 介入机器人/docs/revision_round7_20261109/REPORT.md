# 第七轮交付与停项报告（2026-11-09）

## 1. 结论与实际交付范围

**本轮未达到完整验收，不能按“DLS 已完成”部署。** 四项极简修改在开工快照中全部存在，已通过静态核对。

- 已修改运行代码：定位臂 `JOINT_VCAP=[24,30,30,30,30]`，以及归零拒绝提示、归零状态/错误码的协议传递和界面显示。
- **DLS 停止合入**：严格按指定公式实现的候选，在合法 E2 姿态进入阻尼区时，轴3加速度达到 **357.43785821296115°/s²**，超过 E8 的 300°/s²。按“规格冲突即停止该项”的要求，没有加入加速度限幅、改变阻尼参数或其他替代控制。
- 运行中的 X/Y/偏航仍为原闭式逆解和二分算法；DLS 的删除旧逻辑、`limited` 新含义和新提示文字均随该项停止，没有加入切换开关。候选只在 `docs/revision_round7_20261109/dls_candidate/`，不被 PLC 工程或运行程序引用。
- 主端、数据端、手柄宿主和测试项目均构建成功，0 警告、0 错误。PLC **未构建、未下载、未激活**。
- 本体、单轴点动、Z（轴1）、俯仰（轴5）、定位臂所有权/停机状态机未修改。`ArmCartJog`、`ArmCartesian` 与本轮前快照逐字节相同；`ArmCartJogGen` 和 C++ 运行参考仅限速数组一处改变。

证据：[static_audit.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_round7_20261109/static_audit.log>)；[dls_acceptance.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_round7_20261109/dls_acceptance.log>)；[static_audit.json](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_round7_20261109/static_audit.json>)。完整源码 diff：[round7_full.diff](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_round7_20261109/round7_full.diff>)，逐文件/行号清单：[change_manifest.json](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_round7_20261109/change_manifest.json>)。diff 基于本轮开工快照，不包含其他并行工作对影像传输等目录的修改；日志、基线快照和构建二进制独立保留。

## 2. DLS 冲突的可复现实例

初始 PLC 单位关节位置：`[100, -94.127305679518571, 135, -104.89215442093372, 0]`（mm、°），X−、快档 30 mm/s，`dt=0.001 s`，按住3秒，随后松开0.5秒。

第1094周期：

| 量 | 实测值 |
|---|---:|
| 周期前 q3 | 150.19246756468937° |
| 周期后 q3 | 150.22199034536837° |
| s | 0.49708803820938091 |
| λ | 3.2330382257153532 mm |
| 上周期轴3速度 | 29.880218537215342°/s |
| 本周期轴3速度 | 29.522780679002381°/s |
| `(v−v_prev)/dt` | −357.43785821296115°/s² |

速度本身未超30°/s，位置和前馈速度一致，无 NaN/Inf。全套候选验收的独立 Eigen LDLT 解与闭式伴随矩阵解的关节速度最大差为 `1.730171561575844e−12 rad/s`，ST 转译/C++ 为0。因此这不是转译或闭式求逆数值误差。指定速度解和直接积分没有同时提供 E8 的加速度约束。

早期探索曾沿用旧验收的“两连杆内角”定义，得到过258.223°/s²的 E6 结果；该 E6 结论已撤回。**最终验收按实际 q3 角度取值**：q3=40° 的 E6 扫描通过，停项依据是上表实际 q3=135° 出发的 E8 反例。最终日志和 CSV 均来自修正定义后的重跑。

## 3. 解析雅可比及来源

由 [kinematics.cpp:86](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:86>) 的正运动学解析求导。取 mm/rad，定义：

```text
θ = q2
β = q2 + q3 + π             （肘部常量保持不变）
ψ = q2 + q3 + q4             （已合入的新轴4零位）
a = 456.3 mm
b = 554.5 cos(q5) mm

x = L1 sinθ + L2 sinβ + a cosψ + b sinψ + base_x
y = −L1 cosθ − L2 cosβ + a sinψ − b cosψ + base_y
c = −a sinψ + b cosψ
d =  a cosψ + b sinψ

J = [ L1 cosθ + L2 cosβ + c,   L2 cosβ + c,   c
      L1 sinθ + L2 sinβ + d,   L2 sinβ + d,   d
      1,                       1,             1 ]

det(J) = L1 L2 sin(q3+π) = −L1 L2 sin(q3)
Jw = diag(1,1,200) J
```

正运动学的两连杆、工具姿态分别位于 `kinematics.cpp:86、91、95`；原 `computeFrameJacobian:106` 的叉积表达式提供同一几何真值。候选 ST 的闭式 J 从第94行开始，求逆在第106–119行，DLS 速度解在第127行，积分在第139行，前馈加速度在第153行：[候选 ArmCartJogGen:94](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_round7_20261109/dls_candidate/ArmCartJogGen.TcPOU:94>)。

q1、q5保持锚点。候选没有环带、闭式逆解、cos夹紧、分支选择、二分搜索、TCP0/d0偏航锚定或关节角范围限制。X/Y 的 `path_speed` 是指令方向速度分量；偏航为带符号的实际角速度。以上仅描述未合入候选。

## 4. 归零无响应：根因、核对与修复

| 层级 | 源码证据与结论 |
|---|---|
| 界面按钮 | [PositioningArmPanel.xaml:102](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Module.PositioningArm/PositioningArmPanel.xaml:102>)：原 `CanOperate` 启用条件保留。[PositioningArmPanel.xaml.cs:142](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Module.PositioningArm/PositioningArmPanel.xaml.cs:142>) 原先在无控制权、末端点动或单轴点动占用时静默返回；现在显示明确拒绝原因，原门控保留。 |
| MainViewModel / RemoteRobotLink | [MainViewModel:489](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Service.Robot/ViewModels/MainViewModel.cs:489>)、[RemoteRobotLink:254](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Service.Robot/Services/RemoteRobotLink.cs:254>)：命令路由原本存在；5000ms命令应答只表示数据端已分发，不等于运动成功。 |
| Zenoh / main 分发 | [zenoh_service:943](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/zenoh_service.cpp:943>)、[main:5119](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/main.cpp:5119>)：确实调用 `request_program_zero()`，这两层原逻辑无需改变。 |
| 数据端占用门控 | [arm_manual_ads_service:265](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/arm_manual_ads_service.cpp:265>)：原先 `cjog_mode_!=0` 或 `snapshot_.cjog_owned` 时直接返回；现在保持拒绝、记录 `status=5,error=1008`，不抢占点动、不丢弃已有停机条件。重复的在途归零请求仍保留已有状态。 |
| 归零规划与 already_home | [arm_manual_ads_service:304](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/arm_manual_ads_service.cpp:304>)：既有规划错误进入 status5；already_home进入status3且不提交曲线；PLC完成进入status2。原先这些结果未到界面，是“无动作也无提示”的主要观测缺口。目标、`plan_arm_curve(mode0)`、`planProgramZeroReturn`、曲线插补不改。 |
| 保活 | [PositioningArmPanel.xaml.cs:153](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Module.PositioningArm/PositioningArmPanel.xaml.cs:153>)：原代码本来就在 await 应答前置 `_programReturnActive=true`；100ms保活→RemoteRobotLink→Zenoh→main→`keep_cartesian_alive` 链路完整。原500ms租约及规则保留，全链路 mock 已走通。保活不是本轮修复根因。 |
| PLC拒绝 | `ArmCartesian.TcPOU:91–126` 的2001–2006，以及后续2007/2008和NC FB ErrorID均保留。`update_cartesian:358` 保留PLC错误码，由新增状态字段回传中文原因。 |

状态帧从55项同步变为57项：索引54 `ArmCartesianStatus:int32`、55 `ArmCartesianError:int32`、56 `AuthTag`。沿仓库既有字段扩展惯例，版本号和topic仍为v1；严格长度校验同步修改，没有兼容分支。主端和数据端必须成套部署。三份协议文档、C#/C++序列化与校验、黄金样本生成器和共享样本已同步。

归零按钮旁新状态文字：进行中、已完成、已在零位、已停止、失败（错误码+中文原因）。覆盖1001–1008、1202/1203/1206/1207、PLC2001–2008和NC错误。1008表示末端点动占用；本地按钮占用另在原错误提示行立即显示。源位置：[MainViewModel:410](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Service.Robot/ViewModels/MainViewModel.cs:410>)、[PositioningArmPanel.xaml:103](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Module.PositioningArm/PositioningArmPanel.xaml:103>)、[zenoh_service:1095](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/zenoh_service.cpp:1095>)、[RemoteRobotLink:488](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Service.Robot/Services/RemoteRobotLink.cs:488>)。

## 5. E1–E11 结果

真值为仓库 `PositioningArmKinematics`，新偏航零位，dt=0.001s；随机种子20261109。DLS结果均为候选数学验收，不能解释为已部署。

| 编号 | 实测结果 | 判定 |
|---|---|---|
| E1 | 1000姿态，解析J/中心差分最大差 `5.0560743147798348e−8`；差分步长1e−5rad。 | 通过 |
| E2 | 1152场景，q3=45/90/135°，每种q5=0/±30°、16组随机q2/q4，X±/Y±、5/30mm/s、按住3s松开0.5s。全程远离阻尼区1027条：最大直线偏差0.011079mm、最大偏航变化2.1885e−12°、位移/实际速度积分最大相对误差0.010431%。125条途中进入阻尼区，允许让步，纳入E8而不宣称其全程直线。全部1152条中35条发生限速缩放，最小k=0.856301631。 | 远离奇异条件通过；转入阻尼的场景单独披露 |
| E3 | 零位8条场景，位移/名义速度积分80.8136%–97.2194%；各方向见下表。 | 通过 |
| E4 | 实际q3=175°，腕点径向对准+X，X+向外、Y为切向。5/30mm/s：向外位移10.868863/62.754061mm，Y+位移12.359540/74.189101mm；也覆盖反向。最大加速度7.161845°/s²，无超速和非有限值。 | 通过 |
| E5 | q3=90°，±0.8/±2.4°/s，最大TCP漂移0.007624311mm；偏航变化与积分相符。零位TCP漂移2.475095、7.422021、2.479204、7.460299mm，均无NaN。 | 通过 |
| E6 | q3=40°；额外144组q2/q4网格、4方向，共576条，每条6000周期，144条进入阻尼区。最大关节加速度26.331124°/s²（每周期速度变化≤0.026332°/s）。 | 通过 |
| E7 | 复用原T3/T5/T6/T7/T8输入时序和物理阈值：加速1000拍、减速500拍，T3峰值7.49999mm/s²、停止位移1.2475mm（原阈值1.25±0.5%）；换档1000/500拍；换模式500拍且先降到0；重按不归零；保活丢失500拍触发正常减速。 | 通过 |
| E8 | 7,606,601周期，速度上限满足 `cap·(1+1e−9)`，最大30.000000000001137°/s；前馈速度与Δq/dt最大差0；无NaN/Inf。最大加速度357.437858°/s²。 | **失败，DLS停项** |
| E9 | E2–E8全部逐周期：ST转译/C++位置、速度、加速度及路径状态最大差0，离散状态相同；常量编译期核对通过。 | 通过 |
| E10 | 五种情况：正常、已在零位、规划失败1002、PLC拒绝2003、末端点动占用1008；服务mock、界面mock、实际WPF→Zenoh→生产ADS服务/规划器→内存PLC接口替身→WPF均通过。正常提交`format=0,target=[100,0,0,0,0]`，界面先进行中后完成。 | 通过；未执行真实Tc2_MC2 |
| E11 | WPF、Zenoh、ADS和本体通过；原定位臂T1、T2、T10(Z/俯仰)、T12通过。历史黄金文件和旧算法诊断仍失败，详见下一节。 | **未全通过** |

E2完整精度、各场景限速系数和最大加速度见 [acceptance_summary.json](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_round7_20261109/acceptance_summary.json>)、[dls_cases.csv](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_round7_20261109/dls_cases.csv>)；E10见 [home_chain.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_round7_20261109/home_chain.log>)。

### E3：四方向与两档实测

| 方向 | 档位速度 mm/s | 沿指令位移 mm | 位移/名义速度积分 | 偏航变化 ° |
|---|---:|---:|---:|---:|
| X+ | 5 | 12.068989 | 96.532602% | +0.044052 |
| X+ | 30 | 72.814353 | 97.066391% | +0.282755 |
| X− | 5 | 12.077631 | 96.601730% | -0.058104 |
| X− | 30 | 72.929161 | 97.219438% | -0.648450 |
| Y+ | 5 | 10.165111 | 81.304627% | +1.257216 |
| Y+ | 30 | 64.274783 | 85.682574% | +6.466383 |
| Y− | 5 | 10.103718 | 80.813581% | -1.264756 |
| Y− | 30 | 64.016577 | 85.338369% | -6.195667 |

比例分母使用名义平滑速度的逐周期积分：慢档12.5025mm、快档75.015mm。`path_speed`已是让步后的实际指令分量，若用其积分作分母会掩盖DLS让步，因此70%判定使用更严格的名义积分。

### 旧用例的取代关系

| 原用例 | DLS候选对应验收 |
|---|---|
| T9、T13（环带边界、零位脱离） | E3、E4、E6 |
| S1、S2（奇异裕量/边界往返） | E3、E4、E6 |
| S4（关节加速度） | E6、E8 |
| Y3、Y4、Y5（偏航二分、环带、零位） | E5、E6、E8 |

因为DLS停止合入，**运行参考的原用例没有删除或跳过**，原验收程序仍全部运行并保留失败；上表仅说明候选替换算法的覆盖关系。原T1/T2、Z/俯仰T10和T12继续作为回归。

## 6. 基线、构建与回归明细

开工快照及初次日志在 `baseline/`、`baseline_results/`。原托管runner在WPF首次失败后退出；另外用当时保留的 `UV3Release` 基线二进制、55字段基线夹具补跑协议与SDK替身，结果另存 `baseline_saved_results.json`，没有用修改后程序冒充基线。

| 项目 | 基线→最终 | 说明/日志 |
|---|---|---|
| 主端MasterConsole、WPF测试、协议测试、VI.HandleHost | 均构建成功→均成功，0警告0错误 | `final_results/master_build.log`、`wpf_build.log`、`protocol_build.log`、`handlehost_build.log` |
| 数据端ADS.sln Release/x64 Rebuild | 0警告0错误→0警告0错误 | `final_results/ads_build.log` |
| PLC | 未构建 | 无本机TwinCAT XAE编译；XML、ST块配对、调用/所有权/停机接口静态检查及ST转译完成，不能代替XAE编译。 |
| WPF | 原夹具在极简轮旧速度Tooltip处失败→11621断言通过 | 只更新极简轮已确认的Z→轴1、上电/复位合并、速度说明预期。历史夹具保留，逐项映射到现控制；Z的方向、保活、松开停止继续执行。新增归零显示/门控/保活断言。 |
| C++ Zenoh黄金/边界 | 84通过→84通过，服务边界通过 | `final_results/zenoh.log` |
| C# 协议+真实Zenoh网络 | 181通过→182通过 | 增加1条归零状态映射断言。非网络142通过，SDK替身模式160通过；基线为141、159。 |
| ADS/本体原生 | 全通过→全通过 | 11个原生程序：ads_diagnostics、cylinder_manual、dual_control、handle_velocity、homing_record_events、homing_restore_ack、main_recovery、recording_feedback、remote_handle、return_recovery、round5_main_recovery；另跑handle_net_distance及第五轮recovery/ST。 |
| 第五轮PLC完整模型回归 | 11049周期差0→11049周期差0 | SelfCheck从开始至self_check_done为TRUE共5686周期、普通err435周期、四轴回退108周期，完整脚本总11049周期；本轮没有改这些POU。 |
| 定位臂ADS mock | 通过→通过 | 原点动/租约/互斥/停机全部保留并新增归零5场景。 |
| 运动学 | 19/20→20/20 | 唯一基线失败是极简轮新零位对应的旧坐标/姿态期望。更新为新零位和相同俯仰几何，没有改变运动学生产代码。 |
| 本轮前归零二进制→最终归零 | 100组、1700个double，差0 | 使用保存的基线程序生成独立本轮黄金，再与最终程序逐位比较；原历史home_baseline.txt保留。 |
| 原定位臂ST/C++ | 765402周期差0→765311周期差0 | 限速提升会改变部分按停止条件结束的循环长度；最终T/Y用例内305946周期也差0。包装状态机另3232周期逐位一致。 |
| 原T1/T2/T10/T12 | 均通过→均通过 | `final_results/arm/round4/arm_cart_jog_tests.log` |

### 保留的失败，没有把它们记作通过

- 历史 `home_baseline.txt`：基线和最终均274个值不一致，来源为极简轮新零位；本轮独立100组逐位比较为0差。旧黄金SHA256保留在结果JSON中。
- 第二轮/第三轮/第四轮二进制黄金：基线已与新零位不符；最终再叠加规定的30°/s限速，仍失败。没有重写旧黄金来制造通过。
- 原 `arm_cart_jog_tests`：基线和最终均 Y1、S4 两项失败。最终旧算法S4最大加速度约25583.45°/s²；此前10°/s上限下已有约8775.06°/s²，30°/s上限并未使旧算法满足新的加速度验收。
- 旧round4诊断：基线S1/S2/S4失败；最终另有B见证失败（总4项）。B是针对10°/s限速的固定二分非单调见证，改变为30°/s后该固定见证不再成立；未把该诊断删除或换成另一见证。S1/S2/S4为仍保留的旧算法边界问题。
- 旧静态检查的10°/s常量和“与第三轮原文件完全相同”预期已按本轮限速、极简轮零位同步，只接受这两处规定变化，其他源码仍严格比较；原安全接口/状态机断言保留。

因此 E11 的“全部通过”没有满足；当前运行代码保留的旧边界问题和DLS候选E8超限均在交付中明确保留，不建议把当前成果视为完整第七轮现场版本。

## 7. 常量与NC参数

| 常量/规则 | 值 | 状态 |
|---|---|---|
| JOINT_VCAP | `[24,30,30,30,30]`，mm/s、°/s | 运行ST与C++参考同值；轴1不变 |
| DLS_PSI_WEIGHT | 200 mm/rad | 仅候选ST/C++同值 |
| DLS_S0 | 0.5 | 仅候选 |
| DLS_LAMBDA_MAX | 30 mm | 仅候选 |
| λ | s≥0.5为0；否则30√(1−(s/0.5)²) | 仅候选，无额外限幅 |
| L1 / L2 | 200 / 180 mm | 既有值不变 |
| TOOL_LATERAL_OFFSET_MM / TOOL_LENGTH_MM | 456.3 / 554.5 mm | 既有值不变 |
| T_ACC / T_DEC | 1.0 / 0.5 s | 曲线及换档、换模式、重按规则不变 |
| PLC任务/离线dt | 1000μs / 0.001s | 不变 |
| PI / RAD | 3.14159265358979323846 / PI÷180 | ST/C++一致 |
| 主端归零保活 / 数据端租约 | 100 / 500ms | 既有值不变 |
| 归零命令应答等待 / PLC曲线确认等待 | 5000ms / 2s | 既有值不变 |
| 归零占用错误 | 1008 | 新增明确结果，不改变占用条件 |
| StateFrame长度/版本 | 57 / 1 | 两端和夹具同步 |

`250902.tsproj` 链接明确把 `G.arm_axis[1..5]` 分别映射到 NC Axis1–5。显式 `<Velo>`：

| NC轴 | SlowManual | FastManual | Maximum |
|---|---:|---:|---|
| Axis1 | 10 | 50 | 30 mm/s |
| Axis2 | 1 | 5 | 未显式保存 |
| Axis3 | 3 | 10 | 未显式保存 |
| Axis4 | 3 | 10 | 未显式保存 |
| Axis5 | 3 | 10 | 未显式保存 |

Axis2–5单位为°。**FastManual不是Maximum，源码无法确认NC是否允许30°/s**；没有假定TwinCAT默认值，也没有修改NC配置。工程机需读取轴2–5实际NC最大速度参数确认。原始链接和参数见 `static_audit.json`。

## 8. 可复现命令

在仓库根目录（`D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人`）执行，仅连接本机回环模拟器，不接触PLC/手柄硬件：

```powershell
& 'D:/Study_software/Anaconda/python.exe' -X utf8 tools/revision_round7/audit_scope.py
& 'D:/Study_software/Anaconda/python.exe' -X utf8 tools/revision_round7/run_dls.py
& 'D:/Study_software/Anaconda/python.exe' -X utf8 tools/revision_round7/run_home_chain.py
& 'D:/Study_software/Anaconda/python.exe' -X utf8 tools/revision_round7/run_managed.py
& 'D:/Study_software/Anaconda/python.exe' -X utf8 tools/revision_round7/run_regressions.py
& 'D:/Study_software/Anaconda/python.exe' -X utf8 tools/revision_round7/run_saved_baseline.py
& 'docs/revision_round7_20261109/final_results/arm_build/home_regression.exe' --verify docs/revision_round7_20261109/home_round7_baseline.txt
```

预期退出码：audit_scope、home_chain、managed、saved_baseline和本轮归零compare为0；DLS因E8为1；regressions因上述旧定位臂失败为1。构建在脚本中使用Release/x64及警告视为错误；主端脚本使用本地 `Get-ViDotnet.ps1` SDK，目标仍.NET8；数据端用VS2022/MSVC14.29。原始命令、完整输出和逐项返回码保留在 `baseline_results/`、`final_results/`、`dls_results.json`、`home_chain_results.json` 中。

`run_saved_baseline.py` 依赖开工时保存于 `A主端/工程/build/UV3Release` 的基线二进制；这一步是补跑，不会回写生产源码。`home_round7_baseline.txt`由该轮修改前已编译的原生归零程序生成，旧历史黄金文件未覆盖。

## 9. 部署与现场验证

当前可审阅的是限速与归零反馈修复；DLS候选不能下载运行。PLC限速修改仍需工程机编译、下载、激活；主端、手柄宿主、数据端和PLC成套部署，尤其主/数据两端57字段状态帧必须一致。本机没有执行任何硬件运动。

现场步骤：

1. 先读取NC Axis2–5的Maximum，确认允许30°/s；不把手动快档速度当作最大速度。
2. 验证归零：非零姿态点击，观察进行中→完成；已在零位时显示“已在零位”；末端点动尚未释放所有权时显示1008；驱动未就绪/PLC拒绝时显示错误码和中文原因。核对轴1保持、轴2–5目标仍为0，轨迹格式仍为mode0。
3. DLS停项解决并经新的完整验收后，再现场验证：零位直接X±/Y±，记录位移与偏航让步；肘角90°附近X/Y直线与偏航保持；偏航点动时TCP保持；靠近伸直时流畅运动而非卡住或急停。**这些DLS现场项目当前均未执行，不能由候选离线结果替代。**
4. 回归Z±轴1方向、俯仰轴5、五轴单轴点动、总使能/复位、松开/保活丢失/急停；与本轮前操作规则比较。

## 10. 修改文件与全部改动位置

下表是相对于本轮开工快照的统一diff块起始行号；每块的完整内容见 `round7_full.diff`。新增测试文件中复用的ADS传输替身从原测试移到 `arm_ads_mock.h`，原断言未删除，供五场景联通测试共用。候选代码的路径明确位于docs目录。

| 文件（链接定位首个改动） | 本轮修改行号 |
|---|---|
| [protocol/fixtures/zenoh-v1/README.md](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/protocol/fixtures/zenoh-v1/README.md:4>) | 4 |
| [protocol/fixtures/zenoh-v1/challenge.hex](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/protocol/fixtures/zenoh-v1/challenge.hex:1>) | 1 |
| [protocol/fixtures/zenoh-v1/generate_vectors.py](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/protocol/fixtures/zenoh-v1/generate_vectors.py:57>) | 57 |
| [protocol/fixtures/zenoh-v1/invalid_state_auth.hex](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/protocol/fixtures/zenoh-v1/invalid_state_auth.hex:1>) | 1 |
| [protocol/fixtures/zenoh-v1/invalid_state_length.hex](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/protocol/fixtures/zenoh-v1/invalid_state_length.hex:1>) | 1 |
| [protocol/fixtures/zenoh-v1/invalid_state_result_id.hex](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/protocol/fixtures/zenoh-v1/invalid_state_result_id.hex:1>) | 1 |
| [protocol/fixtures/zenoh-v1/invalid_state_results.hex](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/protocol/fixtures/zenoh-v1/invalid_state_results.hex:1>) | 1 |
| [protocol/fixtures/zenoh-v1/invalid_state_task.hex](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/protocol/fixtures/zenoh-v1/invalid_state_task.hex:1>) | 1 |
| [protocol/fixtures/zenoh-v1/invalid_state_window_float64.hex](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/protocol/fixtures/zenoh-v1/invalid_state_window_float64.hex:1>) | 1 |
| [protocol/fixtures/zenoh-v1/invalid_state_window_nan.hex](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/protocol/fixtures/zenoh-v1/invalid_state_window_nan.hex:1>) | 1 |
| [protocol/fixtures/zenoh-v1/state.hex](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/protocol/fixtures/zenoh-v1/state.hex:1>) | 1 |
| [protocol/fixtures/zenoh-v1/state_force_controls.hex](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/protocol/fixtures/zenoh-v1/state_force_controls.hex:1>) | 1 |
| [介入机器人/A主端/协议/PROTOCOL.md](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/协议/PROTOCOL.md:72>) | 72, 130 |
| [介入机器人/A主端/工程/master/MasterConsole.Protocol.Tests/Program.cs](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/MasterConsole.Protocol.Tests/Program.cs:138>) | 138 |
| [介入机器人/A主端/工程/master/MasterConsole.Protocol.Tests/RobotServiceTests.cs](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/MasterConsole.Protocol.Tests/RobotServiceTests.cs:58>) | 58 |
| [介入机器人/A主端/工程/master/MasterConsole.Protocol/Frames.cs](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/MasterConsole.Protocol/Frames.cs:72>) | 72 |
| [介入机器人/A主端/工程/master/MasterConsole.Protocol/ZenohFrames.cs](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/MasterConsole.Protocol/ZenohFrames.cs:138>) | 138 |
| [介入机器人/A主端/工程/master/MasterConsole.Protocol/ZenohWire.cs](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/MasterConsole.Protocol/ZenohWire.cs:69>) | 69 |
| [介入机器人/A主端/工程/master/VI.Control.Tests/HomeChainChecks.cs](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Control.Tests/HomeChainChecks.cs:1>) | 1 |
| [介入机器人/A主端/工程/master/VI.Control.Tests/Program.cs](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Control.Tests/Program.cs:21>) | 21, 254, 422, 523, 536 |
| [介入机器人/A主端/工程/master/VI.Control.Tests/ResponsiveLayoutChecks.cs](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Control.Tests/ResponsiveLayoutChecks.cs:19>) | 19 |
| [介入机器人/A主端/工程/master/VI.Control.Tests/RevisionUChecks.cs](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Control.Tests/RevisionUChecks.cs:20>) | 20, 34, 59, 109, 218, 246 |
| [介入机器人/A主端/工程/master/VI.Module.PositioningArm/PositioningArmPanel.xaml](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Module.PositioningArm/PositioningArmPanel.xaml:94>) | 94 |
| [介入机器人/A主端/工程/master/VI.Module.PositioningArm/PositioningArmPanel.xaml.cs](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Module.PositioningArm/PositioningArmPanel.xaml.cs:141>) | 141 |
| [介入机器人/A主端/工程/master/VI.Service.Robot/Services/RemoteRobotLink.cs](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Service.Robot/Services/RemoteRobotLink.cs:484>) | 484 |
| [介入机器人/A主端/工程/master/VI.Service.Robot/ViewModels/MainViewModel.cs](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Service.Robot/ViewModels/MainViewModel.cs:406>) | 406, 566 |
| [介入机器人/A主端/工程/protocol/PROTOCOL.md](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/protocol/PROTOCOL.md:72>) | 72, 130 |
| [介入机器人/B数据端/工程/ADS项目/arm_manual_ads_service.cpp](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/arm_manual_ads_service.cpp:265>) | 265 |
| [介入机器人/B数据端/工程/ADS项目/protocol/PROTOCOL.md](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/protocol/PROTOCOL.md:72>) | 72, 130 |
| [介入机器人/B数据端/工程/ADS项目/protocol/cpp/zenoh_protocol.h](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/protocol/cpp/zenoh_protocol.h:15>) | 15, 102, 114 |
| [介入机器人/B数据端/工程/ADS项目/protocol/cpp/zenoh_validation.h](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/protocol/cpp/zenoh_validation.h:89>) | 89 |
| [介入机器人/B数据端/工程/ADS项目/tests/zenoh/protocol_tests.cpp](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/tests/zenoh/protocol_tests.cpp:107>) | 107 |
| [介入机器人/B数据端/工程/ADS项目/zenoh_service.cpp](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/zenoh_service.cpp:1092>) | 1092 |
| [介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_ads_mock.h](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_ads_mock.h:1>) | 1 |
| [介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_ads_test.cpp](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_ads_test.cpp:1>) | 1, 60, 102, 131, 151 |
| [介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:12>) | 12 |
| [介入机器人/B数据端/工程/PositioningArmKinematics/tests/audit_cart_jog_st.py](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/audit_cart_jog_st.py:62>) | 62, 102 |
| [介入机器人/B数据端/工程/PositioningArmKinematics/tests/kinematics_test.cpp](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/kinematics_test.cpp:123>) | 123 |
| [介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:30>) | 30 |
| [介入机器人/docs/revision_round7_20261109/dls_candidate/ArmCartJogGen.TcPOU](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_round7_20261109/dls_candidate/ArmCartJogGen.TcPOU:1>) | 1 |
| [介入机器人/docs/revision_round7_20261109/dls_candidate/arm_cart_jog_reference.h](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_round7_20261109/dls_candidate/arm_cart_jog_reference.h:1>) | 1 |
| [介入机器人/docs/revision_round7_20261109/dls_candidate/dls_acceptance.cpp](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_round7_20261109/dls_candidate/dls_acceptance.cpp:1>) | 1 |
| [介入机器人/tools/revision_round7/audit_scope.py](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/tools/revision_round7/audit_scope.py:1>) | 1 |
| [介入机器人/tools/revision_round7/home_chain_simulator.cpp](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/tools/revision_round7/home_chain_simulator.cpp:1>) | 1 |
| [介入机器人/tools/revision_round7/make_delivery.py](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/tools/revision_round7/make_delivery.py:1>) | 1 |
| [介入机器人/tools/revision_round7/run_dls.py](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/tools/revision_round7/run_dls.py:1>) | 1 |
| [介入机器人/tools/revision_round7/run_home_chain.py](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/tools/revision_round7/run_home_chain.py:1>) | 1 |
| [介入机器人/tools/revision_round7/run_managed.py](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/tools/revision_round7/run_managed.py:1>) | 1 |
| [介入机器人/tools/revision_round7/run_regressions.py](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/tools/revision_round7/run_regressions.py:1>) | 1 |
| [介入机器人/tools/revision_round7/run_saved_baseline.py](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/tools/revision_round7/run_saved_baseline.py:1>) | 1 |
