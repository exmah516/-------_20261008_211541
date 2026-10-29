# 影像传输：H.264 + WebRTC

DSA 桌面由 FFmpeg 采集裁剪并编码，两路海康由 MediaMTX 直接读取 RTSP 压缩码流。三路经 WebRTC/WHEP 送到主端 WebView2，不再发送 JPEG 分片或跨机透传海康 SDK 端口。

```text
DSA 桌面 → FFmpeg H.264 ─────────────┐
海康 1 → RTSP（绑定 .1.97 网口）───┼→ MediaMTX → WebRTC/SRTP → 主端三路播放器
海康 2 → RTSP（绑定 .1.96 网口）───┘
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

已按 2026-10-28 现场探测修正默认网口映射：`Camera1Ip=192.168.1.64` 对应 `Camera1LocalIp=192.168.1.97`，`Camera2Ip=192.168.1.32` 对应 `Camera2LocalIp=192.168.1.96`。启动 ScreenCut 后在后台检查这两条 TCP 连接；若当前绑定不可达、另一相机网口可达，窗口以独立红色文字提示可能接反，并列出应修改的 `Camera*LocalIp`。提示不被普通日志覆盖，不自动改网卡或配置；修改后重启重新检查。两条路径都失败时提示检查 RTSP、网口和端口。

数据端双击 `ScreenCut/检查相机连接.cmd` 可单独检查各网口和相机 RTSP 的对应关系，输出同目录 `camera-connectivity.log`。工具只探测，不修改系统配置；PowerShell 入口的 `-SelfTest` 参数只连接回环地址。

1. R300A 组网和相机网口就绪；相机网页/iVMS 里确认 RTSP 启用，码流选普通 **H.264**，关闭 H.265、H.264+ 和 B 帧。
2. 数据端启动 `ScreenCut.exe`，媒体服务和两路相机自动启动。核对裁剪区域，点“预览区域”，再点“开始传图”。坐标支持负数多屏位置，按实际桌面像素计算；停止传图只停 DSA，关闭程序会关闭自己启动的媒体进程。
3. 主端启动 `ImagingMaster.exe`，自动连接三路视频。上方按钮只切换主画面；三路保持连接、保持原比例，允许黑边。拖动窗口和最大化不拉伸视频。也可先开主端，它会持续重连等待数据端。

影像链路可独立运行，不需要先启动介入 ADS、PLC 或 Zenoh。

## 码率与延迟

按 **2 Mbps 总带宽、流畅优先**部署，当前默认预算如下：

| 视频源 | 分辨率 | 帧率 | 视频码率 | 关键帧间隔 |
| --- | --- | --- | --- | --- |
| DSA | 960×540 | 25 fps | 512 kbps CBR | 25 帧（1秒） |
| 摄像头 1 子码流 | 640×360（设备不支持时用640×480） | 15 fps | 128 kbps CBR | 15 帧（1秒） |
| 摄像头 2 子码流 | 同上 | 15 fps | 128 kbps CBR | 15 帧（1秒） |

三路视频码率合计 **768 kbps**。剩余容量用于介入手柄、状态、力反馈、VTK、网络封装和重传；这不是对控制通信的硬带宽保证。关闭摄像头音频，测试期间关闭 iVMS 的跨网预览和其他大流量传输。画面放大后会更模糊，但不改变比例。

DSA 在运行包 `ScreenCut.exe.config` 中设置 `VideoWidth=960`、`VideoHeight=540`、`VideoFps=25`、`VideoBitrateKbps=512`。编码采用 x264 veryfast/zerolatency、2 个编码线程、B 帧为0，GOP随帧率保持1秒。宽高允许320～1920/180～1080范围内的偶数，帧率10～30，码率128～3000 kbps；保留高码率选项用于以后升级网络。2 Mbps 链路请先使用默认值。WebView2 按系统能力选择硬件解码或软件回退。

**相机设置必须改在相机设备上，仅更新程序不能降低相机码率。** 在相机网页或 iVMS 的远程配置中，找到“视频/音频 → 视频”，选择“子码流”，按上表设置：普通 H.264、CBR/定码率、128 kbps、15fps、I帧间隔15、视频流不含音频，关闭 H.264+/H.265及B帧（型号提供时选Baseline）。先用128kbps；若画面难以辨认，可降低相机子码流分辨率，或在并发测试稳定后再单独升到192kbps。保存后重新读取配置确认生效，再重启 ScreenCut。

数据端 `ScreenCut/config/hik_cameras.ini` 两节都设 `channel=1`、`stream=1`（RTSP 102），保留各自用户名密码。INI 优先于 EXE.config 的 `Camera1/2RtspChannel=102`，旧 INI 的 `stream=0` 会继续选择主码流。子码流只改变远程预览，摄像机主码流仍可保留原有本地高清设置。

三路仍然同时连接；切换主画面不节省带宽。相机是直接中继，没有新增转码或自动码率控制，因此设备端设置是部署的必要一步。

WAN 优先走 WebRTC UDP，数据报目标 1200 字节，使用 NACK 重传与浏览器抖动处理；建连时 UDP ICE 不可用才会选择 TCP 8189，不会在已连接的 UDP 会话中途切换传输协议，未额外实现 FEC。播放器将可用的 jitter-buffer 目标设为1秒（1000ms），用更多等待时间吸收网络抖动；这是接收端目标，不是端到端延迟上限，3 秒没有解码新帧会先遮住旧画面，8 秒后才重建会话；判断依据是接收端解码计数，不是窗口是否呈现帧，避免窗口隐藏或视频暂停时误判断流。关闭并重建会话时也会释放旧 WHEP 会话。恢复画面后自动移除遮罩，不按历史帧排队播放。重连可能等待下一关键帧。当前1秒接收缓冲目标另加网络、采集、编码与显示时间；本版优先连续播放，不承诺100～150ms端到端上限。

## 验证与排错

仅验证关键帧画质可运行 `powershell -ExecutionPolicy Bypass -File .\单机回环测试.ps1 -EncodingOnly`，使用实际编码参数与相同动态素材逐帧对照，检查关键帧边界的画质变化。VBV 码率预算已从0.1秒改为0.5秒、I/P 帧比例设为0.9，减小每秒关键帧的清晰度跳变；这不是新增半秒播放缓冲。

仅验证日志可运行 `powershell -ExecutionPolicy Bypass -File .\单机回环测试.ps1 -LoggingOnly`，检查EXE目录、正常提示即时落盘、时间戳、脱敏、并发写入、轮转及目录不可写处理，不启动媒体服务或连接设备。

开发机执行 `powershell -ExecutionPolicy Bypass -File .\单机回环测试.ps1`。测试只用 127.0.0.x 模拟相机，无需连接真实设备，也不修改实际 IP/防火墙。检查包括配置校验、源网口绑定、RTSP 认证与字节透传、三路实际 H.264 解码、等比例显示、暂停画面呈现时不误重连、未接摄像头不影响另两路、短时/持续 UDP 中断恢复，以及 TCP-only ICE 建连。原有弱网回环注入双向每个 UDP 包单程 50～200ms 抖动和约 1% 丢包，用于检查中断/恢复。另运行 `powershell -ExecutionPolicy Bypass -File .\单机回环测试.ps1 -BandwidthOnly`，检查三路视频共用2 Mbps瓶颈：单程时延在50～200ms间连续变化、约1%丢包、另计每包80字节封装及双向100Hz/200字节的模拟控制流量。该流量只是排队探针，不是真实Zenoh或PLC；结果不等于三个实际主端的现场并发验收。逐包独立剧烈乱序比连续链路抖动更苛刻，可能仍显著掉帧。结果及截图写到 `验证记录`；这是功能测试，不是端到端延迟测量。

部署后在主端分别运行 `Test-NetConnection 192.168.60.1 -Port 8889` 和 `Test-NetConnection 192.168.60.1 -Port 8189` 检查 TCP 信令与媒体候选；这不能代替 UDP 连通性检查。无画面时继续核对 UDP 8189、TCP 8189、R300A 规则及两端 IP。数据端日志在 **ScreenCut.exe 同目录的 `streaming.log`**，自动记录正常动态提示、MediaMTX/FFmpeg 输出及界面操作结果，带完整日期和毫秒时间；RTSP 用户名密码脱敏。每份约2 MB，保留当前文件及 `streaming.1.log`、`streaming.2.log`、`streaming.3.log`（1最新），合计约8 MB，重启继续追加。逐条写磁盘，不在内存保留历史；窗口每200ms只刷新最新提示。日志中相机错误查554、设备IP、凭据、通道和指定网卡本地地址。复现后复制这四份日志；旧版用户目录日志不搬移也不删除。若EXE目录不可写，窗口会提示日志写入失败。

现场延迟请用同一台相机同时拍摄数据端动态毫秒计时器和主端画面，逐帧比较。需在真实 R300A 4G 环境测量延迟、丢包恢复和画质；本次回环结果不能替代现场测量。

## 本次更新怎么用

1. 关闭两端影像程序，更新整个 `B数据端/ScreenCut` 和 `A主端/运行` 文件夹；保护数据端原有相机密码、IP、网卡和截图区域配置。不要仅替换 EXE。
2. 本工作区运行包已同步为上表低码率。若部署时保留了现场旧配置，请按上面的四个DSA键和INI的 `stream=1` 手动同步；编译脚本会保留旧值，不自动覆盖现场配置。
3. 两台相机本机配置按上表改为子码流低码率。少接一台不会影响另两路，无须处理那台的连接错误。
4. 先启动介入数据端和主端确认连接，再启动影像数据端、开始传图、启动影像主端，最后打开造影主端并运行VTK测试。启动顺序只便于定位，不是运行依赖。三者应能共存。
5. 若仍出现手柄轮询失败，保留当时数据端日志。当前代码超过250ms未收到有效远程手柄采样会返回失败，主端有效state超2000ms会视为离线；本次没有放宽这些门控。VTK每个测试帧等待主端回执，拥塞会拉长测试或触发5秒超时，本次没有把超时加长来掩盖拥塞。

主端开发者工具中执行 `window.player.snapshot()` 可查看各路 `fps`（解码帧率）、`bitrateKbps`、`rttMs`、`jitterBufferMs`、`generation`。RTT是WebRTC网络往返时间，抖动缓冲不是端到端延迟；完整视频延迟仍按前述同画面拍摄方法测量。
