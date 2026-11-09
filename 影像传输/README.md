# 影像传输：截图与监控分机部署

数据端有两份独立运行包，支持不同电脑、不同 IP：**ScreenCut 截图端**负责 DSA 桌面采集，**CameraMonitor 监控端**负责两路海康 RTSP。主端继续显示三路画面，通过两份 INI 分别连接它们，不需要修改主端播放器。

```text
截图电脑 → FFmpeg + MediaMTX → WebRTC ──┐
监控电脑 → 两路 RTSP + MediaMTX → WebRTC ┴→ 原影像主端
```

完整配置步骤、三台电脑 IP 示例、端口表和画质档位见 [有线分机部署与画质设置](B数据端/有线分机部署与画质设置.md)。

## 编译与部署

开发机需要 VS2022 的 .NET 桌面开发工作负载、.NET Framework 4.7.2 开发包及 .NET SDK。运行机需要 Windows 10/11 x64、.NET Framework 4.7.2 或更新版本；主端另需 WebView2 Evergreen Runtime。影像链路不依赖 ADS、PLC、Zenoh、海康 SDK 或 VLC。

1. 依赖已准备好；缺失时运行 `powershell -ExecutionPolicy Bypass -File .\准备依赖.ps1`，固定版本与 SHA256 校验沿用现有脚本。
2. 运行 `B数据端/编译并启动数据端.cmd`，生成并启动两个独立软件。仅构建用 `powershell -ExecutionPolicy Bypass -File .\B数据端\build-and-run.ps1 -BuildOnly`。
3. 复制整个 `B数据端/截图端` 到截图电脑，整个 `B数据端/监控端` 到监控电脑。在各自的 streaming.config 中填写主端 IP 与该电脑监听 IP。
4. 使用原 `A主端/运行`，在其 config/dsa_screencut.ini 配截图电脑，在 config/hik_cameras.ini 配监控电脑。修改后重启主端。
5. 依次或分别打开两个软件。截图端预览区域后点“开始传图 / 应用画质”；监控端打开即接入相机。任意一端关闭不影响另一端，也能使用默认不同端口部署在同机。

首次构建从旧包迁移相机凭据、设备 IP、网口和截图区域，新截图包默认 1080p25、6000 kbps。已有独立运行包的配置不会被再次构建覆盖。旧 `B数据端/运行` 仅保留兼容包，`B数据端/ScreenCut` 保留更早版本；分机部署使用两个新目录，切换前关闭旧程序。

## 默认端口

| 服务 | 截图端 | 监控端 |
| --- | --- | --- |
| WebRTC 信令 TCP | 8889 | 8890 |
| WebRTC 媒体 UDP/TCP | 8189 | 8190 |
| 本机 RTSP | 8554 | 8555 |
| 本机管理 API | 9997 | 9998 |

仅对主端放行信令和媒体端口。RTSP 和 API 限定本机。各端的 MasterIp 是允许读取的主端地址，RelayListenIp 是该软件所在电脑的实际有线地址。

## 画质与行为

截图窗口提供 540p/720p/1080p 快捷档位，也能手动填写分辨率、帧率和码率，点应用后保存并重启本路编码。允许最高 1920×1080、30 fps、20000 kbps。画面保持原比例，必要时加黑边。

相机原码流不转码：请在相机网页或 iVMS 中配置普通 H.264，关闭增强编码和 B 帧；推荐起点为每路 1080p25、4096 kbps，监控端 INI 选择 `stream=0` 主码流。现有相机凭据和码流选择会保留。

主端沿用原来的三路并行连接、等比例显示、约 1 秒接收缓冲目标、3 秒无解码遮住旧帧、8 秒无解码重建会话。更换分辨率时截图画面可能短暂重连，监控会话不受影响。TCP ICE 是建连时 UDP 不可用的备用路径。

## 验证与日志

`powershell -ExecutionPolicy Bypass -File .\单机回环测试.ps1` 使用不同回环 IP 和端口模拟两台发送电脑，检查三路真实 H.264 WebRTC 解码、在线切换 1080p、停开一端不影响另一端、网口绑定、认证、丢包/抖动恢复和 TCP ICE。测试不修改现场 IP、网卡或防火墙。

可用 `-LoggingOnly` 单测日志，`-EncodingOnly` 单测原低带宽编码画质。`-BandwidthOnly` 保留原 2 Mbps 模拟链路回归用途。结果写入“验证记录”，真实双机硬件链路仍需现场验收。

构建后可运行 `pwsh -STA -NoProfile -File .\B数据端\ScreenCut源码\tools\ScreenCutCheck\SplitDeploymentCheck.ps1`，将两个实际运行包复制到临时目录，用独立 IP/服务进程检查画质档位、手动参数、非法输入、配置保存及进程启停恢复；测试期间相机被禁用。

截图日志为截图端 streaming.log，监控日志为监控端 monitoring.log，各保留三份约 2 MB 历史日志，RTSP 凭据脱敏。部署细节见上述使用指南。
