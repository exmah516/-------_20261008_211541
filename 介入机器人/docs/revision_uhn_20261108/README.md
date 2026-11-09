# U / H / N 修订交付与验收记录

日期：2026-11-08。源码基线见[git_head.txt](git_head.txt)，关键新旧源码及行号见[source_evidence.txt](source_evidence.txt)。本轮完成H软件修改和离线回放；N修复重复完成分支，但现场ADS持续非Running根因未解决；U因唯一预览缺失停止。**本轮不满足“U/H/N全部完成、所有原有测试全通过”的总验收条件。**

## U：停止，界面源码未改

提示词称随附`主端界面预览.html`，本次却没有可读附件。搜索整个允许仓库（含隐藏和忽略目录）未发现该文件或任何`*预览*.html`，另一个允许的可视化目录为空。证据：[preview_search.txt](preview_search.txt)。按“唯一参照”“无法按规定实现时停止该项”约束，没有凭文字补造预览。

MainWindow、主题、InterventionPanel、PositioningArmPanel及code-behind、MainViewModel和WPF测试源码均与HEAD相同，见[ui_unchanged.json](ui_unchanged.json)。没有控件重命名/删除，也没有因U改动WPF测试。

1.3节原控件清单在[ui_mapping.csv](ui_mapping.csv)，记录Command/事件、Tag、CommandParameter、IsEnabled、ToolTip。新控件列明确标记“U停止，保留原控件”。这是基线清单，**不是新界面映射落实或测试通过记录**。

| 1.3区域 | 原控件→新控件 | 状态 |
|---|---|---|
| 状态条、提示行 | 尚无新布局对应 | 未实现 |
| 器械准备、力反馈/夹持、电缸逐项绑定 | 尚无新布局对应 | 未实现 |
| 轴6窗口注释、器械辅助 | 保留原布局 | 未实现 |
| 定位臂分段速度、10个末端按钮、调试区 | ComboBox等原控件保留 | 未实现 |
| 启用条件、ToolTip和事件链映射验收 | 原文件相同，未建新界面映射测试 | 未执行 |

原182项WPF回归产生[baseline_ui](baseline_ui/README.md)中的19张原界面PNG。要求的1280×800、2560×1600、准备展开、定位臂调试展开、未连接五张**新布局**RenderTargetBitmap截图未生成，不以原图替代。

## H：修改与根因

详见[handle.md](handle.md)、[protocol.md](protocol.md)。重新打开、超过1.5 s采样间隔、设备epoch变化进入500 ms连续有效采样静置；中断重计时，以结束样本重建已有滤波与四轴基准。静置走原连接保持停跟随和清力，并补上循环末尾力反馈调用的手柄保持条件。

确认的缺口是首帧立即跟随无法排除开机后续机械运动、快速重开零点改变但协议无设备会话信号、入口清力后末尾又施力。原源码已复位线性/旋转滤波；fVels用于力阻尼而非四轴位置前馈；正常跨圈的SDK约定缺少证据。五项逐项核查、旧路径重现与修复后连续性证据均在H报告。

主端、HandleHost、数据端、HMAC/严格解码、双方测试与黄金向量同步修改。HandleFrame由24项扩展为26项，按仓库既有严格数组扩展惯例保留v1和Topic，不兼容旧长度；主端协议两份副本和数据端协议说明均同步。

## N：重复完成已修，持续ADS故障未解决

详见[network.md](network.md)。源码为本机AMS优先、远程`169.254.119.135.1.1:851`回退；现场实际路由和网线是否共用无法由仓库确定。

HEAD分支在手柄保持期间错误报告完成547次；增加手柄就绪门控后，六场景各仅一次完成。新增ADS状态变化原因和保持来源日志，移除周期重复日志。只断Zenoh、只断ADS、两者同时断，各10/30 s，六项通过，ADS Running及保持退出耗时109–516 ms。

mock未复现现场ADS持续非Running，未将这一根因标为修复，也未增加自动RUN或恢复绕过。

## 构建与测试

| 项目 | 结果 | 证据 |
|---|---|---|
| MasterConsole.sln（包括VI.HandleHost）Release x64 | 0警告、0错误 | master_build.log |
| VI.Control.Tests.slnx Release x64 | 0警告、0错误 | wpf_build.log |
| MasterConsole.Protocol.Tests | 0警告、0错误 | protocol_build.log |
| ADS.sln Release x64 Rebuild | 成功，无警告/错误；启用warnaserror和TreatWarningAsError | ads_build.log；network/ads_build.log含首次完整汇总 |
| 原WPF | 182/182 | wpf_tests.log |
| Zenoh C++ | 84通过，原81条标签全部保留 | zenoh.log |
| C#真实Zenoh loopback | 181通过，原174条标签全部保留 | zenoh.log |
| 真实HandleHost + fake SDK | 159通过，原151条标签全部保留 | host_sdk_results.log |
| 原生main/dual/recovery/cylinder/ADS/handle/recording | 全部通过 | native.log、native/*.log |
| V5恢复/寻参 | 129检查通过 | v5_recovery/results.log |
| V5 PLC ST | 29228检查；11049基线/当前周期差为0 | v5_st/results.log |
| 原净距离测试 | 通过 | handle_net_distance_test.log |
| 定位臂 | 归零、round3/4确定性、stage、ST等价、ADS、kinematics20项通过；两套诊断失败 | arm/round4/results.json |
| H新增 | 11场景通过；HEAD9个缺口场景确认重现 | handle/{fixed,baseline}/results.log |
| N新增 | 六场景通过，日志单次变化审计通过 | network/log_audit.json |

定位臂失败未忽略：`arm_cart_jog_round4_diagnostics`有S1、S2、S4共3个失败；`arm_cart_jog_tests`有S4共1个失败。两份日志规范化后与上一轮记录逐字相同，17个相关源码文件与HEAD一致，见[arm_baseline_comparison.json](arm_baseline_comparison.json)。按“不改变其它控制行为”约束未改发生器、未降低断言。[regression_results.json](regression_results.json)保留arm=1，不能宣称全部原测试通过。

原Release输出被正在运行的VI.Control占用。没有终止现有进程，使用临时导入原Directory.Build.props并覆盖OutputPath，隔离构建到`A主端/工程/build/UhnRelease`。ADS输出也隔离；只编译、不运行生产exe。源码项目配置未改。

`regression_results_attempt1.json`保留两个旧runner可选名称但源码不存在的调用失败：net_distance_gateway_test、remote_recovery_test。确认非现存测试后，从本轮runner目标清单移除；未补造测试或把失败改写为通过，原失败log仍保留。

### 测试改动逐项说明

- `VI.Control.Tests/Program.cs`未改，原182项保留。无控件删除或重命名引起的测试变更。
- `remote_handle_test.cpp`原“严格大于1500 ms”改为触发静置、不立即rebase；连续采样500 ms后才rebase。原1000/1600/4000/6000 ms场景、门槛和断开语义保留；不再断言本轮明确废止的“立即重建”。
- `main_recovery_test.cpp`保留原开始控制、模式、恢复断言，增加手柄失效100拍不得报告基准完成。
- `ads_diagnostics_test.cpp`的mock增加network_down注入，默认false；原93条诊断标签继续执行。
- C# `Program.cs`和C++ `protocol_tests.cpp`更新26字段黄金样本，断言不同非零epoch 7/9、认证篡改拒绝；新增负数/字符串epoch非法向量。旧非法向量同步索引，保留原错误类别。
- C++ `service_tests.cpp`增加真实认证接收边界两设备epoch分别到桥接的断言，原边界断言保留。
- `HandleHostTests.cs`替身发共享计数epoch；crash/stall/USB插入/拔出以及真实SDK替身重开验证递增，保留原重启周期、采样速率和清力检查。

[final_test_counts.json](final_test_counts.json)、[test_counts.json](test_counts.json)、[test_inventory.csv](test_inventory.csv)提供计数和原标签比对。标签比对不替代语义审查，具体调整如上。

## 源码与提示差异

1. HTML未实际提供，U停止，未按文字推测唯一预览。
2. 实际映射：导管SN587→轴1/2，导丝SN582→轴6/7。滤波复位和直线base_rel重建原本存在。
3. SDK负状态原来只发invalid，没有每设备重新open的既有流程；未另加重开策略，只在每次真实成功open递增epoch。
4. 原监管器已有至少1 s启动间隔，不能把现场“50 ms”直接当作整个监管重启周期。50 ms回放验证快速坐标会话改变不再依赖1.5 s检测。
5. SDK未定义手柄关节0的物理长度单位；离线输入使用现有缩放的2 mm等效量，实物精确2 mm未验收。
6. ADS存在本机优先/远程回退，现场路径与网线共用未确认。
7. 定位臂原测试已有失败，本轮未将其忽略。

## 现场验收：交用户执行，均未执行

1. U完成后，对照新截图检查1/4屏和全屏；逐个试按按钮，核对原命令、按住/松开、启用条件和ToolTip。当前U未重排，此项尚未就绪。
2. 控制中分别拔插导丝582和导管587 USB，核对有效采样恢复后连续约0.5 s才跟随，接回时实物位移2 mm、旋转10°时四轴不窜动；静置中再次中断应重计时。既有宿主重启/设备重试等待保留，从物理插入算起可能超过0.5 s。
3. 控制中断网10 s和30 s后接回，确认ADS Running、原控制在当前位置自动恢复、保持完成只打印一次。保留[ADS-STATE]及exe旁ads_fault_diagnostics.log；若仍非Running，用目标NetId、操作/错误码与PLC/Router状态补齐N现场证据。

## 文件与复现

- 完整源码、测试、协议及说明差异：[changes.patch](changes.patch)；纳入范围与哈希：[diff_inventory.json](diff_inventory.json)。原始日志、轨迹CSV、PNG旁置，不复制成补丁；构建产物不进入源码diff。
- 未提交或推送。原其它项目脏子模块未动；测试生成的三份已跟踪exe恢复为基线。
- 在介入机器人目录运行：`python tools/revision_uhn/run_managed.py`；`python tools/revision_uhn/run_handle.py --baseline`；`python tools/revision_uhn/run_handle.py`；`python tools/revision_uhn/run_network.py --baseline`；`python tools/revision_uhn/run_network.py`；`python tools/revision_uhn/run_regressions.py`。最后一个脚本因上述定位臂既有失败预期非零，不吞失败。

未执行：新UI、五张新截图与映射测试、实物USB/网线/PLC验收、SDK物理跨圈及长度校准。
风险：26字段HandleFrame与旧24字段不互通，主端、HandleHost、数据端须同步部署；ADS持续非Running现场根因未确认，当前交付不能视为整体验收通过。
