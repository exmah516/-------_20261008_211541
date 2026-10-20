# C++ 上位机项目代码说明

活动源码位于 `介入机器人/B数据端/工程/ADS项目`，输出为 `ADS.exe`。

| 模块 | 职责 |
|---|---|
| `main.cpp` | 初始化、主循环及业务命令处理；保留原控制流程和安全门控 |
| `zenoh_service.h/.cpp` | 独立管理 Zenoh Session/Topic、路由器复用或启动、认证、命令 FIFO、状态与力反馈发布 |
| `protocol/cpp/zenoh_protocol.h` | 四个 Topic 的 MessagePack 位置数组类型、字段顺序和校验 |
| `control_messages.h` | 内部 `VisCommand`、`VisState` 等控制域类型，与网络线格式解耦 |
| `ads_communication.*`、`ADSComm.cpp`、`plc_io.*` | ADS 连接、快照、输出和 PLC 接口 |
| `control_types.*` | 控制状态、参数和预设 |
| `startup_sequence.*`、`motion_sync.*`、`guidewire_mode.*`、`delivery_tracking.*` | 准备、运动同步、递送/撤出与位移跟踪 |
| `force_calibration.h`、`force_feedback.*` | ADS 力传感器标定、纯净力及手柄反馈 |
| `手柄/remote_handle_bridge.h` | 接收主端手柄采样、回传力目标；数据端不加载本地手柄 SDK |
| `arm_manual_ads_service.*`、`arm_cartesian_motion.h` | 介入机器人定位臂 |
| `experiment_recorder.*`、`async_csv_writer.h` | 实验会话和异步 CSV 写入 |

`ZenohService` 接收 handle/command，发布 state/force；业务指令转换为 `VisCommand` 后交给主循环消费。命令 FIFO 容量 256、入队时效 2 秒；session 与序号用于防重放。手柄与力反馈维持 100 Hz，state 约 30 Hz。具体协议见本端 `工程/protocol/PROTOCOL.md`。

`guidewire_mode.*` 是保留的内部控制模块名，界面操作仍为递送/撤出。数据端没有本地 UI、键盘、旧 TCP/UDP/JSON 或命名管道入口。PLC ADS 符号、运动契约与独立急停路径沿用原工程。

主端通过 `IRobotLink → RemoteRobotLink → VI.Service.Net` 使用该服务。运动细节见 [运动流程说明](运动流程说明.md)，PLC 通信见 [ADS 通讯说明](ADS通讯说明.md)，反馈见 [力反馈说明](../guides/力反馈说明.md)，记录见 [实验记录说明](../guides/实验记录说明.md)，界面职责见 [界面架构说明](可视化界面架构说明.md)。

造影模型、截图和摄像头服务由另外两个业务目录承担。
