# 介入机器人主端工程

解决方案：`master/VI.Control.slnx`。`VI.Control` 为 .NET 8、x64 的 WPF 程序，负责介入界面、本机手柄和数据端通信。SDK 由 `global.json` 固定为 `9.0.315`；缺少 SDK 时运行 `tools/Prepare-Sdk.ps1`，安装在工程内 `.tools`。

Visual Studio 直接调试应打开 `master/VI.Control.slnx`，并将 `VI.Control` 设为启动项目、平台选为 `x64`。`VI.Service.Robot`、`VI.Service.Net`、协议和模块项目均为类库，不能单独启动；旧的 `MasterConsole.sln` 不属于当前入口。`VI.Control` 的项目调试配置提供模拟和远程两种启动参数。

从上一级目录运行 `编译并启动主端.cmd`；无硬件查看界面可用 `模拟运行主端.cmd`。输出目录为 `build/Release/bin/VI.Control`。配置及启动方式见 [使用说明](../使用说明.md)。模拟模式不初始化真实手柄 SDK。

依赖保留 `FLCatheter.dll`、`Runtime/VC142` 和 `MasterConsole.Protocol`。`VI.Service.Robot` 保存真实/模拟链路、共享会话、手柄及视图模型；`VI.Service.Net` 使用 Zenoh-C 1.10.1 + MessagePack 3.1.12 位置数组，四个 Topic 共用数据端 TCP 7447；密钥在 `config/remote.token`。

框架以 `Packages/Robotics.HostFramework.0.1.0-dev.nupkg` 和 `Packages/Robotics.HostFramework.Contracts.0.1.0-dev.nupkg` 二进制交付，业务精确引用 `[0.1.0-dev]`。`NuGet.Config` 将这两个包限定到本地源，其余依赖由 nuget.org 提供。框架源码在交付仓库之外的 `重构框架/HostFramework` 维护。

启动扫描应用目录、`Modules/Platform`、`Modules/Features`，通过 `[FeatureModule]` 加载 `intervention`、`positioning-arm`，两个面板共享一个链路与手柄生命周期。模块目录仅部署模块 DLL 和资源，共享依赖在应用根目录提供。开发接口与验证入口见 [界面架构说明](docs/architecture/可视化界面架构说明.md)。

协议定义见 `protocol/PROTOCOL.md`，固定测试向量在软件包根目录的 `protocol/fixtures/zenoh-v1`。`MasterConsole.Protocol.Tests` 离线验证编解码、认证和 2000 ms 状态失联保护；`tools/check_master_ui.ps1` 验证模块、界面和停止行为。完整构建与隔离缓存验证使用 `tools/verify-migration.ps1`。

数据端源码位于 `介入机器人/B数据端/工程/ADS项目`。文档导航见 [项目架构总览](docs/architecture/项目架构总览.md)。

迁移的软件验收结果、复验命令、包哈希和台架待验项见 [VI.Control 迁移验收](docs/VI.Control迁移验收.md)。
