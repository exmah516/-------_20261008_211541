# 主端界面重排 U：修改与验收报告

日期：2026-11-08。范围按补充要求收敛为第六轮第 1 节 U，验收执行第 4 节第 1–4 项。

U 的源码重排、界面映射测试及五张 WPF 截图已完成。四个指定构建全部 0 警告、0 错误；WPF、Zenoh、C#、V5 与原生离线测试通过。**原定位臂测试仍有 4 个既有失败断言，因此“全部原有测试全绿”的验收条件尚未满足。** 两份失败日志与本轮开始前的记录逐字一致，未修改控制源码或放宽断言。

## U：修改摘要

- 主窗口使用 `Viewbox Stretch="Uniform"` 包裹 1280×800 DIP 设计面；设计面启用 `TextFormattingMode="Ideal"`，窗口背景填充非 16:10 区域。启动位置、宽度与高度计算保留原逻辑。
- 状态条和提示行直接使用构造函数传入的 MainViewModel。原模块实例按稳定模块 ID 挂入左右 ContentPresenter，面板自身 DataContext 不变。移除大标题、外层 Expander 及其折叠停止处理器；窗口 OnClosing 的逐面板 Stop 保留。
- 器械准备为手动展开/收起，默认收起；力反馈分为左导丝、右导管两张卡片，卡片内显示力、力矩、角度表盘及对应电缸。轴 6 窗口位置整块保存在指定的 XAML 注释中，属性与原有窗口位置计算不变。
- 器械辅助保留全部按住、松开和丢失捕获事件。定位臂为五行十个末端按钮；模式来自按钮 Tag，速度刷新使用原 GetArmCartesianJogSpeed。100 ms 保活、松开停止、丢失捕获停止及按住切档立即重发保持原流程。五轴调试控件移入手动展开区，默认收起。
- MainViewModel 仅增加两个单柄在线只读属性、两个角度文本只读属性及对应通知；电缸只改 Title。新增 ReadoutPartConverter 只拆分原有完整读数字符串的数值与单位以匹配预览字号，不读取状态、不写回、不改变原绑定产生的数值与单位。
- LightClinical 保留全部 **41 个既有显式资源键**；更新配色、开关、按住按钮、状态胶囊、方向、分段单选、读数卡片及标题样式。没有新增依赖。

完整 U 源码、测试及复现脚本 diff：[u-only.patch](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/u-only.patch>)（18 个文件；已通过 `git apply --reverse --check`；真实 Git 暂存区未改变）。

补丁 SHA-256：`b582c0e457f3129991debaf7c5399c3d783e5f2ac7b096d4c26c6f1b3f2cda55`。

## 1.3 对应关系落实

逐控件属性明细：[74 行完整映射表](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/ui_mapping.md>)；[CSV](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/ui_mapping.csv>)。74 行包括 48 条原交互控件/模板映射、10 个末端模式与方向矩阵项、16 条原有只读显示绑定。测试比较 Command 与五类事件、Tag、CommandParameter、本地与祖先 IsEnabled、ToolTip，并检查 IsChecked、Text、GroupName；每个电缸还断言实际命令对象及参数对象相同。

| 原控件/内容 | 新位置 | 行为或绑定 |
| --- | --- | --- |
| ModulePanels 与两块外层 Expander | 两个固定 ContentPresenter，右栏 384 DIP | 继续使用原模块实例；按要求删除折叠停止路径，OnClosing 保留 |
| ConnectButton | 主窗口状态条 | ConnectCommand / ConnectButtonText 不变 |
| 从端、ADS、单柄状态、阶段、方向 | 主窗口状态条 | IsConnected + HasControl、AdsText、Handle582Online / Handle587Online、PhaseText、ModeText；颜色仅绑定和触发器 |
| OperateHintText / RecoveryText / LastWarning | 主窗口提示行 | 顺序不变，空项隐藏，全空时整行折叠 |
| CatheterPosition / WirePosition | 器械准备展开内容 | 名称、Text 双向更新及自动化名称不变 |
| Prepare / StartControl | 准备卡片与收起摘要行 | 原命令；StartControl 仍叠加 CanStartControl；整区 CanOperate |
| ForceZero / ForceHold | 力反馈标题行右侧 | 原命令、状态绑定、原 ToolTip 不变 |
| GuidewireForceFeedback / CatheterForceFeedback | 导丝/导管卡片右上 | 原命令及 OneWay IsChecked 不变 |
| Force582F/N；Force587F/N | 导丝/导管卡片 | 继续取原绑定；纯文本拆分数值/单位样式 |
| Axis7PointerX/Y；Axis2PointerX/Y | 导丝/导管旋转表盘 | 原坐标绑定；角度文本分别取 AxisPos[6] / AxisPos[1] |
| 电缸 ItemsControl 的一项模板 | 明确 Cylinder1–4 控件 | 同一 CylinderCommand、同一项 CommandParameter、同一 Engaged 和 ToolTip；3/4 在导丝、1/2 在导管 |
| Axis6WindowProgress 与提示 | 完整 XAML 注释 | 按要求不显示；VM 的只读属性测试保留 |
| Y 阀 | 器械辅助标题行右侧 | YValveCommand / YValveClosed / YValveCaption 不变 |
| Axis4Reverse / Axis4Forward | 球囊输送行 | 原名称、A4 Tag、−1/+1、Down/Up/LostMouseCapture 不变 |
| 注射器 1/2 的推/拉 | 器械辅助后两行 | 原 Tag、方向、Down/Up/LostMouseCapture 不变；为原匿名按钮补稳定测试名称 |
| 总使能 / 三个速度 RadioButton | 定位臂标题、分段速度条 | 原 Click / Checked、名称、绑定、ToolTip 和启用条件不变 |
| ArmCartesianMode + CartesianReverse/Forward | 五行十按钮 | 原两按钮名称留给 X 行；Tag=1–5，方向=−1/+1；原事件链；仍不绑定 IsEnabled |
| ArmCartesianRate | ArmCartesianRate1–5 | 每行用原函数和原单位刷新；中档 15 / 15 / 2.5 mm/s、0.8 / 1.6°/s |
| ProgramHome / Stop / 两条错误提示 | 点动行下 | 原处理器；归零受 CanOperate，停止不受启用绑定；空错误隐藏 |
| 五轴上电、复位、± | 定位臂调试展开区 | 原名称（含 Axis1Forward）、Tag、事件、方向、CanOperate 保留 |

## 构建与测试

全部构建使用 Release/x64、warnings as errors。为避开运行中主端的 DLL 锁，主端使用独立 `A主端/工程/build/URelease/bin` 输出，未关闭或重启运行中的控制窗口。ADS 仅编译，未运行生产控制程序。

| 项目 | 结果 | 日志 |
| --- | --- | --- |
| MasterConsole.sln | 0 警告、0 错误 | [master_build.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/master_build.log>) |
| VI.Control.Tests.slnx | 0 警告、0 错误 | [wpf_build.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/wpf_build.log>) |
| VI.HandleHost.csproj | 0 警告、0 错误 | [handlehost_build.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/handlehost_build.log>) |
| ADS.sln | 0 警告、0 错误 | [ads_build.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/ads_build.log>) |
| WPF，含界面映射、五图及新增显示测试 | **1186 个断言通过，0 失败** | [wpf_tests.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/wpf_tests.log>) |
| Zenoh C++ | 84 通过、0 失败（包含原 81 项） | [zenoh.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/zenoh.log>) |
| C# 实际 Zenoh 本地框架 | 181 通过、0 失败（包含原 174 项） | 同上 |
| C# 基础/宿主替身 | 141 通过、0 失败 | [managed_tests.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/managed_tests.log>) |
| C# 真实 HandleHost + 假 SDK | 159 通过、0 失败 | [host_sdk_results.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/host_sdk_results.log>) |
| 原生 ADS、手柄、电缸、归零、记录等离线测试 | runner 退出码 0 | [native.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/native.log>) |
| 手柄净距离回归 | 退出码 0 | [handle_net_distance_test.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/handle_net_distance_test.log>) |
| V5 恢复 | 129 个检查通过 | [v5_recovery.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/v5_recovery.log>) |
| V5 完整 ST | 29228 个检查；11049 周期，基线差异 0 | [v5_st.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/v5_st.log>) |
| 定位臂既有测试 | 4 个既有失败，未全绿；其他测试继续执行 | [arm.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/arm.log>) |

四个既有失败：`arm_cart_jog_round4_diagnostics` 的 S1、S2、S4 共 3 个；`arm_cart_jog_tests` 的 S4 共 1 个。两份逐例输出与本轮前 `docs/revision_uhn_20261108/arm/round4` 的记录字节相同；相关数据端/PLC 源码哈希未变。证据见 [scope_and_artifacts.json](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/scope_and_artifacts.json>) 的 `known_arm_failures`，也可对照 [上一轮既有失败核对](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_uhn_20261108/arm_baseline_comparison.json>)。本轮没有更改这些控制算法或测试期望。

### 原 WPF 182 项中因指定结构变化调整的检查

| 原检查 | 本轮调整 | 保留或替代的断言 |
| --- | --- | --- |
| 九组尺寸/DPI 布局：600×560、1000×900、1920×1080，各 1/1.25/1.5 | 仍逐组布局并用 RenderTargetBitmap 在内存中渲染 | 原像素尺寸和 DPI 检查继续执行；增加固定设计面、Uniform、Ideal、384 DIP 右栏 |
| 未展开调试中的 Axis1Forward 必须可见 | 可见性检查使用始终显示的 CartesianForward | 正常界面始终有可达点动按钮；Axis1Forward 另外在调试展开图与映射中检查 |
| 小窗口必须竖滚、底部滚动可达、无横向溢出 | 外层滚动被固定设计面替换，删除滚动条断言 | 改断言等比缩放、设计比例、每个可见交互控件在设计面以内；完整保留九组渲染 |
| 18 张旧布局顶/底部截图与轴 6 警告截图 | 替换为用户指定的五张新布局截图 | 旧截图不再代表验收结果；每张新图检查 PNG 尺寸、缩放几何和可见控件边界 |
| Axis6WindowProgress.Value 与告警 | 控件整块注释，断言控件不存在且注释与冻结原块结构相等 | 原五个位置点、移动窗口、不可用、NaN、过期、断开等测试继续断言 VM 百分比、提示及不发停止命令 |
| 折叠介入/定位臂外层 Expander 要停止 | 两个外层 Expander 按要求删除 | 检查无外层 Expander、无 Panel_Collapsed；手动准备/调试只改显示；窗口关闭停止与焦点变化不全局停止的原断言保留 |
| 壳不能有连接状态条 | 按新规格改成有状态胶囊且无日志面板 | 同时保留“切换焦点不触发全局停止” |
| ArmCartesianMode、单个 ArmCartesianRate 删除 | 原 WPF 测试没有直接以这两个名称查找控件 | 新增五模式全部方向发送、五行速度、失捕停止、保持保活、按住时切档保留模式等运行测试 |
| 电缸 ItemsControl 改为四个显式绑定 | 原执行测试无依赖该 ItemsControl 的名称查找 | 冻结模板逐一比对四个新控件；运行时比较命令/参数引用、名称与原打开值 |

其余原命令及按住事件检查继续执行；没有为取得通过而放宽或删除定位臂控制断言。原 182 是旧布局的断言计数；本轮有上述必要替换并新增检查，不能把新结果表述为未经调整的“182/182”。

## 五张新布局截图

五图均由测试的 RenderTargetBitmap 从真实编译 WPF 控件生成。采用 RecordingRobotLink 提供现有绑定数据，未连接真实设备。尺寸表示界面客户区，不包含操作系统标题栏。2560×1600 图由同一个 1280×800 设计面以 Viewbox 放大，非把第一张 PNG 拉伸。

1280×800：

![1280×800 新布局](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/screenshots/01-layout-1280x800.png>)

2560×1600：

![2560×1600 等比放大](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/screenshots/02-layout-2560x1600.png>)

器械准备展开：

![器械准备展开](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/screenshots/03-preparation-expanded.png>)

定位臂调试展开：

![定位臂调试展开](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/screenshots/04-arm-debug-expanded.png>)

未连接状态：

![未连接状态](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/screenshots/05-disconnected.png>)

HTML 参照：[用户提供的源文件](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/主端界面预览 (1).html>)；[Chrome 实际渲染的 1280×800 参照](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/preview-reference.png>)。参照 SHA-256：`8d87ec15d0d4daa4c4f44ecb7df0a0b3832879f7b07804ca88b6879a45cec342`。

已逐张查看五张 PNG。HTML 与 WPF 的六个卡片矩形逐项比较：状态条、准备卡、左右器械卡、辅助卡、定位臂卡的横坐标与宽度一致；两个力反馈卡的高度存在 0.22 DIP 字体度量差异，辅助卡纵坐标相应差 0.22 DIP，其余矩形一致。见范围证据中的 `card_geometry_comparison`。字体栅格化由各自渲染引擎完成，截图未作像素级相同的声明。

## 源码事实与提示词/预览差异

| 项目 | 源码事实与本轮处理 |
| --- | --- |
| 导丝/导管映射 | 582=导丝、587=导管；轴 7=导丝旋转、轴 2=导管旋转。按源码映射放置，未调换通道 |
| 准备输入标签 | 原标签为“导管搓捻机构位置 (5–95 mm)”，预览为“旋捻”。按输入名称沿用要求保留原标签与绑定；收起摘要按预览使用“旋捻” |
| 力/力矩绑定 | Force582F/N、Force587F/N 原字符串含单位，力矩已转换为 N·mm。只拆显示字号，不另算物理量，不使用示例数字 |
| 速度 | 原中档偏航速度为 1.6°/s，与预览一致；保留各档原值和原单位，无新增系数 |
| 默认窗口尺寸 | 原逻辑为工作区左上角、宽 max(MinWidth, WorkArea.Width×0.65−8)、高 WorkArea.Height。沿用该逻辑；未把启动窗口强制改成 1280×800 或屏幕 1/4 |
| 原测试数量 | 本轮开始时 Zenoh C++ 已是 84、C# 已是 181，而非提示词的 81、174；保留这些既有扩展用例并运行 |
| 定位臂既有失败 | 上述 4 个失败在 U 前已经存在且输出相同，故“原有测试全部通过”本轮未满足，不修改控制行为来消除失败 |

## H、N 与范围证明

H、N 按补充要求不再修改、不新增根因分析或恢复规则。起始工作区本来已有上一轮 H/N 未提交修改，本轮保持其内容。对 **460 个已跟踪文件**核对起始哈希，九个允许修改的既有 U 文件之外无变化；对本轮前后完整既有 diff（包含仓库其他目录）剔除 U 后逐字节相等。InterventionPanel.xaml.cs 哈希相等，原 OnClosing 与处理器链通过测试验证。Zenoh 测试生成的三个已跟踪 EXE 已恢复为核验过的本轮起始字节。

范围、PNG 尺寸/哈希、资源键、构建摘要、卡片矩形及既有失败核对均见 [scope_and_artifacts.json](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_20261108/scope_and_artifacts.json>)。

## 复现与现场待验

在工程根目录执行：

```powershell
python tools/revision_u/run_managed.py
python tools/revision_u/run_regressions.py
python tools/revision_u/verify_and_package.py
```

第二条会因上述定位臂既有失败返回 1，并保留全部逐例输出；回归脚本会在 Zenoh 测试后自动还原三个已跟踪 EXE，保证验收运行不把测试产物带入数据端差异。源码补丁只包括 U，报告和测试日志另附；未提交或推送 Git。

用户现场仍需执行 U 检查：

1. 使用新构建的主端，在 1/4 屏和全屏分别对照五张截图；确认非 16:10 时背景填充、按钮和文字等比缩放。
2. 手动展开/收起器械准备和定位臂调试；逐个验证按钮、每一轴及注射器的按住/松开、丢失捕获、速度切档行为与改动前一致。
3. 关闭窗口，确认原逐面板停止路径仍有效。现有运行实例未被本轮重启；新构建位于 `A主端/工程/build/URelease/bin/VI.Control`。

本轮没有执行实体设备、PLC、USB 拔插或现场断网验收；H/N 的现场复验不在补充授权范围。Windows 实机字体/DPI 与真实长状态文本仍以现场对照为准。
