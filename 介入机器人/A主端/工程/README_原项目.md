# 介入机器人主端工程

解决方案：`master/MasterConsole.sln`。`MasterConsole` 为 .NET Framework 4.7.2、x64 的 WPF 程序，负责介入界面、本机手柄和数据端通信。

从上一级目录运行 `编译并启动主端.cmd`；无硬件查看界面可用 `模拟运行主端.cmd`。输出目录为 `master/MasterConsole/bin/Release/net472`。配置及启动方式见 [使用说明](../使用说明.md)。

依赖保留 `FLCatheter.dll`、`Runtime/VC142` 和 `MasterConsole.Protocol`。VI.Service.Net 使用 Zenoh-C 1.10.1 + MessagePack 位置数组，四个 Topic 共用数据端 TCP 7447；密钥在 `config/remote.token`。

协议定义见 `protocol/PROTOCOL.md`，固定测试向量在 `protocol/test_vectors`。`MasterConsole.Protocol.Tests` 可离线验证编解码和认证。

数据端源码位于 `介入机器人/B数据端/工程/ADS项目`。文档导航见 [项目架构总览](docs/architecture/项目架构总览.md)。
