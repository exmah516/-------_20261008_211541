# 影像传输：H.264 + WebRTC

DSA 桌面由 FFmpeg 采集裁剪并编码，两路海康由 MediaMTX 直接读取 RTSP 压缩码流。三路经 WebRTC/WHEP 送到主端 WebView2，不再发送 JPEG 分片或跨机透传海康 SDK 端口。

```text
DSA 桌面 → FFmpeg H.264 ─────────────┐
海康 1 → RTSP（绑定 .1.96 网口）───┼→ MediaMTX → WebRTC/SRTP → 主端三路播放器
海康 2 → RTSP（绑定 .1.97 网口）───┘
```

## 编译与部署

开发机需要 VS2022「.NET 桌面开发」、.NET Framework 4.7.2 开发包及 .NET SDK；运行机需要 Windows 10/11 x64、.NET Framework 4.7.2 或更新版本。主端还需 Microsoft Edge WebView2 Evergreen Runtime，数据端不需要海康 SDK、VLC 或 WebView2。

1. 已随本次改造准备好运行库和本地 NuGet 包。新取源码若缺少依赖，在本目录执行 `powershell -ExecutionPolicy Bypass -File .\准备依赖.ps1`，所有下载均固定版本并检查 SHA256。VS2022 可打开两端原来的 `.sln` 编译，数据端已不再依赖 C++ 工具集。
2. 运行 `B数据端/编译并启动数据端.cmd`，首次编译会生成完整的 `B数据端/ScreenCut` 运行包；用 `build-and-run.ps1 -BuildOnly` 可只生成、不启动。**复制整个 ScreenCut 文件夹到数据端**，包含 `ScreenCut.exe.config`、`config`、`runtime`。
3. 运行 `A主端/编译并启动主端.cmd`，生成完整的 `A主端/运行` 包。**复制整个“运行”文件夹到主端**，包含 WebView2 DLL、`Player`、`runtimes` 和 `config`，不要只复制 EXE。

编译脚本保留运行包中已有的现场配置；源码 `App.config` 或主端 `工程/config` 的修改不会覆盖已存在的运行包配置。部署后应修改运行包中的配置，再重启对应程序。

## 两台 R300A 的配置

| 配置位置 | 必要设置 |
| --- | --- |
| 数据端 `ScreenCut/ScreenCut.exe.config` | `MasterIp=192.168.50.1`；`RelayListenIp=192.168.60.1`；`WebRtcPort=8889`；`WebRtcMediaPort=8189` |
| 同一文件的相机参数 | 原有 `Camera1/2Ip` 和 `Camera1/2LocalIp` 保留；新增 `Camera1/2RtspPort=554`；旧 `Camera*Port=8000`、`Camera*RelayPort` 停用 |
| 数据端 `ScreenCut/config/hik_cameras.ini` | 每台相机的 `username`、`password`、`channel`、`stream`。`channel=1, stream=0` 对应 RTSP 101；`stream=1` 对应 102 |
| 主端 `运行/config/dsa_screencut.ini` | `screenCutIp=192.168.60.1`；`webrtcPort=8889`；`streamPath=stream_dsa` |
| 主端 `运行/config/hik_cameras.ini` | 两节的 `ip=192.168.60.1`；`webrtcPort=8889`；路径分别为 `stream_cam1`、`stream_cam2` |

数据端第一次生成运行包时，会把原主端私有 `hik_cameras.ini` 复制到数据端作为凭据配置；已有文件不会覆盖。数据端只从这份 INI 读取凭据、通道和码流，设备 IP、绑定网口仍从 EXE.config 读取。INI 优先于 EXE.config 中可选的 `Camera*Username/Password/RtspChannel`。主端不再读取海康用户名、密码或旧 SDK 端口。

**数据端放行 TCP 8889、UDP 8189 和 TCP 8189，来源限定为主端 `192.168.50.1`。** UDP 是低延迟首选；TCP 8189 是建连时 UDP ICE 不可用的备用候选。已建立的 UDP 会话遇到丢包时不会自动切换成 TCP，因此需要同时放行 UDP 与 TCP。两台 R300A 需允许这三项跨网段通信。无需继续开放 36661～36664、18000、18001。RTSP 8554 和管理 API 9997 只监听本机，不向虚拟局域网开放。相机侧在本地网卡可访问 TCP 554 即可。

`MasterIp` 现在是媒体读取允许地址，视频由主端主动连接 `192.168.60.1` 获取；数据端不再向该 IP 发送 UDP 截图。WebRTC 使用配置的 UDP/TCP 媒体入口及 SD-WAN 可达地址，无需公网 STUN/TURN。信令是私网 HTTP，视频始终用 DTLS-SRTP；不要把服务暴露到公网。

## 启动顺序

1. R300A 组网和相机网口就绪；相机网页/iVMS 里确认 RTSP 启用，码流选普通 **H.264**，关闭 H.265、H.264+ 和 B 帧。
2. 数据端启动 `ScreenCut.exe`，媒体服务和两路相机自动启动。核对裁剪区域，点“预览区域”，再点“开始传图”。坐标支持负数多屏位置，按实际桌面像素计算；停止传图只停 DSA，关闭程序会关闭自己启动的媒体进程。
3. 主端启动 `ImagingMaster.exe`，自动连接三路视频。上方按钮只切换主画面；三路保持连接、保持原比例，允许黑边。拖动窗口和最大化不拉伸视频。也可先开主端，它会持续重连等待数据端。

影像链路可独立运行，不需要先启动介入 ADS、PLC 或 Zenoh。

## 码率与延迟

DSA 固定 30fps、默认 1280×720、2500kbps CBR、GOP 30、B 帧 0、x264 ultrafast/zerolatency；可在 EXE.config 改为 `VideoWidth=1920, VideoHeight=1080`，`VideoBitrateKbps` 仅允许 2000～3000。编码保留截图比例并填黑边。本版使用随包 FFmpeg 的软件 x264，避免依赖某一型号 GPU；WebView2 按系统能力选择硬件解码或软件回退。

相机直接中继，不重新编码，也不会自动降低摄像头原码率。弱网优先使用子码流，建议每路 0.5～1Mbps、GOP 1 秒、25/30fps。**总带宽是 DSA 加两路相机再加协议/重传开销**；三路保持连接时，切到小画面不会节省带宽。固定 CBR 不能自动适应低于总码率的上行网络。

WAN 优先走 WebRTC UDP，数据报目标 1200 字节，使用 NACK 重传与浏览器抖动处理；建连时 UDP ICE 不可用才会选择 TCP 8189，不会在已连接的 UDP 会话中途切换传输协议，未额外实现 FEC。播放器将可用的 jitter-buffer 目标设为最低值，3 秒没有解码新帧会先遮住旧画面，8 秒后才重建会话；判断依据是接收端解码计数，不是窗口是否呈现帧，避免窗口隐藏或视频暂停时误判断流。关闭并重建会话时也会释放旧 WHEP 会话。恢复画面后自动移除遮罩，不按历史帧排队播放。重连可能等待下一关键帧。不存在真正的零缓冲，100～150ms 是稳定链路目标；网络本身出现 200ms 抖动时不能保证该上限。

## 验证与排错

开发机执行 `powershell -ExecutionPolicy Bypass -File .\单机回环测试.ps1`。测试只用 127.0.0.x 模拟相机，无需连接真实设备，也不修改实际 IP/防火墙。检查包括配置校验、源网口绑定、RTSP 认证与字节透传、三路实际 H.264 解码、等比例显示、暂停画面呈现时不误重连、未接摄像头不影响另两路、短时/持续 UDP 中断恢复，以及 TCP-only ICE 建连。弱网回环注入双向每个 UDP 包单程 50～200ms 抖动和约 1% 丢包。结果及截图写到 `验证记录`；这是功能测试，不是端到端延迟测量。

部署后在主端分别运行 `Test-NetConnection 192.168.60.1 -Port 8889` 和 `Test-NetConnection 192.168.60.1 -Port 8189` 检查 TCP 信令与媒体候选；这不能代替 UDP 连通性检查。无画面时继续核对 UDP 8189、TCP 8189、R300A 规则及两端 IP。数据端日志在 `%LOCALAPPDATA%/VI.Imaging/logs/streaming.log`，其中 `stream_dsa/stream_cam1/stream_cam2` 应显示 `H264` 和 online；相机错误查 554、设备 IP、凭据、通道和指定网卡本地地址。密码不写日志。

现场延迟请用同一台相机同时拍摄数据端动态毫秒计时器和主端画面，逐帧比较。需在真实 R300A 4G 环境测量延迟、丢包恢复和画质；本次回环结果不能替代现场测量。
