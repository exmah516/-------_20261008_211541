# 第六轮补充 v3：主端流式布局验收

日期：2026-11-09。基线：开始 v3 时工作区已有的第六轮 U 实现。唯一布局参照：用户提供的 `主端界面预览_自适应v3.html`，原文件 SHA-256 为 `840543ae7788e14ca1d54ce7f533da7d7c7ef6709d632096de5b7971d62d80e0`；[参照副本](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/preview.html>)。

本报告只描述 v3 新增变化。H、N 源码、手柄协议、数据端控制代码及两个面板的 code-behind 均保持开始本补充时的内容。保留所有命令、参数、按住/松开/丢失捕获事件、启用路径及原 ToolTip 信息；电缸 ToolTip 仅按 v3 要求增加完整名称。

## 1. 修改摘要与 diff

- 删除主窗口 `DesignViewbox` 和 1280×800 `DesignSurface`。根内容 `WindowContent` 直接填满窗口，使用 Ideal 文本渲染。
- 新增 Small、Medium、Large 三份尺寸字典。MainWindow 的内容区 SizeChanged 仅选择字典和宽/窄排布标记，阈值集中为常量；不增加连接或控制状态分支。
- 顶部状态条与提示行固定；主体使用垂直 Auto、水平 Disabled 的 ScrollViewer。宽排布为 73*/27*、定位臂限宽；窄排布将同一个定位臂实例排在操作区下方。
- 操作区按 Auto、Auto、3*、2* 排布；辅助三行和末端五行均以等权星号行分配高度。按住按钮在 HoldMinHeight 与 HoldMaxHeight 间伸展，超过上限时居中；窄排布末端按钮维持最小行高。
- 四个读数采用 UpOnly Viewbox，MaxHeight=96，内层字号取 FontSizeReadout。准备摘要改为“导管 … mm · 导丝 … mm”；电缸按钮显示“夹持/旋捻”，ToolTip 保留原状态并增加完整名称。
- 原有 53 个具名主题资源键全部保留，现有 56 个。用于边距的 Thickness、用于间隔行的 GridLength 等类型资源随档位字典一起切换；没有增加转换器或布局类。
- 保留启动位置、工作区宽度计算与启动高度的代码。为允许本补充要求的窄矮窗口，将 XAML MinWidth 从 600 改为 520、MinHeight 从 560 改为 0；这些最小限制的变化不会改写原启动计算式。

[完整 v3 增量补丁](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/v3-only.patch>)，共 18 个源码、测试、基线夹具及复现脚本文件。补丁相对于本补充开始时的工作区，不包含此前 U/H/N 的既有改动。SHA-256：`be22754d77dad3cabe682782493dce29cc28551030ee5d365d6118acea362580`。

已用临时 Git index 导出，并通过 reverse apply --check；真实 Git 暂存区未改变。核对了 4120 个受版本控制文件，范围外文件变化为 0。证据见 [范围与产物核对](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/scope_and_artifacts.json>)、[开始时的文件清单](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/start_manifest.json>)。

主要入口：[MainWindow.xaml](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Control/MainWindow.xaml>)、[唯一新增的尺寸选择逻辑](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Control/MainWindow.xaml.cs>)、[InterventionPanel.xaml](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Module.Intervention/InterventionPanel.xaml>)、[PositioningArmPanel.xaml](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Module.PositioningArm/PositioningArmPanel.xaml>)、[LightClinical.xaml](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Control/Themes/LightClinical.xaml>)。

## 2. 三档尺寸资源

单位均为 DIP。下列 12 项与预览 :root 三组变量逐项一致，测试对三组 36 个值逐一断言。

| 资源键 | 小 | 中 | 大 |
| --- | ---: | ---: | ---: |
| FontSizeBody | 13 | 15 | 17 |
| FontSizeSmall | 12 | 13 | 15 |
| FontSizeTitle | 14 | 16 | 19 |
| FontSizeReadout | 20 | 26 | 32 |
| ButtonHeight | 26 | 32 | 38 |
| HoldMinHeight | 30 | 38 | 46 |
| HoldMaxHeight | 64 | 72 | 84 |
| Gap | 6 | 8 | 10 |
| Pad | 8 | 10 | 12 |
| DialSize | 26 | 32 | 38 |
| ArmMinWidth | 236 | 300 | 360 |
| ArmMaxWidth | 300 | 380 | 460 |

字典：[SizesSmall.xaml](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Control/Themes/SizesSmall.xaml>)、[SizesMedium.xaml](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Control/Themes/SizesMedium.xaml>)、[SizesLarge.xaml](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Control/Themes/SizesLarge.xaml>)。

附加资源仅用于 WPF 属性类型或从上述数值派生的尺寸：按钮 Padding=正文×0.9，开关 Padding=正文×0.75，开关圆点=正文×0.5，按住按钮字号=正文+1，末端 ± 字号=正文+4，胶囊高=按钮高−2。方向标签和提示行按预览 content-box 边框计算，外高=按钮高+2。边距和间隔分别以 Thickness 与 GridLength 保存，控件仍用 DynamicResource。

小档区块内容最小高度分别为：准备 40、力反馈标题 26、器械卡片 138、辅助 152；区块之间另有 Gap。它们在三份字典中保持相同下限，其余档位内容还会受到该档字号和按钮自然高度的约束。

## 3. 构建、回归及映射

所有构建均使用 warnings-as-errors，并输出到独立目录，未停止或替换运行中的主端。

| 构建 | 警告 | 错误 | 日志 |
| --- | ---: | ---: | --- |
| MasterConsole.sln | 0 | 0 | [master_build.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/master_build.log>) |
| VI.Control.Tests.slnx | 0 | 0 | [wpf_build.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/wpf_build.log>) |
| VI.HandleHost | 0 | 0 | [handlehost_build.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/handlehost_build.log>) |
| MasterConsole.Protocol.Tests | 0 | 0 | [protocol_build.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/protocol_build.log>) |
| 数据端 ADS.sln，Release x64 Rebuild | 0 | 0 | [ads_build.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/ads_build.log>) |

| 测试 | 结果 | 证据 |
| --- | --- | --- |
| WPF 原行为、界面映射、档位、渲染与几何 | 11,532 项断言通过，0 失败 | [wpf_tests.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/wpf_tests.log>) |
| 第六轮逐控件映射 | 74 行映射保留，命令/事件/Tag/参数/启用路径/ToolTip 逐项核对 | [ui_mapping.csv](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots/ui_mapping.csv>) |
| C++ Zenoh | 84 通过、0 失败，含原 81 项 | [zenoh.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/zenoh.log>) |
| C# Zenoh 实际互通 | 181 通过、0 失败，含原 174 项 | [zenoh.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/zenoh.log>) |
| C# 协议基础 | 141 通过、0 失败 | [managed_tests.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/managed_tests.log>) |
| 实际 HandleHost + SDK 替身 | 159 通过、0 失败 | [host_sdk_results.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/host_sdk_results.log>) |
| 数据端原生离线测试与实际行程测试 | 全部通过 | [native.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/native.log>)、[handle_net_distance_test.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/handle_net_distance_test.log>) |
| V5 恢复及归零 | 129 项检查通过 | [v5_recovery.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/v5_recovery.log>) |
| V5 ST 一致性 | 29,228 项检查通过；11,049 周期最大差异 0 | [v5_st.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/v5_st.log>) |
| 定位臂整套回归 | 保留 4 项基线失败；其余通过 | [arm.log](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/arm.log>) |

**因此“其余原有测试全部通过”尚未满足：** `arm_cart_jog_round4_diagnostics` 的 S1、S2、S4 三项，以及 `arm_cart_jog_tests` 的 S4 一项仍失败。这两个失败日志与开始 v3 前的第六轮记录逐字节一致，比较结果已保存在 scope_and_artifacts.json；未修改定位臂算法或这些断言。

原“WPF 182 项”是重排前的基线断言口径。本轮保留其命令、行为与事件测试，扩展后的 11,532 项包含对多个尺寸下所有可见元素逐项检查的重复断言，不能解释为新增了相同数量的独立功能用例。

本补充调整的旧测试仅有以下布局相关部分：

| 文件/原断言 | v3 调整 | 保留的行为保证 |
| --- | --- | --- |
| Program.cs 的 3 窗口×3 DPI 固定设计面断言 | 改断言内容铺满、无全窗缩放、Ideal，并保留位图像素/DPI 检查 | 模块仍加载，按住控件可布置，DPI 渲染有效 |
| RevisionUChecks 的 5 张固定 1280×800 截图/384 定位臂断言 | 替换为本补充要求的 11 张及响应式几何检查 | 命令、启用、释放/捕获停止、保活、换档重发等原测试不变 |
| 提示行的 WrapPanel 查询 | 改为 HintMessages Grid 查询 | 非空消息顺序与空项折叠不变 |
| 电缸原 ToolTip 精确比较 | 断言“完整名称前缀 + 完全相同的原 ToolTip 内容” | 仅增加 v3 明确要求的完整名称 |
| 新增 ResponsiveLayoutChecks / V3Baseline 三份夹具 | 与 v3 开始时每个原按钮、开关、单选和输入框逐项比较 | 包括 10 个末端按钮和手动展开开关，参数及事件逐项相等 |

没有因本补充重命名或删除 ArmCartesianMode、ArmCartesianRate、电缸 ItemsControl；它们在 v3 开始前已经由第六轮 U 替换。v3 新删除的是主窗口 DesignViewbox、DesignSurface，对应测试已列在上表。

可复现命令，在仓库“介入机器人”目录执行：

```powershell
python tools/revision_u_v3/run_managed.py
python tools/revision_u_v3/run_regressions.py
python tools/revision_u_v3/verify_and_package.py
```

第二条会如实以非零退出，原因是上述 4 项既有定位臂失败。构建输出位于 `A主端/工程/build/UV3Release/bin`；报告中的截图使用同一次隔离构建的真实 WPF BAML 控件生成。

## 4. 档位与几何验收

尺寸指 WPF 内容区，含固定顶部，不含 Windows 标题栏和窗口边框。

| 内容区 | 档位 | 排布 | 结果 |
| --- | --- | --- | --- |
| 944×509 | 小 | 宽 | 通过 |
| 806×470 | 小 | 窄 | 通过 |
| 944×1010 | 小 | 宽 | 通过 |
| 1280×760 | 中 | 宽 | 通过 |
| 1920×1010 | 大 | 宽 | 通过 |
| 1499×1010 | 中 | 宽 | 通过 |
| 701×415 | 小 | 窄 | 通过 |
| 560×600 | 小 | 窄 | 通过 |
| 819×700 | 小 | 窄 | 通过 |
| 820×700 | 小 | 宽 | 通过 |

另覆盖 1150×639、1150×640、1500×859、1500×860，以及 520 DIP 支持边界；额外位图检查了 125%、150%、200% 渲染 DPI，档位与 DIP 字号保持不变。原 3 尺寸×3 DPI 渲染用例继续执行。

几何测试遍历每张图中所有可见 FrameworkElement，包含控件模板中的元素，而非仅断言 ScrollViewer 被禁用了水平滚动。

- 所有右边缘在内容区宽度以内；无横向滚动范围或可见横条。
- 944×509 普通状态的全部控件上下完整可见，主体垂直滚动范围为 0。
- 主体视口右、下边缘到内容区边缘严格等于该档 Pad；不以扩大空白换取“无滚动”。
- 主体不低于视口高度；超高的窄布局和展开状态由垂直滚动查看。测试滚到定位臂调试行，确认可到达且顶部状态条位置不变。
- 小档所有 TextBlock 和 Control 的字号不低于 12 DIP，读数缩放不低于 1；数字与单位不超出其自己的读数卡片。
- 辅助三行和末端五行均分剩余高度；按住按钮在上下限之间伸展，额外高度居中；窄排布末端使用最小高度。
- 520 DIP 额外用原测试遥测中的 `25.000 N·mm`、`-9.000 N·mm` 校验读数宽度。

几何数据：[responsive_geometry.json](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots/responsive_geometry.json>)。下表的“垂直条”只描述主体；除 944×509 普通状态外，规格允许按自然高度上下滚动。

| 截图 | 实际位图 DIP/像素（96 DPI） | Pad | 垂直条 | 水平条 |
| --- | --- | ---: | --- | --- |
| 701×415，窄排布 | 701×415 | 8 | 有 | 无 |
| 701 DIP 宽、全部内容长图 | 701×876 | 8 | 无 | 无 |
| 560×600，窄排布 | 560×600 | 8 | 有 | 无 |
| 944×509，普通状态 | 944×509 | 8 | 无 | 无 |
| 806×470，窄排布 | 806×470 | 8 | 有 | 无 |
| 944×1010，小档高窗口 | 944×1010 | 8 | 无 | 无 |
| 1280×760，中档 | 1280×760 | 10 | 无 | 无 |
| 1920×1010，大档 | 1920×1010 | 12 | 无 | 无 |
| 944×509，器械准备展开 | 944×509 | 8 | 有 | 无 |
| 944×509，定位臂调试展开 | 944×509 | 8 | 有 | 无 |
| 944×509，未连接 | 944×509 | 8 | 有 | 无 |

## 5. 全部指定截图

以下均由 RenderTargetBitmap 在 96 DPI 渲染，使用编译后的 MainWindow、InterventionPanel、PositioningArmPanel 和现有绑定。遥测来自隔离测试的 RecordingRobotLink，数值不写死在 XAML 中。截图不包含操作系统标题栏。

[浏览全部截图（可点开原图）](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots.html>)；额外边界、DPI 和带符号读数图片也在该页面中。

### 1. 701×415，窄排布

![701×415，窄排布](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots/01-layout-701x415.png>)

### 2. 701 DIP 宽、全部内容长图

![701 DIP 宽、全部内容长图](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots/02-layout-701-full.png>)

### 3. 560×600，窄排布

![560×600，窄排布](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots/03-layout-560x600.png>)

### 4. 944×509，普通状态

![944×509，普通状态](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots/04-layout-944x509.png>)

### 5. 806×470，窄排布

![806×470，窄排布](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots/05-layout-806x470.png>)

### 6. 944×1010，小档高窗口

![944×1010，小档高窗口](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots/06-layout-944x1010.png>)

### 7. 1280×760，中档

![1280×760，中档](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots/07-layout-1280x760.png>)

### 8. 1920×1010，大档

![1920×1010，大档](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots/08-layout-1920x1010.png>)

### 9. 944×509，器械准备展开

![944×509，器械准备展开](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots/09-preparation-expanded-944x509.png>)

### 10. 944×509，定位臂调试展开

![944×509，定位臂调试展开](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots/10-arm-debug-expanded-944x509.png>)

### 11. 944×509，未连接

![944×509，未连接](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/screenshots/11-disconnected-944x509.png>)

## 6. 与预览不一致的项目及原因

核心 12 项尺寸资源、颜色、分组和宽窄切换按要求实现；本实现不是对 HTML 的逐像素复刻。以下差异保留在交付中：

| 项目 | 不一致 | 原因 |
| --- | --- | --- |
| 定位臂列宽 | 944 宽时 HTML 约 254.88，WPF 250.56；1280 时 HTML 用 26vw，WPF 用内容宽的 27% | 本补充明确要求 73*/27*，优先于 CSS 的 27/26/25vw 算法；大档上限 460 时一致 |
| 上下空间分配 | 高窗口中卡片与辅助的高度不等同 CSS flex 的结果 | WPF 按明确要求使用 3*、2*；HTML flex 的比例作用于扣除自然基高后的剩余空间 |
| 状态、准备行折行位置 | 窄窗口中胶囊可能提前折行；准备状态文字与展开按钮的折行位置不同 | WPF 使用 Grid/DockPanel + WrapPanel；阶段方向固定为整体靠右，动作按钮保持完整，未增加测量回调或自定义布局算法 |
| 读数字号和单位比例 | 在较大卡片中数字和单位的增长幅度与 CSS 容器查询不逐像素相同 | 按明确要求把“数字 + 单位”放入同一个 UpOnly Viewbox、MaxHeight=96；CSS 对数字、单位分别计算 clamp |
| 极窄卡片的读数内边距/两列宽度 | 读数横向 Padding 为 3 DIP；力矩列设置静态最小宽度（小/中/大 124/161.2/198.4） | 为使既有带符号读数、单位及基准字号在 520 DIP 下完整显示，不缩小文字；HTML 读数使用 .6em 内边距和等宽列 |
| 字体度量与细节 | 某些行高、基线、末端行距及表盘笔画与浏览器略有差别 | 使用 WPF 原生字体度量与现有表盘几何；没有通过整体缩放或新增动画来补偿 |
| 未连接截图 | “—”、禁用状态和提示来自现有 MainViewModel；可出现垂直滚动 | 预览只有在线示例状态；本补充要求使用真实绑定且保留字号下限，不用示例数字覆盖缺失遥测 |

浏览器实测与 WPF 的主体起点、定位臂宽度如下（同样的内容区尺寸）：

| 尺寸 | HTML 主体 y | WPF 主体 y | HTML 定位臂宽 | WPF 定位臂宽 |
| --- | ---: | ---: | ---: | ---: |
| 944×509 | 88.00 | 88.00 | 254.88 | 250.56 |
| 560×600 | 119.00 | 110.00 | 544.00 | 544.00 |
| 1920×1010 | 124.00 | 124.00 | 460.00 | 460.00 |

浏览器参照截图与原始度量：[944×509](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/preview_reference/preview-944x509.png>)、[560×600](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/preview_reference/preview-560x600.png>)、[1920×1010](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/preview_reference/preview-1920x1010.png>)、[geometry.json](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/revision_u_v3_20261109/preview_reference/geometry.json>)。

## 7. 未执行项与剩余限制

- 未连接实机逐个按键；真实机器人、手柄和 PLC 操作需要现场验收。
- 未实际搬动窗口跨不同 DPI 显示器或切换 Windows 缩放设置；已覆盖离线 RenderTargetBitmap 的多 DPI 用例。
- 4 项既有定位臂回归失败仍在；本补充未修改算法或降低断言。
- 本次未重新修改或处理 H、N；其既有文件保持不变。
