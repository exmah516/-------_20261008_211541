# 第五轮修订：可靠性与自动恢复

日期：2026-11-07；补充更新：2026-11-08。[本次仅四项补充的报告](supplement/report.md)。工作目录为介入机器人，修改前提交为 806036a8f0b65a2d6557b3f7d47bcf5f65990acf。

本轮三端源码、独立手柄宿主、离线替身和测试均已写入工作区。四项 Windows 构建为 0 警告、0 错误。PLC 未构建、未下载、未激活。

**离线软件验收与现场验收分开记录：** 本次补充已实现“任一拔出同时撤销两柄并清力重启”，不再要求单柄设备映射；已新增100Hz速度记录和待现场数据代入的追赶公式，且补齐SelfCheck全过程对拍。真实USB/驱动恢复、NC位置保持前提、电子限位机械动作与PLC原生构建仍未执行。定位臂既有S1/S2/S4失败与基线相同，本轮不修复。

## 0. 基线、范围和交付文件

开工前已通读 reliability_survey_report.md 第 1、2 节及本轮对应正文。报告调查基点是 713d…，本轮实际修改前基点是上面的 806036…；事实和数值按本轮源码核对。

- 修改前源码与哈希：[baseline/source.zip](baseline/source.zip)、[baseline/source_hashes.json](baseline/source_hashes.json)；禁止把它们作为当前运行包。
- 修改摘要：本报告 R1–R10。
- 全部源码、配置和测试差异：[full.diff](full.diff)，配套 [patch_manifest.json](patch_manifest.json)。共用文件在 diff 中只出现一次，以下按 R 编号说明各自变更，避免重复应用同一 main.cpp 补丁。
- 低频搬迁前清单：[low_frequency_inventory.md](low_frequency_inventory.md)。
- 构建：[build/](build/)；最终既有回归：[final_native.log](final_native.log)、[final_zenoh.log](final_zenoh.log)、[after/](after/)。
- ST 对拍与条件计算：[st/results.log](st/results.log)、[st/catchup.csv](st/catchup.csv)，生成的 Old.h／New.h／Common.h 可查看翻译结果。
- SDK 静态证据：[sdk_probe.json](sdk_probe.json)、[sdk_exports.txt](sdk_exports.txt)、[sdk_disassembly.txt](sdk_disassembly.txt)、[sdk_devices.json](sdk_devices.json)。
- 编码、PLC XML／工程引用、类型尺寸及未改动源文件检查：[delivery_audit.json](delivery_audit.json)。

父目录已有的“造影机器人/261010/下位机”子模块用户改动保持原样。测试重建的三个已跟踪 Zenoh EXE 已恢复基点内容，运行日志保留在本目录。没有提交 commit。

父级 .gitignore 的 ADS项目/ 规则会忽略本轮新增的 return_recovery.h、homing_record.h、handle_velocity_recorder.h、reliability_config.json／md；五个文件已包含在完整 diff 和清单中。后续若用 Git 提交或打包，不要只取普通 git diff，须按清单纳入这些文件（Git 提交时需强制添加）。

以下四项仍保留：handle 回退状态 30 内恒假的 IF return_state[i] <> 30；定位臂第四轮 B 奇异裕量；B08 单／双柄模式锁；SelfCheck 无取消入口。SelfCheck、clear_err、定位臂 PLC POU 源文件与基线字节一致。

## R1. 回退失败恢复与一次续做

位置：B数据端/工程/ADS项目/return_recovery.h、main.cpp。

ReturnRecovery 独立于 main，管理涉及轴 mask、恢复条件、250 ms Clear 重提、按原始触发统计的一次续做。失败仍执行原有停控、清力、取消本地任务及 Clear；return_ads_fault_hold 只表示恢复进行中。

解除要求 Running、新鲜有效快照、无急停／主机超时、handle_reinit_done、所需手柄有效、无启动／其它独占任务，以及涉及轴 Req／Busy 全 FALSE。Clear 提交成功后还必须看到更新的快照，不能拿提交前的 FALSE 当确认。Clear 周期重提只依赖 Running 和有效快照，不能等待 handle_reinit_done，避免 PLC 等撤 Req、数据端等初始化的循环等待。

恢复调用原 sync_all(20)，control_active 仍由 startup.completed 和 Done 决定。保存原 mode、scope、触发方向和双柄换手意图；续做按当前位置、当前窗口重算目标，再进入原 begin_planned_return 和全部目标安全检查。原首次握手安全重试一次未改变，也不消耗本次续做次数。

失败或急停打断未完成的回退最多自动续做一次；第二次失败只恢复当前位置控制；正常交接完成清零计数。运动已经 Done、仅交接被打断时，不重复整段运动。目标安全拒绝后保持恢复后的当前位置控制。

验证：recovery_tests.cpp 的 129 项生产类／记录断言；main_recovery_tests.cpp 抽取真实失败、急停、取消、恢复门控、规划入口、安全校验和续做分支；含 ACK 超时、执行超时、PLC Error、Clear 结果失败、Clear 入队失败重提、第二次失败、双方向、完成清计数，以及 PLC 重初始化期间持续 Clear。

## R2. ADS 断线与低频通道

位置：ads_communication.h／cpp；全部低频调用方清单见独立文件。

- kHardTimeoutMs 从 300 改为 1000。原快照等待 300 ms、运行期单次 ADS 调用 300 ms、连接握手 1000 ms、PLC 心跳看门狗 2 s 保持不变。
- 100 Hz 快线程保留快照、核心输出、心跳／会话、watchdog 恢复和计划回退专用 Prepare／Commit／Clear。
- 独立 CADSComm、AMS 端口和慢线程执行 read／write／read_sum／write_sum。调用方函数签名不变，原同步请求的调用先后仍保留。
- SoftHold 不清失败队列，先用核心读写完成一个完整成功周期，下一拍才消费尚未开始的专用回退命令。真正断线、PLC 非 RUN／重启、服务停止仍失败清队列。已经开始的原生请求若实际读写失败，仍返回失败；这与“排队请求因 SoftHold 被取消”不同。
- 慢通道连续两次实际请求失败后自行关闭，1 s 间隔重连；快通道重启 epoch 变化时重建端口和句柄。跨重启的旧请求不能返回成功。慢通道不改变快通道状态机。

排队阶段 API 的 timeout_ms 参数仍存在，但不再因等待超时取消排队；否则无法满足 SoftHold 保队列。原生 ADS 执行时限仍由 CADSComm 的 300 ms 设置控制，已开始请求和同步调用仍可能等待。这一语义变化在调用清单中逐项核对过。

原 fail_queued 调用分类：

| 位置／入口 | 处理 |
|---|---|
| stop／快线程退出 | 保留，服务停止 |
| 建连失败且连接未初始化 | 保留，尚未连接或真正重连 |
| ReadDeviceState 成功但 PLC 非 RUN | 保留，PLC 停止／重启 |
| ReadDeviceState 本次失败 | 仅达到 1000 ms 硬超时才清队列、CloseComm |
| 快照／核心输出失败 | SoftHold 保队列，累计失败达到硬超时才清队列、CloseComm |
| PLC 时间倒退／应用改变／重启重建 | 保留，重启 |
| clear_runtime_connection_state | 保留，仅用于断线／重建／停止；没有 SoftHold 调用方 |

300 ms 相关注释和代码已检查：运行期 SetTimeout(300) 的注释明确是单次调用；ADSComm 原默认 300、main 等快照 300、定位臂自己的续期时限均不是累计断线阈值，不改。原 reconnect_count 处理块保持。

V3 真实服务双线程＋CADS mock 结果：

- 精确一次读失败、20 ms、500 ms 的读失败／核心写失败：不断线，排队回退和低频请求成功恢复。
- 1200 ms：执行 CloseComm，排队请求失败，随后重连。
- 慢请求阻塞 2 s：快照增加 200，心跳增加 200；最终定向运行 RTT 为 27 μs。该数值是本机替身测量，不代表实网 RTT。

## R3. 采样间隔与恢复原因

位置：手柄/手柄.cpp、Handle.h、remote_handle_bridge.h、main.cpp。

重建基准的接收间隔条件为严格大于 1500 ms；断开为样本龄期严格大于 5000 ms；速度清零仍是 250 ms。主端 valid=FALSE 直接忽略，不覆盖有效样本、不刷新接收时间。

主端 GetSample 只把新的宿主序号作为有效新样本，避免宿主卡住时把同一缓存重复包装为新样本。数据端 close 保留暂停前接收时刻，用于断开后重接时判断重建。

日志／EventText 分开显示 ADS 快照恢复、手柄 587／582 采样间隔 X ms 后重建、连接保持结束。初次尚无样本显示“无有效样本或采样断开”，不会虚报已等待超过 5 s。

补充：新增独立100Hz接收速度CSV及退出四关节最大值，原始SDK速度与关节量接收差分速度都保留；不改控制。最大速度×1.5s及各轴PLC限速估计见[补充第2项](supplement/report.md)。以下全局物理上界讨论保留为解释，不能用测试注入值代替现场数据。

### 1.5 s 停顿的追赶量与条件计算

在不超过重建门槛且没有其它保持条件时，恢复后的有效样本仍沿用之前基准，因而会追赶停顿期间的手柄位移。既有 PLC 速度限制及轨迹滤波平滑此变化；本轮没有新增限幅。

源码换算为直线量 Δx = 750 × Δh mm，旋转量 Δθ = −57.29577951308232 × Δr deg。主端信任边界只要求输入是有限浮点数，没有物理最大 Δh、Δr 或手柄速度。仅凭停顿 1.5 s 无法推算各轴最大追赶量和最长追赶时间；该“全局最大值”子项按用户约束停止，等待现场提供物理上界。

| 轴 | 源码能确认的量／限制 | 1.5 s 对应的全局最大追赶量 |
|---|---|---|
| 1 直线 | 固定活动窗口 5～30 mm，单个未移动窗口内跨度最多 25 mm | 跨换手、移动基准等不能当作同一个固定窗口，无法给全局值 |
| 2 旋转 | 旋转换算；本体轨迹滤波不启用，v_limit 为每扫描增量 | 缺物理转角上界 |
| 3 直线 | 模式、夹持和投送停止位控制；轨迹速度 80 mm/s、加速度 2000 mm/s² | 缺手柄位移与初始姿态上界 |
| 4 旋转 | 手动／联动路径；轨迹滤波不启用 | 缺输入和动作时长上界 |
| 5 直线 | 模式／间距约束；轨迹速度 80、加速度 2000 | 缺手柄位移与初始姿态上界 |
| 6 直线 | 相对轴5窗口 1～33 mm，固定轴5时跨度 32 mm | 轴5会移动，不能把 32 mm 当全局值 |
| 7 旋转 | 同旋转换算／每扫描增量限制 | 缺物理转角上界 |

v_limit = [7.5,7.5,1.5,1.5,1.5,7.5,1.5] 是每个 1 ms 扫描的最大参考增量，不能误写为 mm/s 或 deg/s。直线轨迹 wn=60、vmax=80、amax=2000；旋转没有这组轨迹速度限制。手柄低通 α 为直线 0.25、旋转 0.20，100 Hz 下到 99% 分别约 170／210 ms。

为使计算可复现，run_st.py 另用真实 handle ST 转译体计算“静止起点、固定参考阶跃、普通路径、无换手／限位／运动FB完成反馈”的条件响应：

| 轴／条件目标阶跃 | 到位置误差 <0.001 且轨迹速度 <0.01 的首次时刻 |
|---|---|
| 1：7.5 mm；25 mm | 662 ms；881 ms |
| 2：5.72958° | 21 ms |
| 3：7.5 mm | 287 ms |
| 4：5.72958° | 23 ms |
| 5：7.5 mm | 287 ms |
| 6：7.5 mm；32 mm | 662 ms；968 ms |
| 7：5.72958° | 32 ms |

7.5 mm／5.72958° 分别是假定 Δh=0.01、Δr=0.1 的示例，不能由 1.5 s 自行推出。这是参考链路的条件数值计算，未包含主端低通、主循环其它模式、真实伺服跟踪及机械负载；旋转分支 traj_vel 固定为零，表中判定不代表真实旋转速度已为零。完整数值见 catchup.csv。

V4：真实 Handle 的 1.0／1.6／4／6 s、valid=FALSE 0.3／6 s 及接收时间不更新测试通过；恢复原因字符串测试通过。V6 再以真实 Zenoh 传输持续 6 s 无有效采样，确认断柄后恢复基准和控制资格。

## R4. SDK 隔离与宿主管理

位置：A主端/工程/master/VI.HandleHost、VI.Service.Robot/Services/HandleService.cs、HandleHostMemory.cs、HandleDeviceNotifications.cs；工程／输出目录同步修改。

全部 FLCatheterNative 调用只编译进 VI.HandleHost。主端只监管进程、读采样缓存和写力缓存；网络、state 回调和 UI 的锁中没有 FlCatheter 原生调用。

IPC 采用 1024 字节命名共享内存：采样区偏移 128、力区偏移 512；两区各单写、序号锁，读交叠则保留上次缓存，不等待宿主。头部有心跳、停止标志和错误码。选择共享内存是因为固定大小 200 Hz 双柄采样／力输出不需要请求应答；命名管道的同步读写会引入本轮要消除的阻塞关系。每柄有独立 valid 和序号，主端仍每 10 ms 发 HandleFrame。

宿主退出、IPC 心跳超过 1 s、Cypress 接口重新插入都会终止旧宿主并重建，两次实际启动至少间隔 1 s。采样循环以 Stopwatch 截止时间和 1 ms Windows 定时分辨率调度到 200 Hz。原 ApplyForce 的轴／力／扭矩、3 s 新鲜度、失效清零及 100 ms 重复零力规则保留。DLL／入口缺失仅宿主报告错误并退出，主端继续监管与提示。

### SDK、设备识别和停止实施的子项

随仓库 DLL SHA256 为 2788f7b266917e259395cdbd8f7bfcbbbe4ea05a8f0980827068710bfbf3dca0，共 23 个导出。存在 deviceStatus（RVA 0x6150）和 getSerialNumber（RVA 0x6120）。反汇编显示分别读取对象缓存偏移 0x894／0x890，不能仅根据导出名证明 USB 拔出后会立即变化。

DLL 文件偏移 0x9030 含设备接口类 {AE18AA60-7F6A-11D4-97DD-00010229B959}；主端使用 CM_Register_Notification 注册该类。系统枚举没有匹配 Cypress／Catheter／VID_04B4 的现有设备，因此 VID／PID 尚未核实。SDK 私有序列号 582／587 与 Windows 接口路径不存在已证实映射。

用户补充明确允许保守双柄处理：类级任一移除通知立即清空两柄采样和两路力目标，禁止旧宿主缓存重新置valid以及重启期间写入力；宿主沿既有退出清力路径结束，阻塞时按既有1s退出规则终止，继续遵守启动间隔至少1s。无需接口路径到582/587映射。插入通知同样重建宿主。

V5最终模拟SDK结果：200.0Hz，151/0；通知到两路原生力/扭矩清零6.4ms，新宿主启动到双柄有效为拔出45.8ms、插入52.0ms。实际Windows设备通知与USB驱动耗时仍须现场验证。完整口径、测试和日志见[补充第1项](supplement/report.md)。

## R5. Zenoh 已关闭会话重建

位置：RemoteRobotLink.cs。

ConnectAsync 遇到非空但不可用的旧会话，先在主锁之外释放旧会话，再创建新会话。CheckConnection 自动以不小于 1 s 间隔重建，走原 Hello／认证。保留先前控制权申请意图；清掉连续运动方向和待处理旧命令，不重放旧运动。

V7 用实际本机 Zenoh native session 关闭事件验证重新 Hello、认证及 Acquire；通过。V6 新主端对象也能在同一数据端模拟器进程中完成新 Hello／认证／Acquire。没有更改认证、租约和正常控制权申请规则。

## R6. 电子限位退离

位置：err.TcPOU、G.TcGVL、handle.TcPOU 的常量引用。

仅自检完成、无其它错误且全部上电时，对轴 1／3／5／6 的 DriveLimitActive 做退离。先确认外部给定已禁用；按 ActPos 到 leftlimit 与 right_slimit 的距离择远离近端方向，距离相等选正向。right_slimit 统一使用 G.rslimit_distance，原 handle 常量移到 G，共享同一来源，无复制数值。

每轴独立 MC_MoveRelative，±2 mm、5 mm/s、加减速 50 mm/s²；最多三次。Done 后仍需限位解除才允许走原 _clear_err 出口；Error／CommandAborted 回原复位流程。三次仍压限位设置 G.err_backoff_failed，继续原 err 循环。期间 estop_hold_req=TRUE、外部给定保持禁用。未自检按原流程，不退离。

V8：左／右方向、成功、三次失败、未自检通过。替身不模拟机械移动和限位信号，真实退离量须现场确认。没有新增左侧软限位。

## R7. Axis15 独立恢复

位置：Axis2Sync.TcPOU、err.TcPOU、G.TcGVL、ADS 快照和 EventText。

本体 err 的 has_err／has_other_error 去掉 axis2_sync_axis；handle 原本体故障判断不新增 Axis15。Axis15 错误独立触发 200 ms MC_Reset，每 2 s 重试；与轴2 Reset 的原跟随条件 OR 合并。成功清除同步内部 latch，沿原条件重新 GearIn。

G.axis2_sync_error／error_id 提供给数据端并显示 Axis15 原因。V9 验证 Axis15 单独错误时本体保持 _handle、200 ms 脉冲、2 s 重试、恢复后同步；正常同步 1000 周期新旧一致。

## R8. handle 等待出口及 Clear 语义

位置：handle.TcPOU；超时／次数／取消码集中在 G 常量。

- 初始化先做七轴错误／未上电判断，再进入原初始化等待。Enable Error 拉低 Execute 一个完整周期后再触发，最多 3 次，2 s 未全 Done 转 _err。
- 计划回退 10／20：Disable Error、该状态 2 s 超时、Req 撤销都以 return_finish_error 进入 30。20 中先 MC_Stop，等停稳并释放 Stop 后再 Enable。
- 31：Enable Error 同样下降沿重触最多 3 次，2 s 未成转 _err。
- 轴4 10／30／40：对应 Disable／Stop／Enable 的 Error 重触最多 3 次；2 s 结束错误或 Enable 失败转 _err。每次切换状态重置相应计时。

**Clear 核对结论**：main 的 cancel_active_return_motion／专用 Clear 把 Req 拉低，用来撤销事务并要求 PLC 受控停稳，不是“只清显示状态”。新 PLC Clear 出口与设计一致。取消 ErrorId 定义为 16#80005001；真实 FB Error 使用原 FB 的 ErrorID。仅超时且 FB 未报错时，按规格取 FB 错误码，可能是 0，没有发明其它超时码。

数值注意：规格明确要求状态 20 也在 2 s 出口，故超过 2 s 的运动会先由 PLC 结束；数据端原执行 5000 ms 保持。不能把两者解释为相同超时，也没有为了保留长运动而放宽 PLC 的 2 s 要求。

V10 覆盖 init、10／20、31、轴4 10／30／40 的出错重触成功、2 s 超时、Clear 中止运动、Stop 释放。未触发新出口的正常回退对拍相同。

## R9. 寻参记录、复用和自动恢复

位置：homing_record.h、reliability_config.json／md、main.cpp、MAIN.TcPOU、G.TcGVL。

唯一新增生产配置 homing_reuse_enabled 默认 FALSE。运行目录配置只在进程启动读取；在现场前置测试通过前保持 FALSE。构建仅在目标配置不存在时复制默认文件。

记录包含 leftlimit[7]、init_pos[7]、完成时间、last_abs[7]、位置时间及 PLC 应用名。自检完成上升沿登记待写，消费原有坐标刷新成功事件后原子写；刷新失败不抢写旧值。随后仅 Running、有效新快照、自检完成时每 1 s 更新位置。断线／重启／新 SelfCheck／复用事务期间冻结，不覆盖断开前最后值。临时文件写完并关闭后 MoveFileEx 原子替换。

检测 PLC 重启后，在 _self_check 尚未开始、七轴 Done／axis Error／DriveDeviceError／Power Error／Reset Error 满足健康条件时判定。复用待判定时应用名也可在自检等待态读取，不能等待 self_check_done 才读，否则会互相等待；普通启动的应用名读取门控保留。当前绝对位置直接读取 G.axis[i].NcToPlc.ActPos；_self_check 等待态不刷新 G.Act_pos，不能用其默认值冒充当前 NC 位置。读失败拒绝复用并说明。

开关、记录完整性、应用名、七轴容差全部通过才提交两数组、清状态、最后置 req。直线轴 1／3／5／6 为 0.5 mm，旋转轴 2／4／7 为 1.0°；边界值允许。PLC 拒绝码：1 已开始或轴未就绪；2 Disable Error。

### 外部给定状态一致性结论

SelfCheck 原完成块把 SetPointGenDisable.Execute 置 FALSE；直接从 _self_check 等待态恢复时，不能仅凭此证明外部给定已经禁用。新增恢复请求先拉低 Enable、调用 Disable 并等 Done／Enabled=FALSE，再复刻完成块的电缸、坐标、参考、self_check_done、handle_reinit_req 和 _handle；只把 startup_loading_ready 强制 FALSE。随后仍进入原 handle 初始化 Enable 链路。这是实现一致性所需步骤，未改变正常 SelfCheck。

ST 测试逐变量比较收尾：leftlimit、init_pos、电缸1～4、self_check_done、handle_reinit_req、Act_pos、act_pos_from_left、refer、ref_slow、act_h、act_hf、refer_from_left、gen_state 全相同；startup_loading_ready 是规定差异。原完成块没有写 cylinder5，本轮不添加其值。

复用回执 applied 会重新触发自检完成事件／坐标同步路径。重启前控制中的会话重新获得 Done 资格，在当前位置 sync 后自动恢复；原先未控制的仍 WaitForEnter。拒绝走原流程，日志／UI 附原因。

V11：完整记录往返／原子替换、1 s 更新、冻结规则、坐标刷新未成功不落盘、位置有效而完整快照无效不落盘、重新 SelfCheck 覆盖；开关关、应用不同、各轴越界、缺记录及容差边界；PLC 收尾和复用回执恢复控制资格均通过。驱动器断电后能否被绝对位置容差拒绝，必须现场测量；软件不凭“掉电事件”推定位置变化。

## R10. 恢复状态文字

复用协议 v1 已有 EventText（key 47），不改线上 schema、版本和 golden vectors。VisState 新增本地 recovery_text；数据端发布、主端映射到 StatusFrame.RecoveryText，与宿主 RecoveryText 合并为介入面板一条可换行状态文字，并保留可访问性实时播报。

涵盖回退恢复及原因、本体急停、两柄中断／断开／重建、宿主重启、ADS 重连、Axis15 错误及码、寻参复用／拒绝、退离中／失败。不同状态可同时显示。没有更改按钮、控制路径、窗口或模式选择。

## 1. 常量、阈值及重试对照

| 项目 | 当前值 | 来源／说明 |
|---|---|---|
| ADS 累计硬断线 | 1000 ms | ads_communication.cpp kHardTimeoutMs，原 300 |
| ADS 运行单次调用／main 等快照 | 各 300 ms | 原值不改，含义不同 |
| ADS 初次握手 timeout | 1000 ms | 原值 |
| ADS 慢通道重连／连续失败 | 1000 ms／2 次实际失败 | 独立慢线程，不改变快线程 |
| 快线程频率 | 100 Hz | 原值 |
| PLC 主机看门狗 | 2 s | 原值不改 |
| R1 Clear 重提 | 250 ms | ReturnRecovery::clear_interval_ms |
| R1 自动续做 | 每原始触发最多 1 次 | 与原握手安全重试 1 次分开 |
| 原回退 ACK／retry Clear／执行 | 250／500／5000 ms | control_types.h，保持 |
| 手柄重建／断开 | >1500／>5000 ms | 原 >250／>3000 |
| 手柄速度清零 | >250 ms | 原值 |
| 宿主采样／主端发送 | 200／100 Hz | 5／10 ms |
| 宿主心跳失鲜／重启间隔 | >1000／至少1000 ms | 按实际 launch 计时 |
| 力新鲜度／重复零力 | <3000 ms／100 ms | 原 ApplyForce 语义 |
| 宿主单柄 open 失败再试 | 1000 ms | 沿用原打开周期 |
| Zenoh 重建间隔 | 至少1000 ms | 原 Hello／认证逻辑不改 |
| PLC 等待出口／最大重触 | 2 s／3 次 | G.recovery_wait_timeout／max_retries |
| 限位退离 | ±2 mm，5 mm/s，50 mm/s² | G.err_backoff_*，最多3次 |
| 右软限位距离轴1～7 | [99,0,666,0,688,688,0] mm | G.rslimit_distance；原 handle 数值 |
| Axis15 Reset | 200 ms，每2 s | G.axis15_reset_* |
| Clear 取消码 | 16#80005001 | G.return_clear_error_id |
| 寻参位置记录更新 | 1000 ms | main，仅健康有效时 |
| 寻参复用容差 | 直线0.5 mm，旋转1.0° | homing::reject_reason |
| 寻参开关默认 | FALSE | 唯一新增生产开关 |
| PLC 参考增量 | [7.5,7.5,1.5,1.5,1.5,7.5,1.5]／扫描 | 原 G.v_limit，不是速度单位 |
| 直线轨迹滤波 | wn60、vmax80、amax2000、dt0.001 | 原 handle，旋转不启用 |
| 手柄低通 | 直线0.25、旋转0.20 | 原值不改 |

规格中的硬超时 300、采样重建 250、断开 3000 与实际基线一致。需要额外说明的源码事实是：G.v_limit 的单位为“每扫描增量”；SelfCheck 原收尾无 cylinder5 赋值；直接等待自检态的 G.Act_pos 不刷新；调查报告行号随实际基点变化，不能照抄。

## 2. 构建、回归、新用例与一致性结果

所有命令在“介入机器人”目录执行，仅运行测试替身／本机回环 Zenoh，未启动生产 ADS.exe 或真实 SDK。

### 2.1 可复现命令

    $dotnet = & 'A主端/工程/tools/Get-ViDotnet.ps1'
    & $dotnet build 'A主端/工程/master/MasterConsole.sln' -c Release -p:Platform=x64 -warnaserror -v:minimal
    & $dotnet build 'A主端/工程/master/VI.Control.Tests.slnx' -c Release -p:Platform=x64 -warnaserror -v:minimal
    & $dotnet build 'A主端/工程/master/VI.HandleHost/VI.HandleHost.csproj' -c Release -p:Platform=x64 -warnaserror -v:minimal
    & 'D:/Work_software/VS2022/MSBuild/Current/Bin/MSBuild.exe' 'B数据端/工程/ADS项目/ADS.sln' /t:Rebuild /p:Configuration=Release /p:Platform=x64 /nologo /verbosity:minimal /consoleloggerparameters:Summary
    python -X utf8 tools/reliability_round5/run_existing.py after
    python -X utf8 tools/reliability_round5/run_recovery.py
    python -X utf8 tools/reliability_round5/run_st.py
    python -X utf8 tools/reliability_round5/run_host_sdk.py
    python -X utf8 tools/reliability_round5/probe_sdk.py
    python -X utf8 tools/reliability_round5/delivery_audit.py
    node 'B数据端/工程/ADS项目/tools/run_offline_tests.cjs' all 'docs/reliability_round5/final_native'
    & powershell.exe -NoProfile -File 'B数据端/工程/ADS项目/tests/run-zenoh-tests.ps1' -DotnetPath $dotnet

baseline 子命令仅在原始源码上运行过一次，现已有保护禁止覆盖；不要在当前源码上删除基线后重跑 baseline。

### 2.2 构建及既有回归

| 项目 | 修改前 | 修改后 | 证据 |
|---|---|---|---|
| MasterConsole.sln | 通过 | 0警告0错误 | build/master.log |
| VI.Control.Tests.slnx | 通过 | 0警告0错误 | build/wpf.log |
| VI.HandleHost 单工程 | 无 | 0警告0错误 | build/host.log |
| ADS.sln Rebuild Release x64 | 通过 | 0警告0错误 | build/ads.log |
| WPF 运行回归 | 182次通过 | 182次通过（114种标签） | baseline／after/wpf_tests.log |
| Zenoh 原协议／golden／服务测试 | 通过 | 全通过，C++81/0、C#174/0，含新增联测 | final_zenoh.log |
| ADS mock、原离线运动／夹爪／回退／反馈／记录 | 修复测试替身后通过 | 全通过 | baseline/native_repaired.log；final_native.log |
| 净位移／接口回归 | 通过 | 通过 | after/net_distance.log |
| 定位臂既有验收 | S1/S2/S4失败，其余通过 | 相同失败项与数值 | baseline／after/arm_existing.log |
| 定位臂手写参考／ST转译 | 通过 | 761218周期，最大差0 | after/arm_existing.log |
| 运动学 | 20/20 | 20/20 | after/arm_existing.log |
| PLC 原生构建 | 未执行 | 未构建 | 仅ADS开发库；无已配置XAE |

初次 native 基线因既有测试替身缺少实验事件枚举而编译失败，原失败日志未删除；补齐替身定义后在修改生产逻辑前重跑，证据是 native_repaired.log。不能把原失败日志当作产品回归通过。过程中的宿主启动间隔和 V6 观察器测试失败已修正，历史日志与最终日志分开保留。

### 2.3 V1–V11

| 用例 | 离线结果 | 限制／日志 |
|---|---|---|
| V1 | 通过：四类失败、原停机、Clear入队失败重提、恢复、恰好一次续做、再次失败、完成清计数 | main_integration.log；recovery/results.log |
| V2 | 通过：抽取真实急停分支，原取消／清力，恢复后双方向重规划 | main_integration.log |
| V3 | 通过：单拍／500ms／1200ms，独立慢请求阻塞2s | ads_v3.log；final_native.log |
| V4 | 通过：1.0/1.6/4/6s、invalid0.3/6s、原因分类 | handle_test.log；handle_recovery_reasons.log |
| V5 | 模拟SDK／进程监管部分通过，真实宿主200.0Hz，C#151/0 | host_sdk/results.log；已按补充改为拔出双柄撤销；真实USB未测 |
| V6 | 通过：新主端认证、同一数据端模拟器、6s断流后恢复 | final_zenoh.log；未运行生产main完整控制循环 |
| V7 | 通过：实际native会话关闭后重新认证／Acquire | final_zenoh.log |
| V8 | 通过：左右、成功／三次失败／未自检 | st/results.log；未测真实2mm动作 |
| V9 | 通过：Axis15不暂停本体、脉冲与重试、重同步 | st/results.log |
| V10 | 通过：新增出口、Error重触、超时、Clear Stop及释放 | st/results.log |
| V11 | 通过：记录／判定129项组合、记录事件13项、复用回执及应用名门控10项、PLC收尾逐变量比较 | recovery、main_integration、st日志；实机位置保持前提未测 |

V6 模拟器使用真实 Zenoh 和真实 Handle 采样实现，但其控制资格是替身；生产 main 的 sync_all／恢复门控由独立抽取测试验证。两者不能合称真实机器人端到端验收。V5 的 151 项包含共享的协议测试，不能理解为151种故障。

### 2.4 ST 逐周期一致性与静态检查

run_st.py 从 baseline/source.zip 和当前 POU 读取完整 ST 控制语句并分别生成 C++；没有拿新模型与自身比较。FB 输出由脚本注入，TON 用 1 ms 逻辑时钟，MAIN 的 FBD action 用 hook 占位。没有仿真驱动器、NC 或 PLC 实时调度。

| 正常场景 | 对拍周期 | 最大数值差 |
|---|---:|---:|
| handle初始化／跟随 | 1000 | 0 |
| 原单轴计划回退及Clear | 40 | 0 |
| 原普通非限位err | 2500 | 0 |
| clear_err | 250 | 0 |
| 原SelfCheck完成块 | 10 | 0 |
| 完整SelfCheck从MAIN发起到self_check_done=TRUE | 5686 | 0 |
| MAIN正常等待分派 | 20 | 0 |
| Axis2Sync正常同步 | 1000 | 0 |
| 普通err完整MAIN→clear_err→handle重初始化 | 435 | 0 |
| 轴1/3/5/6回退完整运动／Done／Clear | 108 | 0 |
| 合计 | 11049 | 0 |

共同G输出和共同内部状态逐周期相同。总29228个检查，其中新增出口62。完整SelfCheck不写入中间stu，使用MAIN原请求入口并记录七轴全过程，实际到self_check_done=TRUE；仍只仿真NC/FB反馈，不代表物理寻参。TON替身已补ET，使原错开延时生效。MAIN中新err.enter是新增接口字段，其余共同状态及实际err内部状态分别比较；原handle局部all_powered初始化计算仍不当作旧路径输出比较。完整清单、结束条件及轨迹见[补充第4项](supplement/report.md)。

静态自查覆盖五个被修改 PLC 文件的 XML、plcproj 引用、新变量尺寸及集中常量。G 的 BOOL=1、DINT／UDINT=4、七个 LREAL=56、七个 BOOL=7，ADS mock 逐符号检查大小。原文件 UTF-8、BOM、LF／CRLF 已按基线恢复；不做整文格式化。

## 3. 现场验收清单

以下均交给用户在工程机执行；当前代码已改变 PLC／ADS 必需符号，必须三端同步部署。先编译 PLC、下载、激活，再用本轮主端和数据端。正式运行前先确认 R4 的真实USB通知和SDK恢复实测。

1. 寻参完成，记录 NC Axis6–15 实际位置；保持驱动器带电，激活配置／重启 TwinCAT。逐轴核对绝对位置未变化，保存截图／数值。
2. 驱动器断电 10 s 后上电，记录位置变化，核对 R9 七轴容差能拒绝不一致记录。若位置变化不超容差，该前置测试不能算通过，不应直接打开复用。
3. 上述测试通过后，将运行目录 homing_reuse_enabled 设 true、重启数据端读取；重启 PLC，确认“寻参已复用”，原控制会话在当前位置恢复；未控制会话仍待机。
4. 控制中分别拔插 582、587 USB；记录拔出到 invalid／清力时间、插入到采样恢复时间；必须无需重启主端／数据端。另确认任一拔出通知同时撤销两柄、清零两路力并重启宿主，不再要求区分系统通知所属单柄。
5. 控制中拔网线 3 s、8 s 后恢复；分别核对保持后恢复、超5s断柄后恢复，PLC 2s看门狗仍触发且自动恢复握手。
6. 低速让各直线轴触电子限位；核对左右方向、约2mm／5mm/s退离、外给定禁用、急停保持、成功恢复；模拟持续压限位核对三次上限和失败位。
7. 换手中制造 ADS 中断或急停；核对取消停机、Clear确认、原安全校验、自动续做一次；续做再失败只恢复控制；正常完成后下一原始回退重新计数。
8. 仅使 Axis15 报错；核对本体仍 _handle，Axis15 200ms复位／2s重试并在条件满足后重同步。

请在验收日志记录 PLC 工程版本、三端版本、配置开关、轴／手柄编号、实际故障码、触发与恢复时间，以及期望／实际转移。没有现场数据的项目保持“未执行”，不使用离线替身的 PASS 替代。
