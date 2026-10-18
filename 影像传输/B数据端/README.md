# 影像传输 B数据端

这里运行 DSA 电脑上的 ScreenCut：截取指定屏幕区域、压缩为 JPEG，并通过 UDP 直接发送给主端。

- 现成程序：`ScreenCut/ScreenCut.exe`
- 源码工程：`ScreenCut源码/ScreenCut.sln`
- 使用说明：`使用说明.md`

ScreenCut 不依赖 ADS、PLC、VTK 或海康转发。当前主端直接接收 UDP 影像；本阶段不增加中继层。
