# 影像传输 B 数据端

- 截图端/ScreenCut.exe：区域预览、DSA 桌面采集、输出分辨率/帧率/码率快捷设置。
- 监控端/CameraMonitor.exe：两路海康相机接入、转发状态和网口告警。

两份完整运行包可分到不同电脑，各自修改 streaming.config，主端分别填写两台发送电脑的 IP。也可同机运行，默认端口已错开。

工程：ScreenCut源码/ScreenCut.sln，VS2022、.NET Framework 4.7.2、x64。双击“编译并启动数据端.cmd”生成并启动两端；只打包加 -BuildOnly。已有两端配置不会覆盖。

部署与画质操作见 [有线分机部署与画质设置](有线分机部署与画质设置.md)。“运行”和“ScreenCut”是此前版本目录，分机部署请用“截图端”和“监控端”。
