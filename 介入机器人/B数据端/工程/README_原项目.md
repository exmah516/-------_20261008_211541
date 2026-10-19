# 介入机器人 B数据端 ADS 工程

本目录保留原有 ADS/PLC 上位机整块工程，当前只迁移目录，不改控制逻辑。

## 工程入口

- `ADS项目/ADS.sln`：ADS C++ 数据端
- `ADS项目/AdsControlUI/AdsControlUI.sln`：本地调试台
- `PositioningArmKinematics/`：介入机器人五轴定位臂运动学
- `协议/`：部署包协议文件；工程内部协议位于 `工程/ADS项目/protocol/`

## 当前职责

`ADS.exe` 负责 ADS/PLC 通信、介入机器人运动、手柄、力反馈、远程网关和定位臂。现场 PLC 程序和 ADS 符号本阶段不改。

`hik_camera_relay.*` 已整体移至 `影像传输/B数据端/ScreenCut源码/CameraRelay`，由 ScreenCut 加载运行；ADS 工程已移除相关编译和启动入口。

## 启动

在上一级目录运行 `编译并启动数据端.cmd`。旧入口 `编译并启动从端.cmd` 继续保留。本地调试台运行 `编译并启动本地调试台.cmd`。


