# A 工程全貌

> 调研口径：以工作区 `主从分机软件包_20261008_211541` 在 2026-11-12 的磁盘内容为准。本文只描述现状；除逐项注明“推断/未知”或未运行外，本附件事实表各行及源码布局/协议说明均标为**已确认**；“已确认”来自源码/文档直接证据，“推断”来自静态调用关系，“未知”表示源码中没有可验证证据。未启动联机主端或加载真实SDK的手柄宿主；既有离线测试运行了Fake SDK宿主和模拟主端，执行边界见 `F_测试现状.md:7-16,52-56`。

## A.0 本附件的路径引用约定

下表中的根均相对于 `D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/`。同名镜像/历史文档不作为当前实现。为保留表格可读性，本文既有引用采用以下唯一映射，行号为当前磁盘文件的物理行号；`文件:起点-终点` 表示同一文件的证据区间。

| 本附件出现的前缀或单文件名 | 唯一展开位置 |
|---|---|
| `A主端/` | `介入机器人/A主端/` |
| `B数据端/` | `介入机器人/B数据端/` |
| `C从端/` | `介入机器人/C从端/` |
| `D/`、`ADS项目/` | `介入机器人/B数据端/工程/ADS项目/` |
| `K/`、`PositioningArmKinematics/` | `介入机器人/B数据端/工程/PositioningArmKinematics/` |
| `P/`、`POUs/` | `介入机器人/C从端/250902/Untitled2/POUs/` |
| `Untitled2/`、`250902.tsproj` | 分别为 `介入机器人/C从端/250902/Untitled2/`、`介入机器人/C从端/250902/250902.tsproj` |
| A.10以后未加前缀的C++源/头及`ADS.vcxproj` | 一律相对于`D/`；`BuildAndTest.ps1`相对于`K/` |
| `工程/`、`A主端/工程/` | `介入机器人/A主端/工程/` |
| `master/` | `介入机器人/A主端/工程/master/` |
| `VI.Service.Robot/`、`Services/` | 分别为 `介入机器人/A主端/工程/master/VI.Service.Robot/`、其 `Services/` 子目录 |
| `VI.Service.Net/`、`VI.HandleHost/`、`MasterConsole.Protocol/` | `介入机器人/A主端/工程/master/` 下同名目录 |
| `HandleService.cs`、`HandleHostMemory.cs`、`HandleDeviceNotifications.cs`、`RemoteRobotLink.cs` | `介入机器人/A主端/工程/master/VI.Service.Robot/Services/` 下同名文件 |
| `MainViewModel.cs` | `介入机器人/A主端/工程/master/VI.Service.Robot/ViewModels/MainViewModel.cs` |
| `ZenohSession.cs` | `介入机器人/A主端/工程/master/VI.Service.Net/ZenohSession.cs` |
| `ZenohFrames.cs`、`ZenohWire.cs`、`ProtocolConstants.cs`、`FrameAuth.cs` | `介入机器人/A主端/工程/master/MasterConsole.Protocol/` 下同名文件 |
| `App.xaml.cs`、`MainWindow.xaml`、`MainWindow.xaml.cs`、`StartupSelection.cs` | `介入机器人/A主端/工程/master/VI.Control/` 下同名文件 |
| `launchSettings.json` | `介入机器人/A主端/工程/master/VI.Control/Properties/launchSettings.json` |
| `InterventionModule.cs`、`InterventionPanel.xaml`、`InterventionPanel.xaml.cs` | `介入机器人/A主端/工程/master/VI.Module.Intervention/` 下同名文件 |
| `PositioningArmPanel.xaml`、`PositioningArmPanel.xaml.cs` | `介入机器人/A主端/工程/master/VI.Module.PositioningArm/` 下同名文件 |
| `Directory.Build.targets`、`VI.HandleHost.csproj` | 分别为 `介入机器人/A主端/工程/Directory.Build.targets`、`介入机器人/A主端/工程/master/VI.HandleHost/VI.HandleHost.csproj` |

未列出的带完整 `介入机器人/` 路径直接相对于上述调研根；章节 A.8 的项目名路径均相对于 `master/`，不相对于 B 数据端。

## A.1 主端目录与工程边界

| 层 | 当前目录/入口 | 现状与用途 | 构建/输出 | 确定度与出处 |
|---|---|---|---|---|
| A 主端应用 | `介入机器人/A主端/工程/master/VI.Control` | .NET 8 WPF x64 `WinExe`；创建 HostFramework 宿主、扫描两个功能模块、创建 `MainWindow`。 | `VI.Control.slnx`；输出由 `Directory.Build.props` 统一到 `工程/build/<Configuration>/bin/<ProjectName>/`，应用入口为 `VI.Control.exe`。 | 已确认：`工程/master/VI.Control/VI.Control.csproj:1-24`、`工程/Directory.Build.props:1-14`、`工程/master/VI.Control.slnx:1-10`。 |
| 主端业务服务 | `master/VI.Service.Robot` | `IRobotLink`、真实 `RemoteRobotLink`、模拟 `SimulatedRobotLink`、`HandleService`、共享内存定义、`MainViewModel`。 | 类库；被 `VI.Control` 引用。 | 已确认：`master/VI.Service.Robot/VI.Service.Robot.csproj:1-19`、`Services/RemoteRobotLink.cs:17-34`、`App.xaml.cs:31-42`。 |
| Zenoh 封装 | `master/VI.Service.Net` | P/Invoke `zenohc.dll` 的 Client Session、publisher/subscriber、MessagePack 编解码入口。 | 类库；构建时复制仓库内 `zenoh-c-1.10.1-msvc/bin/zenohc.dll`。 | 已确认：`master/VI.Service.Net/VI.Service.Net.csproj:1-17`、`ZenohSession.cs:8-18,29-60`。 |
| 协议库 | `master/MasterConsole.Protocol` | MessagePack 位置数组、协议版本、认证、字段范围校验；不含网络收发。 | 类库；NuGet `MessagePack 3.1.12`。 | 已确认：`master/MasterConsole.Protocol/MasterConsole.Protocol.csproj:1-14`、`ZenohFrames.cs:5-23`、`ZenohWire.cs:14-27`。 |
| 介入功能模块 | `master/VI.Module.Intervention` | 介入准备、力反馈、电缸、Y 阀、轴4、注射器 UI；共享 `MainViewModel`。 | 类库；`VI.Control.csproj` 的 `CopyFeatureModules` 复制至输出 `Modules/Features`。 | 已确认：`master/VI.Module.Intervention/VI.Module.Intervention.csproj:1-13`、`master/VI.Control/VI.Control.csproj:18-24`、`InterventionModule.cs:1-14`。 |
| 定位臂功能模块 | `master/VI.Module.PositioningArm` | 定位臂总使能、轴1-5点动、末端点动、程序归零和调试展开区。 | 类库；同上复制至 `Modules/Features`。 | 已确认：`master/VI.Module.PositioningArm/VI.Module.PositioningArm.csproj:1-13`、`PositioningArmPanel.xaml.cs:18-28`。 |
| 手柄宿主 | `master/VI.HandleHost` | 独立进程调用 `FLCatheter.dll`，双手柄采样、触觉输出、本地阻尼；每 5 ms 的主端监管器负责启动/重启。 | `Exe`；输出 `build/<Configuration>/bin/VI.HandleHost/VI.HandleHost.exe`，带 `FLCatheter.dll` 和 `Runtime/VC142/*.dll`。 | 已确认：`master/VI.HandleHost/VI.HandleHost.csproj:1-15`、`VI.HandleHost/Program.cs:29-103`、`VI.Service.Robot/Services/HandleService.cs:32-37,102-179`。 |
| 协议离线测试 | `master/MasterConsole.Protocol.Tests` | 单进程测试入口，包含协议、认证、HandleHost/本地力和 RemoteRobotLink 测试；项目为 `Exe`。 | 独立 `VI.Control.Tests.slnx`；不属于日常六个应用项目。 | 已确认：`master/MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.csproj:1-19`、`master/VI.Control.Tests.slnx`、`A主端/使用说明.md:5-7`。 |
| 界面离线测试 | `master/VI.Control.Tests` | WPF 离屏布局、停止行为、定位臂/主窗口基线检查。 | 独立测试 `Exe`；复制 XAML fixtures 和 Feature DLL。 | 已确认：`master/VI.Control.Tests/VI.Control.Tests.csproj:1-34`、`A主端/工程/docs/VI.Control迁移验收.md:31-35`。 |

## A.2 构建、部署和启动

| 项目 | 事实 | 确定度与出处 |
|---|---|---|
| SDK | `global.json` 固定 .NET SDK `9.0.315`、只允许 latest patch；`tools/Get-ViDotnet.ps1` 优先使用工程内 `.tools/dotnet-sdk-9.0.315`，否则接受已安装的 9.0.3xx；`Prepare-Sdk.ps1` 下载并 SHA-512 校验。 | 已确认：`工程/global.json:1-7`、`工程/tools/Get-ViDotnet.ps1:1-26`、`工程/tools/Prepare-Sdk.ps1:1-18`。 |
| 平台/输出 | `Directory.Build.props` 设 `PlatformTarget=x64`、`Platforms=x64`、关闭 framework/runtime 子目录追加；输出为 `工程/build/<Configuration>/bin/<ProjectName>/`。 | 已确认：`工程/Directory.Build.props:1-14`。 |
| 日常构建 | `工程/build-and-run.ps1` 默认 `Release`，调用 `dotnet build master/VI.Control.slnx --configuration ... -p:Platform=x64 -m:1`；检查运行文件清单；默认随后启动 `VI.Control.exe`。 | 已确认：`工程/build-and-run.ps1:1-34`。 |
| 联机启动 | `编译并启动主端.cmd` 调用上述脚本；非 `-Sim` 时要求 `工程/config/remote.token` 至少 16 UTF-8 字节，并传 `--host 192.168.60.1 --token <path>`。 | 已确认：`A主端/编译并启动主端.cmd:1-7`、`工程/build-and-run.ps1:11-16,30-32`。 |
| 模拟启动 | `模拟运行主端.cmd` 传 `-Sim`；`StartupSelection.Parse` 进入模拟时不读取 token，DI 注册 `SimulatedRobotLink`，提示“命令不会发给从端”。 | 已确认：`A主端/模拟运行主端.cmd:1-7`、`master/VI.Control/StartupSelection.cs:14-22`、`master/VI.Control/App.xaml.cs:31-40`。 |
| 手柄宿主复制 | `Directory.Build.targets` 在 `Build;Publish` 后把 `build/<Configuration>/bin/VI.HandleHost/*` 复制到应用输出下 `HandleHost/`；应用运行清单要求原生 DLL、两个 Feature DLL、协议/网络 DLL 和 MessagePack。 | 已确认：`工程/Directory.Build.targets:1-9`、`工程/tools/runtime-files.txt:1-24`。 |
| 单实例/停止 | HostFramework 选项定义单实例名 `VI.Control.Master`，停止超时 8 s；退出窗口先向全部模块执行 `ApplicationCommands.Stop`。 | 已确认：`master/VI.Control/App.xaml.cs:13-24`、`master/VI.Control/MainWindow.xaml.cs:43-56`。 |
| 未知 | 当前工作区没有可验证的现场部署脚本来确认主端与数据端进程启动顺序之外的服务管理器/开机自启设置；`build-and-run.ps1` 只构建并可启动主端。 | 未知：脚本与 `A主端/使用说明.md:15,26` 未出现服务注册。 |

## A.3 主端线程、定时器和数据交换

| 线程/定时器 | 周期/触发 | 交换数据 | 关键行为 | 确定度与出处 |
|---|---:|---|---|---|
| `VI.HandleHost` 主循环 | 目标 200 Hz（5 ms）；QPC/`Stopwatch` 绝对 deadline，尾部 `Sleep(1)` 或 `SpinWait(50)`。 | 通过命名 MemoryMappedFile 接收 `HostForces`，写回 `HostSamples`，另写 heartbeat。 | 每拍读取力目标、遍历 SN582/SN587，调用 SDK `deviceStatus/getEncVel/getJoints/getEncoders/getSwitch`，计算阻尼和输出。 | 已确认：`VI.HandleHost/Program.cs:29-94`、`工程/docs/guides/力反馈说明.md:65-71`。 |
| 主端 `HandleService` 监管线程 | `Thread.Sleep(5)`，约 200 Hz 轮询。 | `_samples/_forces` 在 `_lock` 内；与宿主共享映射；`Process` 生命周期。 | 宿主退出、USB 通知或 heartbeat 超过 1000 ms 时清零并重启；启动间隔至少 1000 ms。 | 已确认：`Services/HandleService.cs:15-27,42-100,102-164`。 |
| Zenoh `RemoteRobotLink.HandleLoop` | `ControlPeriodMs=10`，目标 100 Hz；`timeBeginPeriod(1)`。 | 主端手柄/注射器/轴4快照在 `_lock`；`ConcurrentDictionary<int,TCS>` 保存按需命令结果；发布 HandleFrame。 | 每拍先 `CheckConnection`，再检查状态新鲜度、会话密钥、publisher；不满足则 `continue`，不会发布旧手柄帧。 | 已确认：`Services/RemoteRobotLink.cs:20-21,291-303,380-426`。 |
| Zenoh 回调线程（库内部） | 由 `zenohc.dll` subscriber 回调触发。 | `OnState`/`OnForce` 进入 `_lock`；状态回调向 UI 触发事件，力回调写 `_handles.SetHaptic`。 | 严格检查版本、会话、认证 tag、序号；错误载荷被 `ZenohSession.OnSample` 丢弃。 | 已确认：`VI.Service.Net/ZenohSession.cs:150-167`、`RemoteRobotLink.cs:434-527`。 |
| WPF UI DispatcherTimer（统计） | 250 ms。 | 读取 `IRobotLink.GetStats()` 与缓存状态，触发属性通知。 | 重新计算状态年龄和 `CanOperate`；不执行机器人控制循环。 | 已确认：`MainViewModel.cs:49-95,505-529,617-623`。 |
| WPF 定位臂保活定时器 | 100 ms。 | 读取 panel 内 `_jog*` / `_cartesian*` / `_programReturnActive`，调用 `IRobotLink`。 | 轴点动重发ArmAxisJog、末端点动重发完整ArmCartesianJog、程序归零才发ArmCartesianAlive；CanOperate为false且轴点动或归零活动时先StopHoldActions，纯末端点动未纳入此if。 | 已确认：`VI.Module.PositioningArm/PositioningArmPanel.xaml.cs:14-28,160-170`。 |
| WPF Dispatcher/UI 回调 | 事件触发；`BeginInvoke`。 | `RemoteRobotLink` 状态/日志事件进 UI dispatcher；`MainViewModel` 更新 `_s` 和可绑定属性。 | UI 面板的按住动作通过鼠标按下/抬起、失去捕获和卸载归零。 | 已确认：`MainViewModel.cs:533-556,609-623`、`InterventionPanel.xaml.cs:18-59`、`PositioningArmPanel.xaml.cs:30-38,97-140`。 |

主端不存在“共享内存队列”：HandleHost 使用两个单写区的 seqlock 结构；网络线程使用主端锁和 `ConcurrentDictionary` 命令结果；UI 使用 WPF Dispatcher 和对象状态。`HandleHostMemory.Read` 只尝试一次，若读到奇数版本或版本改变则返回 false，不等待宿主完成写入。证据：`Services/HandleHostMemory.cs:9-18,31-46`。

## A.4 手柄宿主与主端共享内存布局

命名映射名由监管器生成：`Local\\VI.HandleHost.<Guid:N>`，创建大小 1024 字节；宿主从命令行第一个参数打开同名映射。证据：`HandleService.cs:137-144`、`VI.HandleHost/Program.cs:29-32`、`HandleHostMemory.cs:15-18`。

| 偏移 | 大小/类型 | 生产者 → 消费者 | 字段/语义 | 证据 |
|---:|---:|---|---|---|
| 0 | 8 B `long` | 宿主 → 监管器 | `Heartbeat=Environment.TickCount64`。 | `HandleHostMemory.cs:28`、`VI.HandleHost/Program.cs:81-82` |
| 8 | 4 B `int` | 宿主 → 监管器 | `Error`：1 DLL 缺失，2 位数错误，3 入口缺失；宿主异常捕获后写值并退出。 | `HandleHostMemory.cs:29`、`VI.HandleHost/Program.cs:99-102` |
| 12 | 4 B `int` | 监管器 → 宿主 | `Stop`；监管器停止/重启时写 1。 | `HandleHostMemory.cs:30`、`HandleService.cs:126-133,168-172` |
| 16 | 16 B | 监管器 → 宿主 | 两个 `ulong DeviceEpoch[2]`；每次成功打开设备递增，宿主帧带回。 | `HandleHostMemory.cs:20-26`、`VI.HandleHost/Program.cs:51-60,73-75` |
| 128 | 120 B（8 B seqlock 版本 + 112 B 负载） | 宿主 → 监管器 | `HostSamples { A,B }`；每侧 56 B：`long Sequence, Tick; ulong DeviceEpoch; int Valid, Buttons, Encoder0, Encoder1; float Joint0,Joint1,Vel0,Vel1`。按声明类型大小与顺序计算。 | `HandleHostMemory.cs:31-45,50-61` |
| 512 | 72 B（8 B seqlock 版本 + 64 B 负载） | 监管器 → 宿主 | `HostForces { A,B }`；每侧 32 B：`long Tick; int Enable,Axis; double Force,Torque`。按声明类型大小与顺序计算。 | `HandleHostMemory.cs:31-45,63-70` |
| 其它 | 保留 | 无 | 当前源码没有定义其它可用字段。 | `HandleHostMemory.cs:14-46` |

seqlock 写入为：版本写奇数 → 内存屏障 → 结构体 → 内存屏障 → 版本写偶数；读取时版本必须为偶数且前后相同。证据：`HandleHostMemory.cs:31-46`。主端 `GetSample` 另外过滤 `_stop`、`Tick==0` 和重复 Sequence；证据：`HandleService.cs:64-77`。

已确认：宿主不是另一份手写布局；`VI.HandleHost.csproj:10` 直接把主端 `Services/HandleHostMemory.cs` 作为链接源文件编译。两端当前均为 .NET 8 x64，使用同一 `Sequential` 声明；以上 56/112/32/64 B 按字段布局可直接确定，结构体间没有额外自定义打包。源码未声明当前共享映射必须兼容旧 C++ `手柄.cpp` 的任何内存布局，因此不能把旧实现当作当前映射契约。证据：`VI.HandleHost.csproj:3-10`、`master/VI.Service.Robot/VI.Service.Robot.csproj:3-6`、`HandleHostMemory.cs:50-70`。

## A.5 Zenoh 话题和线格式

当前只有四个 Topic，协议版本固定 `1`。频率为 Handle 100 Hz、Command 按需、State 约 30 Hz、Force 100 Hz。证据：`master/MasterConsole.Protocol/ZenohFrames.cs:5-12`、`工程/protocol/PROTOCOL.md:5-14`、`master/MasterConsole.Protocol/Frames.cs:14-47`。Zenoh Session 只打开 client，连接配置为 `mode:'client'`、`connect.timeout_ms=3000`、`exit_on_failure=true`；证据：`VI.Service.Net/ZenohSession.cs:29-50`。

### Topic 清单

| Topic | 方向 | 频率/发布线程 | 载荷 | 证据 |
|---|---|---|---|---|
| `vi/ads/handle/v1` | 主端 → 数据端 | 100 Hz；`RemoteRobotLink.HandleLoop` | `HandleFrame` 26 项位置数组；双手柄、注射器1/2保持方向、轴4方向、设备 epoch 和 AuthTag。 | `ZenohFrames.cs:8,25-54`、`RemoteRobotLink.cs:380-426` |
| `vi/ads/command/v1` | 主端 → 数据端 | 按需；按钮/界面命令、握手 | `CommandFrame` 15 项；命令枚举 0–19，命令参数 `Param1/Param2/Value1/Value2`。 | `ZenohFrames.cs:14-23,56-74`、`RemoteRobotLink.cs:224-289,326-377` |
| `vi/ads/state/v1` | 数据端 → 主端 | 约 30 Hz；状态回调 | `StateFrame` 57 项；认证状态、flags、模式/阶段、自检、ADS、四电缸、力、七轴位置/距左限位、净距离、任务状态、命令结果、定位臂状态和 AuthTag。 | `ZenohFrames.cs:84-144`、`工程/protocol/PROTOCOL.md:75-137` |
| `vi/ads/force/v1` | 数据端 → 主端 | 100 Hz；力反馈发布 | `ForceFrame` 13 项；两侧 Enable、SDK 轴、轴向力、扭矩、echo 时间和 AuthTag。 | `ZenohFrames.cs:146-162`、`RemoteRobotLink.cs:506-526` |

### HandleFrame 字段

| Key | 字段名 | 类型 | 含义 | 出处（均已确认） |
|---:|---|---|---|---|
| 0 | `Version` | `byte` | 线协议版本，当前1 | `ZenohFrames.cs:28` |
| 1 | `SessionId` | `ulong` | 认证会话ID | `ZenohFrames.cs:29` |
| 2 | `Sequence` | `ulong` | 本话题会话内单调序号 | `ZenohFrames.cs:30` |
| 3 | `MonotonicMs` | `ulong` | 主端本地单调毫秒；用于回显，不能直接与另一电脑tick相减 | `ZenohFrames.cs:31` |
| 4 | `Buttons582` | `byte` | SN582原始按键位，不在主端重排 | `ZenohFrames.cs:32` |
| 5 | `Valid582` | `bool` | SN582样本有效位 | `ZenohFrames.cs:33` |
| 6 | `Encoder582_0` | `int` | SN582第0通道编码器值，原SDK整数 | `ZenohFrames.cs:34` |
| 7 | `Encoder582_1` | `int` | SN582第1通道编码器值，原SDK整数 | `ZenohFrames.cs:35` |
| 8 | `Joint582_0` | `float` | SN582第0关节值；数据端按配置映射，线协议无单位换算 | `ZenohFrames.cs:36` |
| 9 | `Joint582_1` | `float` | SN582第1关节值；数据端按配置映射，线协议无单位换算 | `ZenohFrames.cs:37` |
| 10 | `Velocity582_0` | `float` | SN582第0通道SDK速度；SDK单位未在仓库注明 | `ZenohFrames.cs:38` |
| 11 | `Velocity582_1` | `float` | SN582第1通道SDK速度；SDK单位未在仓库注明 | `ZenohFrames.cs:39` |
| 12 | `Buttons587` | `byte` | SN587原始按键位，不在主端重排 | `ZenohFrames.cs:40` |
| 13 | `Valid587` | `bool` | SN587样本有效位 | `ZenohFrames.cs:41` |
| 14 | `Encoder587_0` | `int` | SN587第0通道编码器值，原SDK整数 | `ZenohFrames.cs:42` |
| 15 | `Encoder587_1` | `int` | SN587第1通道编码器值，原SDK整数 | `ZenohFrames.cs:43` |
| 16 | `Joint587_0` | `float` | SN587第0关节值；数据端按配置映射，线协议无单位换算 | `ZenohFrames.cs:44` |
| 17 | `Joint587_1` | `float` | SN587第1关节值；数据端按配置映射，线协议无单位换算 | `ZenohFrames.cs:45` |
| 18 | `Velocity587_0` | `float` | SN587第0通道SDK速度；SDK单位未在仓库注明 | `ZenohFrames.cs:46` |
| 19 | `Velocity587_1` | `float` | SN587第1通道SDK速度；SDK单位未在仓库注明 | `ZenohFrames.cs:47` |
| 20 | `Injector1Direction` | `sbyte` | 注射器1持续方向，-1/0/+1 | `ZenohFrames.cs:48` |
| 21 | `Injector2Direction` | `sbyte` | 注射器2持续方向，-1/0/+1 | `ZenohFrames.cs:49` |
| 22 | `Axis4Direction` | `sbyte` | 轴4持续手动方向，-1/0/+1 | `ZenohFrames.cs:50` |
| 23 | `DeviceEpoch582` | `ulong` | SN582每次成功重开递增的epoch，数据端据变化触发静置/重基准 | `ZenohFrames.cs:51` |
| 24 | `DeviceEpoch587` | `ulong` | SN587每次成功重开递增的epoch，数据端据变化触发静置/重基准 | `ZenohFrames.cs:52` |
| 25 | `AuthTag` | `byte[]` | 会话HMAC前16字节；初始默认空数组 | `ZenohFrames.cs:53` |

来源：`ZenohFrames.cs:25-54`、范围校验 `ZenohWire.cs:140-166,195-203`。

### CommandFrame 与命令枚举

| Key | 字段名 | 类型 | 含义 | 出处（均已确认） |
|---:|---|---|---|---|
| 0 | `Version` | `byte` | 线协议版本，当前1 | `ZenohFrames.cs:59` |
| 1 | `SessionId` | `ulong` | 认证会话ID | `ZenohFrames.cs:60` |
| 2 | `Sequence` | `ulong` | 本话题会话内单调序号 | `ZenohFrames.cs:61` |
| 3 | `MonotonicMs` | `ulong` | 主端本地单调毫秒；用于回显，不能直接与另一电脑tick相减 | `ZenohFrames.cs:62` |
| 4 | `CommandId` | `int` | 主端分配的按需请求ID，结果按ID关联 | `ZenohFrames.cs:63` |
| 5 | `Command` | `ZenohCommand` | 命令号，底层byte；完整枚举见下 | `ZenohFrames.cs:64` |
| 6 | `Param1` | `int` | 命令相关整数参数1 | `ZenohFrames.cs:65` |
| 7 | `Param2` | `int` | 命令相关整数参数2 | `ZenohFrames.cs:66` |
| 8 | `Value1` | `double` | 命令相关浮点参数1 | `ZenohFrames.cs:67` |
| 9 | `Value2` | `double` | 命令相关浮点参数2 | `ZenohFrames.cs:68` |
| 10 | `Payload` | `string` | 字符串参数，默认空；线校验最多4096字符 | `ZenohFrames.cs:69` |
| 11 | `NonceClient` | `byte[]` | Hello/Authenticate客户端随机数，16字节 | `ZenohFrames.cs:70` |
| 12 | `NonceServer` | `byte[]` | 挑战随机数，16字节 | `ZenohFrames.cs:71` |
| 13 | `Proof` | `byte[]` | Authenticate的HMAC证明，32字节 | `ZenohFrames.cs:72` |
| 14 | `AuthTag` | `byte[]` | 会话HMAC前16字节；握手前默认空数组 | `ZenohFrames.cs:73` |

命令：`Hello=0, Authenticate=1, Acquire=2, Release=3, PreparePosition=4, StartControl=5, ForceFeedback=6, Cylinder=7, ArmManualEnable=8, ArmAxisEnable=9, ArmAxisReset=10, ArmAxisJog=11, ArmCartesianJog=12, ArmStop=13, ArmProgramZero=14, ArmCartesianAlive=16, YValve=17, ForceZero=18, ForceHold=19`；15 保留。来源：`ZenohFrames.cs:14-23,56-74`。

### StateFrame 全量字段（57 项）

以下声明、索引、赋值语义均为**已确认**。表内 `ZState` 唯一展开为 `介入机器人/B数据端/工程/ADS项目/zenoh_service.cpp`；字段的 C# 定义为 `ZenohFrames.cs:87-143`，主端转内部状态为 `RemoteRobotLink.cs:473-489`。表中索引 25–38 的尾号 0–6 对应本体轴 1–7；`AxisPos*` 是相对 `init_pos` 的 PLC 实际坐标，`AxisFromLeft*` 是对应距左限位坐标（`B数据端/工程/ADS项目/main.cpp:180-184,4695-4699`）。`Phase` 是启动阶段；`Mode` 是 `guidewire_mode`；界面显示的递送/撤出由 `Flags.GlobalReverse` 决定（`ZState:1055-1058`、`MainViewModel.cs:195-200`）。

| Key | 字段名 | C# 类型 / 线宽 | 含义与赋值来源 |
|---:|---|---|---|
| 0 | `Version` | `byte` | 协议版本 1；`ZenohFrames.cs:87`、`ZState:1029`。 |
| 1 | `SessionId` | `ulong` | 当前认证会话 ID；挑战帧使用 challenge session；`ZenohFrames.cs:88`、`ZState:1030-1035`。 |
| 2 | `Sequence` | `ulong` | State Topic 递增序号；`ZenohFrames.cs:89`、`ZState:1031`。 |
| 3 | `AuthState` | `byte` | 0 未认证、1 挑战、2 已认证；`ZenohFrames.cs:90`、`ZState:1032-1034`。 |
| 4 | `NonceServer` | `byte[]` / BIN | 当前服务端 nonce 或挑战 nonce；`ZenohFrames.cs:91`、`ZState:1036`。 |
| 5 | `EchoMainMonotonicMs` | `ulong` | 原样回显最近有效主端单调毫秒；主端据此算 RTT，不能作为跨机器绝对时钟；`ZenohFrames.cs:92`、`ZState:1037`、`RemoteRobotLink.cs:490-493`。 |
| 6 | `Flags` | `uint` | 下表 16 个状态位；`ZenohFrames.cs:93`、`ZState:1038-1055`。 |
| 7 | `Mode` | `int` | 控制域 `guidewire_mode`；`ZenohFrames.cs:94`、`ZState:1057`。 |
| 8 | `Phase` | `int` | 控制域 `startup_phase`；`ZenohFrames.cs:95`、`ZState:1057`。 |
| 9 | `SelfCheckStatus` | `int` | 器械准备状态；UI 解释 1 等待开始、2 运行中、3 PLC 拒绝目标参数、4 数据端拒绝/ADS 写失败、5 已发送等确认，其它待机；`ZenohFrames.cs:96`、`ZState:1058`、`MainViewModel.cs:269-279`。 |
| 10 | `AdsState` | `int` | 0 未启动、1 连接中、2 正常、3 软保持、4 重连中、5 PLC 重启、6 错误；`ZenohFrames.cs:97`、`ZState:1058`、`B数据端/工程/ADS项目/control_messages.h:86-87`。 |
| 11 | `Cylinder1` | `ushort` | 电缸 1 的当前控制域命令值；`ZenohFrames.cs:98`、`ZState:1059`。 |
| 12 | `Cylinder2` | `ushort` | 电缸 2 的命令值；`ZenohFrames.cs:99`、`ZState:1059`。 |
| 13 | `Cylinder3` | `ushort` | 电缸 3 的命令值；`ZenohFrames.cs:100`、`ZState:1060`。 |
| 14 | `Cylinder4` | `ushort` | 电缸 4 的命令值；`ZenohFrames.cs:101`、`ZState:1060`。 |
| 15 | `CylinderManualMask` | `byte` | 四电缸手动覆盖位，bit0..3 对应 1..4；不是按开合阈值推测；`ZenohFrames.cs:102`、`ZState:1061`、`B数据端/工程/ADS项目/control_messages.h:131-133`。 |
| 16 | `Injector1Direction` | `sbyte` | 注射器 1 当前方向 -1/0/+1；`ZenohFrames.cs:103`、`ZState:1062`。 |
| 17 | `Injector2Direction` | `sbyte` | 注射器 2 当前方向 -1/0/+1；`ZenohFrames.cs:104`、`ZState:1062`。 |
| 18 | `Axis4Direction` | `sbyte` | 轴 4 当前手动方向 -1/0/+1；`ZenohFrames.cs:105`、`ZState:1063`。 |
| 19 | `Force582F` | `float` / float32 | 物理 SN582/导丝反馈轴向力，N；含反馈门控、滤波和换手保持后的输出；`ZenohFrames.cs:106`、`ZState:1064`、`MainViewModel.cs:205`、`B数据端/工程/ADS项目/force_feedback.cpp:56-98,138-139`。 |
| 20 | `Force582N` | `float` / float32 | 名字中的 N 沿用旧名，实际为 SN582 扭矩，N·m；`ZenohFrames.cs:107`、`ZState:1064`、`MainViewModel.cs:206`、`B数据端/工程/ADS项目/force_feedback.cpp:35,95-96`。 |
| 21 | `Force587F` | `float` / float32 | 物理 SN587/导管反馈轴向力，N；`ZenohFrames.cs:108`、`ZState:1065`、`MainViewModel.cs:207`。 |
| 22 | `Force587N` | `float` / float32 | 实际为 SN587 扭矩，N·m；`ZenohFrames.cs:109`、`ZState:1065`、`MainViewModel.cs:208`、`B数据端/工程/ADS项目/force_feedback.cpp:36,97-98`。 |
| 23 | `CleanForceN` | `float` / float32 | 第一组/导管纯净轴向力，N；无效时线值 0，`CleanForceValid` 关闭，主端还原为 NaN；`ZenohFrames.cs:110`、`ZState:1053-1054,1066`、`RemoteRobotLink.cs:481`、`B数据端/工程/docs/guides/实验记录说明.md:7`。 |
| 24 | `AdsActualHz` | `float` / float32 | ADS 线程实际周期频率；`ZenohFrames.cs:111`、`ZState:1066`。 |
| 25 | `AxisPos0` | `float` / float32 | 本体轴 1 实际位置；`ZenohFrames.cs:112`、`ZState:1067`。 |
| 26 | `AxisPos1` | `float` / float32 | 本体轴 2 实际位置；`ZenohFrames.cs:113`、`ZState:1069`。 |
| 27 | `AxisPos2` | `float` / float32 | 本体轴 3 实际位置；`ZenohFrames.cs:114`、`ZState:1071`。 |
| 28 | `AxisPos3` | `float` / float32 | 本体轴 4 实际位置；`ZenohFrames.cs:115`、`ZState:1073`。 |
| 29 | `AxisPos4` | `float` / float32 | 本体轴 5 实际位置；`ZenohFrames.cs:116`、`ZState:1075`。 |
| 30 | `AxisPos5` | `float` / float32 | 本体轴 6 实际位置；`ZenohFrames.cs:117`、`ZState:1077`。 |
| 31 | `AxisPos6` | `float` / float32 | 本体轴 7 实际位置；`ZenohFrames.cs:118`、`ZState:1079`。 |
| 32 | `AxisFromLeft0` | `float` / float32 | 本体轴 1 距左限位位置；`ZenohFrames.cs:119`、`ZState:1068`。 |
| 33 | `AxisFromLeft1` | `float` / float32 | 本体轴 2 距左限位位置；`ZenohFrames.cs:120`、`ZState:1070`。 |
| 34 | `AxisFromLeft2` | `float` / float32 | 本体轴 3 距左限位位置；`ZenohFrames.cs:121`、`ZState:1072`。 |
| 35 | `AxisFromLeft3` | `float` / float32 | 本体轴 4 距左限位位置；`ZenohFrames.cs:122`、`ZState:1074`。 |
| 36 | `AxisFromLeft4` | `float` / float32 | 本体轴 5 距左限位位置；`ZenohFrames.cs:123`、`ZState:1076`。 |
| 37 | `AxisFromLeft5` | `float` / float32 | 本体轴 6 距左限位位置；`ZenohFrames.cs:124`、`ZState:1078`。 |
| 38 | `AxisFromLeft6` | `float` / float32 | 本体轴 7 距左限位位置；`ZenohFrames.cs:125`、`ZState:1080`。 |
| 39 | `HasNetDistance` | `bool` | 净距离可用标志；当前发布函数固定 true；`ZenohFrames.cs:126`、`ZState:1081`。 |
| 40 | `Axis1NetDistanceMm` | `double` / float64 | 本体轴 1 连续手柄跟随段的实际累计净位移，mm；按上一实际坐标减当前坐标累计，换手/坐标重建停计；`ZenohFrames.cs:127`、`ZState:1082`、`B数据端/工程/ADS项目/handle_net_distance.h:5-25`。 |
| 41 | `Axis6NetDistanceMm` | `double` / float64 | 本体轴 6 同一口径的实际净位移；非有限值发布为 0；`ZenohFrames.cs:128`、`ZState:1083`、`B数据端/工程/ADS项目/handle_net_distance.h:5-25`。 |
| 42 | `PrepareTaskState` | `byte` | 准备任务 0 空闲、1 运行、2 完成、3 拒绝/失败；`ZenohFrames.cs:129`、`ZState:1084`、`工程/protocol/PROTOCOL.md:171`。 |
| 43 | `PrepareTaskCommandId` | `int` | 准备任务关联命令 ID；`ZenohFrames.cs:130`、`ZState:1084`。 |
| 44 | `StartTaskState` | `byte` | 开始控制任务，与 PrepareTaskState 同一 0..3 编码；`ZenohFrames.cs:131`、`ZState:1085`、`工程/protocol/PROTOCOL.md:171`。 |
| 45 | `StartTaskCommandId` | `int` | 开始控制任务关联命令 ID；`ZenohFrames.cs:132`、`ZState:1085`。 |
| 46 | `Results` | `CommandResultFrame[]` | 最多 32 项的命令结果数组，元素结构见下；`ZenohFrames.cs:133`、`ZState:1086`、`工程/protocol/PROTOCOL.md:171`。 |
| 47 | `EventText` | `string` | 控制域 `recovery_text` 恢复事件/原因文字；`ZenohFrames.cs:134`、`ZState:1086`。 |
| 48 | `Axis6WindowStartFromLeftMm` | `float` / float32 | 轴 6 实际窗口起点，距左限位 mm，仅显示；`ZenohFrames.cs:135`、`ZState:1087-1088`、`B数据端/工程/ADS项目/control_messages.h:142-144`。 |
| 49 | `Axis6WindowEndFromLeftMm` | `float` / float32 | 同坐标系的轴 6 窗口终点，仅显示；`ZenohFrames.cs:136`、`ZState:1089-1090`。 |
| 50 | `ArmCjogState` | `uint` | 定位臂 PLC 末端点动状态；`ZenohFrames.cs:137`、`ZState:1091`。 |
| 51 | `ArmCjogError` | `uint` | 定位臂 PLC 末端点动错误码；`ZenohFrames.cs:138`、`ZState:1092`。 |
| 52 | `ArmCjogLimited` | `bool` | 定位臂末端点动限幅标志；`ZenohFrames.cs:139`、`ZState:1093`。 |
| 53 | `ArmCjogPathSpeed` | `double` / float64 | 定位臂末端点动路径速度，平移 mm/s 或旋转 °/s；`ZenohFrames.cs:140`、`ZState:1094`、`工程/protocol/PROTOCOL.md:132`。 |
| 54 | `ArmCartesianStatus` | `int` | 定位臂程序归零：0 空闲、1 进行中、2 完成、3 已在零位、4 停止、5 失败；`ZenohFrames.cs:141`、`ZState:1095`、`工程/protocol/PROTOCOL.md:133`。 |
| 55 | `ArmCartesianError` | `int` | 定位臂程序归零错误码；`ZenohFrames.cs:142`、`ZState:1096`、`工程/protocol/PROTOCOL.md:134`。 |
| 56 | `AuthTag` | `byte[]` / BIN | 认证后的 16 B HMAC 截断值；`ZenohFrames.cs:143`、`ZenohWire.cs:44-63`。 |

**已确认：`Flags` 全量位定义。** 出处为 `ProtocolConstants.cs:14-35` 与 `ZState:1038-1055`。同一位的 C# `FfEnabled/FfZeroing` 在 C++ 中分别称 `ForceFeedbackEnabled/ForceZeroing`。

| bit | C# 名称 | 含义/置位条件 |
|---:|---|---|
| 0 | `ControlActive` | 控制已激活。 |
| 1 | `EstopHold` | PLC 急停/保持门控当前生效。 |
| 2 | `SelfCheckDone` | 器械准备已完成。 |
| 3 | `FfEnabled` | 任一侧力反馈开关开启。 |
| 4 | `CalZeroed` | 装机零点已采集。 |
| 5 | `HostCommTimeout` | PLC 主机通信超时反馈。 |
| 6 | `YValveClosed` | `!extra_.y_valve_open`。 |
| 7 | `LeaseHeld` | 数据端控制权租约被持有。 |
| 8 | `StartupCompleted` | 启动序列已完成。 |
| 9 | `AdsHealthy` | `ads_state==2`。 |
| 10 | `FfZeroing` | 数据端正在采集力传感器零点。 |
| 11 | `GlobalReverse` | `guidewire_mode==0` 时看轴 1 reverse，否则看轴 6 reverse。 |
| 12 | `CleanForceValid` | `clean_force_valid` 且纯净轴向力可表示为有限 float32。 |
| 13 | `Force582Enabled` | SN582/导丝力反馈开关开启。 |
| 14 | `Force587Enabled` | SN587/导管力反馈开关开启。 |
| 15 | `ForceHoldEnabled` | 换手力反馈保持开关开启。 |

**已确认：嵌套 `CommandResultFrame` 三项。** 声明见 `ZenohFrames.cs:76-81`，语义见 `工程/protocol/PROTOCOL.md:171`。短命令的“完成”表示通过门控并被控制循环消费；准备、开始、力反馈、电缸、Y 阀另外由实际快照确认，不能把所有 Result=0 都当作机械到位。

| Key | 字段 | 类型 | 含义 |
|---:|---|---|---|
| 0 | `CommandId` | `int` | 与请求匹配的命令 ID。 |
| 1 | `Result` | `byte` | 0 完成、1 拒绝。 |
| 2 | `Reason` | `string` | 拒绝或完成时的原因/提示文字。 |

### ForceFrame 全量字段（13 项）

以下均为**已确认**，定义见 `ZenohFrames.cs:149-161`，数据端组帧见 `ZState:1118-1136`。同一名字 `Force582N/Force587N` 在本帧表示**轴向力 N**，与 StateFrame 中沿用的扭矩旧名不同；扭矩在本帧明确叫 `Torque582Nm/Torque587Nm`。只有持有控制权租约时才填入输出，并同时要求远程手柄输出 Enable 与该侧力反馈开关；不满足时字段保持默认 false/0。

| Key | 字段名 | C# 类型 / 线宽 | 含义/来源 |
|---:|---|---|---|
| 0 | `Version` | `byte` | 版本 1；`ZenohFrames.cs:149`、`ZState:1119`。 |
| 1 | `SessionId` | `ulong` | 当前认证会话；`ZenohFrames.cs:150`、`ZState:1118-1119`。 |
| 2 | `Sequence` | `ulong` | Force Topic 独立递增序号；`ZenohFrames.cs:151`、`ZState:1120`。 |
| 3 | `EchoMainMonotonicMs` | `ulong` | 最近主端单调时间的回显；`ZenohFrames.cs:152`、`ZState:1120`。 |
| 4 | `Enable582` | `bool` | SN582 输出使能；`ZenohFrames.cs:153`、`ZState:1125`。 |
| 5 | `Axis582` | `sbyte` | SN582 SDK 轴选择，数据端夹在 0..2；`ZenohFrames.cs:154`、`ZState:1125`。 |
| 6 | `Force582N` | `float` / float32 | SN582/导丝轴向力，N，禁用或非有限时为 0；`ZenohFrames.cs:155`、`ZState:1126`。 |
| 7 | `Torque582Nm` | `float` / float32 | SN582/导丝扭矩，N·m，禁用或非有限时为 0；`ZenohFrames.cs:156`、`ZState:1127`。 |
| 8 | `Enable587` | `bool` | SN587 输出使能；`ZenohFrames.cs:157`、`ZState:1128`。 |
| 9 | `Axis587` | `sbyte` | SN587 SDK 轴选择，数据端夹在 0..2；`ZenohFrames.cs:158`、`ZState:1128`。 |
| 10 | `Force587N` | `float` / float32 | SN587/导管轴向力，N，禁用或非有限时为 0；`ZenohFrames.cs:159`、`ZState:1129`。 |
| 11 | `Torque587Nm` | `float` / float32 | SN587/导管扭矩，N·m，禁用或非有限时为 0；`ZenohFrames.cs:160`、`ZState:1130`。 |
| 12 | `AuthTag` | `byte[]` / BIN | 16 B 认证 tag；`ZenohFrames.cs:161`、`ZState:1134-1136`。 |

### 认证和序号

主端发送 16-byte client nonce 的 Hello；数据端回 challenge；主端用 token、两个 nonce 计算 HMAC proof 和 16-byte session key；认证后的每帧 AuthTag 为 session key HMAC 前 16 字节。主端按每个 topic 严格拒绝重复/乱序序号，回调只接受当前 generation/session。来源：`FrameAuth.cs:7-30`、`RemoteRobotLink.cs:436-471,508-526`、`ZenohWire.cs:44-64,140-183`。

### 协议文件版本不一致

当前代码 `ZenohFrames.cs` 和 `工程/protocol/PROTOCOL.md` 都定义 StateFrame 57 项，索引 54/55 为 `ArmCartesianStatus/ArmCartesianError`，56 为 AuthTag；但 `介入机器人/B数据端/协议/PROTOCOL.md` 的旧段落仍写 55 项，索引 54 为 AuthTag。该文档与当前 C# / C++ 代码不一致，属于已确认的文档漂移，不据此推断运行时行为。证据：`A主端/工程/master/MasterConsole.Protocol/ZenohFrames.cs:129-143`、`A主端/工程/protocol/PROTOCOL.md:121-137`、`B数据端/协议/PROTOCOL.md:71-131`、`B数据端/工程/ADS项目/protocol/cpp/zenoh_protocol.h:102-118`。

## A.6 运行时配置和界面可调项

| 配置/默认值 | 是否运行时可改 | 使用位置 | 确定度与出处 |
|---|---|---|---|
| `remote.token`，当前文件内容为 20 个 ASCII 数字；最低 16 UTF-8 字节 | 文件可改，启动时读取；不是 UI 项 | `StartupSelection` 向上查找命令行指定路径、可执行文件目录和其上级 `config/remote.token`；`build-and-run.ps1` 联机启动要求 `工程/config/remote.token`。 | 已确认：`A主端/工程/config/remote.token`、`master/VI.Control/StartupSelection.cs:23-42`、`工程/build-and-run.ps1:11-16`。 |
| Host `192.168.60.1` | 可用 CLI `--host` 改；没有 UI 编辑 | `RemoteLinkSettings.Host`，未指定 endpoint 时构成 `tcp/<host>:7447`。 | 已确认：`RemoteRobotLink.cs:10-15,142-149`、`StartupSelection.cs:11-18`。 |
| Endpoint | 仅代码/测试可注入；CLI 没有 `--endpoint` | `RemoteLinkSettings.Endpoint` 非空时直接使用，否则 host+7447。 | 已确认：`RemoteRobotLink.cs:10-15,142-149`。 |
| `--sim` | CLI/调试配置 | 选择 `SimulatedRobotLink`，不创建 `HandleService` 或加载真实 SDK。 | 已确认：`StartupSelection.cs:14-22`、`App.xaml.cs:31-40`、`launchSettings.json:1-12`。 |
| 准备位置导管 `28` mm、导丝/Y 阀 `610` mm | UI 文本框可在发送前修改；限制导管 5–95、导丝 10–639 | `MainViewModel._catheterText/_wireText`、`Prepare()`。 | 已确认：`MainViewModel.cs:59-61,262-305`、`InterventionPanel.xaml:51-55`。 |
| 电缸 1/3 打开值 2000，2/4 打开值 10 | 不提供数值编辑；按钮只切换目标状态 | `CylinderItem` 和 UI 四个 ToggleButton。 | 已确认：`MainViewModel.cs:14-34,71-74`、`InterventionPanel.xaml:129-149,221-241`。 |
| 力反馈保持 | 默认由数据端 flags 决定；UI 可点 `ForceHold` 请求开/关 | `SetForceFeedbackHoldAsync`，不本地猜测状态。 | 已确认：`MainViewModel.cs:184-192,329-331`、`InterventionPanel.xaml:68-69`。 |
| 导管/导丝反馈开关、取零 | UI 可操作；首次开启且未取零时数据端自动取零；取零为独立命令 | `ToggleForceFeedback`、`ZeroForceSensor`。 | 已确认：`MainViewModel.cs:315-327`、`工程/docs/guides/力反馈说明.md:14-24`。 |
| 定位臂速度档 | UI 可选快/中/慢，默认中；倍率 2.0/1.0/0.5；平移/旋转速度由代码表给出 | `MainViewModel.GetArmCartesianJogSpeed` 与 `SetArmJogSpeedScale`。 | 已确认：`MainViewModel.cs:445-473`、`PositioningArmPanel.xaml:29-31`。 |
| `DAMPING_B=0.01`、`DAMPING_SIGN=1`、`DAMPING_MAX_N=0.5`、`FORCE_SLEW_N_PER_S=8`、`FORCE_MAX_N=1.2` | 只能改源码后重编译；无配置文件/界面项 | `VI.HandleHost/Program.cs` 常量。 | 已确认：`VI.HandleHost/Program.cs:9-14,104-118`、`工程/docs/guides/力反馈说明.md:73-84`。 |
| `SHOW_FORCE_CONSOLE=false`（宿主和监管器） | 只能改源码后重编译 | 是否显示 HandleHost 控制台。当前工作区代码为 false；旧说明仍写 true，属于说明漂移。 | 已确认：`VI.HandleHost/Program.cs:14`、`HandleService.cs:14,34-35`；文档对比 `工程/docs/guides/力反馈说明.md:82-84`。 |
| `ProtocolConstants.StatusTimeoutMs=2000`、`HapticTimeoutMs=3000` | 只能改源码后重编译 | 主端状态新鲜度/控制门控、宿主力目标时效。 | 已确认：`MasterConsole.Protocol/ProtocolConstants.cs:5-12`、`HandleService.cs:90-91`、`RemoteRobotLink.cs:87-98,521-525`。 |
| UI 布局阈值 | 不能由用户配置；窗口 1000×900，尺寸 tier：Large 1500×860、Medium 1150×640、否则 Small；宽度 820 切换 Wide/Narrow。 | `MainWindow.xaml:1`、`MainWindow.xaml.cs:12-40` 和主题 XAML。 | 已确认。 |

## A.7 主端可执行文件、库和输出清单

`工程/tools/runtime-files.txt:1-24` 是当前部署检查清单，包含：`VI.Control.exe/dll/deps/runtimeconfig`、`VI.Service.Robot.dll`、`VI.Service.Net.dll`、`MasterConsole.Protocol.dll`、`MessagePack*.dll`、`Robotics.HostFramework*.dll`、`Modules/Features/VI.Module.Intervention.dll`、`VI.Module.PositioningArm.dll`、`FLCatheter.dll`、`zenohc.dll`、VC142 运行库。其余项目均为类库或测试 executable。主应用、宿主和测试的职责见 A.1；运行清单没有 `VI.HandleHost.exe` 条目，因为它通过 `Directory.Build.targets` 作为应用输出下 `HandleHost/` 子目录存在，入口/复制规则以 `Directory.Build.targets:2-8` 和 `VI.HandleHost.csproj:3-14` 为准。

## A.8 规模（主端当前磁盘文件）

以下为物理行数（含注释、空行）；只统计当前 `master/` 的 `.cs/.xaml`，排除 `bin/obj`。计数脚本为 `docs/arch_survey_r1/scratch/master_line_counts.ps1`，逐文件结果保存在 `scratch/master_line_counts.csv`；数据为 2026-11-12 当前磁盘读数，未修改源文件。

| 文件 | 行数 | 用途 | 证据 |
|---|---:|---|---|
| `VI.Service.Robot/ViewModels/MainViewModel.cs` | 625 | UI 状态、命令和门控 | 行数由当前磁盘 UTF-8 行数统计；关键块 `VI.Service.Robot/ViewModels/MainViewModel.cs:47-95,112-177,260-371,505-624`。 |
| `VI.Service.Robot/Services/RemoteRobotLink.cs` | 620 | Zenoh 会话、认证、100 Hz 手柄和命令 | 当前磁盘行数；关键块 `VI.Service.Robot/Services/RemoteRobotLink.cs:118-219,224-377,380-427,434-620`。 |
| `VI.Service.Robot/Services/SimulatedRobotLink.cs` | 295 | 离线模拟链路 | 当前磁盘行数；周期与模拟延时 `VI.Service.Robot/Services/SimulatedRobotLink.cs:34-57,73-114,242-288`。 |
| `VI.Service.Net/ZenohSession.cs` | 287 | 原生 Zenoh Session / 发布订阅 | 当前磁盘行数；构造与发送 `VI.Service.Net/ZenohSession.cs:29-167`、释放 `VI.Service.Net/ZenohSession.cs:169-191`。 |
| `VI.Module.Intervention/InterventionPanel.xaml` | 336 | 介入 UI | 当前磁盘行数；按钮与绑定 `VI.Module.Intervention/InterventionPanel.xaml:17-69,129-149,221-241,299-331`。 |
| `VI.Module.PositioningArm/PositioningArmPanel.xaml` | 182 | 定位臂 UI | 当前磁盘行数；点动/调试区 `VI.Module.PositioningArm/PositioningArmPanel.xaml:20-31,40-106,109-173`。 |
| `MasterConsole.Protocol/ZenohWire.cs` | 206 | 位置数组严格校验和认证 tag | 当前磁盘行数；字段 schema `MasterConsole.Protocol/ZenohWire.cs:66-124`、值校验 `MasterConsole.Protocol/ZenohWire.cs:140-204`。 |
| `VI.HandleHost/Program.cs` | 141 | 200 Hz 手柄宿主 | 当前磁盘行数；循环 `VI.HandleHost/Program.cs:29-103`。 |
| `VI.Service.Robot/Services/HandleService.cs` | 184 | 宿主监管 | 当前磁盘行数；监管循环 `VI.Service.Robot/Services/HandleService.cs:102-179`。 |

| 其它主要文件 | 物理行数 | 用途/确定度 |
|---|---:|---|
| `VI.Control/Themes/LightClinical.xaml` | 560 | 已确认：主题资源和控件样式；当前文件 `VI.Control/Themes/LightClinical.xaml:1-560`。 |
| `VI.Control/MainWindow.xaml` | 238 | 已确认：主窗口和模块容器；当前文件 `VI.Control/MainWindow.xaml:1-238`。 |
| `VI.Module.PositioningArm/PositioningArmPanel.xaml.cs` | 174 | 已确认：点动保活、按住/释放和禁用清理；当前文件 `VI.Module.PositioningArm/PositioningArmPanel.xaml.cs:1-174`。 |
| `MasterConsole.Protocol/ZenohFrames.cs` | 163 | 已确认：四类线帧和命令号；当前文件 `MasterConsole.Protocol/ZenohFrames.cs:1-163`。 |
| `VI.Service.Robot/Services/IRobotLink.cs` | 97 | 已确认：UI 与真实/模拟链路接口；当前文件 `VI.Service.Robot/Services/IRobotLink.cs:1-97`。 |
| `MasterConsole.Protocol/Frames.cs` | 82 | 已确认：内部手柄/力/状态数据对象；当前文件 `MasterConsole.Protocol/Frames.cs:1-82`。 |
| `VI.Service.Robot/Services/HandleHostMemory.cs` | 71 | 已确认：共享映射和 seqlock；当前文件 `VI.Service.Robot/Services/HandleHostMemory.cs:1-71`。 |
| `VI.Control/App.xaml.cs` | 65 | 已确认：应用宿主、DI 和模块装载；当前文件 `VI.Control/App.xaml.cs:1-65`。 |
| `MasterConsole.Protocol/FrameAuth.cs` | 61 | 已确认：HMAC 和会话 key；当前文件 `MasterConsole.Protocol/FrameAuth.cs:1-61`。 |
| `VI.Module.Intervention/InterventionPanel.xaml.cs` | 60 | 已确认：注射器/轴4保持和释放；当前文件 `VI.Module.Intervention/InterventionPanel.xaml.cs:1-60`。 |
| `VI.Control/MainWindow.xaml.cs` | 58 | 已确认：响应式布局、窗口退出停止；当前文件 `VI.Control/MainWindow.xaml.cs:1-58`。 |
| `VI.Control.Tests/Program.cs` | 549 | 已确认：WPF 离线检查入口；当前文件 `VI.Control.Tests/Program.cs:1-549`。 |
| `MasterConsole.Protocol.Tests/RobotServiceTests.cs` | 304 | 已确认：真实链路服务的 fake transport 测试；当前文件 `MasterConsole.Protocol.Tests/RobotServiceTests.cs:1-304`。 |
| `VI.Control.Tests/RevisionUChecks.cs` | 292 | 已确认：UI 修订验收检查；当前文件 `VI.Control.Tests/RevisionUChecks.cs:1-292`。 |
| `MasterConsole.Protocol.Tests/NetworkSmoke.cs` | 246 | 已确认：Zenoh 回环 smoke；当前文件 `MasterConsole.Protocol.Tests/NetworkSmoke.cs:1-246`。 |
| `VI.Control.Tests/ResponsiveLayoutChecks.cs` | 217 | 已确认：离屏布局检查；当前文件 `VI.Control.Tests/ResponsiveLayoutChecks.cs:1-217`。 |
| `MasterConsole.Protocol.Tests/Program.cs` | 178 | 已确认：协议离线检查入口；当前文件 `MasterConsole.Protocol.Tests/Program.cs:1-178`。 |
| `MasterConsole.Protocol.Tests/HandleHostTests.cs` | 144 | 已确认：fake 子进程监管测试；当前文件 `MasterConsole.Protocol.Tests/HandleHostTests.cs:1-144`。 |
| `VI.Control.Tests/HomeChainChecks.cs` | 128 | 已确认：归零相关 UI/状态检查；当前文件 `VI.Control.Tests/HomeChainChecks.cs:1-128`。 |
| `MasterConsole.Protocol.Tests/LocalForceTests.cs` | 67 | 已确认：本地阻尼/力限幅检查；当前文件 `MasterConsole.Protocol.Tests/LocalForceTests.cs:1-67`。 |

## A.9 当前未能从主端源码确定的事实

| 未决事实 | 缺少证据 |
|---|---|
| 实际网络状态帧/力帧的端到端频率分布、Zenoh RTT | 主端只有本地 `StatusHz`/RTT 累计值，仓库没有现场运行日志；需要离线网络回环或现场记录。 |
| SDK 拔出时 `deviceStatus()` 是否一定变为无效 | `VI.HandleHost/Program.cs:65-66` 自身注释明确写“是否更新仍需现场核实”。 |
| 是否存在仓库外的程序依赖当前 HandleHost 映射 | 当前主端与宿主编译同一个 C# 布局文件，当前双方布局已确认（A.4）；未发现仓库外消费者资料，无法判断是否另有旧 C++ 工具需要兼容。证据：`VI.HandleHost.csproj:10`、`HandleHostMemory.cs:50-70`。 |
| `StopTimeout=8 s` 是否足以覆盖现场 PLC/数据端退出 | 主端源码只配置宿主停止上限，未包含 PLC 停止时延。 |
| `B数据端/协议/PROTOCOL.md` 的 55 项段落是否被任何部署脚本使用 | 当前 C#、C++ 线格式均为 57 项；未发现读取该旧文档的程序。 |

## A.10 数据端工程、构建与进程

以下是当前磁盘 `介入机器人/B数据端/工程/`，不是旧的 `B_从端电脑/` 镜像。**已确认**：ADS 数据端是一个 C++17、v142、WinSDK10 的 x64 控制台程序，工程文件同时保留 Win32 配置；Release 输出为 `B数据端/工程/ADS项目/x64/Release/ADS.exe`，构建项目为 `ADS.sln`/`ADS.vcxproj`。来源：`B数据端/工程/ADS项目/ADS.vcxproj:1-20,91-142,151-225`。

| 工程/程序 | 当前用途 | 构建与输出 | 确定程度与出处 |
|---|---|---|---|
| `ADS.sln` / `ADS.vcxproj` | 数据端生产程序；链接 TcAdsDll、zenohc、bcrypt、ws2_32，编译 main、ADS通信、Zenoh、手柄、力反馈、tracking、定位臂ADS服务和运动学源。 | MSBuild VS2022；`x64/Release/ADS.exe`或`x64/Debug/ADS.exe`，复制zenohc、zenohd、TcAdsDll、VC142和`reliability_config.json`。 | 已确认：`ADS.vcxproj:91-142,151-225`。 |
| `PositioningArmKinematics` | 定位臂运动学/程序归零库及其离线模型测试；不是第二个现场运动进程。 | `CMakeLists.txt`/`BuildAndTest.ps1`；ADS.vcxproj把`src/kinematics.cpp`和`program_zero.cpp`静态编入ADS。 | 已确认：`PositioningArmKinematics/CMakeLists.txt:1-51`、`ADS.vcxproj:209-221`、`BuildAndTest.ps1:1-42`。 |
| `B数据端/编译并启动数据端.cmd` | 包装数据端构建/启动入口；实际工程脚本是`工程/build-and-run.ps1`。 | 默认会构建并启动现场ADS.exe；本轮只使用`-BuildOnly`等价构建路径，没有启动。 | 已确认：`B数据端/编译并启动数据端.cmd:1-6`、`B数据端/工程/build-and-run.ps1:1-38`。 |
| 离线测试程序 | `tools/*.cpp`由`run_offline_tests.cjs`抽取生产代码分支编译；Zenoh、ADS诊断和定位臂模型另有测试入口。 | 输出只在`docs/arch_survey_r1/scratch/tests*`，不覆盖生产输出。 | 已确认：`ADS项目/tools/run_offline_tests.cjs:1-142`；结果详见F。 |

### A.10.1 数据端主要文件规模和 main 最大函数

行数为当前磁盘UTF-8物理行，含空行/注释。**已确认**：`main.cpp`共5424行，生产主循环位于966–5402；脚本输出存于`scratch/main_spans.json`。最长20个函数/lambda如下（`main`本身也列出，闭包按源码推导跨度）：

| 名称 | 行区间 | 行数 |
|---|---:|---:|
| `main` | 37–5424 | 5388 |
| `step_planned_return` | 2662–3076 | 415 |
| `run_axis6_crawl_state` | 3121–3213 | 93 |
| `apply_planned_return_outputs` | 2563–2637 | 75 |
| `refresh_planned_return_leg` | 2283–2340 | 58 |
| `begin_planned_return` | 2471–2527 | 57 |
| `planned_return_targets_safe` | 2382–2424 | 43 |
| `begin_direct_positioning` | 381–420 | 40 |
| `cancel_active_return_motion` | 664–703 | 40 |
| `engage_axis6_soft_limit_hold` | 714–751 | 38 |
| `zero_force_sensor_impl` | 759–794 | 36 |
| `planned_return_event_view` | 2247–2281 | 35 |
| `submit_planned_return_ads_command` | 582–615 | 34 |
| `planned_return_settle_wait_ms` | 2529–2561 | 33 |
| `fail_planned_return` | 2440–2469 | 30 |
| `apply_emergency_retract_v_limit` | 427–454 | 28 |
| `poll_planned_return_ads_command` | 622–648 | 27 |
| `apply_mode_selection` | 874–896 | 23 |
| `constrain_axis1_follow_cmd_abs` | 4009–4031 | 23 |
| `lock_handle_mode` | 130–151 | 22 |

来源：`docs/arch_survey_r1/scratch/main_spans.json`；主文件和主要模块物理行数：`main.cpp:1-5424`、`ads_communication.cpp:1-2322`、`zenoh_service.cpp:1-1142`、`plc_io.cpp:1-677`、`control_types.h:1-761`、`手柄/手柄.cpp:1-153`、`arm_manual_ads_service.cpp:1-738`、`experiment_recorder.cpp:1-1033`、`motion_sync.cpp:1-524`、`startup_sequence.cpp:1-261`。

### A.10.2 数据端线程、周期和交换

| 线程/任务 | 周期或触发 | 交换方式和职责 | 证据 |
|---|---:|---|---|
| `main`控制线程 | 等待新ADS快照；目标随ADS快线程100Hz，不是独立固定sleep。单拍由`wait_for_snapshot(...,300ms,...)`开始，最后发布输出/状态。 | 读取ADS snapshot、手柄对象和Zenoh命令缓存；写`AdsOutputCommand`和记录器。 | `D/main.cpp:966-1012,4471-4522`。 |
| ADS快线程 | 100Hz目标；每次读取完整快照并写快输出。 | 快照队列容量2048，互斥/条件变量；最新输出用独立mutex及generation交换；失败进入SoftHold，持续1s进入Reconnecting。 | `D/ads_communication.cpp:15-21,205-216,923-1005`；快照队列`D/ads_communication.h:322-326`，planned队列/结果容量32/128另见`D/ads_communication.h:279-281`。 |
| ADS低频线程 | 仅在Running时处理；低频请求和坐标刷新，循环尾部等待1ms；重连间隔1000ms，单次ADS超时300ms。 | 低频请求队列、坐标缓存和互斥量；定位臂服务使用此通道。 | `D/ads_communication.cpp:2050-2115`、`D/arm_manual_ads_service.cpp:13-18,108-116`。 |
| Zenoh服务线程 | worker loop；命令队列容量256、pending任务8，命令最大龄期2s，状态新鲜度500ms，hold续约100ms。 | subscriber回调锁住remote handle bridge/command queue；状态由main发布，命令通过poll。 | `D/zenoh_service.cpp:25-30,183-300,980-1020`、`D/zenoh_service.h:44-78`。 |
| 手柄远程缓存 | 不另起生产控制线程；Zenoh回调更新slot缓存，main每拍poll。 | 每个serial一个slot、锁、有效/采样年龄/epoch；无效样本不覆盖最近有效缓存。 | `D/手柄/remote_handle_bridge.h:18-90`、`D/手柄/手柄.cpp:44-103`。 |
| 定位臂ADS服务 | worker；读周期50ms，点动租约300ms，笛卡尔点动租约500ms；调用传250ms，但底层慢通道300ms且调用者等待无总上限。 | 服务内部desired/dirty字段和snapshot互斥；`ArmManualSnapshot`由主端状态发布。 | `D/arm_manual_ads_service.cpp:13-35,108-116,601-718`。 |
| 实验记录writer | 每个CSV一个AsyncCsvWriter线程；容量8192，空队列sleep2ms，64行flush。 | 无锁/原子head-tail环形槽；生产线程enqueue，writer写文件。 | `D/async_csv_writer.h:34-95,116-177,215`。 |
| 手柄速度记录sampler | 100Hz，`timeBeginPeriod(1)`后按deadline采样。 | 独立sampler+writer，读remote_handle_bridge两槽缓存，不参与运动控制。 | `D/handle_velocity_recorder.h:22-102`。 |
| 实验记录stop_worker | 按需，stop_async状态转换成功时创建；非周期线程。 | 等三个writer清队列/停止后写最终session.json，stop_and_wait会join；线程创建失败在调用方同步收尾。 | `D/experiment_recorder.h:228`、`D/experiment_recorder.cpp:568-611`。 |

**已确认**：源码未显示数据端另有用户创建的运动线程或共享内存；跨线程边界是ADS快/慢线程、Zenoh worker、定位臂worker和记录器线程，控制状态主要在main及其引用结构体内。**未知**：Windows调度抖动、ADS API实际完成时间和PLC侧任务交错需要现场/在线采样。

## A.11 PLC工程、任务和POU

PLC根为`介入机器人/C从端/250902/`。`250902.sln`引用系统工程`250902.tsproj`和测量工程；当前系统实例化`Untitled2`，PLC端口851。`Untitled1`是未被当前系统引用的旧10ms工程，`Untitled3`是EL6900 TwinSAFE安全PLC项目。来源：`C从端/250902.sln:2-21`、`C从端/250902/250902.tsproj:3594-3598`、`C从端/250902/Untitled2/Untitled2.plcproj:20-70`。

| PLC对象 | 现状 | 证据 |
|---|---|---|
| `PlcTask` | 1ms、优先级20、入口只调用MAIN；AmsPort 851。 | `Untitled2/PlcTask.TcTTO:3-9`。 |
| NC-Task 1 SAF / NC SVB | XML原值10000/50000，优先级4/8；按100ns换算为1/5ms只是推断，源XML未写单位。 | `250902.tsproj:1451-1465`。 |
| MAIN调用顺序 | 轴引用刷新→FirstCycle/init→host watchdog→ArmManual→ArmCartesian→ArmCartJog→Axis2Sync→InjectorManual→轴6许可/POWERED/SETPOSITIONGEN/RESETED→gen_state CASE。 | `POUs/MAIN.TcPOU:35-112,120-204`。 |
| 编译POU | MAIN、init、SelfCheck、handle、err、clear_err、reset、ArmManual、ArmCartesian、ArmCartJog、ArmCartJogGen、Axis2Sync、InjectorManual，共13个；`handle(备份)`/`move_edge`存在但不在Compile Include。 | `Untitled2/Untitled2.plcproj:20-70`。 |
| GVL | `G.TcGVL`当前150项，包含gen_state、轴数组、refer、限位、回退结构、电缸、注射器、同步轴、定位臂、watchdog和通知状态。 | `Untitled2/GVLs/G.TcGVL:5-189`、`docs/arch_survey_r1/scratch/C_GVL全量索引.md`。 |
| PLC构建 | 本机未运行TwinCAT编译；Boot目录有Port_851.app/tizip/autostart等文件，但不能证明现场正在加载它们。 | `C从端/250902/_Boot/TwinCAT RT (x64)/Plc/`；旧构建限制记录`C从端/通讯逻辑改造实施指南.md:439-446`。 |

POU职责按源码可确认如下：`handle.TcPOU`负责refer平滑、每扫描限速、软限位、回退状态10/20/30/31和本体输出；`SelfCheck.TcPOU`负责自检/寻参动作；`ArmManual.TcPOU`负责定位臂轴使能、复位、点动和看门狗；`ArmCartesian.TcPOU`/`ArmCartJog.TcPOU`/`ArmCartJogGen.TcPOU`负责末端轨迹、笛卡尔点动和轨迹生成；`Axis2Sync.TcPOU`负责同步轴复位/跟随；`InjectorManual.TcPOU`负责注射器；`err.TcPOU`处理压限退离；`clear_err`/`reset`处理错误清理和初始化。各职责、状态变量和GVL字面引用见`docs/arch_survey_r1/scratch/C_plc_notes.md`、`docs/arch_survey_r1/scratch/C_GVL全量索引.md`。**已确认**：MAIN同一PLC任务内顺序调用，未见PLC用户线程/队列；**未知**：NC、I/O和驱动实际调度抖动。

## A.12 ADS符号与数据方向清单

### A.12.1 100Hz快速读

**已确认**：本组由ADS快线程解析句柄后每拍Sum Read；所有索引按PLC 1-based展开。表内`[1..7]`表示全部七轴，`[1/3/5/6]`表示四个指定轴。下列“写方”说明正常生产责任，不把NC库内部行为当作显式POU赋值。来源：`D/ads_communication.cpp:1255-1320,1406-1460`。

| 符号/类型 | 数据端读取时机和意义 | 谁写、何时写（PLC） | 直接出处 |
|---|---|---|---|
| `TwinCAT_SystemInfoVarList._TaskInfo[1].CycleCount`、`DcTaskTime`；std::uint32_t/std::int64_t缓存 | 每拍；CycleCount在Sum首尾各读一次；用于跨扫描/停滞诊断 | TwinCAT运行时按任务扫描更新 | `D/ads_communication.h:31-33`、`D/ads_communication.cpp:1255-1256,1424-1425,1459-1460,1515-1550` |
| `G.axis[1..7].NcToPlc.ActPos`；7×LREAL | 优先直接NC绝对位置，减init/left得到两个坐标 | NC/AXIS_REF回读；MAIN先调用ReadStatus | `D/ads_communication.cpp:1310-1320,1426-1435`；`P/MAIN.TcPOU:35-40` |
| `G.Act_pos`；7×LREAL | 若任一直接NC句柄缺失，用整数组回退相对位置 | handle/SelfCheck从NC重建，等待态可能不刷新 | `D/ads_communication.cpp:1257,1275-1279,1433-1435`；`P/handle.TcPOU:489-491`；`D/main.cpp:1402-1408` |
| `G.axis[1/3/5/6].NcToPlc.ActVelo`；4×LREAL | 每拍，换手停稳/恢复门 | NC/AXIS_REF | `D/ads_communication.cpp:1260-1263,1437-1440` |
| `G.ft_1_value`、`fn_1_value`、`fn_2_value`、`ft_2_value`（均带`G.`）；4×INT | 每拍，原始力/扭矩ADC，后按1000 counts/V换算 | EtherCAT输入Box7/8；非数据端写 | `D/ads_communication.cpp:1264-1267,1441-1444`；`介入机器人/C从端/250902/250902.tsproj:7630-7688` |
| `G.estop_hold_req`、`G.host_comm_timeout`；BOOL | 每拍，运动/力门控；也订阅通知 | handle/err/clear_err/reset的急停路径；MAIN/handle会话心跳/停稳恢复路径 | `D/ads_communication.cpp:1268-1269,1445-1446`；`P/MAIN.TcPOU:68-104`、`P/handle.TcPOU:108-136,321-466`、`P/err.TcPOU:32-48` |
| `G.init_pos`、`G.leftlimit`；各7×LREAL | 每拍；初始化和请求坐标刷新时也读 | init/SelfCheck寻参建立；MAIN接受homing_restore时复制 | `D/ads_communication.cpp:1258-1259,1447-1448`；`P/init.TcPOU:74-80`、`P/SelfCheck.TcPOU:118-567`、`P/MAIN.TcPOU:149-151` |
| `G.return_cmd[1/6].Req`、`Busy`；各BOOL | 每拍，恢复/取消观察；仅Req/Busy不能独立确认回退Done | Req由数据端Prepare/Commit/Clear写，PLC故障/复位亦会清；Busy由handle回退状态更新 | `D/ads_communication.cpp:1294-1300,1449-1452`；`P/handle.TcPOU:250-319,579-709` |
| `G.handle_reinit_done`；BOOL | 每拍且通知，重新允许Follow的资格 | handle重新初始化完成/保持路径写 | `D/ads_communication.cpp:1296,1453`；`P/handle.TcPOU:108-136,321-466` |
| `G.err_backoff_active`、`G.err_backoff_failed`；各7×BOOL | 每拍，故障恢复与续做门 | err POU限位退离流程 | `D/ads_communication.cpp:1296,1454-1455`；`P/err.TcPOU:32-263` |
| `G.axis2_sync_error`/`error_id`；BOOL/UDINT | 每拍，同步轴故障门 | Axis2Sync每扫描按Gear/Reset故障锁存和清理 | `D/ads_communication.cpp:1297,1456-1457`；`P/Axis2Sync.TcPOU:19-89` |
| `G.self_check_done`；BOOL | 每拍且通知，自检/寻参资格 | init/SelfCheck置位/清零；MAIN寻参复用可置位 | `D/ads_communication.cpp:1297-1298,1458`；`P/SelfCheck.TcPOU:548-567`、`P/MAIN.TcPOU:149-156` |

### A.12.2 100Hz快速写

**已确认**：数据端main生产`AdsOutputCommand`，快线程执行Sum Write。每拍实际子项由motion_enabled/dirty/mask及planned队列决定，心跳/会话每次追加。PLC不保证一个Sum子项列表在同一扫描原子生效，代码也明确不作此保证：`D/ads_communication.cpp:1752-1761`。

| 符号/类型 | 谁写、何时写（数据端） | PLC使用/回写方 | 出处 |
|---|---|---|---|
| `G.host_session_id`、`G.host_heartbeat_sequence`；UDINT | ADS快线程每拍追加，会话在服务连接生命周期保持，heartbeat递增 | MAIN观察变化喂2s看门狗；handle处理恢复 | `D/ads_communication.cpp:1619-1635,1757-1761`；`P/MAIN.TcPOU:68-104` |
| `G.host_recover_req`；BOOL | watchdog_recovery_pending.exchange后写true；不每拍true | PLC停稳后清req并开始重初始化 | `D/ads_communication.cpp:1759-1761`；`P/handle.TcPOU:321-466` |
| `G.refer`；7×LREAL | main本拍pos，motion_enabled时快写 | handle读取相对init目标，平滑/限速/软限位；自检/急停路径也可重建refer | `D/main.cpp:4471-4494`、`D/ads_communication.cpp:1636-1648`；`P/handle.TcPOU:489-571` |
| `G.axis1_fast_return`、`G.axis6_fast_retract`；BOOL | 随motion输出写；从enabled变disabled补false | handle平滑/限速分支；PLC初始化亦清 | `D/ads_communication.cpp:1638-1647`；`P/handle.TcPOU:491-545` |
| `G.cylinder1_value`…`G.cylinder4_value`；UINT | discrete_dirty且cylinder_valid时按4-bit mask写对应项 | 映射物理电缸输出；PLC自检/回退/急停路径也写，非单写所有权 | `D/ads_communication.cpp:1650-1660`；`P/handle.TcPOU:250-319,321-466`；`介入机器人/C从端/250902/250902.tsproj:7630-7688` |
| `G.cylinder5_press_req`；BOOL | 离散dirty时随同批写 | handle按true=500/false=2000映射cylinder5_value | `D/ads_communication.cpp:1662`；`P/handle.TcPOU:475-479` |
| `G.axis4_fwd_req`、`G.axis4_rev_req`；BOOL | 离散dirty时写；main按方向/500ms租约门控 | handle轴4状态机，冲突/超时路径清理 | `D/ads_communication.cpp:1663-1664`、`D/main.cpp:1609-1639`；`P/handle.TcPOU:706-884` |
| `G.inject_push_req`、`G.inject_pull_req`；各2×BOOL | 离散dirty时写；每个注射器方向经500ms租约 | InjectorManual每拍读；host超时清请求 | `D/ads_communication.cpp:1665-1666`；`P/InjectorManual.TcPOU:23-109` |
| `G.startup_smoothing_bypass`；BOOL | 离散dirty时写启动旁路 | handle平滑分支，PLC初始化/故障清理也改 | `D/ads_communication.cpp:1667`；`P/handle.TcPOU:491-545` |
| `G.return_cmd[1/6].TargetAbs/Velocity/Acc/Dec/Jerk`；LREAL | 专用planned队列Prepare：先Req=false，填两腿参数；一拍至多取一个排队命令 | handle请求锁存→MC_MoveAbsolute | `D/ads_communication.cpp:1692-1701`；`P/handle.TcPOU:579-644` |
| `G.return_cmd[1/6].Req`；BOOL | Prepare=false；Commit=true；Clear=false；PublishHandoff/AwaitHandoffApplied的planned_return_clear_mask另可带false | handle按Req/Busy/Done/Error阶段执行/收尾，故障可清 | `D/ads_communication.cpp:1692-1750`；`P/handle.TcPOU:579-709` |

### A.12.3 ADS通知（全部20项）

**已确认**：注册类型`ADSTRANS_SERVERONCHA`、cycle参数原值10000、maxDelay=0；回调更新受锁事件缓存与事件序号；注册后主动Sum Read同20项初值。10000的物理单位未由本轮在线验证。出处：`D/ads_communication.cpp:1090-1138,1907-1960`。

| 符号 | 类型 | 谁写、何时更新（PLC） |
|---|---|---|
| `G.self_check_done`、`G.selfcheck_status` | BOOL、DINT | init/SelfCheck与MAIN接受复用/新自检时；`P/MAIN.TcPOU:120-180`、`P/SelfCheck.TcPOU:118-567` |
| `G.handle_reinit_req`、`G.handle_reinit_done` | BOOL各一 | 自检/急停恢复请求与handle初始化完成；数据端同步后清req；`P/handle.TcPOU:108-136,321-466`、`D/plc_io.cpp:672-675` |
| `G.estop_hold_req`、`G.host_comm_timeout` | BOOL各一 | 急停/失使能/心跳超时与停稳恢复；`P/MAIN.TcPOU:68-104`、`P/handle.TcPOU:108-136,321-466` |
| `G.startup_loading_ready` | BOOL | SelfCheck完成置true，数据端开始控制前消费false；`P/SelfCheck.TcPOU:548-567`、`D/plc_io.cpp:358-360` |
| `G.axis4_manual_busy`、`done`、`error`、`error_id`（完整前缀均`G.axis4_manual_`） | BOOL×3、UDINT | handle轴4手动状态每扫描；`P/handle.TcPOU:706-884` |
| `G.gen_state` | state/INT16缓存 | MAIN/init/SelfCheck/handle/err/clear_err/reset状态切换；`P/MAIN.TcPOU:120-203`，全部PLC写方逐项见C.6.2 |
| `G.return_cmd[1].Busy/Done/Error/ErrorId` | BOOL×3、UDINT | handle轴1回退状态，每扫描按FB/阶段更新；`P/handle.TcPOU:579-709` |
| `G.return_cmd[6].Busy/Done/Error/ErrorId` | BOOL×3、UDINT | handle轴6回退状态，同上；`P/handle.TcPOU:579-709` |

### A.12.4 低频、初始化和兼容读写

**已确认**：本组经单独AMS端口的低频请求队列；不是每100Hz快事务都写。API调用方timeout参数不构成排队上限，见`D/ads_communication.cpp:559-586,2044-2225`。

| 符号/类型 | 谁读/写、何时 | PLC对端/用途 | 证据 |
|---|---|---|---|
| `TwinCAT_SystemInfoVarList._AppInfo.AppName`；数据端char[64] | 连接初始化读；用于识别应用重载和寻参记录校验 | TwinCAT运行时 | `D/ads_communication.cpp:1070-1077` |
| 通知20项（上表） | 每次连接注册后主动读一次 | 避免等待下一次变量变化 | `D/ads_communication.cpp:1090-1138` |
| `G.init_pos`、`G.leftlimit`；7×LREAL各一 | 连接初始化读；寻参完成/复用后请求低频刷新；此外每拍也快读 | init/SelfCheck/MAIN写基准 | `D/ads_communication.cpp:1391-1403,2078-2103` |
| `G.v_limit`；7×LREAL | startup/emergency先read备份、write缩放，完成/取消再write还原 | handle读取每扫描位置增量上限；GVL声明给默认 | `D/plc_io.cpp:336-344`、`D/startup_sequence.cpp:138-165,236-248`、`介入机器人/C从端/250902/Untitled2/GVLs/G.TcGVL:73`、`D/main.cpp:427-467` |
| `G.startup_loading_ready`；BOOL | read封装生产从通知取值；write用于消费资格false | SelfCheck置true | `D/plc_io.cpp:347-360`、`D/main.cpp:5332-5336` |
| `G.handle_reinit_req`；BOOL | main成功同步后低频write=false | PLC自检/恢复置true，handle判断资格 | `D/plc_io.cpp:672-675`、`D/main.cpp:1911-1985` |
| `G.selfcheck_target_from_left`、`G.selfcheck_start_req`；7×LREAL、BOOL | PreparePosition转换的内部命令校验通过后先写target，再写start=true；不自动重试失败的启动 | MAIN自检请求校验/消费，SelfCheck执行 | `D/main.cpp:4881-4924`、`P/MAIN.TcPOU:164-180` |
| `G.power_output[1..7].Done/Error`、`G.reset_output[1..7].Error`、`G.axis[1..7].Status.Error/DriveDeviceError`；35×BOOL | 寻参复用前read_sum检查七轴健康 | MAIN FBD MC输出/NC状态 | `D/main.cpp:1380-1398`；`P/MAIN.TcPOU:207-2438,2439-3809` |
| `G.axis[1..7].NcToPlc.ActPos`；7×LREAL | 健康门通过后复用前单独低频读，不能使用等待态未刷新的Act_pos | NC实际绝对位置 | `D/main.cpp:1401-1409` |
| `G.homing_restore_leftlimit`、`G.homing_restore_init_pos`；各7×LREAL；`G.homing_restore_status` DINT；`G.homing_restore_req` BOOL | main先写两坐标数组和status=0，成功后单独req=true | MAIN复用校验后复制、设置状态并清req | `D/main.cpp:1420-1426`、`P/MAIN.TcPOU:124-162` |
| `G.homing_restore_status`、`G.homing_restore_reject_reason`；DINT各一 | request提交后每拍低频读，status!=0才结束等待 | MAIN写接受1/拒绝状态与原因 | `D/main.cpp:1436-1455`、`P/MAIN.TcPOU:124-162` |
| `G.return_cmd[1/3/5/6].Busy/Done/Error/ErrorId` | 兼容plc_io read封装，缓存100ms；当前协调器主要用新快照+通知 | handle写回退状态 | `D/plc_io.cpp:394-437` |
| `G.return_cmd[1/3/5/6].Req/TargetAbs/Velocity/Acc/Dec/Jerk` | 兼容prepare/commit/clear封装；axis1组清Req一次清1/3/5/6 | 当前正常计划运动腿只有1/6，3/5符号不等于本轮它们在执行回退 | `D/plc_io.cpp:439-652`、`D/main.cpp:664-703` |
| `G.refer`、四电缸值、`G.startup_smoothing_bypass`、`G.axis4_fwd_req/rev_req` | 存在低频写封装；main正常每拍输出走快通道，启动/同步/退出可调用封装 | handle与自检/停稳对端同上 | `D/plc_io.cpp:330-385,664-669`；当前调用关系见B.2/B.11 |
| `G.Act_pos`、`G.init_pos`、`G.leftlimit`、四轴ActVelo、四力输入 | plc_io保留无ads_service时直连read_sum分支；生产main绑定服务后读最新快照 | 兼容路径不另构成生产100Hz线程 | `D/plc_io.cpp:197-327`、`D/main.cpp:197-206` |

### A.12.5 定位臂慢通道：全部符号族

**已确认**：专用worker目标50ms，dirty改变才写；读状态每轮进行。5轴数组是PLC的1..5。PLC读写方为ArmManual/ArmCartesian/ArmCartJog及它们的FB。下面列出动态生成的Power/Reset字段，避免只列一个总称遗漏实际符号。

| 符号 | 数据端读写方/时机 | PLC侧职责 | 证据 |
|---|---|---|---|
| `G.arm_manual_enable`、`G.arm_enable_req`、`G.arm_reset_req`、`G.arm_jog_pos_req`、`G.arm_jog_neg_req` | worker根据desired_* dirty写；读manual/enable回显；Reset成功后清本地脉冲 | ArmManual读请求，做Power/Reset/MoveVelocity/Stop，host timeout时清方向 | `D/arm_manual_ads_service.cpp:19-27,621-633,648-655,678-679`、`P/ArmManual.TcPOU:30-254` |
| `G.arm_jog_velocity`、`G.arm_jog_acc`、`G.arm_jog_dec`、`G.arm_jog_jerk` | 参数dirty时写5维数组；每次snapshot读回；速度先于方向项加入sum | ArmManual和Cartesian读取运动参数 | `D/arm_manual_ads_service.cpp:624-633,680-683` |
| `G.arm_act_pos`、`G.arm_act_vel`、`G.arm_motion_busy`、`G.arm_motion_done`、`G.arm_motion_error`、`G.arm_motion_error_id`、`G.arm_cmd_dir`、`G.arm_cmd_conflict` | worker每50ms读 | ArmManual/NC每扫描更新 | `D/arm_manual_ads_service.cpp:28-35,684-691`、`P/ArmManual.TcPOU:30-254` |
| `G.arm_power_output[1..5].Done/Busy/Active/Error/ErrorID`、`G.arm_reset_output[1..5].Done/Busy/Active/Error/ErrorID` | 每次snapshot动态展开50个BOOL/UDINT读项 | ArmManual的MC_Power/MC_Reset输出 | `D/arm_manual_ads_service.cpp:37-84,703-715` |
| `G.arm_curve_data`（17×LREAL）、`G.arm_curve_cancel`、`G.arm_curve_request`、`G.arm_curve_heartbeat` | 程序归零先data+cancel=false，成功后单独request；运行每轮heartbeat递增；abort写cancel=true | ArmCartesian在request变化时锁存曲线，500ms监测heartbeat | `D/arm_manual_ads_service.cpp:319-321,348-350,383-398`、`P/ArmCartesian.TcPOU:46-269` |
| `G.arm_curve_ack`、`G.arm_curve_state`、`G.arm_curve_error`、`G.arm_curve_owned`、`G.arm_curve_progress` | 每50ms读，推进请求ACK/执行/完成/错误 | ArmCartesian每扫描写 | `D/arm_manual_ads_service.cpp:353-364,692-696`、`P/ArmCartesian.TcPOU:46-269` |
| `G.arm_cjog_mode`、`G.arm_cjog_dir`、`G.arm_cjog_speed`、`G.arm_cjog_alive` | dirty时一起写；mode非0则alive递增；输入租约到期清mode/dir/speed | ArmCartJog读、500ms保活；ArmCartJogGen生成p/v/a | `D/arm_manual_ads_service.cpp:483-508,608-638`、`P/ArmCartJog.TcPOU:29-146` |
| `G.arm_cjog_owned`、`G.arm_cjog_state`、`G.arm_cjog_error`、`G.arm_cjog_limited`、`G.arm_cjog_path_speed`、`G.arm_cjog_if_version` | 每50ms读；if_version期望1，仅日志警告不作为强制版本闸门 | ArmCartJog更新状态/所有权与诊断 | `D/arm_manual_ads_service.cpp:697-724`、`P/ArmCartJog.TcPOU:29-146` |

**已确认（边界）**：`plc_io.cpp`还声明`G.act_pos_from_left`、`G.refer_from_left`、`G.cylinder5_cmd`、`G.cylinder5_value`等名字，但当前生产C++未见这些常量的实际ADS读写调用；不能把符号声明等同接口正在使用。声明证据`D/plc_io.cpp:14-53`，实际封装为`D/plc_io.cpp:197-677`。完整PLC所有150项变量、初始化值及PLC读写索引在C.6；其中包含数据端当前不访问的内部状态。

## A.13 数据端运行时配置、PLC默认值和记录器

### A.13.1 配置来源

| 配置 | 默认/内容 | 是否UI/文件可改 | 使用位置 |
|---|---|---|---|
| `ControlConfig` | 全部控制参数在`control_types.h:78-201`构造默认值；包括手柄缩放/符号、力反馈比例、B6、窗口/g、夹爪等待、lead、spacing recovery、回退速度/握手超时、滤波、启动目标。完整字段范围以该区间为准；关键值另列于C/D。 | 当前生产main直接构造；无运行时配置文件或当前WPF入口。 | `D/main.cpp:37-126,874-896`、`D/control_types.h:78-201`。 |
| `CylinderPreset` | 四电缸开/夹目标来自当前接线；结构体默认见`control_types.h:203-218`。 | 源码常量；UI只发开/关命令，不编辑数值。 | `D/main.cpp:841-859`、`D/control_types.h:203-218`。 |
| 力标定 | raw/volt斜率、解耦矩阵、安装增益、力/扭矩上限、死区、重力补偿开关等。 | 源码默认；动态取零状态在运行中产生。 | `D/force_calibration.h:4-64`、`D/main.cpp:759-804`。 |
| tracking参数 | Axis1/Axis6 Kp=.5、Ki=2、max_gain=2、max_error=20；数据端保留按字段校验的内部VisCommand处理。 | 当前公共Zenoh枚举及WPF命令注册没有tracking入口；不能把保留处理分支当作操作者可用配置。 | `D/delivery_tracking.h:29-50`、`D/delivery_tracking.cpp:28-82`、`D/control_messages.h:144-199`、`D/protocol/cpp/zenoh_protocol.h:14-24`。 |
| homing reuse | exe旁`reliability_config.json`当前为`{"homing_reuse_enabled": false}`；严格解析，要求应用名、完成标志和绝对位置容差。 | 文件可改；当前主端没有该编辑入口。 | `D/reliability_config.json:1`、`D/homing_record.h:39-95`、`D/main.cpp:66-69`。 |
| ZenohServiceConfig | endpoint=`tcp/192.168.60.1:7447`、token_path=`remote.token`、router_path空、start_router=true；startup_g=13、axis6 soft limit=660。 | main用默认构造，并复制cfg的g/soft limit后调用`zenoh_service.start(remote_cfg)`。 | `D/zenoh_service.h:17-25`、`D/main.cpp:106-115`；完整字段见A.15.5。 |
| ADS连接 | CADSComm构造默认20ms；快服务连接阶段改1000ms、运行改300ms；完整成功硬窗1s，1s重连。优先本地AMS，失败回退`169.254.119.135.1.1:851`，本地也固定端口851。 | 源码常量；main:62的hardcoded_ads_netid未被调用使用；main:98的两个nullptr只是坐标缓存初始化参数。 | `D/ADSComm.cpp:10-13,815-831,872-911`、`D/ads_communication.cpp:1015-1067,2072-2075`、`D/ads_communication.h:180`。 |
| 记录器 | 默认启动名`auto`；记录器启动失败只记错误，main继续。 | 无UI配置。 | `D/main.cpp:953-964`。 |

### A.13.2 实验记录事实

**已确认**：main在进入控制循环前调用`experiment_recorder.start("auto",...)`。`auto`不是特殊实验逻辑，只是raw/cleaned会话名；session JSON实际键是`experiment_name_raw`和`experiment_name_cleaned`，没有单独的`experiment_name`。来源：`D/main.cpp:953-960`、`D/experiment_recorder.cpp:399-446,901-905`。

| 文件 | 默认频率 | 列/内容 | 存放位置 |
|---|---:|---|---|
| `force.csv` | 100Hz | 22列：索引、elapsed_us、valid标志、四路raw/zero/clean force/torque、两侧力输出和hold。 | `records/YYYYMMDD/HHMMSS_auto[_suffix]/force.csv` |
| `motion.csv` | 50Hz | 12列：索引、elapsed_us、position_valid、轴1–7距左限位、两柄valid。主循环每隔偶数ADS序号记录。 | 同上 |
| `events.csv` | 按事件 | event_index、elapsed_us、event_code、value。 | 同上 |
| `session.json` | 会话结束写汇总 | schema8、UTC/QPC、force/motion目标Hz、serial、单柄模式、writer dropped、schedule missed、ADS metrics/RTT/age等。metrics或基线不可用时相应值为null。 | 同上 |
| `handle_velocity_<GetTickCount64>.csv` | 100Hz | 13列：elapsed_ms、rx582_ms、valid582、rx587_ms、valid587；SDK接收速度4列；相邻关节样本差分/接收间隔得到的关节变化率4列，SDK速度单位未在代码注明。 | exe旁目录；`D/main.cpp:66-69,87-91`、`D/handle_velocity_recorder.h:22-40,64-94`。 |

**已确认**：`create_session_paths`使用`GetCurrentDirectoryW`，所以源码不能保证“exe旁”；在默认`build-and-run.ps1`中工作目录是runtime目录，直接从其它cwd启动时records会落在其它cwd。`D/experiment_recorder.cpp:399-446`、`B数据端/工程/build-and-run.ps1:27-34`。**未知**：现场是否有外部启动器改变cwd。

### A.13.3 CSV完整列名

以下字段顺序/拼写均为**已确认**，保持CSV原名。schema8的当前头与历史schema6/7不完全相同；不把新列误当历史文件已经记录。实验force/motion使用记录快照QPC换算elapsed_us，手柄独立采样使用GetTickCount64的elapsed_ms；它们不是main整拍周期计时。出处：`D/experiment_recorder.cpp:17-31`、`D/main.cpp:4608-4680`、`D/handle_velocity_recorder.h:22-40,64-94`。

| 文件/位置 | 完整列名 |
|---|---|
| force，1–4 | `sample_index,elapsed_us,sample_valid,calibrated_valid` |
| force，5–10 | `fn_1_raw_v,ft_1_raw_v,fn_1_zero_v,ft_1_zero_v,clean_force_n,clean_handle_torque_nm` |
| force，11–16 | `fn_2_raw_v,ft_2_raw_v,fn_2_zero_v,ft_2_zero_v,clean_force_2_n,clean_handle_torque_2_nm` |
| force，17–22 | `ff_582_out_f,ff_582_out_n,ff_587_out_f,ff_587_out_n,hold_582_active,hold_587_active` |
| motion，1–12 | `sample_index,elapsed_us,position_valid,axis1_from_left_mm,axis2_from_left_mm,axis3_from_left_mm,axis4_from_left_mm,axis5_from_left_mm,axis6_from_left_mm,axis7_from_left_mm,axis1_handle_valid,axis6_handle_valid` |
| events，1–4 | `event_index,elapsed_us,event_code,value` |
| handle_velocity，1–5 | `elapsed_ms,rx582_ms,valid582,rx587_ms,valid587` |
| handle_velocity，6–13 | `sdk582_0,sdk582_1,sdk587_0,sdk587_1,joint582_0_per_s,joint582_1_per_s,joint587_0_per_s,joint587_1_per_s` |

**已确认**：session.json在启动和停止时均可写，只有final才填写结束UTC/stop_reason及完整汇总；表A.13.2中的“会话结束写汇总”不是只在退出才创建JSON。`D/experiment_recorder.cpp:546,602-611,884-907`。另有exe旁`ads_fault_diagnostics.log`按故障边沿追加，属于诊断事件日志，无固定Hz、不是上述CSV记录器；路径`D/main.cpp:45-53,1006-1060`。生成物可以因磁盘/路径失败而不可用，main只报错继续；`D/main.cpp:953-964`。

## A.14 仍缺的跨端工程事实

| 未知事实 | 当前能确认的边界 | 需要什么 |
|---|---|---|
| 当前现场运行的ADS.exe/PLC应用是否等于工作区源码 | 仓库有构建输出和Boot归档，但没有本轮生成的现场hash/下载记录。 | 设备端可执行文件hash、PLC AppName/Boot指纹、下载时间。 |
| ADS 100Hz快线程实际周期、RTT、快照缺失计数 | 记录器只保存force/motion采样，不保存每拍主循环起止；session中的RTT/failed counters在现有records为null。 | 成功连接的诊断或扩展记录。 |
| NC任务原值10000/50000单位 | XML原值已确认，按100ns解释只是推断。 | TwinCAT任务属性或在线系统设置。 |
| PLC POU是否为当前Boot产物 | 源码与Boot文件同时存在，未运行TwinCAT编译/下载。 | 编译产物hash、在线PLC项目/符号版本。 |

## A.15 数据端配置字段逐项表

下表是**已确认**的当前源码默认表达式与生产引用位置，逐项列出字段名/类型/默认值。默认表达式没有擅自换成现场配置；语义和单位可由名称、声明注释及使用处复核。引用索引按标识符词法匹配，包含读/写两种用法；不把这些位置都称为运行时入口。生产公开Zenoh枚举没有tracking/lead/spacing/内部启动参数命令，当前WPF也没有它们的编辑项；内部`VisCommand`分支仍保留。证据：`D/protocol/cpp/zenoh_protocol.h:14-24`、`D/control_messages.h:144-199`、`D/main.cpp:4870-5402`。

### A.15.1 `ControlConfig`（83个配置字段）

| 字段（类型） | 默认表达式 | 当前生产引用位置 |
|---|---|---|
| `ControlConfig.k_handle_to_mm` (`double`) | `500.0 * (75.0 / 50.0)`；`D/control_types.h:81` | 手柄线性位移差 -> 轴位移增量(mm)；`D/main.cpp:2100,2121` |
| `ControlConfig.axis_push_sign` (`double`) | `-1.0`；`D/control_types.h:82` | 手柄“推/拉”到轴“正/负”方向的映射符号；`D/main.cpp:2100,2121` |
| `ControlConfig.axis_rot_scale_deg` (`double`) | `-Rad`；`D/control_types.h:84` | `D/main.cpp:3081,3842,4117` |
| `ControlConfig.axial_force_axis` (`int`) | `1`；`D/control_types.h:87` | `D/force_feedback.cpp:135-136`；`D/main.cpp:166-167` |
| `ControlConfig.force_feedback_582_force_scale` (`double`) | `0.3`；`D/control_types.h:89` | 导丝582：力比例；`D/force_feedback.cpp:58` |
| `ControlConfig.force_feedback_582_torque_scale` (`double`) | `0.3`；`D/control_types.h:90` | 导丝582：扭矩比例；`D/force_feedback.cpp:35` |
| `ControlConfig.force_feedback_587_force_scale` (`double`) | `0.3`；`D/control_types.h:91` | 导管587：力比例；`D/force_feedback.cpp:61` |
| `ControlConfig.force_feedback_587_torque_scale` (`double`) | `0.3`；`D/control_types.h:92` | 导管587：扭矩比例；`D/force_feedback.cpp:36` |
| `ControlConfig.force_feedback_force_filter_alpha` (`double`) | `0.25`；`D/control_types.h:94` | `D/force_feedback.cpp:38-41` |
| `ControlConfig.btn_b6` (`unsigned char`) | `0x40`；`D/control_types.h:97` | `D/main.cpp:1506-1507` |
| `ControlConfig.axis1_window_left_from_left_mm` (`double`) | `5.0`；`D/control_types.h:100` | `D/motion_sync.cpp:28` |
| `ControlConfig.axis1_window_right_from_left_mm` (`double`) | `30.0`；`D/control_types.h:101` | `D/motion_sync.cpp:33` |
| `ControlConfig.catheter_axis6_window_size_mm` (`double`) | `32.0`；`D/control_types.h:104` | `D/motion_sync.cpp:39` |
| `ControlConfig.catheter_axis6_window_min_gap_from_axis5_mm` (`double`) | `1.0`；`D/control_types.h:105` | `D/motion_sync.cpp:62`；`D/main.cpp:3244,3265,3645,3727` |
| `ControlConfig.guidewire_axis6_window_size_mm` (`double`) | `32.0`；`D/control_types.h:106` | `D/motion_sync.cpp:39` |
| `ControlConfig.guidewire_axis6_window_min_gap_from_axis5_mm` (`double`) | `1.0`；`D/control_types.h:107` | `D/motion_sync.cpp:61` |
| `ControlConfig.axis6_active_delivery_gap_mm` (`double`) | `28.0`；`D/control_types.h:109` | `D/main.cpp:3651,3903` |
| `ControlConfig.axis6_active_retraction_gap_mm` (`double`) | `6.0`；`D/control_types.h:110` | `D/main.cpp:3651,3903` |
| `ControlConfig.axis6_passive_clearance_mm` (`double`) | `3.0`；`D/control_types.h:112` | `D/main.cpp:3649,3901` |
| `ControlConfig.axis6_passive_distance_limit_mm` (`double`) | `27.0`；`D/control_types.h:113` | `D/main.cpp:3649,3900` |
| `ControlConfig.transaction_merge_eps_mm` (`double`) | `5.0`；`D/control_types.h:115` | `D/main.cpp:3729` |
| `ControlConfig.cyl1_release_wait_ms` (`DWORD`) | `100`；`D/control_types.h:117` | `D/main.cpp:2556` |
| `ControlConfig.cyl1_clamp_wait_ms` (`DWORD`) | `150`；`D/control_types.h:118` | `D/main.cpp:2535` |
| `ControlConfig.cyl2_release_wait_ms` (`DWORD`) | `500`；`D/control_types.h:119` | `D/main.cpp:2542` |
| `ControlConfig.cyl2_clamp_wait_ms` (`DWORD`) | `150`；`D/control_types.h:120` | `D/main.cpp:2549` |
| `ControlConfig.cyl3_release_wait_ms` (`DWORD`) | `100`；`D/control_types.h:121` | `D/main.cpp:2558` |
| `ControlConfig.cyl3_clamp_wait_ms` (`DWORD`) | `150`；`D/control_types.h:122` | `D/main.cpp:2537` |
| `ControlConfig.cyl4_release_wait_ms` (`DWORD`) | `500`；`D/control_types.h:123` | `D/main.cpp:2544` |
| `ControlConfig.cyl4_clamp_wait_ms` (`DWORD`) | `150`；`D/control_types.h:124` | `D/main.cpp:2551` |
| `ControlConfig.startup_g_mm` (`double`) | `13.0`；`D/control_types.h:126` | `D/zenoh_service.cpp:151,829,835`；`D/main.cpp:109,392,4897`；`D/zenoh_service.h:23` |
| `ControlConfig.axis56_ready_gap_mm` (`double`) | `15.0`；`D/control_types.h:128` | `D/main.cpp:3402,3571,3590` |
| `ControlConfig.axis3_delivery_stop_from_left_mm` (`double`) | `20.0`；`D/control_types.h:129` | `D/main.cpp:2117,3995,4329` |
| `ControlConfig.axis3_delivery_release_hysteresis_mm` (`double`) | `2.0`；`D/control_types.h:130` | `D/main.cpp:3995,4329` |
| `ControlConfig.guidewire_entry_axis6_from_left_max_mm` (`double`) | `667.0`；`D/control_types.h:131` | `D/main.cpp:1844,1850` |
| `ControlConfig.axis1_post_return_lead_mm` (`double`) | `2.0`；`D/control_types.h:134` | `D/main.cpp:355,1581,3790,4071,5246` |
| `ControlConfig.axis1_post_return_lead_limit_mm` (`double`) | `5.0`；`D/control_types.h:135` | `D/main.cpp:3790,4073,5239` |
| `ControlConfig.axis1_post_return_mapping_span_mm` (`double`) | `10.0`；`D/control_types.h:136` | `D/main.cpp:3789,3791,3793,3829,4060,4062` |
| `ControlConfig.axis6_soft_limit_from_left_mm` (`double`) | `660.0`；`D/control_types.h:139` | `D/zenoh_service.cpp:830`；`D/main.cpp:110,397,712,748,3222,3654,3785,4234,4894,5283`；`D/zenoh_service.h:24` |
| `ControlConfig.spacing_recovery_speed_limit_mm_s` (`double`) | `40.0`；`D/control_types.h:142` | `D/main.cpp:3293,3360` |
| `ControlConfig.spacing_recovery_axis3_max_from_left_mm` (`double`) | `650.0`；`D/control_types.h:143` | `D/main.cpp:3217` |
| `ControlConfig.spacing_recovery_axis5_max_from_left_mm` (`double`) | `665.0`；`D/control_types.h:144` | `D/main.cpp:3219` |
| `ControlConfig.spacing_recovery_axis6_max_from_left_mm` (`double`) | `665.0`；`D/control_types.h:145` | `D/main.cpp:3221` |
| `ControlConfig.spacing_recovery_clamp_settle_ms` (`DWORD`) | `150`；`D/control_types.h:146` | `D/main.cpp:3336` |
| `ControlConfig.spacing_recovery_exit_settle_ms` (`DWORD`) | `100`；`D/control_types.h:147` | `D/main.cpp:4307` |
| `ControlConfig.spacing_recovery_exit_timeout_ms` (`DWORD`) | `2000`；`D/control_types.h:148` | `D/main.cpp:4309` |
| `ControlConfig.spacing_recovery_cyl4_release` (`unsigned short`) | `100`；`D/control_types.h:149` | `D/main.cpp:3328` |
| `ControlConfig.spacing_recovery_button_mask` (`unsigned char`) | `0x07`；`D/control_types.h:151` | `D/main.cpp:1509` |
| `ControlConfig.crawl_arrive_tol_mm` (`double`) | `0.2`；`D/control_types.h:154` | `D/motion_sync.cpp:86,144,199,218,316,332,393,469`；`D/guidewire_mode.cpp:100`；`D/main.cpp:2117,2312,2393,2417,3164-3165,3170-3171,3264-3266,3271,3311-3313,3375,3425-3428,3466-3468,3470-3473,3572-3574,3726,3947-3948,4155-4158,4160-4161,4303-4305` |
| `ControlConfig.handoff_stop_velocity_mm_s` (`double`) | `0.5`；`D/control_types.h:155` | `D/main.cpp:3889` |
| `ControlConfig.linear_increment_noise_deadband_mm` (`double`) | `0.02`；`D/control_types.h:157` | `D/main.cpp:2102,2123` |
| `ControlConfig.reverse_switch_trigger_guard_mm` (`double`) | `2.0`；`D/control_types.h:159` | `D/main.cpp:1675,1701,4143,4149` |
| `ControlConfig.axis1_return_velocity_mm_s` (`double`) | `110.0`；`D/control_types.h:162` | `D/main.cpp:600` |
| `ControlConfig.axis1_return_acc_mm_s2` (`double`) | `4800.0`；`D/control_types.h:163` | `D/main.cpp:602` |
| `ControlConfig.axis1_return_dec_mm_s2` (`double`) | `4800.0`；`D/control_types.h:164` | `D/main.cpp:604` |
| `ControlConfig.axis1_return_jerk_mm_s3` (`double`) | `70000.0`；`D/control_types.h:165` | `D/main.cpp:606` |
| `ControlConfig.axis6_return_velocity_mm_s` (`double`) | `110.0`；`D/control_types.h:166` | 与轴1统一回退速度，便于现场对照；`D/main.cpp:600` |
| `ControlConfig.axis6_return_acc_mm_s2` (`double`) | `4800.0`；`D/control_types.h:167` | `D/main.cpp:602` |
| `ControlConfig.axis6_return_dec_mm_s2` (`double`) | `4800.0`；`D/control_types.h:168` | `D/main.cpp:604` |
| `ControlConfig.axis6_return_jerk_mm_s3` (`double`) | `70000.0`；`D/control_types.h:169` | `D/main.cpp:606` |
| `ControlConfig.planned_return_ack_timeout_ms` (`DWORD`) | `250`；`D/control_types.h:171` | `D/main.cpp:654,2811,2859,2879` |
| `ControlConfig.planned_return_retry_clear_timeout_ms` (`DWORD`) | `500`；`D/control_types.h:172` | `D/main.cpp:2992` |
| `ControlConfig.planned_return_execution_timeout_ms` (`DWORD`) | `5000`；`D/control_types.h:173` | `D/main.cpp:2904` |
| `ControlConfig.linear_handle_alpha` (`double`) | `0.25`；`D/control_types.h:176` | `D/main.cpp:1464,1469` |
| `ControlConfig.rotational_handle_alpha` (`double`) | `0.20`；`D/control_types.h:177` | `D/main.cpp:1465,1470` |
| `ControlConfig.startup_clamp_settle_delay_ms` (`DWORD`) | `300`；`D/control_types.h:179` | `D/main.cpp:3557,3591,3621` |
| `ControlConfig.startup_recovery_stage_delay_ms` (`DWORD`) | `2000`；`D/control_types.h:180` | `D/main.cpp:3523,3533` |
| `ControlConfig.startup_motion_speed_scale` (`double`) | `0.005`；`D/control_types.h:181` | `D/startup_sequence.cpp:155-160`；`D/main.cpp:445-447,935,5305` |
| `ControlConfig.startup_cyl3_open` (`unsigned short`) | `500`；`D/control_types.h:182` | `D/main.cpp:3555,3567` |
| `ControlConfig.startup_cyl4_open` (`unsigned short`) | `5`；`D/control_types.h:183` | `D/main.cpp:3556,3568` |
| `ControlConfig.startup_cyl3_clamp` (`unsigned short`) | `0`；`D/control_types.h:184` | `D/main.cpp:3587,3605,3618` |
| `ControlConfig.startup_cyl4_clamp` (`unsigned short`) | `1000`；`D/control_types.h:185` | `D/main.cpp:3588,3606,3619` |
| `ControlConfig.startup_loading_axis1_from_left_mm` (`double`) | `96.0`；`D/control_types.h:187` | `D/startup_sequence.cpp:31` |
| `ControlConfig.startup_loading_axis3_from_left_mm` (`double`) | `500.0`；`D/control_types.h:188` | `D/startup_sequence.cpp:32`；`D/control_types.h:194` |
| `ControlConfig.startup_loading_axis5_from_left_mm` (`double`) | `530.0`；`D/control_types.h:189` | `D/startup_sequence.cpp:33` |
| `ControlConfig.startup_loading_axis6_from_left_mm` (`double`) | `580.0`；`D/control_types.h:190` | `D/startup_sequence.cpp:34` |
| `ControlConfig.startup_loading_pose_tolerance_mm` (`double`) | `2.0`；`D/control_types.h:191` | `D/startup_sequence.cpp:26` |
| `ControlConfig.startup_axis1_ready_from_left_mm` (`double`) | `28.0`；`D/control_types.h:193` | `D/main.cpp:3400,3569` |
| `ControlConfig.startup_axis5_ready_from_left_mm` (`double`) | `startup_loading_axis3_from_left_mm + 10.0`；`D/control_types.h:194` | `D/main.cpp:3401-3402,3570-3571,3589-3590` |
| `ControlConfig.startup_final_axis1_default_from_left_mm` (`double`) | `28.0`；`D/control_types.h:196` | `D/main.cpp:291` |
| `ControlConfig.startup_final_axis3_default_from_left_mm` (`double`) | `610.0`；`D/control_types.h:197` | `D/main.cpp:292` |
| `ControlConfig.startup_final_axis5_default_from_left_mm` (`double`) | `615.0`；`D/control_types.h:198` | `D/main.cpp:293` |
| `ControlConfig.startup_final_axis6_default_from_left_mm` (`double`) | `628.0`；`D/control_types.h:199` | = axis5(615) + startup_g_mm(13)；`D/main.cpp:294` |
| `ControlConfig.startup_rot_arrive_tol_deg` (`double`) | `0.2`；`D/control_types.h:200` | `D/main.cpp:3429-3430,3474-3475` |

### A.15.2 `CylinderPreset`（10个配置字段）

| 字段（类型） | 默认表达式 | 当前生产引用位置 |
|---|---|---|
| `CylinderPreset.cyl1_open` (`unsigned short`) | `450`；`D/control_types.h:208` | `D/startup_sequence.cpp:42`；`D/main.cpp:2042,2155,2595,2627,3110,3325,3514,3553,3565,3585,3603,3616,3847,4252,4394,4517` |
| `CylinderPreset.cyl1_clamp` (`unsigned short`) | `5`；`D/control_types.h:209` | `D/main.cpp:2595,4394` |
| `CylinderPreset.cyl2_open` (`unsigned short`) | `5`；`D/control_types.h:210` | `D/main.cpp:2597,3554,3566,3586,3604,4395` |
| `CylinderPreset.cyl2_clamp` (`unsigned short`) | `1000`；`D/control_types.h:211` | `D/startup_sequence.cpp:43`；`D/main.cpp:2043,2155,2597,2628,3111,3326,3515,3617,3848,4252,4395,4517` |
| `CylinderPreset.cyl3_open` (`unsigned short`) | `550`；`D/control_types.h:212` | `D/startup_sequence.cpp:44`；`D/main.cpp:2612,2634,3112,3153,3327,3516,3849,4254,4396,4521` |
| `CylinderPreset.cyl3_clamp` (`unsigned short`) | `50`；`D/control_types.h:213` | `D/main.cpp:2612,4396` |
| `CylinderPreset.cyl4_open` (`unsigned short`) | `5`；`D/control_types.h:214` | `D/main.cpp:2614,4397` |
| `CylinderPreset.cyl4_clamp` (`unsigned short`) | `1000`；`D/control_types.h:215` | `D/startup_sequence.cpp:45`；`D/main.cpp:2614,2635,3113,3154,3517,3850,4133,4254,4397,4521` |
| `CylinderPreset.cyl3_follow_release` (`unsigned short`) | `400`；`D/control_types.h:216` | `D/main.cpp:2044,4132` |
| `CylinderPreset.cyl4_follow_release` (`unsigned short`) | `100`；`D/control_types.h:217` | `D/main.cpp:2045` |

### A.15.3 `ForceCalibrationConfig`（25个配置字段）

| 字段（类型） | 默认表达式 | 当前生产引用位置 |
|---|---|---|
| `ForceCalibrationConfig.axial_direct_n_per_v` (`double`) | `force_direct_calibration::fn_1_slope_n_per_volt`；`D/force_calibration.h:30` | `D/force_calibration.h:109,208` |
| `ForceCalibrationConfig.tangential_direct_n_per_v` (`double`) | `force_direct_calibration::ft_1_slope_n_per_volt`；`D/force_calibration.h:31` | `D/force_calibration.h:111,209` |
| `ForceCalibrationConfig.guidewire_axial_direct_n_per_v` (`double`) | `force_direct_calibration::fn_2_slope_n_per_volt`；`D/force_calibration.h:33` | `D/force_calibration.h:128,229` |
| `ForceCalibrationConfig.guidewire_tangential_direct_n_per_v` (`double`) | `force_direct_calibration::ft_2_slope_n_per_volt`；`D/force_calibration.h:34` | `D/force_calibration.h:130,230` |
| `ForceCalibrationConfig.decouple_ff` (`double`) | `0.996063`；`D/force_calibration.h:36` | 当前生产代码未查到声明外引用 |
| `ForceCalibrationConfig.decouple_ft` (`double`) | `0.0`；`D/force_calibration.h:37` | 轴向力反馈只由轴向力通道决定，避免扭矩通道串入轴向力。；当前生产代码未查到声明外引用 |
| `ForceCalibrationConfig.decouple_tf` (`double`) | `-0.103597`；`D/force_calibration.h:38` | 当前生产代码未查到声明外引用 |
| `ForceCalibrationConfig.decouple_tt` (`double`) | `0.996063`；`D/force_calibration.h:39` | 当前生产代码未查到声明外引用 |
| `ForceCalibrationConfig.installed_axial_gain` (`double`) | `1.80`；`D/force_calibration.h:42` | `D/force_calibration.h:92` |
| `ForceCalibrationConfig.installed_axial_offset_n` (`double`) | `0.183108`；`D/force_calibration.h:43` | 当前生产代码未查到声明外引用 |
| `ForceCalibrationConfig.installed_torque_nmm_per_n` (`double`) | `18.440851`；`D/force_calibration.h:44` | `D/force_calibration.h:93` |
| `ForceCalibrationConfig.installed_torque_offset_nmm` (`double`) | `0.020906`；`D/force_calibration.h:45` | 当前生产代码未查到声明外引用 |
| `ForceCalibrationConfig.gravity_comp_enabled` (`bool`) | `false`；`D/force_calibration.h:48` | `D/main.cpp:4761,5373,5375`；`D/force_calibration.h:154`；`D/control_messages.h:44` |
| `ForceCalibrationConfig.gravity_comp_validated` (`bool`) | `false`；`D/force_calibration.h:49` | `D/main.cpp:5367-5368`；`D/force_calibration.h:154` |
| `ForceCalibrationConfig.grav_ft_a` (`double`) | `0.4983`；`D/force_calibration.h:50` | `D/force_calibration.h:167` |
| `ForceCalibrationConfig.grav_ft_phi_deg` (`double`) | `-53.54`；`D/force_calibration.h:51` | `D/force_calibration.h:163` |
| `ForceCalibrationConfig.grav_ft_offset` (`double`) | `-0.1119`；`D/force_calibration.h:52` | `D/force_calibration.h:167` |
| `ForceCalibrationConfig.grav_fa_a` (`double`) | `0.3195`；`D/force_calibration.h:53` | `D/force_calibration.h:170` |
| `ForceCalibrationConfig.grav_fa_phi_deg` (`double`) | `48.42`；`D/force_calibration.h:54` | `D/force_calibration.h:164` |
| `ForceCalibrationConfig.grav_fa_offset` (`double`) | `-0.1861`；`D/force_calibration.h:55` | `D/force_calibration.h:170` |
| `ForceCalibrationConfig.rot_counts_per_deg` (`double`) | `2150.0`；`D/force_calibration.h:56` | 当前生产代码未查到声明外引用 |
| `ForceCalibrationConfig.f_max_n` (`double`) | `force_direct_calibration::operational_force_limit_n`；`D/force_calibration.h:59` | `D/force_calibration.h:188-189` |
| `ForceCalibrationConfig.t_max_nm` (`double`) | `force_direct_calibration::operational_handle_torque_limit_nm`；`D/force_calibration.h:60` | `D/force_calibration.h:190-191` |
| `ForceCalibrationConfig.deadband_f_n` (`double`) | `0.02`；`D/force_calibration.h:61` | `D/force_calibration.h:185` |
| `ForceCalibrationConfig.deadband_t_nm` (`double`) | `0.00001`；`D/force_calibration.h:63` | `D/force_calibration.h:186` |

### A.15.4 `DeliveryTrackingParameters`（4个配置字段）

| 字段（类型） | 默认表达式 | 当前生产引用位置 |
|---|---|---|
| `DeliveryTrackingParameters.kp` (`double`) | `0.5`；`D/delivery_tracking.h:45` | `D/delivery_tracking.cpp:70,98,267` |
| `DeliveryTrackingParameters.ki` (`double`) | `2.0`；`D/delivery_tracking.h:46` | `D/delivery_tracking.cpp:71,249,268,316` |
| `DeliveryTrackingParameters.max_gain` (`double`) | `2.0`；`D/delivery_tracking.h:47` | `D/delivery_tracking.cpp:72,99,273` |
| `DeliveryTrackingParameters.max_error_mm` (`double`) | `20.0`；`D/delivery_tracking.h:49` | `D/delivery_tracking.cpp:74,99,215,262` |

### A.15.5 `ZenohServiceConfig`（6个配置字段）

| 字段（类型） | 默认表达式 | 当前生产引用位置 |
|---|---|---|
| `ZenohServiceConfig.endpoint` (`std::string`) | `"tcp/192.168.60.1:7447"`；`D/zenoh_service.h:19` | `D/zenoh_service.cpp:82,85-89,96,99,150,190,272,274,285,297,302,304,326-328` |
| `ZenohServiceConfig.token_path` (`std::string`) | `"remote.token"`；`D/zenoh_service.h:20` | `D/zenoh_service.cpp:153` |
| `ZenohServiceConfig.router_path` (`std::string`) | `std::string{}（空；无显式赋值）`；`D/zenoh_service.h:21` | `D/zenoh_service.cpp:277,279` |
| `ZenohServiceConfig.start_router` (`bool`) | `true`；`D/zenoh_service.h:22` | `D/zenoh_service.cpp:171` |
| `ZenohServiceConfig.startup_g_mm` (`double`) | `13.0`；`D/zenoh_service.h:23` | `D/zenoh_service.cpp:151,829,835`；`D/main.cpp:109,392,4897`；`D/control_types.h:126` |
| `ZenohServiceConfig.axis6_soft_limit_from_left_mm` (`double`) | `660.0`；`D/zenoh_service.h:24` | `D/zenoh_service.cpp:830`；`D/main.cpp:110,397,712,748,3222,3654,3785,4234,4894,5283`；`D/control_types.h:139` |

### A.15.6 力标定基础常量

全部为**已确认**；运行上限和标定验证上限是两个不同量，源文件注释说明5N运行上限超过0–50g单向实验验证范围。

| 常量 | 当前默认表达式和使用处 |
|---|---|
| `counts_per_volt` | `1000.0`；`D/force_calibration.h:7`；换算/去零在同文件`D/force_calibration.h:20-24,100-160`。 |
| `ft_1_slope_n_per_count` | `0.000703683250522`；`D/force_calibration.h:8`；换算/去零在同文件`D/force_calibration.h:20-24,100-160`。 |
| `fn_1_slope_n_per_count` | `0.000319519429`；`D/force_calibration.h:9`；换算/去零在同文件`D/force_calibration.h:20-24,100-160`。 |
| `fn_2_slope_n_per_count` | `0.001134444473`；`D/force_calibration.h:10`；换算/去零在同文件`D/force_calibration.h:20-24,100-160`。 |
| `ft_2_slope_n_per_count` | `0.000133373639`；`D/force_calibration.h:11`；换算/去零在同文件`D/force_calibration.h:20-24,100-160`。 |
| `ft_1_slope_n_per_volt` | `ft_1_slope_n_per_count * counts_per_volt`；`D/force_calibration.h:12`；换算/去零在同文件`D/force_calibration.h:20-24,100-160`。 |
| `fn_1_slope_n_per_volt` | `fn_1_slope_n_per_count * counts_per_volt`；`D/force_calibration.h:13`；换算/去零在同文件`D/force_calibration.h:20-24,100-160`。 |
| `fn_2_slope_n_per_volt` | `fn_2_slope_n_per_count * counts_per_volt`；`D/force_calibration.h:14`；换算/去零在同文件`D/force_calibration.h:20-24,100-160`。 |
| `ft_2_slope_n_per_volt` | `ft_2_slope_n_per_count * counts_per_volt`；`D/force_calibration.h:15`；换算/去零在同文件`D/force_calibration.h:20-24,100-160`。 |
| `max_validated_force_n` | `0.4903325`；`D/force_calibration.h:16`；换算/去零在同文件`D/force_calibration.h:20-24,100-160`。 |
| `operational_force_limit_n` | `5.0`；`D/force_calibration.h:17`；换算/去零在同文件`D/force_calibration.h:20-24,100-160`。 |
| `operational_handle_torque_limit_nm` | `20.0 * 0.001`；`D/force_calibration.h:18`；换算/去零在同文件`D/force_calibration.h:20-24,100-160`。 |

### A.15.7 运动学与程序归零默认值

**已确认**：库配置含标定、软限位和solver/home参数。`AxisCalibration`、速度/限位及HomeOptions加速度的NaN是未配置占位；生产调用在`plan_arm_curve`中用实际位置、PLC读回的速度和加速度构造，不存在`ArmKinematicConfig::Default()`入口。当前笛卡尔点动在PLC生成，数据端保留程序归零规划。

| 组 | 参数/默认值 | 来源与生产使用 |
|---|---|---|
| AxisCalibration | zero=NaN、scale=NaN、direction=0；生产构造为zero=0、scale=轴1 .001/其余π÷180、direction=1 | `K/include/positioning_arm/kinematics.h:15-19`、`D/arm_cartesian_motion.h:36-47` |
| 几何 | link_lengths=(.200,.180)m、lateral_offset=.4563m、tool_length=.5545m、base_offset_mm=(0,0,0) | `K/include/positioning_arm/kinematics.h:21-25`；生产默认构造 `D/arm_cartesian_motion.h:29` |
| 软限位/最大速度 | lower/upper/max_velocity五维均NaN；生产设轴1–4为±∞，轴5±60°；max_velocity由PLC快照jog_velocity换算 | `K/include/positioning_arm/kinematics.h:26-30`、`D/arm_cartesian_motion.h:30-47`、`D/arm_manual_ads_service.cpp:375-376` |
| SolverOptions | position_tolerance=1e-7、singularity_stop=1e-6、singularity_slow=.03、max_translation=.02、max_rotation=.1 | `K/include/positioning_arm/kinematics.h:33-39`；构造默认参数 `K/include/positioning_arm/kinematics.h:129-130` |
| HomeOptions | max_acceleration=4维NaN、sample_period=.02s、minimum_duration=1s、max_samples=20000、optimization_iterations=30；生产加速度用amax.tail<4>() | `K/include/positioning_arm/kinematics.h:72-81`、`D/arm_cartesian_motion.h:55-57` |
| 定位臂ADS方向/参数默认 | desired手动=false，enable/reset/jog均false；每轴velocity=5、acc/dec=50、jerk=500；speed_scale=1；Cartesian租约500ms/axis租约300ms | `D/arm_manual_ads_service.h:93-112`、`D/arm_manual_ads_service.cpp:13-17,601-642` |
| PLC几何/profile | L1=200、L2=180、offset=456.3、tool=554.5mm；acc1s、dec.5s、branch1、bisection12、joint caps24/30/30/30/30 | `P/ArmCartJogGen.TcPOU:22-33` |

## A.16 补充规模：当前编译源文件

**已确认**：下表从`ADS.vcxproj`的Compile项读取，只统计生产源、声明头和PLC当前Compile项；第三方SDK、build、历史镜像和离线测试不混入生产规模。main最长20项见A.10.1。

| 文件 | 物理行数/证据 |
|---|---|
| `D/ads_communication.cpp` | 2322；`D/ads_communication.cpp:1-2322` |
| `D/ads_communication.h` | 388；`D/ads_communication.h:1-388` |
| `D/ADSComm.cpp` | 1004；`D/ADSComm.cpp:1-1004` |
| `D/arm_cartesian_motion.h` | 73；`D/arm_cartesian_motion.h:1-73` |
| `D/arm_manual_ads_service.cpp` | 738；`D/arm_manual_ads_service.cpp:1-738` |
| `D/arm_manual_ads_service.h` | 131；`D/arm_manual_ads_service.h:1-131` |
| `D/async_csv_writer.h` | 218；`D/async_csv_writer.h:1-218` |
| `D/control_messages.h` | 210；`D/control_messages.h:1-210` |
| `D/control_types.cpp` | 100；`D/control_types.cpp:1-100` |
| `D/control_types.h` | 761；`D/control_types.h:1-761` |
| `D/delivery_tracking.cpp` | 476；`D/delivery_tracking.cpp:1-476` |
| `D/delivery_tracking.h` | 190；`D/delivery_tracking.h:1-190` |
| `D/experiment_recorder.cpp` | 1033；`D/experiment_recorder.cpp:1-1033` |
| `D/experiment_recorder.h` | 229；`D/experiment_recorder.h:1-229` |
| `D/force_calibration.h` | 235；`D/force_calibration.h:1-235` |
| `D/force_feedback.cpp` | 150；`D/force_feedback.cpp:1-150` |
| `D/force_feedback.h` | 19；`D/force_feedback.h:1-19` |
| `D/guidewire_mode.cpp` | 151；`D/guidewire_mode.cpp:1-151` |
| `D/guidewire_mode.h` | 15；`D/guidewire_mode.h:1-15` |
| `D/handle_net_distance.h` | 35；`D/handle_net_distance.h:1-35` |
| `D/handle_velocity_recorder.h` | 107；`D/handle_velocity_recorder.h:1-107` |
| `D/homing_record.h` | 97；`D/homing_record.h:1-97` |
| `D/main.cpp` | 5424；`D/main.cpp:1-5424` |
| `D/motion_sync.cpp` | 524；`D/motion_sync.cpp:1-524` |
| `D/motion_sync.h` | 51；`D/motion_sync.h:1-51` |
| `D/plc_io.cpp` | 677；`D/plc_io.cpp:1-677` |
| `D/plc_io.h` | 110；`D/plc_io.h:1-110` |
| `D/protocol/cpp/zenoh_protocol.h` | 131；`D/protocol/cpp/zenoh_protocol.h:1-131` |
| `D/return_recovery.h` | 70；`D/return_recovery.h:1-70` |
| `D/startup_sequence.cpp` | 261；`D/startup_sequence.cpp:1-261` |
| `D/startup_sequence.h` | 15；`D/startup_sequence.h:1-15` |
| `D/zenoh_service.cpp` | 1142；`D/zenoh_service.cpp:1-1142` |
| `D/zenoh_service.h` | 163；`D/zenoh_service.h:1-163` |
| `D/手柄/Handle.h` | 64；`D/手柄/Handle.h:1-64` |
| `D/手柄/remote_handle_bridge.h` | 125；`D/手柄/remote_handle_bridge.h:1-125` |
| `D/手柄/手柄.cpp` | 153；`D/手柄/手柄.cpp:1-153` |
| `K/include/positioning_arm/kinematics.h` | 167；`K/include/positioning_arm/kinematics.h:1-167` |
| `K/src/kinematics.cpp` | 401；`K/src/kinematics.cpp:1-401` |
| `K/src/program_zero.cpp` | 246；`K/src/program_zero.cpp:1-246` |
| `介入机器人/C从端/250902/Untitled2/DUTs/ST_AxisPlannedReturnCmd.TcDUT` | 19；`介入机器人/C从端/250902/Untitled2/DUTs/ST_AxisPlannedReturnCmd.TcDUT:1-19` |
| `介入机器人/C从端/250902/Untitled2/DUTs/state.TcDUT` | 21；`介入机器人/C从端/250902/Untitled2/DUTs/state.TcDUT:1-21` |
| `介入机器人/C从端/250902/Untitled2/GVLs/G.TcGVL` | 191；`介入机器人/C从端/250902/Untitled2/GVLs/G.TcGVL:1-191` |
| `介入机器人/C从端/250902/Untitled2/PlcTask.TcTTO` | 16；`介入机器人/C从端/250902/Untitled2/PlcTask.TcTTO:1-16` |
| `介入机器人/C从端/250902/Untitled2/POUs/ArmCartesian.TcPOU` | 272；`介入机器人/C从端/250902/Untitled2/POUs/ArmCartesian.TcPOU:1-272` |
| `介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU` | 149；`介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:1-149` |
| `介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU` | 219；`介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:1-219` |
| `介入机器人/C从端/250902/Untitled2/POUs/err.TcPOU` | 271；`介入机器人/C从端/250902/Untitled2/POUs/err.TcPOU:1-271` |
| `介入机器人/C从端/250902/Untitled2/POUs/ArmManual.TcPOU` | 257；`介入机器人/C从端/250902/Untitled2/POUs/ArmManual.TcPOU:1-257` |
| `介入机器人/C从端/250902/Untitled2/POUs/Axis2Sync.TcPOU` | 96；`介入机器人/C从端/250902/Untitled2/POUs/Axis2Sync.TcPOU:1-96` |
| `介入机器人/C从端/250902/Untitled2/POUs/handle.TcPOU` | 966；`介入机器人/C从端/250902/Untitled2/POUs/handle.TcPOU:1-966` |
| `介入机器人/C从端/250902/Untitled2/POUs/InjectorManual.TcPOU` | 112；`介入机器人/C从端/250902/Untitled2/POUs/InjectorManual.TcPOU:1-112` |
| `介入机器人/C从端/250902/Untitled2/POUs/clear_err.TcPOU` | 95；`介入机器人/C从端/250902/Untitled2/POUs/clear_err.TcPOU:1-95` |
| `介入机器人/C从端/250902/Untitled2/POUs/init.TcPOU` | 100；`介入机器人/C从端/250902/Untitled2/POUs/init.TcPOU:1-100` |
| `介入机器人/C从端/250902/Untitled2/POUs/MAIN.TcPOU` | 7076；`介入机器人/C从端/250902/Untitled2/POUs/MAIN.TcPOU:1-7076` |
| `介入机器人/C从端/250902/Untitled2/POUs/reset.TcPOU` | 87；`介入机器人/C从端/250902/Untitled2/POUs/reset.TcPOU:1-87` |
| `介入机器人/C从端/250902/Untitled2/POUs/SelfCheck.TcPOU` | 619；`介入机器人/C从端/250902/Untitled2/POUs/SelfCheck.TcPOU:1-619` |

## A.17 PLC构建和库的补充边界

**已确认**：TwinCAT系统工程为3.1.4024.67，主PLC工程ProgramVersion=3.1.4024.0；引用Tc2_MC2、Tc2_NC、Tc2_Standard、Tc2_System、Tc3_MC2_AdvancedHoming、Tc3_Module。Boot中有Port851产物，但未由本轮TwinCAT重建。证据：`介入机器人/C从端/250902/250902.tsproj:2,3594-3598`、`介入机器人/C从端/250902/Untitled2/Untitled2.plcproj:2-18,79-107`。

**已确认**：NC1..5→G.arm_axis[1..5]、NC6..12→G.axis[1..7]、NC13/14→G.inject_axis[1..2]、NC15→G.axis2_sync_axis。Box7力通道2/电缸3/4、Box8力通道1/电缸1/2/5。证据：`介入机器人/C从端/250902/250902.tsproj:7617-7628,7630-7688`。**未知**：真实接线和在线最大速度配置。

**已确认**：C从端README称没有PLC源码，与当前POUs、GVL和plcproj实际存在相矛盾；目录中handle备份和move_edge也不在当前Compile项。证据：`介入机器人/C从端/README.md:3-5`、`介入机器人/C从端/250902/Untitled2/Untitled2.plcproj:20-70`。


## A.18 数据端模块目录与固定常量

**已确认**：下列为当前数据端工程到主要文件的目录地图；非生产tests、SDK和输出单列。编译项依据`D/ADS.vcxproj:156-220`，库依据`K/CMakeLists.txt:1-51`。

| 目录（相对于`介入机器人/B数据端/工程/`） | 主要文件 | 用途/输出 |
|---|---|---|
| `ADS项目/` 入口/状态 | main.cpp、control_types.h/.cpp、control_messages.h、return_recovery.h、homing_record.h | 主循环、公共类型、内部命令和可靠性状态 |
| `ADS项目/` 通信 | ADSComm.cpp、ads_communication.h/.cpp、plc_io.h/.cpp、zenoh_service.h/.cpp | CADSComm底层；快/慢服务；PLC符号；Zenoh服务 |
| `ADS项目/protocol/cpp/` | zenoh_protocol.h | MessagePack/HMAC线格式 |
| `ADS项目/手柄/` | Handle.h、手柄.cpp、remote_handle_bridge.h | 远程手柄缓存、采样新鲜度/epoch/静置 |
| `ADS项目/` 运动业务 | motion_sync、guidewire_mode、startup_sequence、delivery_tracking各h/cpp；arm_manual_ads_service.h/.cpp、arm_cartesian_motion.h | 同步/导丝/启动/PI/定位臂 |
| `ADS项目/` 力/记录 | force_feedback.h/.cpp、force_calibration.h、experiment_recorder.h/.cpp、async_csv_writer.h、handle_velocity_recorder.h、handle_net_distance.h | 标定、触觉保持、记录和净距离 |
| `PositioningArmKinematics/include/positioning_arm/`、`src/` | kinematics.h、kinematics.cpp、program_zero.cpp | 数学库；ADS项目直接编译两个源 |
| `ADS项目/ADS/`、`PositioningArmKinematics/.deps/`、`Runtime/VC142/` | ADS SDK/库、Eigen、VC运行库 | 第三方依赖；实际复制见`D/ADS.vcxproj:222-225` |
| `ADS项目/tools/`、`tests/zenoh/`、`PositioningArmKinematics/tests/` | 原生抽取/协议/模型测试及runner | 离线测试，不是现场应用；清单与结果见F |
| `ADS项目/x64/Release`、`Debug` | ADS.exe及依赖DLL、可选token/reliability配置和日志 | 默认生产输出；本轮构建重定向scratch，未启动exe |

以下逐项为**已确认**的固定边界值/协议常量；属于源码配置，改值需重编译。与超时有关的到期动作/恢复条件见D（此处不省略数值）。引用的使用行由同文件标识符匹配给出，既可有读也可有声明；公式中间量不作为额外配置。

| 常量 | 当前表达式 | 声明/引用 |
|---|---|---|
| `kDefaultAdsTimeoutMs` | `20UL` | `D/ADSComm.cpp:10`；同文件使用 105。 |
| `kInitialSumItemCapacity` | `32U` | `D/ADSComm.cpp:11`；同文件使用 112,113。 |
| `kInitialSumByteCapacity` | `4096U` | `D/ADSComm.cpp:12`；同文件使用 114,115。 |
| `kMaxAdsSymbolNameLength` | `1024U` | `D/ADSComm.cpp:13`；同文件使用 69。 |
| `kCommunicationRateHz` | `100` | `D/ads_communication.cpp:15`；同文件使用 792。 |
| `kHardTimeoutMs` | `1000` | `D/ads_communication.cpp:16`；同文件使用 903,942。 |
| `kDeviceStateCheckPeriodCycles` | `10` | `D/ads_communication.cpp:17`；同文件使用 865。 |
| `kMaxStalledPlcSnapshots` | `3` | `D/ads_communication.cpp:18`；同文件使用 1525,1539。 |
| `kSnapshotQueueCapacity` | `2048` | `D/ads_communication.cpp:19`；同文件使用 2234。 |
| `kReconnectIntervalMs` | `1000` | `D/ads_communication.cpp:20`；同文件使用 843,2072,2098,2109。 |
| `kMaxRegistrationIdAttempts` | `65536` | `D/ads_communication.cpp:21`；同文件使用 81。 |
| `kPlannedReturnQueueCapacity` | `32` | `D/ads_communication.h:279`；同文件使用 375。 |
| `kPlannedReturnResultCapacity` | `128` | `D/ads_communication.h:280`；同文件使用 377。 |
| `kPlannedReturnFieldsPerAxis` | `6` | `D/ads_communication.h:281`；同文件使用 369。 |
| `kMaxPayloadBytes` | `1024 * 1024` | `D/zenoh_service.cpp:25`；同文件使用 554。 |
| `kMaxCommandQueue` | `256` | `D/zenoh_service.cpp:26`；同文件使用 462,746。 |
| `kMaxPendingTasks` | `8` | `D/zenoh_service.cpp:27`；同文件使用 787。 |
| `kCommandMaxAgeMs` | `2000` | `D/zenoh_service.cpp:28`；同文件使用 501。 |
| `kHoldRenewMs` | `100` | `D/zenoh_service.cpp:29`；同文件使用 674。 |
| `kStateFreshMs` | `500` | `D/zenoh_service.cpp:30`；同文件使用 768,1028。 |
| `kGripSettleMs` | `150` | `D/delivery_tracking.h:92`；同文件使用 声明外未检得字面引用。 |
| `kMaxUiKp` | `1.0` | `D/delivery_tracking.h:167`；同文件使用 声明外未检得字面引用。 |
| `kMaxUiKi` | `100.0` | `D/delivery_tracking.h:168`；同文件使用 声明外未检得字面引用。 |
| `kMaxUiGain` | `5.0` | `D/delivery_tracking.h:169`；同文件使用 声明外未检得字面引用。 |
| `kMaxUiErrorMm` | `20.0` | `D/delivery_tracking.h:170`；同文件使用 声明外未检得字面引用。 |
| `kMaxIntegralMmS` | `20.0` | `D/delivery_tracking.h:171`；同文件使用 声明外未检得字面引用。 |
| `kMaxCompensationStepMm` | `0.25` | `D/delivery_tracking.h:172`；同文件使用 声明外未检得字面引用。 |
| `kIncrementEpsilonMm` | `1e-9` | `D/delivery_tracking.cpp:12`；同文件使用 204,219,233,244,266,297,306,312,454。 |
| `blend_duration_ms` | `300` | `D/force_feedback.cpp:21`；同文件使用 89。 |
| `kAxisReturnStatusCacheMs` | `100` | `D/plc_io.cpp:111`；同文件使用 411。 |
| `clear_interval_ms` | `250` | `D/return_recovery.h:17`；同文件使用 34。 |
| `serial_axis1_handle` | `587` | `D/main.cpp:54`；同文件使用 80,144,954,1122,1170。 |
| `serial_axis6_handle` | `582` | `D/main.cpp:55`；同文件使用 81,144,955,1130,1183。 |
| `ads_snapshot_wait_timeout_ms` | `300` | `D/main.cpp:57`；同文件使用 970。 |
| `vis_publish_rate_hz` | `30` | `D/main.cpp:59`；同文件使用 4685。 |
| `axis4_coupled_axis6_speed_mm_s` | `7.8016217564` | `D/main.cpp:61`；同文件使用 3769,3976。 |
| `remote_single_handle_grace_ms` | `3000` | `D/main.cpp:124`；同文件使用 1098。 |
| `manual_jog_lease_ms` | `500` | `D/main.cpp:513`；同文件使用 5173,5188。 |
| `kHandleResumeSettleMs` | `500` | `D/手柄/Handle.h:6`；同文件使用 声明外未检得字面引用。 |
| `kRemoteMaxAgeMs` | `5000` | `D/手柄/手柄.cpp:6`；同文件使用 29,52。 |
| `kSerialA` | `582` | `D/手柄/remote_handle_bridge.h:15`；同文件使用 20。 |
| `kSerialB` | `587` | `D/手柄/remote_handle_bridge.h:16`；同文件使用 21。 |
| `kVersion` | `1` | `D/protocol/cpp/zenoh_protocol.h:10`；同文件使用 声明外未检得字面引用。 |
| `kHandleFields` | `26` | `D/protocol/cpp/zenoh_protocol.h:15`；同文件使用 声明外未检得字面引用。 |
| `kCommandFields` | `15` | `D/protocol/cpp/zenoh_protocol.h:16`；同文件使用 声明外未检得字面引用。 |
| `kCommandResultFields` | `3` | `D/protocol/cpp/zenoh_protocol.h:17`；同文件使用 声明外未检得字面引用。 |
| `kStateFields` | `57` | `D/protocol/cpp/zenoh_protocol.h:18`；同文件使用 声明外未检得字面引用。 |
| `kForceFields` | `13` | `D/protocol/cpp/zenoh_protocol.h:19`；同文件使用 声明外未检得字面引用。 |
| `kMaxResults` | `32` | `D/protocol/cpp/zenoh_protocol.h:20`；同文件使用 声明外未检得字面引用。 |


**已确认**：未采用命名常量的固定控制阈值也参与运行：力保持捕获100ms、释放2mm或400ms、blend300ms、换手锁存用力/扭矩平滑时间常数100ms（实时扭矩直接输出，不经过该平滑）（`D/control_types.h:540-603`、`D/force_feedback.cpp:21,65-66,93-110`）；屈曲Active dt封顶50ms（`D/main.cpp:3349-3360`）；程序归零snapshot年龄200ms与ACK2s（`D/arm_manual_ads_service.cpp:333-355`）。PLC几何/profile常量见A.15.7；GVL初值和其余POU运动参数、TON等待见C.4/C.6、D.5，均有逐项PLC源码出处。

**未知**：仓库外配置、PLC在线改参和现场启动参数无法由磁盘源码穷尽；本附件没有把软件默认值替代设备实值。具体缺证据项见99。

## A.19 公共命令参数与界面档位

**已确认**：A.5给出所有字段/类型，下面补齐Param1/2、Value1/2的逐命令意义。未使用参数由当前主端发送0/空，不代表数据端对每个未使用字段都强制0。所有业务命令仍受认证/序号、state新鲜度、lease门控；Release豁免state新鲜度。出处：`RemoteRobotLink.cs:224-271`、`D/zenoh_service.cpp:738-798`。

| 线命令/编号 | 参数与当前实际作用 | 直接出处（均已确认） |
|---|---|---|
| Hello/0 | SessionId=0，NonceClient16字节，其它nonce/proof/tag为空；单独建立挑战，不撤销有效控制权 | `D/zenoh_service.cpp:688-703` |
| Authenticate/1 | 挑战SessionId、两nonce各16字节、Proof32字节；验证后替换会话，清旧队列/控制权 | `D/zenoh_service.cpp:705-735`、`FrameAuth.cs:20-30` |
| Acquire/2 | 无业务参数；消费时持有控制权并回结果，无断流自动expiry | `D/zenoh_service.cpp:771-777,962-980` |
| Release/3 | 无业务参数；释放控制权、清持续方向/力和关联动作 | `D/zenoh_service.cpp:779-784,962-980` |
| PreparePosition/4 | Value1导管距左mm、Value2导丝/Y阀基准mm；范围5..95、10..639；内部转换axis1=Value1、axis3=Value2、axis5=Value2+5、axis6=Value2+5+g，再发自检 | `D/zenoh_service.cpp:816-838`、`RemoteRobotLink.cs:240-241` |
| StartControl/5 | 无业务参数；内部SelectDirectControl，成功在当前位置同步后控制 | `D/zenoh_service.cpp:841-850`、`D/main.cpp:5315-5356` |
| ForceFeedback/6 | Param1=0/1关闭/开启，Param2=0两侧/1导管587/2导丝582；首次未zeroed时先取零 | `D/zenoh_service.cpp:852-879`、`D/main.cpp:4992-4995` |
| Cylinder/7 | Param1=1..4电缸，Param2=1接管手动打开（1/3值2000、2/4值10），0撤手动覆盖回自动；不是“夹紧=true”的机械意义 | `D/zenoh_service.cpp:890-916`、`RemoteRobotLink.cs:268-269` |
| ArmManualEnable/8 | Param1=0/1定位臂总手动禁用/启用 | `D/zenoh_service.cpp:919-921` |
| ArmAxisEnable/9 | Param1=1..5定位臂轴，Param2=0/1单轴使能请求 | `D/zenoh_service.cpp:922-924` |
| ArmAxisReset/10 | Param1=1..5轴，复位脉冲；清末端点动并要求释放曲线所有权 | `D/zenoh_service.cpp:925-927`、`D/arm_manual_ads_service.cpp:187-198` |
| ArmAxisJog/11 | Param1=1..5轴，Param2=-1/0/+1方向，Value1=.5/1/2档；旧主端Value1=0兼容为1；CommandId强制0，不等结果 | `D/zenoh_service.cpp:763-765,928-934`、`RemoteRobotLink.cs:255-259` |
| ArmCartesianJog/12 | Param1=1..5模式，Param2=有符号speed×1000；main除1000，服务拒绝speed=0；UI松键另发ArmStop；Value1传speedScale但服务忽略，不再参与末端速度计算 | `RemoteRobotLink.cs:249-252`、`D/zenoh_service.cpp:935-937`、`D/main.cpp:5129-5132`、`D/arm_manual_ads_service.cpp:236-251`、`PositioningArmPanel.xaml.cs:133-139` |
| ArmStop/13 | 无业务参数；清末端/曲线运动并逐轴发送jog=0 | `D/zenoh_service.cpp:938-941` |
| ArmProgramZero/14 | 无业务参数；请求程序归零；后续须ArmCartesianAlive，UI每100ms发 | `D/zenoh_service.cpp:942-945`、`PositioningArmPanel.xaml.cs:160-169` |
| 保留/15 | 旧定位臂参数接口停用；当前枚举无15 | `ZenohFrames.cs:19-22` |
| ArmCartesianAlive/16 | 无业务参数，CommandId=0；仅curve_pending/home_requested且未blocked时延长程序归零300ms租约，不续末端点动500ms租约 | `D/zenoh_service.cpp:763-765,944-945`、`D/arm_manual_ads_service.cpp:297-301` |
| YValve/17 | Param1=1关闭、0打开；内部SetYValveOpen使用反值 | `D/zenoh_service.cpp:946-954`、`RemoteRobotLink.cs:270-271` |
| ForceZero/18 | Param1/2必须0；独立采集装机零点 | `D/zenoh_service.cpp:852-872`、`D/main.cpp:759-804` |
| ForceHold/19 | Param1=0/1关闭/开启换手力保持，清旧锁存；不改变两侧FF开关 | `D/zenoh_service.cpp:882-888`、`D/main.cpp:4997-5004` |

**已确认**：定位臂默认中档（倍率1）；快/中/慢单轴倍率2/1/.5。末端模式1/2/3分别X/Y/Z，速度快/中/慢为30/15/5、30/15/5、3/2.5/1.25mm/s；模式4/5分别俯仰/偏航，为1.2/.8/.4、2.4/1.6/.8°/s。UI先按档位计算绝对末端速度，服务忽略额外speedScale。出处：`MainViewModel.cs:64,445-473`、`D/arm_manual_ads_service.cpp:236-251`、`PositioningArmPanel.xaml.cs:64-73`、`工程/protocol/PROTOCOL.md:132`。PLC模式和几何计算见 `P/ArmCartJogGen.TcPOU:104-164`。
