# F. 测试与质量现状

调查日期：2026-11-12。测试均在离线替身、内存 PLC、回环 Zenoh 或本地数学模型上执行；没有启动真实 ADS 目标、TwinCAT 运行时、PLC 下载或机器人运动。执行产物在 `docs/arch_survey_r1/scratch/`；运行编排脚本只把既有 runner 的输出重定向到该目录，没有改测试源或断言。

## 1. 执行边界与方法

| 项目 | 事实 | 确定程度与出处 |
|---|---|---|
| 数据端标准离线套件 | 既有 `run_offline_tests.cjs` 支持 `all/dual/recovery/cylinder/ads/handle/recording`，从当前 `main.cpp`、头文件和生产模块抽取代码片段后编译测试，依赖内存替身 | **已确认**：`介入机器人/B数据端/工程/ADS项目/tools/run_offline_tests.cjs:7`、`介入机器人/B数据端/工程/ADS项目/tools/run_offline_tests.cjs:34-65`、`介入机器人/B数据端/工程/ADS项目/tools/run_offline_tests.cjs:68-106`、`介入机器人/B数据端/工程/ADS项目/tools/run_offline_tests.cjs:108-142`；各测试源开头注释见下表 |
| 硬件隔离 | `dual_control_test.cpp` 注释说明设备读写使用内存替身；`ads_diagnostics_test.cpp` 替换 `CADSComm`，不连接设备；`remote_handle_test.cpp` 不加载手柄 DLL；`recording_feedback_test.cpp` 只链接内存桥 | **已确认**：`介入机器人/B数据端/工程/ADS项目/tools/dual_control_test.cpp:3`；`介入机器人/B数据端/工程/ADS项目/tools/ads_diagnostics_test.cpp:1`；`介入机器人/B数据端/工程/ADS项目/tools/remote_handle_test.cpp:1`；`介入机器人/B数据端/工程/ADS项目/tools/recording_feedback_test.cpp:1` |
| 定位臂验收 | runner 编译 C++ 运动学/服务模型，翻译 TwinCAT ST 做静态和数值等价比较；runner 明确不连接 ADS/PLC，不重写归零黄金文件 | **已确认**：`介入机器人/B数据端/工程/PositioningArmKinematics/tests/run_cart_jog_acceptance.py:1`、`介入机器人/B数据端/工程/PositioningArmKinematics/tests/run_cart_jog_acceptance.py:20-28`；`介入机器人/B数据端/工程/PositioningArmKinematics/tests/run_cart_jog_acceptance.py:82-95` |
| PLC ST 模型 | `translate_st.py` 把 ST 控制语句翻译为 C++，FB 和时钟由显式离线替身驱动；运行时注释说明不仿真 NC、驱动器或现场实时性 | **已确认**：`介入机器人/tools/reliability_round5/translate_st.py:1`；`介入机器人/tools/reliability_round5/st_runtime.h:11` |
| 主端 WPF | `VI.Control.Tests` 用 `StartupSelection.Parse(["--sim"])`，断言模拟链路不创建 `IHandleService`；因此 WPF 测试不加载真实手柄 | **已确认**：`介入机器人/A主端/工程/master/VI.Control.Tests/Program.cs:42-52` |
| Zenoh 网络 | C# 网络 smoke 固定 loopback `tcp/127.0.0.1:17447`；归零链使用 `127.0.0.1:17448` 内存 PLC 模拟器 | **已确认**：`介入机器人/A主端/工程/master/MasterConsole.Protocol.Tests/NetworkSmoke.cs:30-49`；`介入机器人/tools/revision_round7/home_chain_simulator.cpp:8-9` |

执行前工作区存在既有源码改动和历史产物；本轮没有改动源码、配置、测试或提交。测试输出、重定位编译目录和统计脚本属于本轮 `docs/arch_survey_r1/` 调研附件。

## 2. 数据端原生测试

### 2.1 标准 `run_offline_tests.cjs all`

以下 11 个程序均返回 0；结论为**已确认**，证据是对应完整日志末行和测试源的入口/断言。测试覆盖生产分支的离线抽取，不等同于现场时序验收。

| 程序 | 覆盖内容 | 结果 | 证据 |
|---|---|---|---|
| `dual_control_test` | 双手柄窗口、被动/主动换手、方向、轴4独立、回退、轴1先行映射、边界和恢复门控 | 13,500 个 boundary/input 组合；122,359 个断言通过 | `docs/arch_survey_r1/scratch/tests_native/dual_control_test.log:17`；入口函数和轴1先行测试 `介入机器人/B数据端/工程/ADS项目/tools/dual_control_test.cpp:140-803` |
| `main_recovery_test` | ADS 快照恢复、连接保持、双/单手柄基准重建、开始门控、计划回退故障恢复 | 所有输出为 PASS | `docs/arch_survey_r1/scratch/tests_native/main_recovery_test.log:1-29`；`介入机器人/B数据端/工程/ADS项目/tools/main_recovery_test.cpp:1` 注释说明仅用运动同步替身 |
| `return_recovery_test` | 取消、屈曲/间距恢复进入退出、完整回退推进及双腿回退交叉 | 110 个断言通过 | `docs/arch_survey_r1/scratch/tests_native/return_recovery_test.log:207`；`介入机器人/B数据端/工程/ADS项目/tools/return_recovery_test.cpp:2`、`介入机器人/B数据端/工程/ADS项目/tools/return_recovery_test.cpp:204` |
| `round5_main_recovery_test` | V1/V2 主循环恢复、清零重试、急停后恢复与续做 | V1/V2 分支通过 | `docs/arch_survey_r1/scratch/tests_native/round5_main_recovery_test.log:31`；`介入机器人/tools/reliability_round5/main_recovery_tests.cpp:72-122` |
| `homing_record_events_test` | 寻参记录事件边沿、坐标刷新、断连冻结和周期更新 | 13 个 checks 通过 | `docs/arch_survey_r1/scratch/tests_native/homing_record_events_test.log:1-2`；`介入机器人/tools/reliability_round5/homing_record_events_tests.cpp:21-48` |
| `homing_restore_ack_test` | PLC 应用名、复用寻参确认/拒绝、启动阶段恢复确认 | 10 个 checks 通过 | `docs/arch_survey_r1/scratch/tests_native/homing_restore_ack_test.log:1-9`；`介入机器人/tools/reliability_round5/homing_restore_ack_tests.cpp:31-48` |
| `cylinder_manual_test` | 四个电缸独立值、掩码、手动/PLC/自动所有权门控 | 71 个断言通过 | `docs/arch_survey_r1/scratch/tests_native/cylinder_manual_test.log:1-3`；`介入机器人/B数据端/工程/ADS项目/tools/cylinder_manual_test.cpp:1`、`介入机器人/B数据端/工程/ADS项目/tools/cylinder_manual_test.cpp:74` |
| `ads_diagnostics_test` | 快速读写、心跳、单/短/1.2s 读写失败、SoftHold、低频请求、重连和快通道并发 | 全部 PASS；最后验证 2s 低频阻塞期间 fast snapshot=200、heartbeat=200、RTT=32µs（模拟值） | `docs/arch_survey_r1/scratch/tests_native/ads_diagnostics_test.log:4-119`；`介入机器人/B数据端/工程/ADS项目/tools/ads_diagnostics_test.cpp:1` |
| `remote_handle_test` | 物理 SN 槽位隔离、500ms/1.6s/4s/6s 空档、重建基准、失效样本和关闭清零 | 全部 PASS | `docs/arch_survey_r1/scratch/tests_native/remote_handle_test.log:1-146`；测试源 `介入机器人/B数据端/工程/ADS项目/tools/remote_handle_test.cpp:1`、`介入机器人/B数据端/工程/ADS项目/tools/remote_handle_test.cpp:78-116` |
| `handle_velocity_test` | 手柄速度缓存/重复帧、CSV 列和采样调度 | 100 行/1,000ms，100Hz；全部 PASS | `docs/arch_survey_r1/scratch/tests_native/handle_velocity_test.log:1-17`；`handle_velocity_test.csv`；源 `介入机器人/tools/reliability_round5/handle_velocity_tests.cpp:13-59` |
| `recording_feedback_test` | 标定、记录器 CSV/error/drain/schema8、事件、双手柄保持/反馈、100ms latch、2mm/400ms 释放、300ms blend | 全部 PASS；记录器默认开启 | `docs/arch_survey_r1/scratch/tests_native/recording_feedback_test.log:1-14`；`介入机器人/B数据端/工程/ADS项目/tools/recording_feedback_test.cpp:1`、`介入机器人/B数据端/工程/ADS项目/tools/recording_feedback_test.cpp:375` |

### 2.2 额外本体/可靠性回归

| 套件 | 结果 | 事实范围与证据 |
|---|---|---|
| `handle_net_distance_test` | PASS | 生产门控排除完整换手、自动模式、故障、轴4点动和非活动轴；验证实际带符号位移、边界、无效快照、重建、轴隔离。`docs/arch_survey_r1/scratch/tests/net_distance.log:1-3` |
| 第五轮 recovery | 129 checks PASS | V1/V2/V11 生产恢复和寻参。`docs/arch_survey_r1/scratch/tests/v5_recovery/results.log:1-2` |
| 第五轮 ST 模型 | 29,228 checks PASS | SelfCheck 5,686 cycles；普通 err 链435；四轴回退108；新 ST 退出62 checks；完整 ST 11,049 cycles，最大差0。`docs/arch_survey_r1/scratch/tests/v5_st/results.log:1-7` |
| 手柄 replay | PASS | `reopen_2mm_10deg`、gap1600、epoch50、epoch wrap、500ms settle、force hold 清零等生产抽取路径。`docs/arch_survey_r1/scratch/tests/handle_replay/fixed/results.log:1-45`；场景生成 `介入机器人/tools/revision_uhn/handle_tests.cpp:148-202` |
| 网络 replay | 6/6 PASS | Zenoh、ADS、both 各10s/30s中断；均一 entry/exit；恢复后 control_active=1。示例 `docs/arch_survey_r1/scratch/tests/network_replay/fixed/zenoh_10.log:1-11`；其余五个同目录日志 |

## 3. 主端、协议和回环链

| 测试 | 结果 | 覆盖和限制 | 证据 |
|---|---|---|---|
| WPF `VI.Control.Tests` | 11,621 assertions PASS | WPF 模拟服务、布局/DPI、读数、门控、保持按钮、定位臂点动/归零状态和截图；模拟启动不构造真实手柄服务 | `docs/arch_survey_r1/scratch/tests/wpf_tests.log:11622`；`介入机器人/A主端/工程/master/VI.Control.Tests/Program.cs:42-52` |
| C# protocol managed | 178 PASS / 0 fail | golden encode/auth/replay、RobotService、LocalForce、HandleHost crash/stall/USB/remove；真实宿主只加载本轮构建的 Fake SDK | `docs/arch_survey_r1/scratch/tests/protocol_managed_tests.log:187`；`介入机器人/A主端/工程/master/MasterConsole.Protocol.Tests/HandleHostTests.cs:33-132`；`介入机器人/tools/reliability_round5/run_host_sdk.py:14-26` |
| C++ Zenoh protocol | 84 PASS / 0 fail | 编解码、认证、非法长度/NaN/篡改及 100Hz 单调样本 | `docs/arch_survey_r1/scratch/tests/zenoh_cpp_protocol.log:85`；`介入机器人/B数据端/工程/ADS项目/tests/zenoh/protocol_tests.cpp:20-136` |
| C++ Zenoh service | PASS | FIFO256、溢出/顺序、双手柄隔离、2s expiry、session/sequence replay、结果/任务状态、认证隔离和资源转换 | `docs/arch_survey_r1/scratch/tests/zenoh_cpp_service.log:1-3`；`介入机器人/B数据端/工程/ADS项目/tests/zenoh/service_tests.cpp:417-434` |
| C#↔C++ Zenoh loopback | 200 PASS / 0 fail | 127.0.0.1 网络、100Hz handle/force、约30Hz state、认证/重连/重建 session、release 持久化、失效句柄、外部 router 保留 | `docs/arch_survey_r1/scratch/tests/zenoh_cross_language.log:188-242`；`介入机器人/A主端/工程/master/MasterConsole.Protocol.Tests/NetworkSmoke.cs:30-49` |
| WPF→Zenoh→生产归零服务→内存 PLC→WPF | E10 五场景 PASS | normal、already_home、规划错误1002、PLC拒绝2003、点动占用1008 | `docs/arch_survey_r1/scratch/tests/home_chain.log:1-32`；`介入机器人/tools/revision_round7/home_chain_simulator.cpp:1-9` |

## 4. 定位臂/PLC 模型验收

### 4.1 执行结果

当前 `run_cart_jog_acceptance.py --round4` 编译了九个可执行程序；ST 翻译和静态审计也执行。**已确认**：四个可执行程序退出0，五个退出1；`results.json` 保存了完整 exit map：`docs/arch_survey_r1/scratch/tests/arm/round4/results.json:1-29`。

| 项目 | 结果 | 证据及解释 |
|---|---|---|
| ST translation | PASS | 1,733 tokens、338 C++ 行、语句全部消费；`docs/arch_survey_r1/scratch/tests/arm/round4/st_translation.log:1` |
| stage translation（旧 ST） | PASS | 1,109 tokens、199 C++ 行；`docs/arch_survey_r1/scratch/tests/arm/round4/stage_translation.log:1` |
| stage translation（pre-round4） | PASS | 1,060 tokens、193 C++ 行；`docs/arch_survey_r1/scratch/tests/arm/round4/stage_translation_before.log:1` |
| ST static audit | PASS，但未构建 PLC | XML、调用顺序、1,000µs task、ST 门控/FB API 等检查通过；最后一行明确 no TwinCAT XAE/PLC compiler。`docs/arch_survey_r1/scratch/tests/arm/round4/st_static_audit_round4.log:1-63` |
| `home_regression` | FAIL | 100 cases、1,700 doubles plus error/already_home，mismatches=274；`docs/arch_survey_r1/scratch/tests/arm/round4/home_regression.log:1` |
| `arm_cart_jog_regression` | FAIL | second-round 30 scenarios、99,619 cycles；与同大小 baseline 首差 byte 375,285；`docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_regression.log:1-2` |
| `arm_cart_jog_round4_regression` | FAIL | pre-round4 45 scenarios、160,120 cycles；首差 byte 375,285；`docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_round4_regression.log:1-2` |
| `arm_cart_jog_stage_tests` | PASS | S5.3 与 pre-round4/reference 2,387,200 bytes 首差等于长度；S5 完整阶段 3,232 cycles exact；`docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_stage_tests.log:1-9` |
| `arm_cart_jog_round4_diagnostics` | FAIL | B、S1、S2、S4 共4项；ST/C++ diagnostic cycle equivalence 通过；`docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_round4_diagnostics.log:7`、`docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_round4_diagnostics.log:16`、`docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_round4_diagnostics.log:21`、`docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_round4_diagnostics.log:31`、`docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_round4_diagnostics.log:33-36` |
| `arm_cart_jog_tests` | FAIL | Y1、S4 两项；T1/T2/T3/T4/T5/T6/T10/T12、ST_EQ 等其余断言通过；`docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_tests.log:2-36`、`docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_tests.log:57`、`docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_tests.log:141-146` |
| `arm_cart_jog_equivalence` | PASS | ST translated vs handwritten reference 765,311 cycles，位置/速度/加速度/路径最大差0；`docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_equivalence.log:1` |
| `arm_ads_tests` | PASS | 单轴档位/deadman、快照读失败时原子点动/alive、速度/模式变化、释放/租约、健康门、归零互斥、reset/disable/shutdown；无硬件；`docs/arch_survey_r1/scratch/tests/arm/round4/arm_ads_tests.log:1-8` |
| `kinematics_tests` | PASS | 20/20，无硬件；`docs/arch_survey_r1/scratch/tests/arm/round4/kinematics_tests.log:1-22` |

### 4.2 失败归属

**已确认**：这些失败在当前 survey 前已被历史报告保留，并不是本轮新引入的测试改写。`介入机器人/docs/revision_round7_20261109/REPORT.md:147-151` 明确记录：旧 `home_baseline.txt` 274 个差异来自极简轮新零位；第二/三/四轮二进制黄金与新零位和 30°/s 限速不符；旧 `arm_cart_jog_tests` 的 Y1/S4 仍失败，S4 最大约25,583.45°/s²；round4 B 是针对10°/s固定见证，改为30°/s后该固定见证不再成立，S1/S2/S4仍是旧算法边界问题。`介入机器人/docs/revision_round7_smooth_20261109/REPORT.md:73-78` 又确认生产代码仍保留这些既有失败，DLS候选因E11 T2失败没有合入。

**已确认**：当前 Y1 的具体机制也由代码直接可见：18 cases、63,000 cycles，TCP 漂移等数值很小，但 elbow30、q5=0/±30、dir+ 的三组 `limited_seen=1`，而断言要求全程 `!limited_seen`；代码在 `介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_tests.cpp:363-389` 统计并判定该条件，日志 `docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_tests.log:38-57` 给出各 case。S4 明确把 limited、启动和反向周期也纳入加速度检查，阈值200°/s²（`介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_tests.cpp:530-534`；`介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_round4_diagnostics.cpp:120-146`），日志最大值来自 `docs/arch_survey_r1/scratch/tests/arm/round4/arm_cart_jog_tests.log:132-141`，其中 T9 为25,583.4499529°/s²。

**推断**：历史报告对“零位/限速变化导致黄金不匹配”给出直接时间线，但它没有证明每一个 byte 差异都只由单一源码行造成；需要逐场景黄金 diff 才能做更细归因。**未知**：当前没有现场 NC 轴的实际最大速度配置，不能把离线30°/s模型结果直接等同于设备允许值；历史报告也明确 NC Axis2–5 Maximum 源码未显式保存（`介入机器人/docs/revision_round7_smooth_20261109/REPORT.md:104`）。

## 5. 过时 runner 的失败

以下退出失败不计入当前功能测试失败，因为它们调用了与当前源码布局不符的旧脚本。结论均为**已确认**。

| 旧入口 | 失败 | 当前证据 |
|---|---|---|
| `tools/run_dual_control_test.py` | 编译 `介入机器人/B数据端/工程/ADS项目/tools/dual_control_test.cpp:50` 找不到 `handoff_type.inc` | `docs/arch_survey_r1/scratch/tests/legacy_dual_runner.log:1-3`；现代 `run_offline_tests.cjs all` 的同名 dual test 已通过，见本报告第2节 |
| `tools/check_axis4_window.py` | 查找旧精确 marker `if (planned_return.active() &&\n`，当前源码不存在，抛 `ValueError` | `docs/arch_survey_r1/scratch/tests/legacy_axis4_window.log:1-4`；脚本旧 marker 在 `介入机器人/B数据端/工程/ADS项目/tools/check_axis4_window.py:11` |
| `run_ads_diagnostics_test.py` 的可选项 | runner 仍列出 `remote_recovery_test`、`net_distance_gateway_test`，对应当前目录没有 `.cpp`；本轮没有调用不存在的目标 | **已确认**：`介入机器人/B数据端/工程/ADS项目/tools/run_ads_diagnostics_test.py:17`；历史说明 `介入机器人/docs/revision_uhn_20261108/README.md:63` |

### 5.1 发现但本轮未直接调用的入口

| 入口 | 当前性质 | 本轮处理 | 确定程度与证据 |
|---|---|---|---|
| `介入机器人/A主端/工程/tools/check_master_ui.ps1` | 构建 `VI.Control.Tests` 后运行 UI 离线验证 | 未直接调用；本轮用同一测试 EXE 完成最终输出重定位 | **已确认**：`介入机器人/A主端/工程/tools/check_master_ui.ps1:1-12`；执行结果 `docs/arch_survey_r1/scratch/tests/wpf_tests.log:11622` |
| `介入机器人/B数据端/工程/PositioningArmKinematics/BuildAndTest.ps1` | CMake configure/build/CTest，`BUILD_TESTING=ON` | 未直接调用；本轮 runner 对同一 C++ 模型逐个编译并执行 round4 扩展项目 | **已确认**：`介入机器人/B数据端/工程/PositioningArmKinematics/BuildAndTest.ps1:1-42`；实际执行 `docs/arch_survey_r1/scratch/tests/arm/round4/results.json:1-29` |
| `介入机器人/B数据端/工程/ADS项目/tests/run-zenoh-tests.ps1` | 编译 C++ protocol/service/simulated_ads，并可调用 C# 网络测试 | 未直接调用；本轮 `run_zenoh_and_home.py` 执行等价编译/回环命令，并把输出写入 scratch | **已确认**：`介入机器人/B数据端/工程/ADS项目/tests/run-zenoh-tests.ps1:1-54`；编排 `docs/arch_survey_r1/scratch/run_zenoh_and_home.py:1-32` |
| `介入机器人/A主端/工程/手柄/Test.cpp` | 调用 `Handle.init()` 后每20ms循环 `showinfo()`；属于真实手柄交互程序 | 未运行，避免打开物理手柄 | **已确认**：`介入机器人/A主端/工程/手柄/Test.cpp:1-20` |
| `介入机器人/tools/reliability_round5/probe_sdk.py` | 枚举 Windows 设备、读取 SDK 导出表和 PnP 设备，不加载 SDK/不打开设备 | 未运行；本轮只读取源码和现有 records | **已确认**：`介入机器人/tools/reliability_round5/probe_sdk.py:1-30` |
| `介入机器人/tools/reliability_round5/finalize_delivery.py` | 读取 git diff、校验交付清单，不执行测试 | 未运行；属于历史交付审计工具 | **已确认**：`介入机器人/tools/reliability_round5/finalize_delivery.py:1-45` |
| `介入机器人/tools/revision_round7/run_dls.py`、`run_smooth_e11.py` | 只引用 `docs/` 中未合入的 DLS 候选，后者保留原T1/T2/T10/T12输入/阈值 | 本轮未重跑候选。历史记录：原候选E8加速度失败；smooth候选E11 T2失败，未合入 | **已确认（历史结果）**：`介入机器人/tools/revision_round7/run_dls.py:1-44`；`介入机器人/tools/revision_round7/run_smooth_e11.py:1-9`；`介入机器人/docs/revision_round7_20261109/REPORT.md:8-9`；`介入机器人/docs/revision_round7_smooth_20261109/REPORT.md:3`、`介入机器人/docs/revision_round7_smooth_20261109/REPORT.md:61-65` |

这些入口的“本轮未运行”不代表通过；表内已注明可由同一程序执行结果判断的项目、仅有历史结果的项目和无法离线执行的真实手柄程序。`run_ads_diagnostics_test.py` 中已知的两个不存在目标另见第5节。

## 6. 构建、警告与静态分析

| 构建/检查 | 结果 | 证据 |
|---|---|---|
| ADS.sln Release/x64 | 成功，0 warnings、0 errors，25.31s；使用 `/warnaserror /p:TreatWarningAsError=true`；只构建，未运行 ADS.exe | `docs/arch_survey_r1/scratch/tests/ads_build.log:25-28` |
| MasterConsole.sln | 成功，0 warnings、0 errors | `docs/arch_survey_r1/scratch/tests/master_build.log:12-15` |
| VI.Control.Tests.slnx | 成功，0 warnings、0 errors | `docs/arch_survey_r1/scratch/tests/wpf_build.log:14-17` |
| MasterConsole.Protocol.Tests | 成功，0 warnings、0 errors | `docs/arch_survey_r1/scratch/tests/protocol_build.log:10-13` |
| VI.HandleHost | 成功，0 warnings、0 errors | `docs/arch_survey_r1/scratch/tests/handlehost_build.log:7-10` |
| .NET 环境 | SDK 9.0.315，MSBuild 17.14.43 | `docs/arch_survey_r1/scratch/tests/dotnet_info.log:1-12`、`docs/arch_survey_r1/scratch/tests/dotnet_info.log:23-37` |
| C++/PLC 编译选项 | ADS 工程 WarningLevel3/SDLCheck；定位臂 CMake 测试 `/W4`；PLC 工程未由 TwinCAT 编译 | `介入机器人/B数据端/工程/ADS项目/ADS.vcxproj:91-142`；`介入机器人/B数据端/工程/PositioningArmKinematics/CMakeLists.txt:20-51`；`docs/arch_survey_r1/scratch/tests/arm/round4/st_static_audit_round4.log:63` |
| PLC 静态审计 | XML、ST block nesting、MAIN 调用顺序、task 1000µs、软限位/速度/FB API 和 no-TcUnit 通过 | `docs/arch_survey_r1/scratch/tests/arm/round4/st_static_audit_round4.log:1-61`；审计源码 `介入机器人/B数据端/工程/PositioningArmKinematics/tests/audit_cart_jog_st.py:114-125` |
| 现有 `Project_Code_Analysis_Report.txt` | 文件是变量/生命周期等叙述性工程分析；本轮未将它计为当前编译器或静态分析器通过记录 | **已确认**：`介入机器人/C从端/Project_Code_Analysis_Report.txt:1-22`；当前工具执行证据仍以上方 scratch 日志为准 |
| 独立静态/覆盖工具 | 本轮未找到生产工程中启用的 cppcheck、clang-tidy、CodeQL 或覆盖率输出；C++ ADS `/W3 + SDLCheck`、CMake `/W4` 和 ST audit 是可见检查 | **已确认** 的配置见上方；**未知**：仓库外 CI/IDE 是否另有分析配置 |

Survey harness 曾出现过两类仅由输出重定位造成的错误：把 `obj` 纳入默认编译导致 CS0579，WPF 临时项目资产路径不匹配导致 NETSDK1004；最终使用排除 `obj/bin`、项目目录叶名隔离中间目录后所有正式构建通过。原始 harness 失败保留在 `docs/arch_survey_r1/scratch/tests/*_attempt1.log`、`*_attempt2.log`，不能归因于生产源码（例如 `docs/arch_survey_r1/scratch/tests/master_build_attempt1.log:26-31`、`docs/arch_survey_r1/scratch/tests/master_build_attempt2.log:12`）。另一次 home simulator 首次编译缺 ADS include path，修正 scratch 编译参数后通过；`docs/arch_survey_r1/scratch/tests/home_chain_simulator_build_attempt1.log:2-4`。

## 7. 目前完全没有或仅有间接覆盖的核心流程

下表区分“代码搜索/测试入口直接证明的缺口”和“本轮没有证据”。**没有测试覆盖不等于功能失效**。

| 流程 | 当前覆盖事实 | 确定程度与证据 |
|---|---|---|
| 真实 100Hz main 每拍等待 ADS→判定→发布的周期/抖动 | 离线 tests 用替身逐拍/虚拟时钟；历史 CSV 只有 force/motion 记录间隔，未保存 main 每拍开始/结束时间 | **已确认缺失；时序结果未知**：`介入机器人/B数据端/工程/ADS项目/tools/run_offline_tests.cjs:20-23`；`docs/arch_survey_r1/scratch/C_logs_notes.md:49` |
| 真实 ADS 往返、PLC 快照丢失和设备重连 | `ads_diagnostics_test` 注入 mock 失败，network replay 也是 offline；session RTT/counters 全为 null | **已确认未实测**：`docs/arch_survey_r1/scratch/tests_native/ads_diagnostics_test.log:46-119`；`docs/arch_survey_r1/scratch/C_logs_notes.md:50-51` |
| 真实 TwinCAT/NC/驱动器 1ms 任务 | ST 翻译/模型和静态审计通过，但 `NOT BUILT`；没有下载或现场执行 | **已确认未覆盖**：`docs/arch_survey_r1/scratch/tests/arm/round4/st_static_audit_round4.log:63`；`介入机器人/tools/reliability_round5/translate_st.py:1`、`介入机器人/tools/reliability_round5/st_runtime.h:11` |
| 屈曲恢复的物理按键→主端采样→Zenoh→数据端 Active 分支 | `return_recovery_test` 直接调用 `tick(bool pressed)`；`dual_control_test` 直接构造 `cat` 位掩码并手动设 `SpacingRecoveryPhase::Active`；runner 的 `enter` 抽取在 `if(emergency_retract_active)` 前结束，排除 Active motion branch | **已确认间接覆盖，物理链路未知**：`介入机器人/B数据端/工程/ADS项目/tools/return_recovery_test.cpp:120-142`；`介入机器人/B数据端/工程/ADS项目/tools/dual_control_test.cpp:343-380`；`介入机器人/B数据端/工程/ADS项目/tools/run_offline_tests.cjs:74-76` |
| 按住屈曲恢复键跨 ADS connection hold 后是否自动重入 | 测试覆盖 reset/release/recovery，但没有“请求已置位、保持键仍按住、connection hold 清零后自动重入”的场景 | **推断存在未覆盖窗口**：`介入机器人/B数据端/工程/ADS项目/main.cpp:1058`、`介入机器人/B数据端/工程/ADS项目/main.cpp:1302-1306`、`介入机器人/B数据端/工程/ADS项目/main.cpp:1508-1569`；现有测试入口见上行 |
| 单手柄/双手柄全流程与物理手柄故障 | 单/双手柄状态门控、HandleHost fake SDK、replay 有覆盖；没有真实 USB、真实手柄固件或两手柄同步时序 | **已确认离线；现场未知**：`介入机器人/A主端/工程/master/MasterConsole.Protocol.Tests/HandleHostTests.cs:33-132`；`介入机器人/B数据端/工程/ADS项目/tools/remote_handle_test.cpp:1` |
| 真实 tracking PI/补偿器动作 | `return_recovery_test` 的 `tracking_controller` 是空 `disable_compensation()` 替身 | **已确认缺失**：`介入机器人/B数据端/工程/ADS项目/tools/return_recovery_test.cpp:80` |
| 数据端启动序列、紧急回撤、单手柄 crawl 的完整状态推进 | 标准测试仅用 startup/emergency 等布尔标志测试门控；没有抽取执行 main 的 `emergency_retract_active` 动作分支和随后启动/crawl 完整分支 | **推断为无专门流程夹具**：抽取清单 `介入机器人/B数据端/工程/ADS项目/tools/run_offline_tests.cjs:34-142`；实际动作 `介入机器人/B数据端/工程/ADS项目/main.cpp:3121`、`介入机器人/B数据端/工程/ADS项目/main.cpp:3297`、`介入机器人/B数据端/工程/ADS项目/main.cpp:3394`；PLC SelfCheck模型不是数据端启动序列 |
| 真实急停、启动、回退续做跨设备交错 | 代码抽取测试验证条件和标志清理；没有 PLC/ADS/Zenoh 实时并发或真实急停输入 | **已确认离线；现场时序未知**：`docs/arch_survey_r1/scratch/tests_native/main_recovery_test.log:1-29`；`docs/arch_survey_r1/scratch/tests_native/round5_main_recovery_test.log:1-31` |
| 定位臂真实机械/NC 约束 | 数学模型、ST/C++ 等价、ADS 服务内存模型通过；无 NC/驱动器 | **已确认缺失**：`docs/arch_survey_r1/scratch/tests/arm/round4/arm_ads_tests.log:8`；`docs/arch_survey_r1/scratch/tests/arm/round4/st_static_audit_round4.log:63` |
| 记录器完整停止/最终 session 汇总 | recording feedback 检查内存记录器、CSV、drain 和 I/O failure；历史真实 session 未完成 final JSON | **已确认前者通过，后者未知**：`docs/arch_survey_r1/scratch/tests_native/recording_feedback_test.log:1-14`；`docs/arch_survey_r1/scratch/C_logs_notes.md:7` |

本轮没有生成覆盖率百分比。测试断言数量很大，但没有 instrumentation 数据，不能从 122,359、29,228 或 11,621 这些数字推导源代码覆盖率。

## 8. 附件和复现产物索引

| 路径 | 用途 |
|---|---|
| `docs/arch_survey_r1/scratch/tests_native/` | 标准本体/ADS 11 测试日志和速度 CSV |
| `docs/arch_survey_r1/scratch/tests/` | 额外 native replay、Zenoh、WPF、managed、构建和历史 runner 日志 |
| `docs/arch_survey_r1/scratch/tests/arm/round4/` | 定位臂 round4 全部 runner 日志、results.json、构建输出索引 |
| `docs/arch_survey_r1/scratch/analyze_existing_records.py` | 只读扫描四套 records 和三份 ADS diagnostic 的脚本 |
| `docs/arch_survey_r1/scratch/existing_records_stats.json` | CSV 行数、valid 计数、时间间隔分布、诊断日志分类计数 |
| `docs/arch_survey_r1/scratch/C_logs_notes.md` | 历史 session/ADS 日志对 C.4 时序问题的可证明事实和未知项 |
| `docs/arch_survey_r1/scratch/run_survey_checks.py`、`run_zenoh_and_home.py`、`run_legacy_runners.py` | 本轮只读编排/重定向脚本；没有修改生产测试 |
