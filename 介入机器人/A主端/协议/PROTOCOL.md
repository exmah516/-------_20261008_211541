# 介入机器人 Zenoh + MessagePack 协议 v1

字段依据当前主端和 ADS 工程中实际使用的载荷定义。C# 镜像为 `MasterConsole.Protocol/ZenohFrames.cs`，C++ 镜像为 `ADS项目/protocol/cpp/zenoh_protocol.h`。

## 传输与部署

| Topic | 方向 | 频率 |
|---|---|---|
| `vi/ads/handle/v1` | 主端 → ADS | 100 Hz |
| `vi/ads/command/v1` | 主端 → ADS | 按需 |
| `vi/ads/state/v1` | ADS → 主端 | 约 30 Hz |
| `vi/ads/force/v1` | ADS → 主端 | 100 Hz |

路由器由数据端启动或复用，默认监听 `tcp/192.168.60.1:7447`；仅关闭本进程启动的路由器。主端仅打开 client session。两端 x64，Zenoh-C 1.10.1；C++ 头文件 SDK 同版本；C# 使用 net8.0 + P/Invoke，MessagePack 3.1.12；C++ msgpack-cxx 9.0.0。

## 线格式

所有消息为位置数组，禁止 map。BIN 为二进制，不接受 STR 或整数数组代替。整数按声明范围验证；float 固定为 MessagePack float32，double 固定 float64；拒绝 NaN、Infinity、错误长度/类型及尾部额外数据。版本固定 1。未列出的旧 VisState 内存字段不直接上网。

手柄按物理序列号 582、587 排列，每只 8 个字段直接放在顶层。关节/编码器/速度来自本机 SDK，轴向力单位 N，扭矩 N·m，位置及累计净距离 mm。

## HandleFrame（24 项）

| 索引 | 类型 | 字段 |
|---|---|---|
| 0 | byte | Version |
| 1 | ulong | SessionId |
| 2 | ulong | Sequence |
| 3 | ulong | MonotonicMs |
| 4 | byte | Buttons582 |
| 5 | bool | Valid582 |
| 6 | int | Encoder582_0 |
| 7 | int | Encoder582_1 |
| 8 | float | Joint582_0 |
| 9 | float | Joint582_1 |
| 10 | float | Velocity582_0 |
| 11 | float | Velocity582_1 |
| 12 | byte | Buttons587 |
| 13 | bool | Valid587 |
| 14 | int | Encoder587_0 |
| 15 | int | Encoder587_1 |
| 16 | float | Joint587_0 |
| 17 | float | Joint587_1 |
| 18 | float | Velocity587_0 |
| 19 | float | Velocity587_1 |
| 20 | sbyte | Injector1Direction |
| 21 | sbyte | Injector2Direction |
| 22 | sbyte | Axis4Direction |
| 23 | byte[] | AuthTag |

## CommandFrame（15 项）

| 索引 | 类型 | 字段 |
|---|---|---|
| 0 | byte | Version |
| 1 | ulong | SessionId |
| 2 | ulong | Sequence |
| 3 | ulong | MonotonicMs |
| 4 | int | CommandId |
| 5 | ZenohCommand | Command |
| 6 | int | Param1 |
| 7 | int | Param2 |
| 8 | double | Value1 |
| 9 | double | Value2 |
| 10 | string | Payload |
| 11 | byte[] | NonceClient |
| 12 | byte[] | NonceServer |
| 13 | byte[] | Proof |
| 14 | byte[] | AuthTag |

## StateFrame（51 项）

| 索引 | 类型 | 字段 |
|---|---|---|
| 0 | byte | Version |
| 1 | ulong | SessionId |
| 2 | ulong | Sequence |
| 3 | byte | AuthState |
| 4 | byte[] | NonceServer |
| 5 | ulong | EchoMainMonotonicMs |
| 6 | uint | Flags |
| 7 | int | Mode |
| 8 | int | Phase |
| 9 | int | SelfCheckStatus |
| 10 | int | AdsState |
| 11 | ushort | Cylinder1 |
| 12 | ushort | Cylinder2 |
| 13 | ushort | Cylinder3 |
| 14 | ushort | Cylinder4 |
| 15 | byte | CylinderManualMask |
| 16 | sbyte | Injector1Direction |
| 17 | sbyte | Injector2Direction |
| 18 | sbyte | Axis4Direction |
| 19 | float | Force582F |
| 20 | float | Force582N |
| 21 | float | Force587F |
| 22 | float | Force587N |
| 23 | float | CleanForceN |
| 24 | float | AdsActualHz |
| 25 | float | AxisPos0 |
| 26 | float | AxisPos1 |
| 27 | float | AxisPos2 |
| 28 | float | AxisPos3 |
| 29 | float | AxisPos4 |
| 30 | float | AxisPos5 |
| 31 | float | AxisPos6 |
| 32 | float | AxisFromLeft0 |
| 33 | float | AxisFromLeft1 |
| 34 | float | AxisFromLeft2 |
| 35 | float | AxisFromLeft3 |
| 36 | float | AxisFromLeft4 |
| 37 | float | AxisFromLeft5 |
| 38 | float | AxisFromLeft6 |
| 39 | bool | HasNetDistance |
| 40 | double | Axis1NetDistanceMm |
| 41 | double | Axis6NetDistanceMm |
| 42 | byte | PrepareTaskState |
| 43 | int | PrepareTaskCommandId |
| 44 | byte | StartTaskState |
| 45 | int | StartTaskCommandId |
| 46 | CommandResultFrame[] | Results |
| 47 | string | EventText |
| 48 | float32 | Axis6WindowStartFromLeftMm（距左限位，仅显示） |
| 49 | float32 | Axis6WindowEndFromLeftMm（距左限位，仅显示） |
| 50 | byte[] | AuthTag |

## ForceFrame（13 项）

| 索引 | 类型 | 字段 |
|---|---|---|
| 0 | byte | Version |
| 1 | ulong | SessionId |
| 2 | ulong | Sequence |
| 3 | ulong | EchoMainMonotonicMs |
| 4 | bool | Enable582 |
| 5 | sbyte | Axis582 |
| 6 | float | Force582N |
| 7 | float | Torque582Nm |
| 8 | bool | Enable587 |
| 9 | sbyte | Axis587 |
| 10 | float | Force587N |
| 11 | float | Torque587Nm |
| 12 | byte[] | AuthTag |

## CommandResultFrame（3 项）

| 索引 | 类型 | 字段 |
|---|---|---|
| 0 | int | CommandId |
| 1 | byte | Result |
| 2 | string | Reason |

## 命令及状态语义

`ZenohCommand`：0 Hello，1 Authenticate，2 Acquire，3 Release，4 PreparePosition，5 StartControl，6 ForceFeedback，7 Cylinder，8 ArmManualEnable，9 ArmAxisEnable，10 ArmAxisReset，11 ArmAxisJog，12 ArmCartesianJog，13 ArmStop，14 ArmProgramZero，15 ArmCartesianParameter，16 ArmCartesianAlive，17 YValve。没有软件急停命令；ArmStop 仅停止定位臂，PLC 急停继续走原独立路径。

PreparePosition 的两个 double 是导管与导丝准备位置；Cylinder 的 Param1 为 1..4，Param2 为打开/恢复；YValve Param1 表示关闭；定位臂轴号 1..5；ArmAxisJog 的 Param2 为方向 -1/0/1，Value1 为统一速度倍率：快=2.0、中=1.0、慢=0.5，旧消息的 0 按中档处理，停止不受倍率影响。倍率随每次点动/保活发送，按各轴原基准速度计算，不累乘；默认中档。末端点动速度和参数继续沿用现有 ×1000 整数单位。ForceFeedback 开启沿用零点采集后开启的流程。

AuthState：0 未认证、1 挑战、2 已认证。准备和开始控制任务状态：0 空闲、1 运行中、2 完成、3 拒绝/失败，分别带关联 CommandId。Results 最多 32 项，Result=0 完成、1 拒绝；定位臂点动和保活不入结果队列。普通短命令 Result=0 表示通过服务门控且已由原控制循环消费，不等同于实体机构已到位。准备、开始控制、力反馈切换、电缸和 Y 阀由实际状态快照确认完成。

## 认证、时钟与队列

两端 `config/remote.token` 相同，UTF-8 去 BOM/首尾空白后至少 16 字节。主端 Hello 携带 16 字节随机 nonce；数据端回送 16 字节随机 nonce；Authenticate 的 Proof 为 HMAC-SHA256(token, ASCII "auth" + nonceC + nonceS)，32 字节。session key 为 HMAC-SHA256(token, ASCII "key" + nonceC + nonceS) 的前 16 字节。

已认证消息的 AuthTag 为 HMAC-SHA256(session key, 本消息 AuthTag 置空 BIN 后的规范序列化字节) 的前 16 字节。两端字段次序、整数编码和浮点位宽必须相同；握手帧按各自握手字段验证。handle 和 command 始终带 SessionId；认证成功才替换会话，旧会话的命令不得执行。各 Topic 序号严格递增，拒绝重复和乱序。

MonotonicMs 来自发送方单调时钟。ADS 原样 Echo 最近有效主端时间；主端用自己当前单调时间减 Echo 计算 RTT。不同机器的单调时间不作大小比较，不携带 Unix 时间，不做未来时间检查。

ADS 命令接收 FIFO 为 256 项，满时丢弃最旧并记录诊断；本地记录入队时刻，消费时超过 2000 ms 拒绝。接收回调不运行控制逻辑，高频手柄走独立最新值路径。控制循环按 FIFO 顺序执行原有安全校验。主端仅在 publisher 可用时发送，不缓存、不补发。

IsConnected = Zenoh Session 可用，并且最近通过协议/认证/session/序号校验的 state 接收间隔 <2000 ms。该值不代表 PLC 已就绪。原控制权租约与手柄失联保持继续保留，不新增软件急停。Flags bit12 为 CleanForceValid；该位为 0 时线上 CleanForceN 为 0，主端本地域将其还原为无效值。

## 验证入口

共享黄金 Hex 样本位于包根 protocol/fixtures/zenoh-v1，两端协议测试读取同一组固定文件；从包根运行 `验证记录/20261020_Zenoh迁移/验证.ps1`（完成后以脚本实际输出为准）。测试使用本机回环和模拟控制状态，不启动 ADS.exe，不连接 PLC。
