# E 两项功能的主端链路核查补证

> 全部路径相对调研根 `D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/`。本文件只读源码，无主端或宿主运行。行号为 2026-11-12 磁盘文件。

| 环节 | 事实 | 确定度与出处 |
|---|---|---|
| 物理键定义 | 数据端只把物理 SN587 `buttons2 & 0x07 == 0x07` 作为屈曲恢复按住；`0x07` 是 bit0、bit1、bit2 全置位，`0x47` 另外含 B6/bit6。该代码注释把它称为一枚“物理587屈曲恢复键”；仓库未找到其硬件丝印/键帽说明，不能据此确认它是三枚独立 B0/B1/B2 按钮同时按下。 | 已确认（位条件/注释），未知（物理丝印）：`介入机器人/B数据端/工程/ADS项目/control_types.h:150-151`、`介入机器人/B数据端/工程/ADS项目/main.cpp:1503-1513`；B6 名称/掩码为 `control_types.h:92-93`。 |
| SDK 采集 | 宿主按 serial 创建 582/587 的两个 Device，直接 `getSwitch(out byte buttons, d.Id)`，原样赋入 `HostSample.Buttons`；未取反、移位、映射。P/Invoke 声明同样是 `out byte`。 | 已确认：`介入机器人/A主端/工程/master/VI.HandleHost/Program.cs:32,70-77`、`介入机器人/A主端/工程/master/VI.Service.Robot/Services/FlCatheterNative.cs:49`。 |
| 共享内存 | `HostSample.Buttons` 为 int，转换内部 HandleSample 仅 `(byte)Buttons`；采样本身来自 SDK byte，因此未丢失低三位。 | 已确认：`介入机器人/A主端/工程/master/VI.Service.Robot/Services/HandleHostMemory.cs:50-58`。 |
| 主端缓存 | `GetSample(0/1)` 返回 A/B 的 `Sample`，只根据 stop、Tick 和重复 Sequence 把 Valid 设 false；未改 Buttons。serial A=582、B=587。 | 已确认：`介入机器人/A主端/工程/master/VI.Service.Robot/Services/HandleService.cs:13,64-77`。 |
| 100 Hz 帧 | `CaptureControlFrame` 取 GetSample(0/1)，HandleLoop 将 `Buttons582=a.Buttons`、`Buttons587=b.Buttons`，带独立 Valid；签名后发布，无键位重映射。 | 已确认：`介入机器人/A主端/工程/master/VI.Service.Robot/Services/RemoteRobotLink.cs:291-303,407-421`。 |
| Zenoh | 按键在 `vi/ads/handle/v1`，Key4=Buttons582，Key12=Buttons587，均 byte；协议 V1。 | 已确认：`介入机器人/A主端/工程/master/MasterConsole.Protocol/ZenohFrames.cs:5-12,25-54`。 |
| UI 入口 | 当前 WPF 注册的业务命令只有连接、准备、两路力反馈、取零、保持、四电缸、Y阀、StartControl；Zenoh 命令枚举 0–19 无 axis1 先行/spacing recovery 设置。`IRobotLink` 亦没有 lead 或 spacing 设置方法。当前 UI 不能构造“低三位组合”或修改先行/屈曲配置。 | 已确认：`介入机器人/A主端/工程/master/VI.Service.Robot/ViewModels/MainViewModel.cs:76-110`、`介入机器人/A主端/工程/master/MasterConsole.Protocol/ZenohFrames.cs:14-23`、`介入机器人/A主端/工程/master/VI.Service.Robot/Services/IRobotLink.cs:67-96`；全 master 源码未出现 axis1_post_return / spacing_recovery 标识。 |
| 轴1先行配置 | 数据端当前默认附加 2mm，映射跨度10mm，上限5mm；注释仍称“UI与内部”限制，但当前主端没有相应输入。 | 已确认：`介入机器人/B数据端/工程/ADS项目/control_types.h:132-136`；主端命令证据同上。 |

当前主端文档仍记录旧 WPF/本地管道入口：`介入机器人/A主端/工程/docs/architecture/运动流程说明.md:341` 声称 WPF 有“导管递送前10mm轴1映射量”且发送 `SetAxis1PostReturnLead=24`，默认1mm；`ADS通讯说明.md:439,457-458` 同样写旧命令。数据端 `control_messages.h:144-181` 仍保留 `VisCommandType::SetSpacingRecovery=18` 与 `SetAxis1PostReturnLead=24`，但当前 Zenoh C++/C# 线协议只允许命令 0–19，且 18/19 已占用 `ForceZero/ForceHold`（`protocol/cpp/zenoh_protocol.h:14-24`、`zenoh_service.cpp:56,852-890`）；因此这些是旧本地命令编号，不能从当前主端 Zenoh 链路触发。它们与当前 WPF/Zenoh 枚举和数据端默认值不一致，应作为文档/遗留接口漂移，不能据此声称现在 UI 可改。

尚未知：一枚物理按钮为何同时置位低三位（固件编码、硬件并线或 SDK 解码）；需要手柄键位表、固件/SDK说明或不连接机器人的只读手柄采样记录。仅从源码可以明确 bit0/1/2 条件以及 B6 可叠加。
