# D 超时、保持、租约与恢复

本附件描述当前阈值和执行条件，不提出统一规则或改法。**已确认**仅指代码、历史或离线测试事实；预计连锁影响标为**推断**。所有时间均为本地时钟，不能直接比较不同电脑的时间戳。

路径：`D/`=`介入机器人/B数据端/工程/ADS项目/`；`P/`=`介入机器人/C从端/250902/Untitled2/POUs/`；`G`=`介入机器人/C从端/250902/Untitled2/GVLs/G.TcGVL`。主端裸文件名在 `介入机器人/A主端/工程/master/` 下：`RemoteRobotLink.cs/HandleService.cs/HandleHostMemory.cs` 属于 `VI.Service.Robot/Services/`；`MainViewModel.cs` 属于 `VI.Service.Robot/ViewModels/`；`ZenohSession.cs` 属于 `VI.Service.Net/`；`ProtocolConstants.cs` 属于 `MasterConsole.Protocol/`；`App.xaml.cs` 属于 `VI.Control/`；`PositioningArmPanel.xaml.cs` 属于 `VI.Module.PositioningArm/`。后附逐端明细也遵守这些别名。

## D.1 数据端逐项清单

下表逐行均为**已确认**；“无总期限”表示相应分支没有超时出口，并不保证底层一定永久等待。GetTickCount 为 DWORD 毫秒；GetTickCount64 为 ULONGLONG 毫秒；QPC 和 steady_clock 为各自单调时钟。

| 项目/保护对象 | 数值、时钟 | 到期/失败后的动作 | 恢复条件与相关阈值 | 出处 |
|---|---|---|---|---|
| 主循环等下一 ADS 快照 | 300ms，condition_variable wait_for | 返回失败/无新帧，仍执行后续周期；ads_motion_cycle_valid=false 导致 ADS hold | 新位置有效、Running、self_check_done、handle_reinit_done、非host timeout；并按是否存在planned决定重建基准 | `D/main.cpp:57,969-978,1063-1068,1202-1295`；`D/ads_communication.cpp:242-266` |
| ADS 单次连接阶段调用 | 1000ms，ADS SetTimeout | 连接/握手失败，下次重试 | 至少1s间隔；先local AMS，失败再预设远端 | `D/ads_communication.cpp:1018-1066,825-858` |
| ADS 单次运行调用 | 300ms，ADS SetTimeout | read/write失败，本拍SoftHold；必要时断开 | 完整快读和快写成功才Running | `D/ads_communication.cpp:1059-1066,925-994` |
| ADS硬失联观察窗 | 距last_full_success≥1000ms，QPC | Reconnecting，失败所有排队命令/慢请求，清缓存、关闭端口 | 1s节流重连、重新取句柄/通知/初值；不会自动回放旧低频运动命令 | `D/ads_communication.cpp:16-20,894-919,940-1005` |
| PLC运行状态检查 | 每10次快循环，目标100ms | ADS state非RUN作为PLC重启处理，清连接 | 下轮重建/恢复；不是100ms运动watchdog | `D/ads_communication.cpp:17,864-891` |
| PLC快照时钟停滞 | CycleCount和DcTaskTime同时不变连续3个快照 | 标记重启/重新连接；时间回退也触发 | 新快照时钟推进及重建完成 | `D/ads_communication.cpp:18,1515-1550` |
| ADS快周期deadline | 10ms，QPC | 统计missed_deadlines，跨过已错过deadline，不补跑全部遗漏拍 | 后续继续下一目标deadline | `D/ads_communication.cpp:786-816` |
| 低频ADS调用者等待 | API带50/100/250等timeout参数，实际忽略并cv.wait无时限 | 调用线程等请求完成/失败；传入数值不能当调用者上限 | 慢线程处理、快连接失效清队列或stop才唤醒；运行中排队总时长无上限 | `D/ads_communication.cpp:559-586,2117-2225` |
| 低频ADS底层与重连 | 单请求300ms；失败2次关端口；重试间隔1000ms，GetTickCount64 | 独立AMS端口重开；坐标刷新失败保留pending | 快服务Running才处理；与快ADS读写线程相互独立 | `D/ads_communication.cpp:2044-2115` |
| 手柄最大缓存龄期 | >5000ms，GetTickCount64 | Handle.poll失败，close；单/双拓扑条件置handle hold | 每1s尝试init；重开后连续有效新样本500ms | `D/手柄/手柄.cpp:6,27-104`；`D/main.cpp:1079-1189` |
| 手柄速度新鲜度 | >250ms，GetTickCount64 | fVels2归零；缓存关节和按键仍复制，不独立关闭poll | 新有效帧刷新rx_ms；此阈值不等于位置/按键过期阈值 | `D/手柄/手柄.cpp:91-103` |
| 手柄恢复静置连续性 | 新样本间隔≤250ms，桥内rx_ms；invalid帧立即把continuous_since置0 | 静置计时清零/重开；invalid帧不覆盖最近有效缓存，不刷新缓存年龄 | 只有真实新接收样本持续500ms才完成，重复读缓存不推进 | `D/手柄/remote_handle_bridge.h:61-84`；`D/手柄/手柄.cpp:73-88` |
| 手柄断流后重基准门槛 | 新接收帧距上次>1500ms，或device_epoch变化，或重开 | resume_settling=true，进入handle hold；不立即恢复运动 | 500ms静置完成设置rebase_requested；主循环按实际位置/当前柄重建 | `D/手柄/手柄.cpp:34-88`；`D/手柄/Handle.h:29-31`；`D/main.cpp:1189-1295` |
| 单/双模式初始宽限 | 单只远程柄等待3000ms，GetTickCount64 | 锁单柄；若首次init已单只则直接锁定 | 锁定后没有自动升双柄路径 | `D/main.cpp:124-156,1079-1114` |
| 总connection hold | 无独立倒计时；!initial_sync_done或ADS hold或handle hold | 清手动电缸、PI和力；spacing非ExitSync则reset；活动planned保留 | 有效快照/柄就绪，非planned时sync/rebase后按实际位置保持 | `D/main.cpp:1202-1325` |
| 计划回退夹爪等待 | 1/3夹150、2/4松500、2/4夹150、1/3松100ms，GetTickCount相减 | 达时才推进；本身不是失败保护 | 从各输出generation已被ADS应用起算；等applied的部分未设总期限 | `D/control_types.h:117-124`；`D/main.cpp:2529-2708,3011-3076` |
| 计划回退异步Prepare/Commit/Clear | 250ms，now_ms和ads_command_submit_ms | 判失败/重试；Commit传输不确定时锁possibly_started，不能整组盲重发 | 最多一次且只能在未started/possibly_started的条件下自动重试 | `D/main.cpp:582-654,2639-2660,2710-2996` |
| 计划回退提交保护/清旧状态/ACK | 250ms，GetTickCount now_ms | 到期进入retry或fault hold | 新Busy=true/Done=false事件；漏Busy时新Done=true且到目标可完成 | `D/control_types.h:170-173`；`D/main.cpp:2247-2340,2806-2882` |
| 计划回退执行观察 | 5000ms，从确认started起 | fail、control=false、ReturnRecovery.interrupted、清力/Req | 后续Clear和就绪恢复；PLC自身Move阶段2s另计 | `D/main.cpp:2283-2340,2440-2469,2884-2907` |
| 计划回退RetryClear后等待 | 500ms，从Clear完成起 | 新快照/!Busy/!Error未满足则fail | 一次retry、目标仍安全；清命令自身另有250ms等待 | `D/main.cpp:2909-2996` |
| 计划回退无总期限阶段 | Clamp输出applied、AwaitFreshSnapshot、AwaitHandoffApplied、release applied | 等新序号或generation；没有独立到期分支 | 新快照、rebase成功、输出实际应用；外层hold/急停等仍可接管 | `D/main.cpp:2681-2708,2998-3054` |
| 回退故障恢复清Req | 250ms间隔，uint64毫秒 | 故障保持期间反复Clear；本身无恢复总时限 | Clear已applied后更大快照序号，Running、有效、无estop/timeout、reinit、柄就绪、无exclusive、Req/Busy都false，sync成功 | `D/return_recovery.h:17-60`；`D/main.cpp:1987-2026` |
| 回退故障自动续做 | 最多一次，无按时间强制续做 | consume_resume后增加次数；begin拒绝则不执行原目标 | 原始意图保存，重新计算当前位置合法目标；不是重发不确定Commit | `D/return_recovery.h:53-60`；`D/main.cpp:3628-3663` |
| 晚到的回退ErrorId | 2000ms，GetTickCount64 | 窗口内遇新非零ErrorId补报；过期放弃补报 | 不改变已发生的fault hold | `D/main.cpp:982-1010,2448-2464` |
| 急停恢复请求节流 | 每1000ms，GetTickCount64 | 急停持有期间request_watchdog_recovery | PLC停稳、心跳恢复、显式恢复握手后清锁存；主循环仍要重建基准 | `D/main.cpp:1727-1776`；`P/handle.TcPOU:321-466` |
| 屈曲恢复ClampWait | 150ms，从phase_t0，GetTickCount | 到期Active；未等待电缸反馈或ADS generation | 按键持续且入口几何合法 | `D/control_types.h:146`；`D/main.cpp:3286-3341` |
| 屈曲恢复运动步长 | dt最多50ms，每拍40mm/s帽 | 延迟超过50ms不按全部空档追赶；remaining耗尽不自动退出 | 继续正向axis1增量；松键ExitSync | `D/main.cpp:3343-3388` |
| 屈曲恢复ExitSync | 最少100ms；误差≤.2mm或2000ms后允许同步，GetTickCount | 2s不是报错超时，而是按实际位姿重建的后备条件 | 无hold/急停/fault且有效ADS；sync_all(20)失败下拍再试，无最大次数 | `D/main.cpp:4300-4353` |
| 标准启动夹爪阶段 | 每阶段300ms，GetTickCount | 推下一阶段 | 运动阶段另等位置容差，未设运动总timeout | `D/control_types.h:179-180`；`D/main.cpp:3550-3625` |
| 恢复启动错开 | 1/2/6/7后2000ms加5，再2000ms加3，GetTickCount | 固定时长后启动下一组 | 最后等实际目标；不是每组先到位再等2s | `D/main.cpp:3511-3547` |
| 直接启动/紧急回撤 | 无独立运动总timeout | 一直等待到位或其它门控接管 | .2mm/旋转.2°；限速参数还受PLC约束 | `D/main.cpp:3455-3508,3297-3316,4523-4540` |
| 轴4/注射器UI方向租约 | 500ms，GetTickCount64 | 到期把方向归0 | HandleFrame持续方向每100ms续送；恢复后需新有效方向帧 | `D/main.cpp:508-513,1609-1627,5165-5189`；`D/zenoh_service.cpp:668-680` |
| 力取零 | 3个新快照，每次wait最多500ms | 任一失败立即返回false；最多3次等待，不含其它同步工作时间 | 新force_valid快照；保存四路均值和轴2角度 | `D/main.cpp:759-794` |
| 力反馈保持/平滑 | 换手锁存样本平滑时间常数100ms；交接后移动2mm或400ms开始300ms blend，steady_clock毫秒 | 交接力逐渐混回新实时力 | force/position安全门控仍优先清力；main传steady_clock的int64毫秒，未使用DWORD tick | `D/force_feedback.cpp:21,61-107`；`D/control_types.h:573-591`；`D/main.cpp:4584-4596` |
| tracking夹爪命令稳定 | 150ms，uint32 now_ms | 未满足时不begin_segment；不合格则end/invalidate | 正向Follow、有效夹紧命令/位姿；不是电缸到位反馈 | `D/delivery_tracking.h:92`；`D/delivery_tracking.cpp:128-178` |
| tracking PI dt | 最多0.10s | 超时长增量按封顶dt积分，无输入不自主补偿 | 正向输入/欠账/参数及门控允许 | `D/delivery_tracking.cpp:223-331` |
| tracking内部启用命令的力新鲜度 | QPC差≤frequency/2，即500ms；源QPC必须>0且不在未来 | 缓存过旧时另读force_sample；最终invalid或非正向Follow则拒绝启用 | 仅内部命令，当前公共Zenoh/WPF未映射；fallback后按sample.valid判 | `D/main.cpp:5031-5075` |
| 旧PLC回退状态缓存 | 100ms，GetTickCount64 | 缓存过期重新低频读取 | 当前统一planned以事件/快照判断；缓存只用于兼容I/O helper | `D/plc_io.cpp:111-123,343-398` |
| 寻参记录 | 每1000ms更新last_abs；记录无年龄expiry | 只刷新磁盘记录；不能因时间旧就判失效 | 配置true、app名相同、位置容差、PLC restore ACK才复用 | `D/main.cpp:1378-1456,1944-1970`；`D/homing_record.h:84-95` |
| 记录对齐与写线程 | 手柄历史100ms；writer空队列sleep2ms/64行fflush | 过旧handle匹配无效；队列满记丢样；磁盘失败停记录 | 对机器人动作无直接控制恢复 | `D/main.cpp:1472-1484,4608-4680`；`D/async_csv_writer.h:14,168-191` |

## D.2 数据端网络与定位臂

| 项目 | 数值/时钟 | 到期动作/恢复 | 上下游关系（均已确认） | 出处 |
|---|---|---|---|---|
| Zenoh挑战 | 10000ms，GetTickCount64 | 超期Authenticate拒绝；新Hello建立新挑战 | 主端认证也等10s | `D/zenoh_service.cpp:688-728` |
| 命令排队新鲜度 | >2000ms，入队本地时间 | 消费时拒绝，累计expired；FIFO256满时丢最旧 | 主端命令等待期限另见D.4 | `D/zenoh_service.cpp:25-30,491-507,744-756` |
| 控制域状态新鲜度 | >500ms | 不发state帧；除Release外不接受消费业务命令 | main重新publish_state恢复；worker的33ms状态发送deadline不等于控制域状态更新 | `D/zenoh_service.cpp:767-769,1021-1028` |
| 控制权 | 无“断手柄N毫秒自动Release”计时 | 持有到显式Release或新认证会话替换；断流不撤认证租约 | main另以handle hold门控动作 | `D/zenoh_service.cpp:722-733,773-785,962-980,1005` |
| 持续方向续租 | 同方向非零每100ms，GetTickCount64 | 向main合并投递轴4/注射器续租动作 | 保护端为main500ms租约 | `D/zenoh_service.cpp:668-680` |
| Prepare任务 | 300000ms | state更新时判失败；成功以self_check_done为准 | 主端等待330000ms；不是PLC自检总上限 | `D/zenoh_service.cpp:392-396,838` |
| StartControl任务 | 3000ms | state更新时仍未control_active则失败 | 主端等待10000ms；没有直接撤销已受理运动 | `D/zenoh_service.cpp:442-445,850` |
| ForceZero/Set/Hold、Cylinder、Y阀任务 | 各3000ms；ForceZero成功需Enable则再开3000ms ForceSet窗口 | state更新时出结果/移除pending；至多8任务 | 主端FF/Zero等15s、ForceHold5s、Cylinder/Y阀10s | `D/zenoh_service.cpp:398-440,787-792,871-887,915-916,953-954` |
| Force发布 | 10ms steady_clock；没有state_snapshot年龄判断 | 认证且持租约时继续取bridge输出和旧ff开关，state停止500ms不在这里清力 | 主端2s状态门控/3s力tick下游仍监视；主循环阻塞时的保持力时长需验证 | `D/zenoh_service.cpp:993-1017,1112-1132` |
| router探测/启动 | 端点首探测150ms；启动后最多40次，每次探测100ms+sleep100ms；停止进程等2000ms | 端点已有则复用；启动失败返回false；超时终止自管进程 | 只在start/stop，非持续连接watchdog | `D/zenoh_service.cpp:96-128,224-311` |
| 定位臂快照周期/轴方向租约 | 50ms轮询，方向300ms，steady_clock | 到期清jog正/反dirty，下一慢写归零 | 主端100ms重复；低频请求无调用者deadline会拖延清理 | `D/arm_manual_ads_service.cpp:13-17,207-223,483-556` |
| 定位臂末端点动租约 | 500ms steady_clock | clear_cjog，mode/dir/speed归零，dirty写PLC | PLC独立alive也500ms；输入租约与PLCalive不是同一计时器 | `D/arm_manual_ads_service.cpp:251-263,483-508,635-642` |
| 程序归零用户保活 | 300ms steady_clock | abort(1005)，写curve_cancel | UI100ms Alive；新归零请求重新置deadline；无UI Alive不能持续归零 | `D/arm_manual_ads_service.cpp:265-301,338` |
| 程序归零状态年龄 | >200ms steady_clock | abort(1004)，curve_cancel | 50ms读取新状态；即使PLC heartbeat正常仍可本地中止 | `D/arm_manual_ads_service.cpp:333-337` |
| 程序归零请求ACK | >2s且curve_sequence未匹配 | abort(1007)，curve_cancel | PLC request号改变触发新曲线；曲线本身按规划时长执行 | `D/arm_manual_ads_service.cpp:353-360,383-403` |
| 定位臂ADS参数 | 250ms传read_sum/write_sum | 实际低频API忽略此调用者timeout，底层300ms | 失败保留dirty/invalid；无法把50ms循环当严格deadline | `D/arm_manual_ads_service.cpp:16,315,390,642,718`；`D/ads_communication.cpp:580-582` |

## D.3 谁监视谁与跨端联动

| 监视方 | 被监视对象 | 时限/触发 | 动作 | 出处与确定度 |
|---|---|---|---|---|
| 主端HandleService | 手柄宿主/heartbeat/USB | 1s heartbeat、USB事件、进程退出 | 清样本/力，退出或杀宿主，1s节流重建 | 已确认：`HandleService.cs:92-179` |
| HandleHost | 主端force目标 | 3s tick | 清环境力，停止阻尼使能；每100ms至少送零力 | 已确认：`介入机器人/A主端/工程/master/VI.HandleHost/Program.cs:104-140` |
| 主端RemoteRobotLink | 数据端认证state、Zenoh session | state2s、重认证3s、重建1s | 关资格/力/持续方向，重新握手 | 已确认：`RemoteRobotLink.cs:87-110,536-568` |
| 数据端Handle/bridge | 主端每只柄有效采样 | 250/1500/5000ms及epoch | 归零速度/恢复静置/关闭重开 | 已确认：`D/手柄/手柄.cpp:52-103` |
| 数据端main | ADS新运动快照 | 等300ms/每拍资格 | connection hold，丢故障期间差分 | 已确认：`D/main.cpp:969-978,1063-1068,1202-1325` |
| ADS快线程 | ADS read+write、PLC clock | 单调用300ms、完整成功1s、停滞3帧 | SoftHold/重连/缓存失效 | 已确认：`D/ads_communication.cpp:894-1005,1515-1550` |
| planned coordinator | PLC return事件/输出生效 | 250ms/5s/新序号；部分无总期限 | retry或fault hold，Clear/有限续做 | 已确认：`D/main.cpp:2662-3076` |
| PLC MAIN/handle | 数据端ADS快线程心跳/session | 2s/非零session被替换 | timeout/停稳/显式恢复/重初始化 | 已确认：`P/MAIN.TcPOU:68-104`；`P/handle.TcPOU:108-136,321-466` |
| 数据端arm服务 | UI hold及自身PLC快照 | 300/500/200ms | 归零方向或取消曲线 | 已确认：`D/arm_manual_ads_service.cpp:333-355,483-508` |
| PLC ArmCartesian/CartJog | 数据端专项alive/heartbeat | 500ms | 局部取消/减速/释放所有权 | 已确认：`P/ArmCartesian.TcPOU:56-70`；`P/ArmCartJog.TcPOU:38-96` |
| PLC各FB流程 | NC/驱动/Power/Stop等 | 各自TON；若干无上限 | 重试、收尾、_err，详D.5 | 已确认：`P/handle.TcPOU:255-317,579-885`；`P/SelfCheck.TcPOU:140-507` |

数值组合逐项区分“数值不同”和“已发生故障”：

| 组合 | 当前可证明的关系 | 可能联动/证据边界 |
|---|---|---|
| planned 250ms < ADS单请求300ms | 两数直接冲突于“等待一次成功ADS调用”的最坏时间预算 | **推断**：一次仍未返回的ADS请求可先被main判超时；Commit不确定保护随后阻止盲重发。`D/main.cpp:649-654,2793-2801`；`D/ads_communication.cpp:1059-1066` |
| main等快照300ms与arm状态年龄200ms | main与arm是不同线程，不能简单相加；arm慢队列自身可阻塞 | **推断**：慢队列延迟>200ms可令下一归零状态检查abort1004。`D/arm_manual_ads_service.cpp:334-338`；`D/ads_communication.cpp:559-586` |
| UI100ms→arm300ms→PLC500ms→host2s | 四层独立钟和门控，专项心跳到期通常早于全局host watchdog | **推断**：UI/专项队列失效可只停定位臂，快ADS心跳正常不会触发host2s。`PositioningArmPanel.xaml.cs:23-28,160-170`；`D/arm_manual_ads_service.cpp:15-17`；`P/ArmCartJog.TcPOU:38-56`；`P/MAIN.TcPOU:87` |
| 手柄250ms速度归零、1.5s新帧间断、5s缓存 | 250ms不直接使poll失败；1.5s在恢复新样本时判；5s才poll失败 | **推断**：介于阈值间的缓存按键仍可影响模式/边沿，不能称250ms已完全切断手柄命令。`D/手柄/手柄.cpp:58-103` |
| 手柄重入500ms vs旧逻辑首帧重基准 | 新增静置期间总hold会清spacing请求和mapping片段 | **已确认行为，现场概率未知**。提交`f3353fa`；`D/main.cpp:1189-1198,1302-1307,1579-1596` |
| 数据端state500ms停发 vs主端2s失效 vsforce3s | 数据端force函数未用state500ms闸门，worker可能继续发布旧bridge力 | **推断**：main阻塞时主端至少依赖2s state失效关force gate；收到连续force不代表控制域新鲜。`D/zenoh_service.cpp:1028,1118-1132`；`RemoteRobotLink.cs:87-110,506-527` |
| Prepare300s vsUI330s；Start3s vsUI10s；其它3s vsUI5/15s | UI外层普遍更长；数据端task deadline只在main的publish_state里检查 | **已确认**：main若阻塞，task逻辑逾时也延后被观察；外层Task可先超时。`D/zenoh_service.cpp:376-457,838-887`；`RemoteRobotLink.cs:224-271,326-345` |
| PLC return10/20/31各2s vs上位机5s | PLC阶段可串行，30无独立上限；5s不是PLC各阶段总和 | **推断**：PLC仍在收尾时上位机可先fault；并不能据此认定所有回退实际超5s。`P/handle.TcPOU:599-703`；`D/main.cpp:2902-2907` |
| 主机watchdog由ADS快线程续 | main停顿而快线程继续时，host心跳仍递增 | **已确认写入责任；推断**：PLC2s不能检测每一种main业务停顿。`D/ads_communication.cpp:934-935,1619-1635`；`D/main.cpp:4522` |
| GetTickCount DWORD vsGetTickCount64 ULONGLONG | planned submit字段取64位，推进传32位now；字段又是ULONGLONG | **推断**：系统运行超过DWORD毫秒范围（约49.7天）可发生宽化减法下溢，提前超时；无相应长期测试。`D/main.cpp:613,649-654,2090,2760`；`D/control_types.h:303-315` |
| 局部2/3s重试 vs恢复总完成 | SelfCheck Power、err循环、Axis15 reset等没有总次数/总时长 | **已确认**：不能把局部timeout视为有限时间恢复保证。`P/SelfCheck.TcPOU:294-306`；`P/err.TcPOU:189-263`；`P/Axis2Sync.TcPOU:23-44` |
| 已开session控制权无断流expiry | 网络断手柄本身不Release；main handle hold负责动作门控 | **已确认**：恢复、Release和新认证是不同事件。`D/zenoh_service.cpp:773-785,962-980,1005` |

## D.4 主端逐项证据

| 对象 | 数值/时钟 | 到期动作/恢复 | 出处与确定度 |
|---|---|---|---|
| Zenoh原生建连 | 3000ms，Zenoh config | `z_open`失败；下一次RemoteRobotLink重建 | 已确认：`VI.Service.Net/ZenohSession.cs:29-50` |
| 主端认证等待 | 10000ms，RemoteRobotLink单调时钟 | `ConnectAsync`抛超时，Disconnect；重连生成新nonce | 已确认：`VI.Service.Robot/Services/RemoteRobotLink.cs:118-140` |
| 主端状态新鲜度 | 2000ms；状态帧接收时间 | 清HasControl/Lease/持续输入/力闸门，CanOperate=false | 已确认：`ProtocolConstants.cs:5-12`、`RemoteRobotLink.cs:87-110,291-303` |
| 已连接但重认证阈值 | state超过3000ms且重建间隔≥3000ms | 清session/auth序号，重新Hello；不重放旧命令 | 已确认：`RemoteRobotLink.cs:551-568` |
| 主端force帧新鲜度 | 3000ms；接收时间 | 丢ForceFrame；状态gate关闭时清手柄力 | 已确认：`ProtocolConstants.cs:8-12`、`RemoteRobotLink.cs:506-527` |
| Session重建节流 | ≥1000ms，Environment.TickCount64 | Closed/Unavailable时OpenSession+Hello；pending命令统一拒绝 | 已确认：`RemoteRobotLink.cs:118-128,201-219,536-568` |
| 普通按需命令 | 多数5000ms | Task超时并移除pending，UI需重新发 | 已确认：`RemoteRobotLink.cs:224-271` |
| StartControl命令 | 10000ms | Task超时并移除pending | 已确认：`RemoteRobotLink.cs:242,326-345` |
| ForceFeedback/Zero | 15000ms | Task超时并移除pending；数据端task通常3s | 已确认：`RemoteRobotLink.cs:224-271,326-345`；下游`D/zenoh_service.cpp:398-426` |
| PreparePosition | 330000ms | UI等待可远长于数据端300000ms task | 已确认：`RemoteRobotLink.cs:224-271`；`D/zenoh_service.cpp:838` |
| 电缸/Y阀按需命令 | 各10000ms；主端请求计时 | Task超时并移除pending；下游task3s，与通用5s不同 | 已确认：`RemoteRobotLink.cs:268-271`；下游`D/zenoh_service.cpp:890-916,946-954` |
| HandleService heartbeat | >1000ms，Environment.TickCount64；监管线程约5ms | 清samples/forces，宿主未退出则Kill，释放映射；进程空且距上次启动≥1s才重启 | 已确认：`HandleService.cs:102-179` |
| HandleHost force tick | 3000ms | `UpdateForce`清零；ApplyForce每100ms至少发零力或前次非零立即归零 | 已确认：`VI.HandleHost/Program.cs:104-140` |
| HandleHost采样 | 5ms deadline，Stopwatch | 无单拍超时分支；落后时deadline跳到now+5ms；SDK异常写Error并退出 | 已确认：`VI.HandleHost/Program.cs:37-103` |
| USB通知 | 无时间阈值；CM_Register_Notification回调 | 清样本/力，标记restartRequested；监管器Stop并按1s节流重开 | 已确认：`HandleDeviceNotifications.cs:23-37`、`HandleService.cs:92-149` |
| UI状态刷新 | 250ms DispatcherTimer | 只更新年龄/属性，不使旧state变新 | 已确认：`MainViewModel.cs:90-95,505-529,617-623` |
| 定位臂轴/末端/归零保活 | UI100ms DispatcherTimer | 轴和末端各自重发完整Jog命令；仅程序归零发ArmCartesianAlive。失去CanOperate且轴点动/归零活动时清hold并ArmStop；纯末端点动不满足该if，仍受命令接受与下游租约门控 | 已确认：`PositioningArmPanel.xaml.cs:23-38,160-170`、`D/arm_manual_ads_service.cpp:236-251,297-301` |
| 应用关闭 | StopTimeout=8s | 先停止面板动作，再断link/stop服务 | 已确认：`App.xaml.cs:13-24,51-63`、`MainWindow.xaml.cs:43-56` |

## D.5 PLC逐项证据

PLC所有TON在1ms `PlcTask`中运行；下表中的“无总限时”是当前ST实际结构，而不是建议。

| 对象/阶段 | 数值/时钟 | 到期动作/恢复 | 出处与确定度 |
|---|---|---|---|
| MAIN/handle主机watchdog | 2s TON | `host_comm_timeout`；handle锁存、停止/清请求、要求显式recover与停稳后重初始化 | 已确认：`P/MAIN.TcPOU:68-104`、`P/handle.TcPOU:108-136,321-466` |
| handle初始化/Enable | `G.recovery_wait_timeout=2s`；最多3次EnableError脉冲 | 超时err/estop_hold；成功后进入selfcheck/handle | 已确认：`P/handle.TcPOU:255-317`、`G:181-182` |
| return state10 Disable | 2s TON | 超时进30错误收尾；Req清或FB Error同样进30 | 已确认：`P/handle.TcPOU:599-627` |
| return state20 MoveAbsolute | 2s TON | 超时/Error/Aborted进30错误收尾 | 已确认：`P/handle.TcPOU:629-643` |
| return state30停稳 | 无独立TON | `ActVelo≤0.5`且clear_stop inactive才进31；恒假条件使652–663锁存块不可达，实际物理效果未知 | 已确认矛盾：`P/handle.TcPOU:645-668`；效果未知 |
| return state31重新Enable | 2s TON；最多3次EnableError | 超时err/estop；成功Done/Error后回0 | 已确认：`P/handle.TcPOU:670-703` |
| axis4 state10/30/40 | 各2s；state39只保持1个1ms扫描 | 超时40错误收尾；Reset/Power错误重试 | 已确认：`P/handle.TcPOU:706-885`、`G:181-182` |
| init上电 | 5s TON | 错误/超时err；PowerReady后selfcheck | 已确认：`P/init.TcPOU:40-82` |
| reset清错 | 3s TON | 残错err；无错回init | 已确认：`P/reset.TcPOU:39-76` |
| clear_err稳定 | 200ms TON | 稳定后handle_reinit；未自检则init | 已确认：`P/clear_err.TcPOU:43-84` |
| err普通恢复 | 2s TON；Power-not-ready为3s | Reset/Power循环；无总次数上限 | 已确认：`P/err.TcPOU:189-263` |
| err限位退离 | 轴1/3/5/6退2.0，速度5/acc50；最多3次 | Done/Error后重新判断；3次仍压限failed | 已确认：`P/err.TcPOU:67-139`、`G:183-185` |
| SelfCheck错开扫描 | 轴1=0、轴3=300ms、轴5=600ms、轴6=900ms | 到期启动对应MoveVelocity；无总扫描超时 | 已确认：`P/SelfCheck.TcPOU:20-23,148-164` |
| SelfCheck堵转后备 | 100ms | 速度近0且条件连续成立才记leftlimit/Stop | 已确认：`P/SelfCheck.TcPOU:166-212` |
| SelfCheck限位稳定 | 300ms | 停稳后Power/Reset/下一阶段；轴错进入err链 | 已确认：`P/SelfCheck.TcPOU:214-250` |
| SelfCheck Power pulse/wait | 200ms脉冲，3s等待 | 3s回Power pulse；无最大次数 | 已确认：`P/SelfCheck.TcPOU:285-306` |
| SelfCheck回退错开 | 轴6先动，轴5/3/1等1/2/3s；stagger PT10s但不判断Q | 延时放行，不作为失败总时限 | 已确认：`P/SelfCheck.TcPOU:388-420` |
| Axis15 reset/GearIn | Reset脉冲200ms；间隔2s | 持续错误每2s重试，无总次数；Gear失败锁定到恢复 | 已确认：`P/Axis2Sync.TcPOU:19-89`、`G:186-187` |
| ArmCartesian心跳/enable | heartbeat500ms；stage10/30 enable/arrival>2s | 失活转40/错误2003；新heartbeat可重新启动，但释放中不抢占 | 已确认：`P/ArmCartesian.TcPOU:46-70,167-184` |
| ArmCartesian曲线/到位 | 曲线允许0.02–120s；位置≤0.05、速度≤0.1且至少2周期 | 无额外总timeout；stage40/50/60等Stop/Done/Error释放 | 已确认：`P/ArmCartesian.TcPOU:87-89,186-266` |
| ArmCartJog保活/enable | alive500ms；enable2s | 失活减速/释放；enable超时错误3003 | 已确认：`P/ArmCartJog.TcPOU:5-8,38-146` |
| ArmCartJogGen | 加速1s、减速0.5s；cycle_dt | smoothstep；模式/方向变化先归零再锚定 | 已确认：`P/ArmCartJogGen.TcPOU:22-95` |
| ArmManual/InjectorManual | 无本地总PT；停止看Stop.Done或低速 | host timeout清请求/方向；注射器Power仍保持TRUE | 已确认：`P/ArmManual.TcPOU:35-214`、`P/InjectorManual.TcPOU:23-109` |

## D.6 未知项

尚不能由仓库确定：现场下载PLC版本是否与当前POU相同；NC SAF/SVB XML的100ns单位是否与现场工程设置一致；主端8s关闭是否覆盖数据端/PLC收尾；真实设备下各阈值的抖动分布和连锁先后；主端/数据端是否有仓库外服务改变Zenoh lease或ADS路由。证据边界分别见 `99_未决问题.md:7,13,15-19`、`C_时间敏感动作.md:60-68,430-439`、`F_测试现状.md:124-145`。
