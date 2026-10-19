# 影像传输 B数据端

这里运行 ScreenCut：截取指定屏幕区域、压缩为 JPEG 并发送给主端，同时转发两路海康摄像头。两项功能在同一个进程中运行。

- 现成程序：`ScreenCut/ScreenCut.exe`
- 编译启动：`编译并启动数据端.cmd`
- 地址配置：`ScreenCut/ScreenCut.exe.config`
- 源码工程：`ScreenCut源码/ScreenCut.sln`
- 使用说明：`使用说明.md`

摄像头转发已从介入 ADS 移到这里。只验证截图和监控时，无需启动介入机器人数据端。复制运行程序时须带上同目录的 `ImagingCameraRelay.dll` 和配置文件。
