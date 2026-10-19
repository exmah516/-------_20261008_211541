# C++ 上位机项目代码说明

活动源码位于 `介入机器人/B数据端/工程/ADS项目`。

| 模块 | 职责 |
|---|---|
| `main.cpp` | 初始化、主循环和本地/远程命令处理 |
| `ads_communication.*`、`ADSComm.cpp`、`plc_io.*` | ADS 连接、快照、输出和 PLC 接口 |
| `control_types.*` | 控制状态、参数和预设 |
| `startup_sequence.*`、`motion_sync.*`、`guidewire_mode.*`、`delivery_tracking.*` | 准备、运动同步、递送/撤出与位移跟踪 |
| `force_calibration.h`、`force_feedback.*` | ADS 力传感器标定、纯净力及手柄反馈 |
| `手柄/`、`remote_gateway.*` | 本地/远程手柄来源和主端网络接口 |
| `arm_manual_ads_service.*`、`arm_cartesian_motion.h` | 介入机器人定位臂 |
| `experiment_recorder.*`、`async_csv_writer.h` | 实验会话和异步 CSV 写入 |
| `vis_server.*`、`AdsControlUI/` | 本地调试台命名管道接口和界面 |

`guidewire_mode.*` 是现有介入控制模块，本次不改动其内部实现。

运动细节见 [运动流程说明](运动流程说明.md)，通信见 [ADS 通讯说明](ADS通讯说明.md)，反馈见 [力反馈说明](../guides/力反馈说明.md)，记录见 [实验记录说明](../guides/实验记录说明.md)，本地接口见 [界面架构说明](可视化界面架构说明.md)。

造影模型、截图和摄像头服务由另外两个业务目录承担。
