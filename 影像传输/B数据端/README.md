# 影像传输 B数据端

ScreenCut 管理 FFmpeg 与 MediaMTX：DSA 桌面编码 H.264，两路海康 RTSP 原码流接入，统一通过 WebRTC 传输。

- 工程：`ScreenCut源码/ScreenCut.sln`，VS2022/net472/x64，已移除 C++ CameraRelay 项目引用。
- 编译：`编译并启动数据端.cmd`；只打包可用 `build-and-run.ps1 -BuildOnly`。
- 完整运行包：`ScreenCut`，包括 EXE.config、`config`、`runtime`。
- 相机用户名密码：`ScreenCut/config/hik_cameras.ini`；设备 IP/本地网口/RTSP 端口：`ScreenCut/ScreenCut.exe.config`。

不再使用 ImagingCameraRelay.dll、跨机 8000 透传或 UDP JPEG。详细设置见上级 `README.md`。
