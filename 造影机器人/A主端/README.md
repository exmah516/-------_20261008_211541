# 造影机器人 A主端

工程：工程/DsaMaster/DsaMaster.sln

当前职责：

- 显示 URDF/VTK 造影机器人模型；
- 接收 造影机器人/B数据端 的 TCP 32110 运动数据；
- 保留当前 VTK 测试接收逻辑，后续再接入 DSA 手柄信号；
- 保留旧 DSA CAN 数据发送代码作为后续接入点，当前没有 DSA PLC/数据端，因此不自动启动；
- 不包含血管介入机器人控制、DSA 影像和海康监控。

运行本目录的 编译并启动主端.cmd。运行时需要工程/DsaMaster/URDF 和 VTK 运行库。

模型相关本地库源码（`Math4wpf`、`can4wpf` 及 Eigen）位于 `工程/本地库源码`；运行仍使用 `工程/DsaMaster/Runtime` 中的现有 DLL。
