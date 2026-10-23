# VI.Control 迁移验收

日期：2026-10-21。完成 .NET 8 WPF、x64 主端迁移；SDK 固定为 9.0.315，仅用于构建 net8.0 目标。本记录为无硬件软件验收，实体 SDK、手柄与 PLC 的台架验收单列在末尾。当前 .NET 8 支持周期将于 2026-11-10 结束。

## 交付入口

- 解决方案：`master/VI.Control.slnx`，目标 `net8.0-windows` / x64；SDK `9.0.315` 由 `global.json` 固定。
- 界面：`VI.Control`；业务服务：`VI.Service.Robot`；动态模块：`VI.Module.Intervention`、`VI.Module.PositioningArm`。
- Release 运行目录：`build/Release/bin/VI.Control`。部署复制整个目录，目标电脑需 .NET 8 x64 Windows Desktop Runtime。
- 上一级的 `编译并启动主端.cmd`、`模拟运行主端.cmd` 均指向新入口。
- 框架源码独立维护在 `D:\Work_files\Vessel intervention Robot\重构框架\HostFramework`，交付仓库仅保留 `Packages` 内两个 Release NuGet 包。
- 旧 `master/MasterConsole` 实现和 `tools/MasterUiCheck.cs` 已移除；未跟踪的 `master/MasterConsole.sln` 保留且不进入新解决方案。旧实现可从既有 Git 历史恢复。未创建提交，保留既有文档和影像传输改动。

## 验证结果

| 检查 | 结果 |
| --- | --- |
| 新建独立 NuGet 缓存还原 | 通过；两个 Robotics 包的 `.nupkg.metadata.source` 均指向工程 `Packages` |
| Debug / Release x64 | 均 0 警告、0 错误 |
| 包内容和版本 | 仅框架 DLL、NuGet 必需元数据；无源码、PDB、符号包、仓库 URL 或本机路径；精确引用 `[0.1.0-dev]` |
| 运行文件 | 模块、共享依赖、runtimeconfig 及原生 DLL 完整；FLCatheter、Zenoh、VC runtime 核验为 x64 |
| 框架独立假模块 / 生命周期 | Debug / Release 各 43 项通过 |
| 协议 / 业务保护 | Debug / Release 各 86 项通过，包含原有 56 项黄金样本 |
| WPF STA / 界面交互 | Debug / Release 各 71 项通过 |
| C++ 黄金样本 | 54 项通过，0 失败 |
| C++ 服务边界 | FIFO、认证、序号、2 秒到期、结果队列、任务和资源切换检查通过 |
| C# + C++ 原生 Zenoh 本机回环 | 109 项通过，0 失败；handle 100.0 Hz、state 30.3 Hz、force 100.0 Hz |
| 日常 Release 构建脚本 | `build-and-run.ps1 -Configuration Release -BuildOnly` 通过，0 警告、0 错误 |
| 真实应用模拟入口 | `VI.Control.exe --sim` 启动后保持运行，无硬件初始化 |

框架测试覆盖仅复制独立插件 DLL 的发现、应用根目录扫描、BAML 资源、重复 / 缺失模块、错误注册签名、坏 DLL、共享身份冲突、服务缺失依赖 / 循环、启动失败回滚、逆序停止与唯一释放。子进程测试同时覆盖业务 CLI 参数不进入 Host 配置，以及直接 `Application.Shutdown()` 的异步清理。

业务保护覆盖 1999 / 2000 ms 边界、认证 / 会话 / 序号无效帧不刷新状态、接收时刻早于锁等待、清除连续输入和本地力、拒绝迟到力帧、恢复后不回放旧目标、断开并发回调，以及独立的 3000 ms 力帧到期保护。

界面测试覆盖 600×560、1000×900、1920×1080 逻辑视口和 100% / 125% / 150% 渲染 DPI，各有顶部 / 底部截图，共 18 张每种构建配置。验证折叠、滚动可达、四读数单位、松手、丢失捕获、折叠及关闭停止；失焦不触发新增全局停止。隐藏末端点动与程序归零保持隐藏。截图为离屏 WPF 渲染，实体多显示器 DPI 切换仍属现场检查。

旧主窗口的业务绑定、操作事件与两个模块逐项对应，保留四电缸、Y 阀、轴 4、两个注射器、定位臂总使能和五轴上电 / 复位 / 点动，以及参数范围、单位换算和数据端确认。

## 可复验命令与证据

从主端工程目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify-migration.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_master_ui.ps1 -Configuration Release
```

从 `介入机器人/B数据端/工程/ADS项目` 执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run-zenoh-tests.ps1
```

首次还原需要访问 nuget.org 及官方包签名服务。构建使用 `-m:1`，兼容受限执行环境。测试仅运行内存模拟数据端和本机回环，不链接 ADS/PLC。测试生成的 C++ EXE 未纳入本次源码改动。

本机证据位于忽略的构建目录：`build/verification-final.log`、`build/verification/zenoh-final.log`、`build/verification/{Debug,Release}/ui`。

| 包 | SHA-256 |
| --- | --- |
| `Robotics.HostFramework.0.1.0-dev.nupkg` | `D43D7103441E0EDF3B2D6334A282BA81657928E80F8BC8A67200926DEDF091A4` |
| `Robotics.HostFramework.Contracts.0.1.0-dev.nupkg` | `6C7C01F7D974B0294DEF6F79955E93E8E50D3F7C4F35AC3F3C56E62D058BE93F` |

本次 `0.1.0-dev` 为首次交付基线；后续框架更新应发布新版本，避免同版本缓存混淆。Default ALC 模块依赖由应用根目录和 `.deps.json` 统一提供，不支持热卸载或并存多个共享契约版本。

## 台架待验

尚未连接真实 FLCatheter、手柄、机器人或 PLC。需核验实际 SDK 加载、设备序列号和标定、双手柄采样、力 / 力矩方向和限幅、拔线及重连清力、实际执行结果与设备停止行为。原生 DLL 的 x64 检查和模拟回环不替代这些硬件检查。
