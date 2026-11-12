# 影像传输 A主端

WPF net472/x64 + WebView2，常驻播放 DSA、监控 1、监控 2。随包 MediaMTX 从两台发送电脑各拉一次上游 WHEP，同时向本机播放器转发和保存三路原码流 fMP4。按钮、显示比例和播放器自动重连保持原样。

- 工程：`工程/ImagingMaster/ImagingMaster.sln`
- 编译：`编译并启动主端.cmd`，完整部署包生成在 `运行`。
- 部署配置：`运行/config/dsa_screencut.ini` 和 `运行/config/hik_cameras.ini`。
- 运行机先安装 Microsoft Edge WebView2 Evergreen Runtime；整个运行包一并复制。
- 本地服务：信令 127.0.0.1:8891，媒体 UDP/TCP 127.0.0.1:8191，API 127.0.0.1:9999；RTSP/RTMP/HLS/SRT/MoQ/Playback 关闭。
- 截图上游 192.168.2.27:8889，监控上游 192.168.60.1:8890；均从 INI 读取，播放器只访问本机。
- dsa_screencut.ini 的 [relay] recordingRoot 默认为 recordings，相对 EXE 目录，也可设绝对目录。
- 每次启动创建时间命名的会话目录，三路分子目录保存。连续流不足 24 小时时每路一个 MP4；版本上限 24h、断流或进程重启会新建文件，不自动清理旧录像。
- 正常退出等待 MediaMTX 的 Ctrl+C 收尾；强制退出可播放已写入分片，最后约一秒可能丢失。窗口最小化和隐藏不影响录像。
- 状态区显示录像目录或当前无新视频写入的原因；完整日志与自动生成的 mediamtx.yml 在会话目录。

选择理由、完整配置、防火墙、现场源地址和流量验收见 `../B数据端/有线分机部署与画质设置.md`。离线录像测试：在影像传输目录运行 `单机回环测试.ps1 -RecordingOnly`。
