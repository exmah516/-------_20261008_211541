# 影像传输 A主端

工程：工程/ImagingMaster/ImagingMaster.sln

当前职责：

- 接收 DSA JPEG 影像（UDP 36661、36663）；
- 显示两路海康威视监控；
- 支持 DSA 画面、监控1、监控2 的主画面切换；
- 支持 DSA 影像快发/慢发命令。

运行本目录的 编译并启动主端.cmd。现场设备地址在工程/config/hik_cameras.ini 和 dsa_screencut.ini 中配置。

海康转发与截图发送均由 `影像传输/B数据端/ScreenCut` 承担。当前两路相机入口为数据端 `192.168.50.2:18000/18001`，快发命令目标为 `.50.2`；先启动影像 B，再启动本主端。
