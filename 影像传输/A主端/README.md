# 影像传输 A主端

WPF net472/x64 + WebView2，常驻播放 DSA、监控 1、监控 2 三路 WebRTC。按钮只调整主画面，视频保持比例。

- 工程：`工程/ImagingMaster/ImagingMaster.sln`
- 编译：`编译并启动主端.cmd`，完整部署包生成在 `运行`。
- 部署配置：`运行/config/dsa_screencut.ini` 和 `运行/config/hik_cameras.ini`。
- 运行机先安装 Microsoft Edge WebView2 Evergreen Runtime；整个运行包一并复制。

旧 JPEG 接收器和海康 SDK 播放器已移除。端口、启动顺序和验证步骤见上级 `README.md`。
