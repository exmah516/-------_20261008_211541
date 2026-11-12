# D 主端超时、租约、看门狗与恢复事实（供主报告整合）

> 范围仅覆盖 A 主端/手柄宿主以及主端到数据端 Zenoh 的监管。数据端 ADS 和 PLC 超时需与本文件中的上游/下游阈值对照；本文件没有运行设备或网络。

## D.1 逐项清单

| 对象/位置 | 数值/时钟 | 到期/失败动作 | 恢复条件 | 关系与证据 |
|---|---:|---|---|---|
| Zenoh 原生连接建立 `ZenohSession` | `connect.timeout_ms=3000`；Zenoh 配置，连接时 | 原生 `z_open` 失败抛异常；`ConnectAsync` 轮询认证状态独立使用 10 s | `ConnectAsync` 重建 session 后重新 Hello/认证 | 已确认：`VI.Service.Net/ZenohSession.cs:29-50`、`RemoteRobotLink.cs:118-140`。 |
| 主端认证等待 | 10,000 ms；主端 `_time` 单调时钟 | `ConnectAsync` 抛 `TimeoutException`，UI 捕获后 `Disconnect()` | 重新连接，新的 nonce/session | 已确认：`RemoteRobotLink.cs:129-140`。 |
| 状态新鲜度/连接判定 | `<2000 ms`；`TimeProvider` 单调 elapsed | `ExpireStateLocked` 清 `_hasControl`、`_inj1/_inj2/_axis4`；若力闸门开则 `ClearHaptic()`；`IsConnected=false`，UI `CanOperate=false` | 收到新的认证、会话和递增序号 state；不会补发旧运动/手柄输入 | 已确认：`ProtocolConstants.cs:8-12`、`RemoteRobotLink.cs:87-110,291-303`、`MainViewModel.cs:114-120,169-176`。 |
| 主端 state 轮询刷新 | 250 ms UI `DispatcherTimer` | 只刷新状态年龄和属性，不能让过期状态变新 | 后续 state 回调 | 已确认：`MainViewModel.cs:90-95,505-529,617-623`。 |
| 力帧独立新鲜度 | `<3000 ms`；主端接收时 `receivedMs=NowMs` | `OnForce` 丢弃过期帧；状态过期/断开也关闭 force gate 并清本地力 | 新 state 先打开 gate；之后只接受 gate 打开序号后的新力帧 | 已确认：`ProtocolConstants.cs:8-12`、`RemoteRobotLink.cs:506-527`、`HandleService.cs:90-91`。 |
| Zenoh 会话不可用重建 | 至少每 1000 ms 一次；`_lastRebuildMs` | 释放 closed session、拒绝 pending commands（不补发），OpenSession+Hello；失败记 warn | 下一次周期到达 1 s 间隔，且 publisher/session 可用 | 已确认：`RemoteRobotLink.cs:118-128,201-219,536-568`。 |
| 已打开 session 但 state 超时的重认证 | state 未收到或距上次 `>3000 ms`，且 handshake 间隔 ≥3000 ms | 清 `_authenticated/_authSent`、序号和 session key，生成新 nonce，发 Hello；若原已认证则拒绝 pending 命令 | 收到 challenge、Authenticate 和认证 state；不补发旧命令 | 已确认：`RemoteRobotLink.cs:551-568`。 |
| 主端按需命令等待 | `Acquire/Release/Arm...` 多为 5000 ms；StartControl 10000 ms；ForceFeedback/Zero 15000 ms；PreparePosition 330000 ms | `Task` 返回“数据端无响应（超时）”，pending 移除；若 session 失效则统一拒绝 pending | UI 重新发命令；命令不缓存、不重发 | 已确认：`RemoteRobotLink.cs:224-271,326-345`。 |
| HandleService supervisor heartbeat | `now - max(lastStart, memory.Heartbeat) >1000 ms`；`Environment.TickCount64` | 清 `_samples/_forces`；若宿主未退出则 `Kill(true)`；释放映射 | 进程为空且距上次启动 ≥1000 ms 时新建映射并启动宿主 | 已确认：`HandleService.cs:102-149`。 |
| HandleService 重启节流 | 进程为空且 `now-lastStart>=1000 ms`；启动失败后仍按 1 s | 启动失败写状态，不连接设备 | 1 s 后下一轮再尝试 | 已确认：`HandleService.cs:137-149`。 |
| USB 插拔通知 | `CM_Register_Notification`；无时间阈值 | 回调立即清样本/力并 `_restartRequested=true`；拔出提示“两柄暂停”，插入提示“USB 重新插入” | supervisor 让宿主沿 Stop 路径退出/最多等待 1 s，再重启 | 已确认：`HandleDeviceNotifications.cs:23-37`、`HandleService.cs:92-100,126-135`。 |
| HandleHost force tick | `HostForce.Tick` 为 `Environment.TickCount64`，有效窗口 `<3000 ms` | `UpdateForce` 将环境力清零；`ApplyForce` 每 100 ms 至少发一次零力，或立即在之前为非零时发零 | 主端再次 `SetHaptic` 写新 tick，且设备有效、Enable 非零 | 已确认：`VI.HandleHost/Program.cs:104-140`、`HandleHostMemory.cs:63-70`。 |
| HandleHost 采样周期 | 200 Hz deadline；`Stopwatch` | 代码没有单拍超时分支；若循环落后，下一 deadline 追到 `now+5ms` | 继续循环；SDK 异常落到宿主错误码并退出 | 已确认：`VI.HandleHost/Program.cs:37-102`。 |
| 手柄采样“新鲜” | `HandleService.GetSample` 不按 Tick 数值判断年龄；只过滤 `Tick==0`、重复 Sequence、`_stop` | 重复/空样本标 invalid，网络 HandleFrame 仍按 100 Hz 发布 `Valid=false` 结构 | 宿主产生新 Sequence 且监管器成功读到 seqlock | 已确认：`HandleService.cs:64-77`、`RemoteRobotLink.cs:407-420`。实际 3 s 数据端时效在协议文档，主端本地不重复实现。 |
| 定位臂轴点动/末端保活 | UI `DispatcherTimer=100 ms`；数据端阈值不在本主端 | UI 每 100 ms 重发当前点动/末端点动；失去 `CanOperate` 停止所有 hold 并发 `ArmStop` | `CanOperate` 恢复后需重新按住按钮；程序归零保活同理 | 已确认：`PositioningArmPanel.xaml.cs:23-28,30-38,160-170`。 |
| 主端关闭 | HostFramework `StopTimeout=8 s`；窗口 Closing 先 stop panel | 所有持续输入/定位臂动作归零；HostedService 断开 link，再 Stop HandleService | 重新启动应用 | 已确认：`App.xaml.cs:13-24,51-63`、`MainWindow.xaml.cs:43-56`。 |

## D.2 谁监视谁

| 监视者 | 被监视对象 | 条件 | 反应 |
|---|---|---|---|
| `HandleService.Run` | `VI.HandleHost` 进程和映射 heartbeat | 进程退出、USB 通知、heartbeat 超过 1 s、宿主 Error | 清共享内存、让宿主退出/杀进程、按 1 s 节流重启。 |
| `VI.HandleHost` | 主端写入 `HostForce.Tick` | force tick 无效或年龄达到 3 s | 不输出环境力，按 100 ms 节流发零力；本地阻尼也停止。 |
| `RemoteRobotLink` | Zenoh Session/publisher | session closed/unavailable | 每 ≥1 s 重建 session；pending 命令拒绝；发送 Hello。 |
| `RemoteRobotLink` | 数据端认证 state | 最近认证 state ≥2 s | 关闭本地控制资格、清持续输入、清力闸门。 |
| `RemoteRobotLink` | force frame 接收时间 | frame 接收间隔达到 3 s，或状态 gate 关闭 | 丢弃力帧；HandleService 清本地力。 |
| WPF UI | `RemoteRobotLink` 状态年龄 | 每 250 ms 重新评估 | 更新 `StatusFresh/CanOperate`，锁定操作区。 |
| 定位臂 UI timer | `CanOperate` 与 hold 状态 | 每 100 ms | 不满足时发送轴停止/末端停止并清状态；满足时续发保活。 |
| HostFramework | 应用停止 | 8 s StopTimeout | 主端受管服务应在宿主超时内结束；PLC/数据端是否同步停止未知。 |

## D.3 数值组合与可见联动

1. **2 s 状态过期 < 3 s 重认证阈值**：`RemoteRobotLink` 在 2 s 先把 `IsConnected/HasControl/CanOperate` 置为失效并清力；如果 3 s 仍无状态才重置认证并发 Hello。由 `RemoteRobotLink.cs:87-98,551-568` 直接可见。已确认。
2. **HandleService 1 s heartbeat < HandleHost force 3 s tick**：宿主停止/卡住通常会先被 supervisor 发现并清力；即便 supervisor 未发现，宿主自身 3 s force tick 也会清零。两者使用同一 `Environment.TickCount64` 域。已确认：`HandleService.cs:118-149`、`VI.HandleHost/Program.cs:104-140`。
3. **主端 100 Hz 手柄周期 vs UI 100 ms 保活**：轴点动/末端保活由 UI 每 100 ms 下发（约10 Hz），数据端能否接受该保活频率由数据端阈值决定；主端没有自己的超时数字。已确认/下游未知。
4. **HandleHost 200 Hz vs supervisor 5 ms**：两者同目标周期；supervisor heartbeat 判断采用 `>1000 ms`，不会因单个 5 ms 采样抖动触发。已确认。
5. **Zenoh 原生 3 s connect vs 主端认证等待 10 s**：Open session 建连阶段最多 3 s（原生库）；连接成功后主端还允许 10 s 等认证 state。组合具体故障时的总等待受异常位置影响，源码没有单一总超时。已确认。
6. **Force gate 的恢复顺序**：收到认证 state 时打开 gate 并记录 callback ordinal；同一时刻或更早到达的 ForceFrame 被丢弃，只有后续新 force frame 才写手柄。该逻辑避免重认证期间回放旧力，但会在 state/force 调度交错时产生短暂清力窗口。后半句为推断。证据：`RemoteRobotLink.cs:461-471,520-525`。

## D.4 需要跨端核对/现场验证的项目

| 项目 | 主端能确认的范围 | 仍缺什么 |
|---|---|---|
| 数据端手柄新鲜度 | 主端持续发送 HandleFrame 100 Hz，Valid 由宿主 SDK 状态决定 | 数据端 `kHandleFreshMs`、丢帧处理和 PLC 输出动作。 |
| 控制权租约 | 主端看 state Flags.LeaseHeld；过期仅清本地资格，不发软件急停 | 数据端租约持有、失联是否保持及 PLC/ADS 处理。 |
| ADS 状态/命令超时 | 主端只看到 state/CommandResult；准备位置命令等待 330 s，Start 10 s | 数据端任务 deadline、ADS 重连和 PLC state 触发。 |
| 手柄 SDK 拔出 | 主端有 USB 通知和 1 s heartbeat | `deviceStatus()` 对物理拔出是否更新，源码作者明确要求现场核实。 |
| StopTimeout 与 PLC | 主端 8 s 应用停止 | 数据端/PLC 是否在此期间完成安全保持或仍继续运动。 |
