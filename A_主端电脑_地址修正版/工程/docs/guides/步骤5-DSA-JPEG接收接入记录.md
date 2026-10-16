# 步骤 5：主端直接接收独立 DSA ScreenCut JPEG

实施日期：2026-10-17

## 旧协议核实

已读取旧工程 `A/ScreenCut/ScreenCut/MainWindow.xaml.cs`，确认：

- 旧 ScreenCut 的“持续传图”模式把画面 1/画面 2 都发送到 UDP `36661`，首字节分别为 `0/1`，命令接收端口为 `36662`。
- 旧 ScreenCut 的“持续传图2”模式只发送第二个发送通道的画面，发送到 UDP `36663`，首字节为 `0`，命令接收端口为 `36664`；旧主端把它作为心电影像接收。
- 每帧先发送 5 字节：`流编号(1) + JPEG长度(4，大端)`。
- JPEG 数据随后按最多 `8192` 字节的数据报发送，不能按单个 UDP 包解码。
- 控制命令固定为 2 字节：`36662` 上画面 1 快发为 `[1,0]`、画面 2 快发为 `[0,1]`；`36664` 上独立通道快发为 `[1,0]`。原程序通过停止保活并等待其超时回到慢发。

没有修改或合并旧 ScreenCut 工程。

部署时旧 ScreenCut 的 `txtIP` 应填写主端 IP；主端 `dsa_screencut.ini` 的 `screenCutIp` 用于快慢控制回路，应填写 DSA 电脑上 ScreenCut 的 IP。

## 目标工程改动

- `master/MasterConsole/Services/DsaJpegReceiver.cs`
  - 后台监听 36661/36663，可由 `config/dsa_screencut.ini` 配置。
  - 36661 按旧协议首字节分流为画面 1、画面 2；36663 映射为心电影像。
  - 每路独立重组 JPEG，校验长度上限、SOI/EOI，5 秒未完成则丢弃当前帧。
  - 只保留完整 JPEG 交给显示层；不转发、不重编码。
  - 向 ScreenCut 的 36662/36664 发送两字节快慢控制命令。
- `master/MasterConsole/Controls/DsaImageView.xaml(.cs)`
  - 接收完整 JPEG 字节；后台解码 `BitmapImage` 并 `Freeze`。
  - UI 定时器只交接最新已解码图像到 WPF，保持比例并显示等待/在线/过期/失败。
- `master/MasterConsole/Views/MainWindow.xaml(.cs)`
  - 启动/关闭独立 DSA 接收器；提供“画面1/画面2/心电”选择；增加“DSA快发/DSA慢发”按钮。
- `config/dsa_screencut.ini.example`
  - 提供端口、ScreenCut IP 和 JPEG 最大长度配置样例。

## 地址适配

当前包已提供 `config/dsa_screencut.ini` 占位配置：

```ini
screenCutIp=192.168.50.3
ecgScreenCutIp=192.168.50.4
listenPort0=36661
listenPort1=36663
commandPort0=36662
commandPort1=36664
```

把 `screenCutIp` 改成运行“持续传图”的 DSA 电脑 IP；把 `ecgScreenCutIp` 改成运行“持续传图2”的电脑 IP。ScreenCut 窗口中的“主端IP”填写主端电脑实际 IP。没有第二台截图电脑时，`ecgScreenCutIp` 可留空。

## 离线验证

```powershell
dotnet build master/MasterConsole/MasterConsole.csproj -c Debug -p:Platform=x64 --no-restore
dotnet build master/MasterConsole/MasterConsole.csproj -c Release -p:Platform=x64 --no-restore
```

Debug/Release 均通过，0 错误。离线回环测试已覆盖：36661 的首字节 0/1 分流、36663 的独立通道、8192 字节分片、JPEG 解码显示、无效长度丢弃、快发保活切换和端口释放。没有运行真实 DSA ScreenCut，未声称实机影像联调成功。
