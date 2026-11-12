# B 数据端 main.cpp 解剖

范围为当前磁盘源码。路径前缀 `D/` 唯一代表 `介入机器人/B数据端/工程/ADS项目/`；本附件 `main.cpp:行号`、`control_types.h:行号` 等省略的文件均在 D/ 下。`K/` 代表 `介入机器人/B数据端/工程/PositioningArmKinematics/`。行号计空行。除明确写“推断/未知”的行外，表中“已确认”均指静态实现事实，不指现场动作验证。变量逐项索引见本附件后半部分。

## B.1 入口和分块地图

**已确认**：main.cpp 为 5424 行；`main` 为 37–5424 行，主 `while(true)` 为 966–5402 行。循环没有退出分支；5404–5423 的正常停止/清请求/关闭记录器代码在当前控制流中不可达。循环内大量 `return/continue/break` 属于 lambda、内部 for 或命令 switch，不能当成主循环提前退出。出处：`main.cpp:37,966,4869,5402,5404-5424`。

| 区间（main.cpp） | 功能 | 主要长驻读写状态 | 外部模块/边界 | 确定度 |
|---|---|---|---|---|
| 1–36 | 头文件和应用模块依赖 | 类型定义 | ADS、Zenoh、运动同步、记录、恢复 | 已确认 |
| 37–115 | 诊断/路径/配置、启动通信和两个记录器中的速度记录器 | cfg、homing_record、ads_communication、arm_manual_ads、zenoh_service | ADS 快/慢两个后台线程；定位臂线程；Zenoh 线程；速度 CSV 两线程 | 已确认 |
| 117–177 | 手柄初始化、单/双模式锁定、力输出入口 | handle_startup_locked、single_handle_mode、输入指针、ff | Handle、remote_handle_bridge | 已确认 |
| 179–238 | PLC 镜像与 AppContext、I/O lambda | pos、plc_*、ctx | plc_io、motion_sync；lambda 按引用捕获 | 已确认 |
| 239–379 | Follow 基准、窗口、状态结构绑定、先行映射 helper | axis*_crawl、startup、planned_return、dual_handoff、spacing_recovery、mapping | motion_sync | 已确认 |
| 380–477 | control_active、保留的直接定位helper（当前无调用）、紧急回撤限速备份/恢复 | startup、emergency_*、plc_v_limit | startup_sequence、plc_io | 已确认 |
| 479–548 | 自检/急停/按需点动/软限位/回退故障状态 | self_check_done、estop_*、lease deadline、ReturnRecovery、softlimit、force_sample | return_recovery.h | 已确认 |
| 549–804 | 回退异步提交/轮询/取消、软限位、取零入口 | planned_return、dual_handoff、pending_error、cal_state、ff | ADS 专用回退队列；force_feedback；取零有阻塞等待 | 已确认 |
| 806–965 | 连接保持、电缸手动覆盖、模式请求、待提交参数、自动实验记录 | hold flags、cylinder_manual_*、pending_mode/startup、自检目标、记录历史 | ExperimentRecorder | 已确认 |
| 966–1068 | 等 ADS 快照、排空记录队列、诊断、重连/运动有效性 | ads_snapshot_sequence、ctx 镜像、pending_error、reconnect flags | ads_communication | 已确认 |
| 1070–1325 | 手柄重试/轮询、恢复静置、连接保持和重建基准 | handle_*、filters、Follow 基准、initial_sync_done、ads_soft_hold_active | Handle、motion_sync、net distance | 已确认 |
| 1327–1456 | 初同步、PLC 应用名、重启与寻参复用 | homing_*、startup、control_active、ff、planned_return | ADS 低频请求、homing_record.h | 已确认 |
| 1460–1606 | 滤波、记录手柄历史、按键边沿、方向、恢复请求、先行清零 | filters、prev、spacing requested、mapping、dual_handoff | bridge 缓存；事件记录 | 已确认 |
| 1608–1776 | 点动租约/门控、方向切换、急停边沿与看门狗恢复 | injector/axis4、dual_reverse、estop、return_recovery | ADS 通知缓存 | 已确认 |
| 1779–1985 | 单手柄导丝模式、自检边沿、寻参记录、重同步请求 | guidewire_mode、window_locked、startup、calibration、homing_record | guidewire_mode.cpp、motion_sync | 已确认 |
| 1987–2081 | 回退故障清理与再启用、缺省电缸、净距离采样 | return_ads_fault_hold、ReturnRecovery、control_active、net distance | return_recovery.h | 已确认 |
| 2083–2245 | 运动门控、增量和 tracking 门控、镜像 helper | pos、Follow 命令、tracking、ff | delivery_tracking | 已确认 |
| 2247–3076 | 统一计划回退协调器的视图/校验/时序/重试/交接 | coordinator 所有阶段、两条 leg、输出 generation | ADS 快照/通知/专用队列 | 已确认 |
| 3078–3213 | 轴7旋转、轴6软限位保持、独立轴6 Follow/触边 | axis6_crawl、axis6_follow_cmd_abs、pos、夹爪 | motion_sync、planned helper | 已确认 |
| 3215–3388 | 屈曲恢复入口、夹爪等待、等位移、限速/限程、退出请求 | spacing_recovery、pos、四电缸 | 当前读取轴1输入增量 | 已确认 |
| 3390–3664 | 启动三条路径及回退故障续做 | startup、ReturnRecovery、original_return_intent、pos | startup_sequence、sync_all | 已确认 |
| 3665–3926 | 计划回退优先分支、独立导丝、双手柄共同窗口/换手 | planned_return、axis1/6、dual_handoff、mapping、pos | tracking、motion_sync | 已确认 |
| 3927–4279 | 单手柄普通导管 Follow、轴4耦合、先行、触边 | axis1_crawl、axis6 mirror、mapping、stop latch、tracking | planned helper | 已确认 |
| 4282–4400 | tracking 无效化、屈曲退出同步、电缸手动覆盖与门控 | PI、spacing、manual overrides、pos | sync_all、plc_io | 已确认 |
| 4401–4606 | 取力/诊断、最终 ADS 帧、generation、力反馈、更新差分基准 | force_sample、ff、ads_output、planned generation、prev | force_feedback、ads publish_output | 已确认 |
| 4608–4680 | 用排空 ADS 快照记录 force/motion | recorder、handle_record_history | 以源 QPC/序号去重，不是此处另采样 | 已确认 |
| 4681–4865 | 约30Hz填内部可视状态并交 Zenoh | VisState、状态/错误/记录/arm 快照 | zenoh_service.publish_state | 已确认 |
| 4868–5402 | 每拍至多64个内部命令；校验参数/模式/启动/记录/取零等 | 几乎全部控制状态 | 只部分命令被公共 Zenoh 接口映射 | 已确认 |
| 5404–5424 | 正常退出清理（当前不可达） | 输出/线程/记录 | service stop | 已确认 |

## B.2 一拍完整执行顺序

| 顺序 | 步骤、门控或跳过范围 | 出处（main.cpp） | 确定度 |
|---:|---|---|---|
| 1 | `wait_for_snapshot` 最长300ms；新快照且 position_valid 时更新 PLC 镜像；无新快照仍执行本轮后续逻辑 | 969–978 | 已确认 |
| 2 | drain 全部待记录 ADS 快照；取 stats 和通知事件；补报延迟 ErrorID；必要时写故障边沿日志 | 979–1050 | 已确认 |
| 3 | 重连边沿请求刷新坐标/恢复看门狗；除ExitSync外清 spacing；关闭 PI。`ads_motion_cycle_valid` 要新帧、位置有效、Running、自检完成、reinit_done、非host timeout | 1051–1068 | 已确认 |
| 4 | 1秒节流初始化手柄；锁单/双模式；轮询，失败关闭；恢复静置参与 handle hold。读取 rebase 请求 | 1079–1200 | 已确认 |
| 5 | ADS运动帧无效进入 hold；原回退在活动时先保留其事务；其它恢复路径重建手柄/实际基准，才解除 ADS hold | 1202–1295 | 已确认 |
| 6 | 计算总 connection hold，进入边清手动电缸/PI/spacing/力；退出边按实际位姿保持 | 1297–1325 | 已确认 |
| 7 | 初始 sync_all(30)；应用名/PLC重启/复用记录处理 | 1327–1456 | 已确认 |
| 8 | 不在handle hold才更新滤波；保存100ms手柄有效性历史；loop_count++；清本拍fast flags；软限位预测状态按条件解锁 | 1460–1501 | 已确认 |
| 9 | 读取两逻辑手柄按键及物理587恢复键；落实排队模式、冻结/更新双手柄方向；恢复按下/抬起边；清不适用的先行；必要时取消回退 | 1503–1606 | 已确认 |
| 10 | 到期清轴4/注射器方向；算手动请求；方向切换清滤波尾量；急停边保存回退中断/清控制/清力，急停期间每秒请求恢复 | 1608–1776 | 已确认 |
| 11 | 单手柄导丝模式进入/退出；自检/重初始化握手；记录坐标 | 1779–1985 | 已确认 |
| 12 | 故障保持时250ms间隔清Req，等新帧和就绪并sync_all(20)再恢复；不在互斥动作时可自动启用已完成启动的控制 | 1987–2041 | 已确认 |
| 13 | 设置本拍四电缸缺省值，计算净距离Follow条件并采样上一拍反馈 | 2042–2081 | 已确认 |
| 14 | 只有非ADS/手柄/回退故障/急停保持，且control/startup/emergency至少一项允许、镜像可读，才进入运动树；否则跳过2086–4279并在4282后使tracking失效 | 2083–2089、4282–4298 | 已确认 |
| 15 | 计算映射增量/方向/窗口、tracking门控及换手欠账，构建各lambda。主动作优先级依次是紧急回撤→轴6软限位→spacing→startup→故障续做→planned→导丝独立→双手柄→单手柄导管 | 2090–2245、3297–3390、3628–3724、3927 | 已确认 |
| 16 | 提交tracking实际输出；更新位置/触边历史 | 4240–4279 | 已确认 |
| 17 | ExitSync达到最短100ms且已到位或满2s，且通信/手柄等允许，执行sync_all(20)，成功才回Idle；失败下拍再试 | 4300–4353 | 已确认 |
| 18 | 离散输出门控/手动电缸覆盖；取力、记录器健康；力无效关闭PI；构建AdsOutputCommand | 4356–4521 | 已确认 |
| 19 | `publish_output` 只更新后台最新期望帧并返回generation；随后记录clamp/commit/handoff/release的generation；并非在此同步ADS成功 | 4522、4542–4582 | 已确认 |
| 20 | 根据最终门控执行力反馈，更新所有差分prev（保持时也更新，防恢复跳变） | 4584–4606 | 已确认 |
| 21 | 排空快照逐条写记录器：force每帧，motion偶数快照序号；用最近≤100ms手柄历史对齐有效性 | 4608–4680 | 已确认 |
| 22 | QPC截止触发约30Hz内部状态快照，publish_state | 4681–4865 | 已确认 |
| 23 | 最后才消费≤64个命令；某些命令直接清状态/写低频参数或执行取零；一般影响下一轮输出 | 4868–5402 | 已确认 |

**已确认**：不存在外层“缺快照就continue”。因此缺帧时仍可能处理UI命令、状态发布和租约，但其执行时间取决于上面的阻塞点。`submit_low_frequency_request` 忽略传入timeout并等待完成；初同步的多样本平均含Sleep；取零最多三组500ms样本。它们使“主循环稳定100Hz”不能由目标ADS频率直接成立。出处：`ads_communication.cpp:550-586`、`control_types.cpp:29-92`、`main.cpp:759-794`、`force_calibration.h:66-74`（取零状态类型）。

## B.3 模式、阶段和可达性

| 模式/变量 | 取值/初值 | 切换条件与位置 | 影响、限制 | 确定度 |
|---|---|---|---|---|
| single_handle_mode / handle_startup_locked | false/false | 两只→双；一只→单；首次init已有一只立即锁；等待阶段一只持续3s才锁。锁后没有重选双模式路径 | 单模式两个逻辑输入指向同一物理柄；双模式axis1=587、axis6=582 | 已确认：main.cpp:54-55、117-156、1079-1114 |
| GuidewireMode | None / Independent，初None | 单手柄requested模式变化，进入/退出调用guidewire模块；双手柄requested固定None | Independent冻结1/2/3/5，只动6/7；当前普通双模式没有旧B7选模式实现 | 已确认：control_types.h:220-224；main.cpp:1534-1547、1779-1889、3672-3722 |
| dual_reverse_active | bool初false | 非planned、非spacing及无恢复键时=两只B6 OR；换手期间冻结 | 任一0x40为全局撤出，两只松开为递送 | 已确认：main.cpp:1523-1528、1534、1651-1664 |
| single_handle_requested_mode | None/Independent | 内部SetGuidewireMode/ModeSelection写；双柄不使用 | 公开WPF/Zenoh未暴露这些内部命令 | 已确认：main.cpp:874-915、4868-5055；zenoh_service.h:27-42 |
| vis_reverse_override_* | active=false，value=false，target0 | 单模式内部ModeSelection覆盖；spacing保存/还原；双模式退出spacing清active | UI命名来自旧内部接口，不能由字段名推断当前WPF可调 | 已确认：main.cpp:860-915、1549-1567、4314-4343 |
| ModeSelection | 0None/1导管递送/2导管撤出/3导丝递送/4导丝撤出 | request遇spacing先排队退出；planned期间忽略；其它立即apply | 每次apply清先行；单导管递送重新arm | 已确认：main.cpp:863-915 |
| startup.phase | 12个枚举，见B.8 | WaitForEnter→启动某条路径→Done；PLC重启可回等待 | completed与phase是两个独立门控字段 | 已确认：control_types.h:439-454；main.cpp:1359-1376、3390-3626 |
| control_active | 初false | sync成功、启动完成、普通恢复、自检复用恢复置true；急停/故障/PLC重启/用户暂停等置false | motion树还同时受各hold门控 | 已确认：main.cpp:380、1369、1733、2021、2038、3442、3502 |
| CylinderManualMode[4] | Automatic/Open/Closed/Position，初Automatic | 内部命令/公共Cylinder→手动；自动接管/hold→清 | 最终离散输出可覆盖自动缺省，但planned/startup等禁止手动 | 已确认：main.cpp:813-858、4356-4399、5060-5160 |
| Axis6SoftLimitReason | None/ActualPosition/HandleTarget/PlannedReturn/Axis1CoupledReturn | 预测危险/实际>660，相关链路保持；预测警告可解锁，Actual持有更严格 | 不是PLC NC硬限位；clear会影响mapping/dual_handoff | 已确认：main.cpp:529-541、705-757、1488-1498、3093-3114 |
| ForceFeedbackState | enabled=false（分柄enable初false）、hold开关见结构 | 网络FF/取零/保持命令；安全hold清力/保持值 | 控制FF和机器人位置门控不是同一个状态 | 已确认：control_types.h:473-628；main.cpp:171-176、4401-4601 |
| Debug/Release | 编译期 | `_DEBUG`只决定诊断构建名；不可运行时切换 | 单手柄调试、电缸手动与编译Debug不是同一概念 | 已确认：main.cpp:40-44、813-829 |

## B.4 planned_return 全阶段

**已确认**：两条leg最多为本体轴1和6；轴3/5保持。事务保存mode、rebase scope、每条腿的目标/通知基线/已启动/可能启动/完成；时间基线用字段ULONGLONG保存，但运动树`now_ms`来自32位GetTickCount；异步提交时间来自GetTickCount64。出处：`control_types.h:270-335`、`main.cpp:582-648、2090`。计时混用的影响列D附件。

| 阶段 | 进入/本阶段输出 | 推进条件→下一阶段 | 等待/失败 | 出处与确定度 |
|---|---|---|---|---|
| Idle | 无专用输出；Follow决定pos | begin校验目标/窗口/软限位，锁当前轴和支撑位姿→ClampSettle | 捕获original_return_intent供故障续做；不是重复触发 | 已确认：main.cpp:2382-2424、2471-2527 |
| ClampSettle stage0 | 参与轴的持有夹爪1/3夹紧；移动夹爪2/4仍夹；参考冻结 | 本拍输出generation被ADS确认后开始150ms；到期切stage1 | 若未applied，无该等待的总期限 | 已确认：main.cpp:2529-2561、2563-2637、2681-2708、4562-4568 |
| ClampSettle stage1 | 持有夹爪1/3夹紧、移动夹爪2/4松开 | 新generation applied后等500ms→SubmitRequest | 双腿等待取参与侧最大值 | 同上，已确认 |
| SubmitRequest/Prepare | 先异步下发Req=false+TargetAbs/V/A/D/J；fast旁路尚受已启动条件 | Prepare成功、PLC空闲无Done/Error后建立commit_guard输出 | 异步完成最多250ms；未启动才允许转RetryClear | 已确认：main.cpp:2710-2865 |
| SubmitRequest/Commit guard | 冻结refer并打开对应fast flags | guard generation applied后捕获通知基线，异步Req=true | guard最多250ms；Commit部分成功或传输未知锁possibly_started | 已确认：main.cpp:2757-2857、4542-4560 |
| AwaitAccepted | 保持fast及夹爪组合；非运动轴冻结 | 基线之后Busy=true或Done=false确认接收→AwaitDone；即使漏Busy，新的Done=true且到目标也能完成 | 250ms未ACK，只在未started且无possibly_started时重试 | 已确认：main.cpp:2247-2340、2867-2882 |
| AwaitDone | PLC负责移动轴1/6；3/5冻结 | 全leg新事件Done且目标到位→AwaitFreshSnapshot并记done_snapshot_sequence | execution 5000ms；Error或超时fail；started后禁止整组重发 | 已确认：main.cpp:2884-2907、2440-2469 |
| RetryClear | 输出冻结，异步清参与轴Req；两条腿共同复位 | Clear成功后需更大快照序号、!Busy/!Error、目标仍安全→SubmitRequest | 最多一次；清命令250ms，清后等新帧/就绪500ms；不合则fail | 已确认：main.cpp:2639-2660、2909-2996 |
| AwaitFreshSnapshot | 开始恢复移动夹爪2/4；持有夹爪仍夹；冻结参考 | 比Done时更大的快照序号；按scope非阻塞rebase→PublishHandoff | 无单独总期限；rebase失败fail | 已确认：main.cpp:2563-2637、2998-3009；motion_sync.cpp:158-248 |
| PublishHandoff | 最终输出含重建refer、fast=false和参与Req清除mask同一SumWrite | publish_output记录handoff_generation→AwaitHandoffApplied | 主循环只发布期望值 | 已确认：main.cpp:4471-4522、4576-4582 |
| AwaitHandoffApplied | 保持重建refer、移动夹爪夹紧 | ADS applied_generation≥handoff_generation→PostHandoffClampSettle stage0 | 未设独立deadline | 已确认：main.cpp:3011-3022 |
| PostHandoffClampSettle stage0 | 2/4夹紧、1/3仍夹 | applied交接起等150ms→stage1并要求新release generation | 已确认写生效起算，非单纯凑拍数 | 已确认：main.cpp:3024-3047 |
| PostHandoffClampSettle stage1 | 1/3松开，2/4夹紧 | release generation applied后等100ms，再次rebase成功；导管正向且轴1参与则arm先行；reset→Idle | 无等release applied总期限；恢复成功ReturnRecovery.completed | 已确认：main.cpp:3048-3076、4569-4575 |
| fail_planned_return | `return_recovery.interrupted(...)`；`return_ads_fault_hold=true`；`control_active=false`；清力；保存延迟ErrorID上下文；随后调用取消路径按需异步清Req | 故障恢复器独立等待Clear/就绪再考虑续做 | 延迟ErrorID最长等待2s；停稳由后续恢复门控确认 | 已确认：main.cpp:2440-2469、1987-2026 |
| 普通 `cancel_active_return_motion` | 清`dual_handoff`；若有活动回退则清mapping/fast、从实际位置装载refer、按需要异步Clear参与轴Req并重置coordinator；不自行设置`control_active=false`、`return_ads_fault_hold`或清力 | 取消调用者自行决定是否进入保持/恢复；取消本身不等PLC完整停稳 | Clear通知与交接代次按调用上下文处理 | 已确认：main.cpp:664-703 |

互斥关系：planned与startup/emergency/spacing Active/soft limit不能同时拥有pos。连接保持可以保留planned，但运动树不推进，等待恢复再按PLC状态确认；spacing requested可以等当前planned完成，不把正在执行的换手丢弃。出处：`main.cpp:1242-1246、1598-1605、3238、3297-3390、3665-3671`，**已确认**。

## B.5 dual_handoff 与 crawl

| 流程/状态 | 输入与转移 | 输出影响/等待 | 出处与确定度 |
|---|---|---|---|
| dual Follow | 窗口axis1=5..30；g=axis6距左−axis5距左=1..33；按本拍截断前增量识别哪一侧触边 | 只裁越界增量，不强迫静止手柄轴被动跟随；夹爪1/3开、2/4夹 | 已确认：main.cpp:3723-3858 |
| dual_handoff Idle→pending | 轴1命令到方向端点且仍同向输入；导丝自身或导管造成g边界；仅轴1触发可合并距被动端<5mm的轴6 | 记录axis1/axis6/wire_caused/gap_boundary/reverse/wire_reverse/axis4_caused；并非马上松夹爪 | 已确认：main.cpp:3859-3875；control_types.h:373-385 |
| dual pending | 实际轴1/实际g到相应边，且轴1/3/5/6各自actual与本拍pos差≤.2、速度≤.5 | 无pending总超时；等待运动完成；方向沿用请求时锁存值 | 已确认：main.cpp:3876-3890 |
| pending→planned Dual | 轴1回另一端；导丝主动g目标递送28/撤出6；被动距离min(27,轴1剩余+3)，g目标由方向算；轴6目标夹到660；差≤.2时不加轴6腿 | begin成功清pending，立即应用持有夹爪输出；后续统一协调器 | 已确认：main.cpp:3891-3924 |
| 轴1crawl | 当前没有名为run_axis1_crawl_state的函数；CrawlState仅保存Follow基准与窗口 | 双模式上表；单模式inline计算Follow、窗口/触边/镜像，并转planned；没有旧多阶段crawl计时 | 已确认：control_types.h:456-471；main.cpp:3991-4227 |
| 轴6独立Follow | 未在窗口时先按模式方向入窗；窗口内允许正负增量；命令夹窗口，不能超660 | pos6跟随；3开4夹；pos1/2/3/5冻结，pos7跟旋转 | 已确认：main.cpp:3121-3154、3672-3722 |
| 轴6触边 | 非零同向增量且实际在端点；轴4撤出兼容命令先触边；目标是窗口另一端 | 不要求到位与输入发生同一拍；安全后begin GuidewireDelivery/Retraction | 已确认：main.cpp:3156-3213 |
| 单柄换向保护 | 新方向端点距离≤2mm时置guard；退出保护条件由后续Follow更新 | 当前仍有同向输入触边路径；不能把guard当独立完整状态机 | 已确认：main.cpp:1665-1723、3160-3180、4090-4227 |

**已确认**：双模式的Pending不暂停手柄采样，统一回退期间仍记录前推欠账；回退控制门控与欠账/力保持并行存在。出处：`main.cpp:1460-1475、2160-2210`、`delivery_tracking.cpp:199-220`。

## B.6 净距离恢复/屈曲恢复（同一个 spacing_recovery）

`净距离`有两个不同对象：`HandleNetDistance`累计实际递送距离；“屈曲恢复/间距恢复”是`SpacingRecoveryState`，不能因用词相似合并。出处：`handle_net_distance.h:6-35`、`main.cpp:84、288、2061-2081、3215-3388`，**已确认**。

| 状态 | 转移条件 | pos/夹爪/标志 | 时间/互斥 | 出处与确定度 |
|---|---|---|---|---|
| Idle/requested | 物理587 `(buttons&7)==7`上升沿置requested，抬起清；等planned完成 | 保存进入前模式/override | 不允许按名字把它认成B7(0x80)；0x47也满足 | 已确认：main.cpp:1505-1513、1549-1575；control_types.h:150-151 |
| Idle→ClampWait | startup Done/completed、control=true、无softlimit、无planned；轴5≥轴3容差；g在1..33容差；共同剩余>.2 | 捕获1/2/7保持及3/5/6命令；清相关Req、手动覆盖；置phase_t0 | 拒绝则reset。代码未显式要求正向或guidewire None，虽然提示说需导管递送Follow | 已确认：main.cpp:3238-3294 |
| ClampWait | phase elapsed≥150ms→Active | cyl1=450开、2=1000夹、3=550开、4=100释放；1/2/7保持 | GetTickCount DWORD；由状态进入起算，未等输出generation或夹爪反馈 | 已确认：main.cpp:3322-3341 |
| Active | 正的axis1_linear_increment累入pending，非正清pending；step=min(pending,40mm/s×dt,remaining) | 3/5/6等量增加；1/2/7保持；axis4由其它门控；力无效化 | dt封顶50ms；axis3≤650、axis5≤665、axis6≤min(665,660)；无自动总时长 | 已确认：main.cpp:3215-3226、3343-3388 |
| Active/ClampWait→ExitSync | requested=false（松键、模式切换等） | pending=0，保持最后3/5/6目标；继续恢复夹爪组合 | phase_t0=now | 已确认：main.cpp:3229-3236 |
| ExitSync→Idle | ≥100ms且三轴误差≤.2或elapsed≥2s，且有效ADS/无hold/无急停/无回退fault，sync_all(20)成功 | 还原单柄模式；双柄回到当前B6；轴3>22可清停止锁；control=true | 2s是允许按实际重基准的后备条件，不是硬失败期限；sync失败无限重试 | 已确认：main.cpp:4300-4353 |
| 强制reset | connection enter/reconnect（ExitSync除外）、急停/回退故障、自检、reinit、PLC重启、emergency | 清requested/phase/pending/moved等，保持位姿字段本身未全部清 | 上升沿prev每拍更新，持续按住而被reset后不会自动重建请求 | 已确认：main.cpp:1058、1306、1370、1575、1773、1916、1975、5211；control_types.h:422-435 |

输入来源矛盾和功能判定见E附件；这里仅列事实。`HandleNetDistance`在新序号有效快照且上一拍following时累计实际位移；失效/换手关闭following后重建原点，不把回退位移算作递送。出处：`handle_net_distance.h:8-35`、`main.cpp:2061-2081、4507-4521`，**已确认**。

## B.7 紧急回撤、先行、tracking

| 流程/状态 | 进入、推进、退出 | 输出和耦合 | 时间 | 出处与确定度 |
|---|---|---|---|---|
| emergency Idle→Active | 内部EmergencyRetract，要求有效/已自检等；保存1/2/7实际值，清spacing/模式/原回退，备份并缩放3/5/6限速 | 1/2/7固定；3/5/6返回 `startup.final_axis3/5/6_from_left_mm`（当前默认610/615/628 mm）；四电缸命令依次1000/0/100/1000；cyl5_req=true | 限速=min(轴1/3/5/6当前限速)×`cfg.startup_motion_speed_scale`，默认比例0.005；无运动总超时 | 已确认：main.cpp:427-452、3297-3313、4357、5195-5232；control_types.h:181,195-200 |
| emergency Active→Idle | 3/5/6各距目标≤.2；输出发布后完成 | 恢复v_limit、startup Done/completed；选择CatheterDelivery、guidewire None；`control_active=sync_all(20)`，同步成功恢复控制、失败保留false | 到位等待，无固定延时；恢复限速失败有日志 | 已确认：main.cpp:4523-4540 |
| axis1 mapping Off→Armed | 单柄选导管递送；或导管正向计划回退完成且轴1参与 | active=true，progress/extra=0 | 按距离，不按时间 | 已确认：main.cpp:347-363、874-881、3064-3069 |
| mapping Active→Off | 前10mm名义正向输入，额外比例lead/10，默认.2，累计最多2mm；完成或任一guard清零 | 只给轴1多走；extra吸收进axis3/5镜像基准；双柄仅轴6回退保留进度 | 不会无输入自主运动；被hold清零不自动续旧段 | 已确认：main.cpp:1579-1596、2484、3787-3844、4053-4090 |
| tracking不合格/夹爪稳定 | update_gate检查正向Follow、夹爪命令；不合格end segment；夹爪连续150ms后可begin | 取的是“夹紧命令已持续”，没有夹爪到位反馈 | DWORD now；grip settle150 | 已确认：delivery_tracking.cpp:128-178；delivery_tracking.h:92；main.cpp:2133-2209 |
| tracking换手欠账 | 回退/交接冻结PI，记录max(0,前推量) | error上限20mm；不记回拉负债；session累计保留完整前推 | 无独立线程/计时器 | 已确认：delivery_tracking.cpp:199-220；main.cpp:2180-2210 |
| tracking PI Active | 手工开启且active、kp>0、maxgain>1；每次当前正向输入可额外放大 | 默认kp=0.5、ki=2、maxgain=2；额外≤.25mm/拍且≤欠账；命令夹紧回退积分；只有实际前进确认才还债 | dt封顶.10s；积分上限20mm·s；无输入不自主追赶 | 已确认：delivery_tracking.h:42-50、167-172；delivery_tracking.cpp:223-331、334-476 |
| tracking safety reset | 安全hold、模式退出等end段清欠账；普通CrawlReturnActive保留欠账；disable_compensation只复位PI | 主循环最后finish/commit与pos约束相连；力样本无效也disable | 不是机器人position位置闭环替代物 | 已确认：delivery_tracking.cpp:119-126、334-476；main.cpp:4240-4298、4448-4454 |

**已确认**：当前公共Zenoh命令没有EmergencyRetract、SetAxis1Lead或tracking参数/开关。它们在main内部命令消费者保留；是否有仓库外注入者**未知**。出处：`control_messages.h:153-199`、`zenoh_service.h:27-42`、`zenoh_service.cpp:797-956`。

## B.8 启动序列

| 路径/阶段 | refer和夹爪 | 转移条件/等待 | 出处与确定度 |
|---|---|---|---|
| WaitForEnter | 等自检完成/内部ExecuteStartup；默认最终1/3/5/6=28/610/615/628，旋转2/7=0 | 内部启动参数可覆盖；StartupState声明里的20/649/649/650被main初始化覆盖；注释641过时。公共StartControl另走当前位置同步，见表后 | 已确认：control_types.h:195-200,678-683；main.cpp:290-294,918-935,5277-5356 |
| 保留的直接定位PositionAxis156 | helper若被调用则保持当前3；1到最终，5/6先到保证相对关系的中间点，2置0；正常抓持 | 三轴实际误差≤.2→PositionAxis356，无总超时。当前生产源码只有begin_direct_positioning定义，未发现调用 | 已确认：main.cpp:381-420,3455-3487 |
| 保留的直接定位PositionAxis356 | 1/3/5/6最终，2/7为0 | 各平移≤.2、旋转≤.2→Done，sync_all(20)决定control；当前无入口调用链，不能当公共StartControl行为 | 已确认：main.cpp:3469-3508,5315-5356 |
| 标准ReleaseClamps | 1/2全开，3=500、4=5；旋转执行目标 | 入阶段起300ms→MoveAxis56ToLeftReady | 已确认：main.cpp:3550-3560 |
| 标准MoveAxis56ToLeftReady | 1=28、5=510、6=525；夹爪全开 | 三轴实际≤.2，保存axis1 hold→ClampCylinder34Wait | 已确认：main.cpp:3562-3580 |
| 标准ClampCylinder34Wait | 1/2开、3=0、4=1000，保持5/6准备点 | 入阶段起300ms→MoveAxis356BackToReady | 已确认：main.cpp:3582-3598 |
| 标准MoveAxis356BackToReady | 1/3/5/6与2/7都去最终，保持3/4夹 | 所有目标误差合格→ClampCylinder2AfterAxis3 | 已确认：main.cpp:3600-3612 |
| 标准ClampCylinder2AfterAxis3 | 2夹、1开；3/4仍启动夹持；保持最终目标 | 300ms且目标合格→Done | 已确认：main.cpp:3614-3625 |
| 恢复RecoveryMoveAxis1267 | 全程1/3开2/4夹；1/6和2/7先动 | 固定2000ms后启动axis5，不要求1/6已到 | 已确认：main.cpp:3511-3528 |
| 恢复RecoveryMoveAxis5 | 加入5最终目标 | 再2000ms后启动axis3，不要求5已到 | 已确认：main.cpp:3530-3538 |
| 恢复RecoveryMoveAxis3 | 加入3最终目标 | 所有目标合格→Done | 已确认：main.cpp:3540-3547 |
| Done | 恢复启动限速、completed=true、sync_all(30) | 同步成功才control=true；失败保留Done/completed，后续普通恢复可再同步 | 已确认：main.cpp:3432-3453、2028-2041 |

**已确认**：公共 `PreparePosition` 仅排队四个 `SetSelfCheckAxisPos` 和一个 `StartSelfCheck`，驱动PLC自检（`zenoh_service.cpp:816-838`）；公共 `StartControl` 仅排队 `SelectDirectControl`（`zenoh_service.cpp:841-850`），main在当前位置 `sync_all(20)` 成功后直接设 `Done/completed/control_active=true`（`main.cpp:5315-5356`）。它不调用 `begin_direct_positioning`。300ms标准夹爪等待和2000ms恢复分段实现仍保留，仅内部 `ExecuteStartup` 调用 `start_startup_sequence`（`main.cpp:5277-5313`、`startup_sequence.cpp:19-185`）；公共协议未映射该内部命令（`zenoh_service.cpp:814-956`）。

## B.9 连接保持、急停、回退故障与寻参复用

| 流程 | 状态/触发链 | 恢复条件 | 输出影响/互斥 | 出处与确定度 |
|---|---|---|---|---|
| 手柄hold | 不足有效柄或resume_settling→handle_soft_hold_active；≤5s缓存允许poll成功，>250ms只归零速度；>1.5s间断重返或epoch变化要静置 | 500ms连续实际新样本、年龄≤250ms，置rebase_requested；重建Follow实际基准 | hold时不更新滤波、不走运动树、清力；不等于立即PLC急停 | 已确认：手柄/手柄.cpp:6、27-104；main.cpp:1189-1295、2083 |
| ADS hold | 无本轮新有效位置快照或非Running/未握手→ads_soft_hold_active | 原planned有活动则保留事务并恢复推进；其它模式须重建基准 | 总connection_hold=初同步未完成、ADS保持或handle保持；进边除ExitSync外清spacing并关闭PI | 已确认：main.cpp:1063-1068、1202-1325 |
| 急停 | PLC通知estop_hold_req边沿→estop_hold_active/control=false；若回退活动保存unfinished再取消 | 每1s请求watchdog recovery；PLC清急停后刷新旋转保持，后续普通/回退恢复再放行 | 清力/spacing/手动；不是仅释放UI按钮就恢复 | 已确认：main.cpp:1727-1776、1987-2041 |
| 回退fault holding | fail或急停打断置ReturnRecovery.interrupted、return_ads_fault_hold=true | 每250ms提交Clear；Clear应用后更大序号新帧、ADS健康、无estop/host timeout、reinit_done、手柄有效、无其它exclusive、相关Req/Busy都false，sync_all(20) | 恢复hold置false，未完事务最多续做一次；不自动重发不确定Commit | 已确认：return_recovery.h:17-59；main.cpp:1987-2026 |
| 回退resume_pending | 保存original mask/方向/mode/scope；用当前位置重新算当前目标与g | consume_resume先增次数，然后begin校验成功才执行；失败回当前位置控制 | 与spacing、startup、其它planned互斥；resumes==0才产生pending | 已确认：return_recovery.h:56-60；main.cpp:3628-3663 |
| PLC restart | 计数变化→清control/startup/模式/spacing/planned/力、保留之前control意图 | 读PLC app名，尝试homing复用；否则正常自检 | 不能用旧Follow基准继续 | 已确认：main.cpp:1359-1376 |
| 寻参复用pending | reliability_config严格解析true、完整record、应用名相同、当前绝对位置容差合格；无记录时间过期判断 | 写leftlimit/init_pos/status/req；PLC status1且自检握手后刷新坐标和sync；拒绝走现有自检 | 只保留参数/位置，不回放旧运动；请求重试依状态而无统一deadline | 已确认：homing_record.h:40-60、84-95；main.cpp:1378-1456；`介入机器人/C从端/250902/Untitled2/POUs/MAIN.TcPOU:124-175` |
| 寻参记录刷新 | 自检完成请求坐标刷新；确认generation后save；正常每1s更新last_abs | 原子临时文件+MoveFileExW替换 | 记录与程序同目录；磁盘写可在主循环发生 | 已确认：main.cpp:1944-1970；homing_record.h:61-82 |

## B.10 跨流程耦合点

下表的“三个以上”按具有不同进入/推进/退出条件的业务流程计；同一字段多次出现不等于多个线程写。主线程中这些状态没有数据竞争证明，但有执行顺序耦合。

| 变量/对象 | 至少三个流程 | 实际相互影响 | 出处与确定度 |
|---|---|---|---|
| control_active | startup、connection recovery、estop、return fault、emergency、UI | 后段命令或故障写false会让下轮mapping清零、Follow停；自动恢复又可置true | 已确认：main.cpp:380、1369、1733、2021、2038、3442、4523-4540、4868-5402 |
| pos[7] | Follow、planned、spacing、startup、emergency、softlimit、sync | 同一帧多个先后赋值，最终门控输出；优先树决定主拥有者，之后sync/commands还可改 | 已确认：main.cpp:2083-4279、4300-4353；motion_sync.cpp:111-248 |
| axis1/6_crawl和follow_cmd_abs | Follow、换向、planned rebase、hold恢复、startup完成 | 重建窗口与基准会丢弃旧手柄差分；axis1 rebase也会改axis6 | 已确认：main.cpp:1247-1295、1665-1723、2998-3076；motion_sync.cpp:158-248 |
| axis3_base_rel / axis5_base_rel | Follow镜像、先行、同步、独立模式退出 | 先行extra吸收入基准；sync以实际重建；不能仅看axis1_command推算支撑位移 | 已确认：main.cpp:240-242、3838-3844、4080-4086；motion_sync.cpp:151-153、204-206 |
| guidewire_mode / vis override / dual_reverse | 模式选择、spacing、planned、hold/selfcheck/startup | spacing强制None并保存退出选择；双模式换手冻结方向，退出后B6重新生效 | 已确认：main.cpp:874-915、1523-1567、1779-1889、4314-4343 |
| planned_return / dual_handoff | Follow触发、connection hold、estop、spacing、softlimit、fault resume | cancel重置dual与coordinator；spacing requested等待planned但active强制取消；softlimit也清pending | 已确认：main.cpp:664-703、1598-1605、3095-3114、3238、3628-3663 |
| spacing_recovery.requested/phase / button_prev | 按键、模式选择、连接、急停、自检、reinit、emergency | reset清请求而prev不清；同一持续按住不重新产生上升沿 | 已确认：main.cpp:900-904、1306、1549-1575、1773、1916、1975、5211 |
| mapping active/progress/extra | Follow、mode selection、planned完成、hold/estop/spacing | guard一次性清零旧段；only axis6 Dual特殊保留；同步本身不arm | 已确认：main.cpp:347-363、874-881、1579-1596、2484、3064-3069 |
| cylinder_manual_* | UI调试、自动换手、启动、恢复/hold | clear_cylinder_manual_overrides由各接管者调用；最终手动写必须通过cylinder_manual_allowed | 已确认：main.cpp:830-839、1304、2471-2527、3282、4356-4399 |
| ff及clear_force_output | feedback、connection hold、estop、spacing、returnfault、selfcheck | clear同时清clamp_holds并归零；并非只改一个enable | 已确认：main.cpp:171-176、1307、1742、1775、1916-1940、2440-2469、4584-4601 |
| startup / speed backup / smoothing | startup、direct定位、emergency、自检/PLC重启 | emergency先恢复startup限速，再备份自己的限速；完成/取消恢复失败可留下低限速需后续确认 | 已确认：main.cpp:381-467、1359-1376、1916-1940、3432-3453 |
| tracking_controller | Follow、换手、手动电缸、安全hold、无力样本 | disable只重置PI；invalidate/end_segment按reason决定是否保留欠账；两个操作语义不同 | 已确认：delivery_tracking.cpp:119-126、172-178、334-476；main.cpp:1305、4240-4298、4448-4454 |
| return_ads_fault_hold / ReturnRecovery | planned fail、急停、自动Clear/restore、resume | fault会终止spacing、清mapping和force；exclusive反过来阻止恢复 | 已确认：main.cpp:1495-1501、1579-1596、1770-1776、1987-2026、2440-2469 |

**未知**：各并行服务内部私有字段的完整读写时序与现场抢占延迟无法由main本身确定；后台接口、锁和队列见A，超时见D。以下变量索引以静态可见声明/引用和显式赋值为证据，方法调用/别名的间接写独立注明，避免把词法命中误当数据流证明。

## B.11 状态变量总表（补充索引）

本节把 `main.cpp` 外层跨周期变量、循环内静态量、lambda 捕获的长期对象，以及 main 直接使用的状态结构体字段集中列出。它是 B.3–B.10 的逐项索引，不新增行为解释；“写入/读取”列只列静态可见位置。结构体内部私有字段只能通过公开 API 改写的对象，单独标注该边界。所有结论均为**已确认的静态事实**，除最后“未知项”表外不表示现场验证。


**已确认**：main.cpp 的物理行数为5424；主循环为 `while(true)`（`main.cpp:966-5402`）。除 `while` 内的 `static next_estop_recover_req_ms` 外，跨拍值均定义在主循环外，或封装在下面的长期对象中。有捕获的 lambda 均为引用捕获（例如 `main.cpp:216-379,549-961`）；无捕获的 `gate_linear_increment_for_mode` 位于 `main.cpp:470-477`，没有按值捕获形成的隐藏历史副本。

### B.11.1 口径和别名

| 口径 | 事实与出处 |
|---|---|
| 外层局部 | `main.cpp:64-961` 定义的对象、数组、标志、计时和基线跨越每次 `while` 迭代；命令处理在 `main.cpp:4868-5402` 仍可修改这些值。 |
| 循环内 static | 只有 `next_estop_recover_req_ms`，在急停保持分支按 1 s 节流 watchdog recovery（`main.cpp:1759-1767`）。 |
| ctx 别名及自有值 | `AppContext ctx` 在 `main.cpp:189-207,297-326` 保存指针；`ctx.pos/plc_act_pos/plc_init_pos/plc_leftlimit/plc_act_pos_from_left/plc_v_limit`、`ctx.axis*_crawl`、`ctx.startup` 等是外层状态的别名。模块通过指针直接写回外层数组/变量；`motion_sync::sync_all/rebase_*` 会批量改写它们（`motion_sync.cpp:158-248,303-385`）。例外是 ctx 自有的速度数组、快照序号及QPC（`control_types.h:723-725`），见下表。 |
| reset 语义 | `PlannedReturnCoordinator::reset()` 是 `*this = PlannedReturnCoordinator{}`，会清整个协调器（`control_types.h:295-371`）；`SpacingRecoveryState::reset()` 只显式清请求、阶段、计时/位移/恢复覆盖字段，不清保持坐标 `axis*_hold_rel/cmd_rel`（`control_types.h:395-438`）。`DualHandoffRequest::reset()` 是整结构清零（`control_types.h:373-385`）。 |
| 证据限制 | 下面的“写入/读取”列列出静态可见位置；服务对象（ADS、tracking、HandleNetDistance、Recorder、Zenoh）内部私有字段的逐字段写入由其类实现负责，main 只列公开 API 调用。精确词法行集另见 `scratch/main_outer_inventory.tsv`，生成脚本为 `scratch/state_inventory.py`。 |

### B.11.2 main.cpp 外层长驻变量

#### B.11.2.1 启动、通信和外设对象

| 名称（类型） | 含义/流程 | 写入位置 | 读取位置 | 确定度 |
|---|---|---|---|---|
| `cfg (ControlConfig)` | 动作缩放、窗口、超时、启动/回退/屈曲参数；默认值在 `control_types.h:78-201`。 | 初始化 `main.cpp:64`；控制域旧命令分支可改 `cfg.startup_motion_speed_scale`、`cfg.axis1_post_return_lead_mm`（`main.cpp:5235-5251,5299-5305`）；当前主端是否可送达见 E 主端链路，不能仅凭分支存在声称 UI 可改。重力补偿开关属于另一个 `cal_cfg`。 | 运动树/计划回退/spacing/启动/滤波/软限位/力反馈（`main.cpp:1464-1465,2100-4309,4894-5383`）。 | 已确认 |
| `cyl (const CylinderPreset)` | 四电缸自动开/夹/跟随值（`control_types.h:203-218`）。 | 构造时初始化；无运行时写入。 | 缺省输出、planned return、startup、spacing（`main.cpp:2042-2045,2594-2635,3325-3388,3514-3617,3847-4397`）。 | 已确认 |
| `run_directory`, `diagnostic_path`, `homing_record_path` | exe 目录、故障日志和寻参记录路径。 | 初始化 `main.cpp:46-75`。 | 故障写入 `main.cpp:1030-1050,1211-1238,1743-1747`；寻参读写 `main.cpp:69,1410-1456,1944-1968`。 | 已确认 |
| `homing_reuse_enabled`, `homing_record`, `plc_application_name` | 寻参复用资格、已保存的七轴坐标/应用名、当前 PLC 应用名。 | `homing_record` 在 `main.cpp:1949-1968` 更新；应用名 `1350-1355`。 | 复用判断 `main.cpp:1347-1456`。 | 已确认 |
| `homing_reuse_pending`, `homing_restore_submitted`, `control_before_plc_restart`, `homing_restore_applied` | PLC 重启后的寻参复用阶段标志。 | PLC 重启/读回路径 `main.cpp:1359-1456`。 | 同区间以及启动/控制门控 `main.cpp:1925-1935`。 | 已确认 |
| `homing_record_update_ms`, `homing_record_pending`, `homing_record_refresh_generation` | 记录保存时间和坐标刷新 generation。 | `main.cpp:1907-1908,1944-1968`。 | 复用/每秒更新门控 `main.cpp:1945-1968`。 | 已确认 |
| `handle_axis1`, `handle_axis6 (Handle)` | 物理逻辑 SN587/SN582 的远程手柄对象；`poll()` 更新关节、按钮、恢复静置和重建请求。 | 初始化/重开/关闭 `main.cpp:80-81,1079-1185,5421-5422`；模块内部状态见 `手柄/Handle.h:10-64`。 | 轮询和输出 `main.cpp:1135-1200,1461-1509,4587-4731`。 | 已确认 |
| `axis1_handle_ready`, `axis6_handle_ready_init`, `axis6_handle_ready`, `handle_startup_locked` | 手柄初始化与单/双手柄锁定状态。 | `main.cpp:117-156,1080-1114`。 | `main.cpp:1137-1187,1327-1344,2033-2068`。 | 已确认 |
| `remote_single_handle_since_ms`, `remote_single_handle_grace_ms` | 远程只有一只手柄时的 3 s 宽限。 | `main.cpp:1092-1113`。 | `main.cpp:1095-1099`。 | 已确认 |
| `single_handle_mode`, `single_handle_requested_mode`, `requested_guidewire_mode_prev` | 单/双输入拓扑、单柄导丝请求及边沿基线。 | 锁定 `main.cpp:130-156`；命令/模式切换 `1779-1889,4975-4978`。 | 按键/模式/运动树 `main.cpp:1503-1547,1779-1889,3672-4227`。 | 已确认 |
| `axis1_input_handle`, `axis6_input_handle` | 逻辑 axis1/axis6 取样指针；双柄为 587/582，单柄指向同一对象。 | `main.cpp:127-148,210-214`。 | 采样/同步/运动 `main.cpp:1140-1187,1252-1276,1461-1470,2174-2185`。 | 已确认 |
| `ads (CADSComm)`, `ads_communication (AdsCommunicationService)`, `arm_manual_ads` | ADS 兼容对象、100 Hz 快照/输出线程、定位臂低频线程。 | 构造/启动 `main.cpp:82,96-104`；服务内部跨线程状态见 `ads_communication.h:18-257`、`arm_manual_ads_service.h:17-129`。 | 主循环通过 wait/stats/event/publish/read/write 调用 `main.cpp:969-1068,1350-1456,1987-2041,4522-4582,4868-5402`。 | 已确认 |
| `tracking_controller` | 两轴主从位移补偿控制器；内部持有两个 `AxisRuntime` 和参数。 | 构造 `main.cpp:83`；通过 `set_parameter/enable/update_gate/begin/commit/finish/invalidate` API 修改（`main.cpp:1059,1305,1369,2160-2231,3699-4769,5030-5089`）。 | 同上；内部字段定义在 `delivery_tracking.h:42-174`。 | 已确认 |
| `axis1_net_distance`, `axis6_net_distance` | 连续 Follow 段净位移累计；换手/无效快照时停计。 | `set_following/sample` `main.cpp:1054-1055,2071-2079,4515-4518`。 | 状态发布和命令 `main.cpp:4848-4862,5030-5038`；私有字段见 `handle_net_distance.h:7-35`。 | 已确认 |
| `experiment_recorder`, `handle_velocity_recorder` | 实验记录器和 100 Hz 手柄速度 CSV 记录器。 | 启动 `main.cpp:85-91,957-960`；各拍 enqueue/health/stop `4401-4680,5094-5121,5416`。 | 状态发布 `main.cpp:4608-4680,4775,5054-5055`；私有线程字段见 `experiment_recorder.h:134-225`。 | 已确认 |
| `zenoh_service` | Zenoh 接收手柄/命令、发布状态/力；内部有 worker、lease、结果队列。 | 构造/启动 `main.cpp:92,108-113`；主循环 `publish_state/poll_command/command_processed` `4864-5402`；停止 `5420`。 | `remote_cfg` 和命令/状态调用 `main.cpp:108-113,4681-4865`；私有字段见 `zenoh_service.h:31-143`。 | 已确认 |

#### B.11.2.2 PLC 镜像、Follow 基线和运动模式

| 名称（类型） | 含义/流程 | 写入位置 | 读取位置 | 确定度 |
|---|---|---|---|---|
| `pos[7]` | 本周期要写入 `G.refer` 的相对目标。 | `load_pos_from_actual`、各运动分支、planned/spacing/startup/手动覆盖 `main.cpp:2086-4582`；最终 `publish_output` 使用。 | `main.cpp:1045-1046,2390-2417,4490-4521`；`plc_io::write_refer`/ADS 输出。 | 已确认 |
| `plc_act_pos[7]`, `plc_init_pos[7]`, `plc_leftlimit[7]`, `plc_act_pos_from_left[7]`, `plc_v_limit[7]` | PLC 实际相对位置、零点偏置、左限位、距左限位和速度上限镜像。 | `plc_io::read_plc_state/load_pos_from_actual/read_v_limit`；`main.cpp:969-978,1051-1068,2083-2089`。 | 全运动树、限位、启动/寻参、输出（`main.cpp:2096-4329`）。 | 已确认 |
| `ctx.plc_act_velocity_mm_s[7] (double[7])`, `ctx.plc_snapshot_sequence (std::uint64_t)`, `ctx.plc_snapshot_qpc_ticks (std::int64_t)` | ctx自有的轴速度及最近镜像的源序号/QPC；不是指针别名。 | 声明 `control_types.h:723-725`；快照路径更新0/2/4/5轴速度、序号/QPC（`plc_io.cpp:210-215`），兼容同步读路径在 `plc_io.cpp:241-251` 更新。 | 主程序直接读四轴速度以确认双手柄换手低速（`main.cpp:3888-3889`）；两个元数据字段在main无直接读取。 | 已确认 |
| `startup_smoothing_bypass` | 写入 PLC 的启动/急退平滑旁路。 | `main.cpp:755,2030,4419-4425`。 | ADS 输出构造 `main.cpp:4517-4521`，PLC 使用见 `plc_io.cpp:130-153`。 | 已确认 |
| `axis3_base_rel`, `axis5_base_rel`, `axis6_mirror_base_rel` | 普通导管 Follow 的支撑/镜像基准。 | `motion_sync::sync_all/rebase_*`（`motion_sync.cpp:132-154,188-210,337-377`）。 | 单柄/双柄镜像和输出 `main.cpp:3672-4227`。 | 已确认 |
| `axis2_hold_rel`, `axis7_hold_rel` | 旋转轴保持值；恢复/急停时常以当前实际位置重建。 | `main.cpp:481-486,1259-1324,1731-1756`；`motion_sync.cpp:105-154,303-385`。 | 每周期 `pos[1]/pos[6]`、planned/spacing/startup `main.cpp:2086-2089,2563-2637,3297-3388`。 | 已确认 |
| `axis1_prev_linear_filtered`, `axis6_prev_linear_filtered`, `axis1_prev_rot_filtered`, `axis6_prev_rot_filtered` | 差分输入上一拍滤波基准；模式切换/恢复时重置以避免跳变。 | `main.cpp:1252-1257,1461-1470,1702-1705,4584-4606`；`motion_sync.cpp:132-154,188-210,337-377`。 | 增量计算 `main.cpp:2099-2129`。 | 已确认 |
| `axis1_follow_cmd_abs`, `axis6_follow_cmd_abs` | 当前 Follow 累计目标绝对坐标。 | `motion_sync.cpp` 同步及运动树 `main.cpp:3788-3830,4060-4089`。 | 触边/软限位/状态输出 `main.cpp:3081-3165,3726-4117,4850-4862`。 | 已确认 |
| `axis4_axis6_coupling_last_ms`, `axis4_axis6_coupling_active_prev` | 轴4手动与轴6耦合的时间/边沿状态。 | `main.cpp:3769-3976`。 | 轴4耦合分支同区间。 | 已确认 |
| `axis1_delivery_mapping_active`, `axis1_delivery_mapping_progress_mm`, `axis1_delivery_mapping_applied_extra_mm` | 普通导管正向回退后前 10 mm 的 axis1 先行附加量。 | clear/arm lambda `main.cpp:347-357`；模式/回退清理 `874-880,1576-1597,2484-2485,3069`；运动分支 `3788-3830,4057-4089`。 | 同上。 | 已确认 |
| `axis1_reverse_switch_guard_active`, `axis6_reverse_switch_guard_active` | 方向切换边沿附近的一次性触发保护。 | `motion_sync.cpp:132-154,188-210,337-377`；方向切换 `main.cpp:1660-1707`。 | 触边/模式切换 `main.cpp:2099-2129,3634-3722`。 | 已确认 |
| `axis1_prev_abs_for_trigger`, `axis6_prev_abs_for_trigger`, `axis1_prev_abs_valid`, `axis6_prev_abs_valid` | 触边判定的上一拍实际位置和有效标志。 | 同步/运动树 `motion_sync.cpp:132-154,188-210,337-377`、`main.cpp:3665-3926`。 | 触边判定 `main.cpp:3634-3722,3859-3924`。 | 已确认 |
| `guidewire_mode` | `None/Independent`；单柄独立导丝模式。 | `main.cpp:270,1780-1889,1922-1924,5214-5215,5338-5347`；`guidewire_mode.cpp`。 | 轴6独立运动、模式/状态发布 `main.cpp:1536-1547,3121-3229,3672-3722,4850`。 | 已确认 |
| `dual_reverse_active` | 双手柄全局撤出方向；任一 B6 置位时为 true，换手/屈曲期间冻结。 | `main.cpp:1523-1528`。 | `axis1_reverse_pressed`、双 Follow `main.cpp:1534-1547,3723-3926`。 | 已确认 |
| `independent_axis1/2/3/5_hold_rel` | 独立导丝模式冻结导管侧轴位。 | 进入/退出/重同步 `guidewire_mode.cpp:64-113,130-149`、`motion_sync.cpp:220-285`。 | `main.cpp:2619-2626,3078-3165,3322-3388`。 | 已确认 |
| `axis6_window_locked`, `axis6_locked_window_start_abs/end_abs` | 独立导丝窗口锁存；普通重建可更新窗口。 | `motion_sync.cpp:66-91,243-248,303-385`。 | `main.cpp:2398-2417,3078-3165,3672-3722`。 | 已确认 |
| `startup (StartupState)` | 启动阶段、目标、保持位姿、恢复启动资格和速度备份。 | `main.cpp:281,3390-3626,4907-5305`；模块 `startup_sequence.cpp`。 | `main.cpp:2029-2041,3423-3626,4861-4865`。 | 已确认 |
| `control_active` | 正常手柄控制是否激活；与 startup/hold/故障标志并行门控。 | `main.cpp:380,1334-1337,1732-1734,2020-2039,3442-3503,5219-5225,5347`。 | 运动入口、tracking、状态发布 `main.cpp:2031-2089,2137-2152,4681-4865`。 | 已确认 |
| `emergency_retract_active (bool)`, `emergency_retract_axis1/2/7_hold_rel (double)`, `emergency_retract_v_limit_backup[7] (double)`, `emergency_retract_v_limit_scaled (bool)` | 极简器械撤出；固定轴1/2/7，轴3/5/6回当前启动准备目标；保存限速以便结束后恢复。 | 初始化/限速备份与恢复 `main.cpp:421-466`；命令启动时锁存坐标并置 active `main.cpp:5198-5228`；到位清 active 并恢复速度 `main.cpp:4523-4527`；退出再次恢复速度 `main.cpp:5404`。 | 运动和输出门控 `main.cpp:1588-1628,2007-2084,3297-3313,4357-4381,4475,4523`。 | 已确认 |

#### B.11.2.3 连接保持、急停、按键、回退故障和输出门控

| 名称（类型） | 含义/流程 | 写入位置 | 读取位置 | 确定度 |
|---|---|---|---|---|
| `self_check_done`, `has_self_check_flag`, `last_self_check_done` | PLC 自检完成缓存及边沿。 | `main.cpp:1012-1022,1337-1344,1891-1943`。 | 启动/运动有效性/状态 `main.cpp:1063-1068,1327-1344,2031-2035,4860-4865`。 | 已确认 |
| `handle_reinit_req`, `last_handle_reinit_req` | PLC 请求主机重建手柄/位置基准及边沿。 | `main.cpp:1022-1023,1972-1985`。 | 连接/同步门控 `main.cpp:1063-1066,1973-1983`。 | 已确认 |
| `estop_hold_req`, `estop_hold_active` | PLC 急停请求与主循环锁存；边沿清控制/回退/力。 | `main.cpp:1023-1050,1727-1756`。 | 全运动和恢复门控 `main.cpp:1730-1777,1987-2035,4356-4606`。 | 已确认 |
| `axis1_push_rearm_after_hold`, `axis1_delivery_stop_latched`, `axis1_delivery_stop_prompted` | 轴1支撑轴3近限位停送和提示状态；`axis1_push_rearm_after_hold` 当前只有初始化及退出屈曲时清false，没有读取，不能据名字认定它仍实现重推门控。 | 声明 `main.cpp:498-500`；stop标志 `main.cpp:3993-4005,4178-4182,4331-4334`。 | stop标志在 `main.cpp:3993-4026,4179` 读取；push_rearm在main没有读取。 | 已确认 |
| `axis1_reverse_pressed_prev`, `axis6_effective_reverse_prev` | B6 方向边沿基线。 | `main.cpp:1674-1725`。 | 方向切换和触发保护。 | 已确认 |
| `axis4_manual_error_prev`, `axis4_manual_error_id_prev` | 轴4点动故障通知去重。 | 声明 `main.cpp:503-504`；逐拍更新 `main.cpp:4353-4354`。 | 错误边沿比较/打印 `main.cpp:4346-4352`。 | 已确认 |
| `axis4_ui_forward_pressed`, `axis4_ui_reverse_pressed`, `y_valve_open` | UI 轴4按住电平与 Y 阀状态。 | 命令消费 `main.cpp:1608-1664,4868-5402`。 | 输出/状态 `main.cpp:2047-2050,4383-4400,4848-4865`。 | 已确认 |
| `injector_ui_direction[2]`, `injector_ui_jog_deadline_ms[2]`, `axis4_ui_jog_deadline_ms`, `manual_jog_lease_ms=500` | 注射器/轴4手动点动方向及 500 ms 租约。 | `main.cpp:508-513,1608-1726,4868-5402`。 | 到期清零和 ADS 输出 `main.cpp:1608-1664,4356-4400,4517-4521`。 | 已确认 |
| `axis1_fast_return`, `axis6_fast_retract` | 每拍清零，由planned阶段决定后写PLC回退旁路；emergency自身使用 `startup_smoothing_bypass`。 | `main.cpp:515-516,1486-1488,2579-2609,3005-3006`。 | ADS输出传入两fast flag（`main.cpp:4478-4479`）；emergency旁路在 `main.cpp:2030`。 | 已确认 |
| `return_ads_fault_hold`, `return_recovery` | 回退 ADS 故障锁存和自动清 Req/续做条件。 | `main.cpp:517-523,1734-1741,2440-2469,1987-2026`；类内部见 `return_recovery.h:14-70`。 | 全运动门控、状态/故障恢复。 | 已确认 |
| `interrupted_return_mode`, `interrupted_return_scope`, `original_return_intent`, `resuming_return`, `recovery_event_text` | 回退故障续做时保存原意图、模式/重建范围和提示文本。 | `main.cpp:2512-2520,3390-3664`。 | 续做入口及状态 `main.cpp:3390-3664,4861-4865`。 | 已确认 |
| `pending_return_error_axis`, `pending_return_error_event_sequence`, `pending_return_error_deadline_ms` | PLC ErrorId 晚到时的 2 s 补报上下文。 | `main.cpp:2449-2464,982-1010`。 | 每周期补报。 | 已确认 |
| `axis6_soft_limit_hold`, `axis6_soft_limit_reason`, `axis6_soft_limit_warning_active`, `axis6_soft_limit_warning_reason`, `axis6_soft_limit_blocked_target_from_left_mm` | axis6 上位机软限位保持/原因/告警目标。 | `main.cpp:705-757,1489-1496,3081-3114,4057-4117`。 | 运动门控、状态发布、mapping 清除。 | 已确认 |
| `force_sample`, `clean_force_monitor_enabled`, `force_feedback_diag_reason`, `force_sample_diag_reason` | 力采样缓存、纯净力监视开关、诊断文本。 | `main.cpp:542-546,759-804,4401-4606,4868-5402`。 | 力反馈/记录/状态。 | 已确认 |
| `loop_count`, `ads_snapshot_sequence` | 主循环计数和已消费 ADS 快照序号。 | `loop_count++` `main.cpp:1486`；序号 `969-979,762-780`。 | 快照等待、记录、状态/每拍输出。 | 已确认 |
| `remote_force_zero_ok_count`, `remote_force_zero_fail_count` | 取零成功/失败累计，作为状态结果。 | `main.cpp:796-803,4983-4984`。 | Zenoh 状态 `main.cpp:4848-4865`。 | 已确认 |
| `initial_sync_done`, `plc_app_name_read` | 首次全量同步及 PLC 应用名读取完成。 | `main.cpp:1327-1357`。 | 连接保持、自动控制恢复、寻参。 | 已确认 |
| `handle1_next_retry_ms`, `handle6_next_retry_ms` | 手柄打开重试 deadline（1 s）。 | `main.cpp:1082-1132`。 | 同区间。 | 已确认 |
| `handle_soft_hold_active`, `connection_hold_active_prev`, `ads_soft_hold_active`, `plc_restart_recovery_latched` | 手柄/ADS/总连接保持边沿和 PLC 重启锁存。 | `main.cpp:1189-1325,1359-1376,1931-1935,2028-2041`。 | 运动门控、恢复重同步。`plc_restart_recovery_latched` 当前可见写入但无读取（`1367,1935,3443,3503,5350`）。 | 已确认 |
| `cylinder_manual_mode[4]`, `cylinder_manual_position[4]`, `cylinder_manual_restore[4]`, `cylinder_manual_written_mask` | 调试电缸手动覆盖及恢复值。 | `main.cpp:813-858,4934-4966,5217,4356-4399`。 | 最终离散输出和 tracking 门控。 | 已确认 |
| `vis_reverse_override_active/value/target` | 单柄 UI 模式选择覆盖、spacing 退出时保存/恢复。 | `main.cpp:860-915,1553-1566,4314-4343,4975-5028`。 | 方向/模式计算 `1534-1547`。 | 已确认 |
| `pending_mode_selection`, `spacing_recovery_button_pressed_prev` | 屈曲恢复期间排队的模式和恢复键边沿基线。 | `main.cpp:898-915,1514-1520,1549-1575,4314-4343`。 | 同区间。 | 已确认 |
| `pending_selfcheck_target[7]`, `pending_selfcheck_axes`, `selfcheck_command_error`, `selfcheck_start_pending` | UI 自检目标收集、轴位掩码和异步启动状态。 | `main.cpp:4868-4923,5317-5335`。 | 自检/启动资格。 | 已确认 |
| `pending_startup (PendingStartupParams)` | 延后提交的启动目标与速度比例；七个字段 `axis1/3/5/6_from_left_mm`, `axis2_deg`, `axis7_deg`, `speed_scale` 都是double。 | 类型/实例 `main.cpp:918-926`；默认 `main.cpp:931-935`；参数命令 `main.cpp:5260-5275`。 | 提交时校验并复制到startup/cfg（`main.cpp:5280-5305`）。 | 已确认 |
| `next_estop_recover_req_ms (static ULONGLONG)` | 循环内唯一可变static，急停时每1s请求watchdog recovery的下一时刻。 | `main.cpp:1761,1765`（GetTickCount64）。 | `main.cpp:1763-1766`。 | 已确认 |
| `tracking_manual_cylinder_override` | 本拍是否有任一手动电缸覆盖，禁用 tracking。 | `main.cpp:2049-2058`。 | `main.cpp:2068,2145,2159`。 | 已确认 |
| `handled_plc_restart_count`, `handled_reconnect_count` | 已处理 ADS 重连/PLC 重启计数。 | `main.cpp:1051-1060,1359-1376`。 | 同区间。 | 已确认 |
| `drained_ads_snapshots`, `handle_record_history` | 记录器排空的 ADS 快照临时容器、最近 100 ms 手柄有效性历史。 | `main.cpp:979,1472-1484,4608-4680`。 | 记录器。容器本身跨拍，但元素是按窗口淘汰。 | 已确认 |
| `vis_qpc_frequency`, `next_vis_publish_qpc` | 30 Hz 状态发布的 QPC 频率和下一 deadline。 | 初始化/发布 `main.cpp:950-952,4681-4865`。 | 同区间。 | 已确认 |
| `recording_info` | 自动记录会话启动信息（两手柄序列号、单柄模式）。 | `main.cpp:953-957`。 | 记录器启动，之后只作启动快照。 | 已确认 |

### B.11.3 直接使用的结构体字段

#### B.11.3.1 `HandleFilterState`、`CrawlState`、`StartupState`

| 结构/字段 | 跨周期含义 | 写入 | 读取/流程 | 出处 |
|---|---|---|---|---|
| `HandleFilterState.axis0_filtered`, `.axis1_filtered`, `.inited` | 手柄线性/旋转一阶滤波值和是否有基准。 | `update/reset` `main.cpp:1252-1257,1461-1470`；`motion_sync.cpp:132-154,188-210,337-377`。 | 增量 `main.cpp:2090-2129`；先行/旋转/重同步。 | `control_types.h:52-76` |
| `CrawlState.enabled` | 该轴 Follow 逻辑是否可用。 | `motion_sync.cpp:243-248,303-385`；模式退出 `main.cpp:1800-1809,1922-1924`。 | `main.cpp:3078-3165,3672-4227`。 | `control_types.h:456-471` |
| `CrawlState.window_active` | 当前命令是否在窗口内。 | 同步/运动树。 | 方向门控和目标裁剪 `main.cpp:2105-2129,3634-3722`。 | 同上 |
| `CrawlState.handle_ref`, `.rot_ref` | 当前段手柄线性/旋转基准。 | `motion_sync.cpp:132-154,188-210,243-285,337-377`。 | Follow 增量和回退后重基准。 | 同上 |
| `CrawlState.base_rel`, `.rot_base_rel` | 当前段 PLC 轴/旋转基线。 | `motion_sync.cpp:132-154,188-210,243-285,337-377`。 | `main.cpp:3672-4227`。 | 同上 |
| `CrawlState.start_abs`, `.end_abs` | 触发窗口两端；`min_abs/max_abs` 为计算方法。 | axis1/axis6 同步、窗口重建 `motion_sync.cpp:66-112,132-154,243-285,303-385`。 | 触边、目标安全检查 `main.cpp:2390-2417,3081-3165,3634-3924`。 | 同上 |
| `StartupState.phase`, `.completed`, `.prompted`, `.direct_positioning`, `.phase_t0` | 启动阶段、完成/提示/直接定位模式和阶段计时。 | `main.cpp:403-417,1334-1342,1359-1376,3390-3626,5219-5347`；`startup_sequence.cpp`。 | 启动/控制门控、状态发布。 | `control_types.h:655-698` |
| `StartupState.axis*_hold_rel` | 启动首拍锁存的轴位。 | `main.cpp:403-410, startup_sequence.cpp:95-104`。 | `main.cpp:3423-3626`。 | 同上 |
| `StartupState.axis3/5/6_move_base_rel`, `direct_axis5/6_stage_from_left_mm` | 恢复启动阶段基准和直接定位中间目标。 | `startup_sequence.cpp:95-152`; `main.cpp:403-410`。 | `main.cpp:3390-3626`。 | 同上 |
| `StartupState.final_axis1/3/5/6_from_left_mm`, `.final_axis2/7_deg` | 当前启动目标；UI 命令写入。 | `main.cpp:291-294,4907-4912,5299-5305`。 | 启动阶段目标构造和状态。 | 同上 |
| `StartupState.recovery_mode`, `.loading_pose_match`, `.loading_ready_symbol_available`, `.loading_ready_plc` | 标准/恢复启动路径资格。 | `startup_sequence.cpp:118-152`, `main.cpp:1074-1075`。 | `main.cpp:3390-3626`。 | 同上 |
| `StartupState.v_limit_backup[7]`, `.v_limit_scaled` | 启动限速缩放和恢复。 | `startup_sequence.cpp:151-185`; `main.cpp:450-466`。 | 启动/PLC重启恢复。 | 同上 |

#### B.11.3.2 planned return、dual handoff、spacing recovery

| 结构/字段 | 跨周期含义 | 写入/读取位置 | 出处 |
|---|---|---|---|
| `PlannedReturnCoordinator.phase/mode/rebase_scope` | 统一回退阶段、业务模式和重基准范围。 | begin/step/fail/reset `main.cpp:2471-3076`。 | `control_types.h:228-269,295-371` |
| `leg_count`, `legs[2]` | 最多轴1/轴6两条腿。 | `main.cpp:2491-2506,2283-2340`。 | 同上 |
| `retry_count`, `phase_t0_ms`, `request_t0_ms`, `execution_t0_ms`, `retry_clear_t0_ms` | 阶段、请求、运动执行、清除重试的时间基线。 | `main.cpp:2483-2489,2650-2657,2710-3076`。 | `control_types.h:304-313` |
| `clamp_output_generation`, `clamp_output_applied`, `handoff_generation`, `release_output_generation`, `release_applied` | ADS output generation 是否已应用，用于夹爪稳定/交接。 | `main.cpp:2681-2708,3011-3076,4542-4582`。 | `control_types.h:314-337` |
| `request_prepared`, `commit_guard_generation`, `retry_clear_issued`, `retry_clear_snapshot_sequence` | Prepare/Commit guard 和重试清除状态。 | `main.cpp:2710-2996`。 | 同上 |
| `ads_command_purpose/sequence/submit_ms` | 异步回退 ADS 命令跟踪。 | `main.cpp:554-648,2710-2996`。 | 同上 |
| `completion_clear_confirmed` | 完成清Req的确认；cancel可依据此字段避免重复清。 | 写 `main.cpp:682,2896,3019`；读 `main.cpp:687,4482`。 | `control_types.h:317` |
| `cancel_clear_confirmed`, `cancel_snapshot_sequence`, `cancel_t0_ms`, `cancel_clear_retry_after_ms`, `cancel_timeout_reported`, `cancel_rebased`, `cancel_hold_generation` | 当前生产源码仅声明并随整结构reset恢复默认；没有按字段的main读写，不能把字段名当成仍在运行的取消子状态机。 | 当前main取消流程为 `main.cpp:664-703`；这些声明本身在 `control_types.h:316,318-323`。 | 已确认 |
| `done_snapshot_sequence` | Done 后等待更大 ADS 快照。 | `main.cpp:2884-3009`。 | 同上 |
| `hold_axis1/3/5/6_rel` | 回退期间保持的轴位。 | `main.cpp:2487-2490,2563-2637`。 | 同上 |
| `clamp_stage`, `restore_stage` | 持有侧/移动侧夹爪两级时序。 | `main.cpp:2529-2561,2681-2708,3024-3076`。 | 同上 |
| `PlannedReturnLeg.active/axis_index/target_abs/entry_rel` | 每腿轴号、目标和入场位置。 | `main.cpp:2491-2506`；输出/安全读取 `2563-2637,2382-2424`。 | `control_types.h:270-294` |
| `request_armed/acknowledged/started/possibly_started/done` | PLC Req 通知基线、是否接收/启动/可能启动/完成。 | `main.cpp:2283-2340,2710-2996`。 | 同上 |
| `request_event_sequence/ack_event_sequence/error_event_sequence/cancel_event_sequence` | 各通知边沿序号。 | 同上。 | 同上 |
| `reported_error_id`, `cancel_event_required` | 回退错误码和取消清理要求。 | `main.cpp:2320-2330,2440-2469`。 | 同上 |
| `DualHandoffRequest.axis1/axis6/wire_caused/gap_boundary/reverse/wire_reverse/axis4_caused` | 双柄换手意图和触发来源；`active()` 为 axis1 或 axis6。 | `main.cpp:3859-3924`；reset `656-667`。 | `control_types.h:373-385` |
| `SpacingRecoveryState.requested/phase` | 物理587组合键请求和 `Idle/ClampWait/Active/ExitSync` 阶段。 | `main.cpp:1508-1575,3215-3388,4300-4353`。 | `control_types.h:387-438` |
| spacing `phase_t0/last_motion_tick` | 夹爪稳定、运动 delta 和退出同步时间基线。 | 同上。 | 同上 |
| spacing `axis1/2/7_hold_rel`, `axis3/5/6_cmd_rel` | 恢复期间固定轴和三轴共同目标。 | `main.cpp:3251-3257,3322-3388`。 | 同上 |
| spacing `pending_delta_mm/moved_mm/remaining_mm/limit_logged` | 输入待移动量、已移动量、剩余安全行程和日志去重。 | `main.cpp:3215-3388`。 | 同上 |
| spacing `restore_mode_selection`, `restore_vis_reverse_override_*` | 松键后恢复的模式/方向覆盖。 | `main.cpp:1553-1566,4314-4343`。 | 同上 |

#### B.11.3.3 力、标定、恢复、tracking 和 net distance

| 结构/字段或私有状态 | 含义 | main 可见写/读 | 出处 |
|---|---|---|---|
| `cal_cfg (ForceCalibrationConfig)` `gravity_comp_enabled/validated` | 重力补偿仅在双条件下生效；两个字段默认 false；控制域旧命令分支可改 enabled，validated=false 时拒绝启用。 | 构造 `main.cpp:282`；写 `cal_cfg.gravity_comp_enabled` 在 `main.cpp:5364-5377`，不是 `cfg` 或 `cal_state`；`force_feedback.cpp:28-33` 传入标定参数。 | `force_calibration.h:27-64` |
| `ForceCalibrationState.f_zero/ft_zero/fn_2_zero/ft_2_zero/theta0_deg/zeroed` | 三帧 ADS 取零后的传感器偏置。 | `main.cpp:759-794`。 | `force_calibration.h:66-74` |
| `ForceSampleFrame` `*_value_v`, `axis1_pos_rel`, `axis2_pos_rel`, `valid`, `tick_ms`, `qpc_ticks` | 当前力样本和源时间；本地 `force_sample` 跨拍缓存。 | `main.cpp:759-794,4401-4606,5056-5066`；填充 `plc_io.cpp:159-225`。 | `control_types.h:630-645` |
| `ForceFeedbackState.enabled_582/587`, `clamp_hold_enabled` | 两侧独立力开关及换手力保持开关。 | `ff.set_enabled`/命令 `main.cpp:1368,4986-5003,5367`。 | `control_types.h:473-535` |
| `force_582_f/n`, `force_587_f/n`, `force_582_theory_f/n`, `force_582/587_filtered_f`, filter flags | 当前/理论/滤波后的反馈输出；结构体没有 `force_587_theory_*` 字段。 | 结构声明 `control_types.h:525-534`；计算/回填 `force_feedback.cpp:35-77,138-148`；main清零 `main.cpp:171-176,743,786-787,4401-4606`。 | `control_types.h:525-534,604-627` |
| `smooth_582/587 (HoldSample)` | 原始平滑力/扭矩及 tick。 | `force_feedback.cpp`；planned capture `main.cpp:2521-2524`。 | `control_types.h:487-490` |
| `hold_582/587 (HandoffHold)` | 换手后保持、完成、运动2 mm/400 ms 后 blend 的状态。 | `ff.capture/complete/observe` 由 main/force 模块调用。 | `control_types.h:475-486,548-591` |
| `ReturnRecovery` 私有 `holding/resume_pending/clear_applied/unfinished/mask/resumes/next_clear/clear_sequence/clear_observed/reason` | 故障回退 Clear/新鲜帧/条件满足后仅续做一次。main 不直接写私有字段，仅调用公开方法。 | `main.cpp:1734-1741,2440-2469,1987-2026`。 | `return_recovery.h:14-70` |
| `DeliveryTrackingController` 私有 `axis_[2].active/grip_command_since_ms/last_pi_tick_ms/last_actual_abs_mm/integral*/pending_handover_error*/nominal_forward_outstanding/compensation_forward_outstanding/snapshot` | tracking 每轴分段、PI、换手欠账和实际进度；main 只通过 API。 | `main.cpp:1059,1305,1369,2160-2231,3699-4769,5030-5089`。 | `delivery_tracking.h:89-174` |
| tracking `DeliveryTrackingParameters kp/ki/max_gain/max_error_mm` | 每轴补偿参数；默认kp=0.5、ki=2、max_gain=2、max_error_mm=20。 | UI 命令 `main.cpp:5085-5089`。 | `delivery_tracking.h:42-51` |
| `HandleNetDistance` 私有 `net_mm/actual_mm/origin_mm/sequence/sample_valid/following` | 只在连续 Follow、同 origin、新序号时累计净位移。 | main 仅 `sample/set_following` `main.cpp:1054-1055,2071-2079,4515-4518`。 | `handle_net_distance.h:7-35` |
| `Handle` 私有 `last_sample_rx_ms/rebase_requested/sample_gap_ms/opened_once/device_epoch_known/device_epoch/resume_settling/resume_since_ms` | 远程采样 5 s 失效、>1.5 s gap/epoch变化进入 500 ms 静置，再请求重建基准。 | main 仅调用 `init/poll/close/take_rebase_request/resume_settling` `1079-1200`。 | `手柄/Handle.h:10-64`、`手柄/手柄.cpp:7-105` |

### B.11.4 结构别名和整结构写入提醒

1. `ctx` 中的指针字段不是第二份状态。比如 `ctx.axis1_crawl->start_abs` 和外层 `axis1_crawl.start_abs` 是同一对象；`motion_sync::sync_all()` 还会同时清 axis6 窗口、设置滤波基准、写 `pos` 与旋转 hold（`motion_sync.cpp:303-385`）。
2. `PlannedReturnCoordinator::reset()` 会把 `phase/mode/rebase_scope/legs/计时/generation/hold/clamp` 全部恢复默认；main 的 `reset_planned_return` 还额外清 `dual_handoff`、两个 fast flag（`main.cpp:656-662`）。
3. `SpacingRecoveryState::reset()` 不清 hold/cmd 坐标，所以“退出请求”与“坐标值缓存”不是同一件事；退出完成分支随后用实际位置重同步并再 `reset()`（`main.cpp:4300-4353`）。
4. `ForceFeedbackState::reset/clear_output` 会清输出、滤波和 hold；`clear_force_output()` 还调用 `ff.clear_clamp_holds()`，因此安全保持/急停/软限位会同时清掉换手力保持（`control_types.h:604-627`、`main.cpp:171-176,743-744`）。
5. `ReturnRecovery` 的私有状态只能通过方法改变；main 不能直接把 `holding_` 或 `clear_applied_` 改回去。`restored()` 会设置 `resume_pending_ = unfinished_ && resumes_ == 0`，`consume_resume()` 才消耗一次续做机会（`return_recovery.h:47-57`）。

### B.11.5 不能从 main 词法直接确定的字段

| 项目 | 未知项 | 需要的证据 |
|---|---|---|
| `AdsCommunicationService` | **未知**：一次真实断连中快照/通知/重连线程的实际交错和到达延迟。队列策略属于可读静态实现，见A的线程/通信表；main仅通过API消费（`main.cpp:969-981,4522`）。 | 需要设备隔离的回放或现场带源时间的通信日志。 |
| `ZenohService` | **未知**：真实网络下回调/worker/main命令消费的实际交错和端到端延迟。main的消费位置为 `main.cpp:4868-5402`，静态线程边界见A。 | 需要发送端/接收端共同序号和时钟对齐的回环记录。 |
| `DeliveryTrackingController` | **已确认**：main约30Hz分支在 `main.cpp:4766-4769` 复制snapshot并于 `main.cpp:4864` 发布；`snapshot()`只返回对象副本（`delivery_tracking.cpp:342-345`）。**未知**的是网络之后UI实际接收/绘制的时间。 | 确认端到端显示延迟需主端接收/绘制时间记录。 |
| 物理键位 | **未知**：`spacing_recovery_button_mask=0x07`的bit0/1/2与手柄外壳丝印/固件键定义；代码只证明587上 `(buttons&0x07)==0x07`（`main.cpp:1505-1513`、`control_types.h:150-151`）。 | 需要固件按键定义或隔离手柄的按键采样记录。 |

### B.11.6 main长驻声明逐项类型与词法引用索引

**已确认**：本表逐项列出181个长生命周期声明（含只读常量、对象/数组，以及循环内唯一可变static）。声明位置、显式赋值、取址和其它引用由 `scratch/state_inventory.py` 从当前源码生成并人工核对；它是B.11.2语义表的精确类型补充。初始化包含在显式赋值列中。

“其它引用”不能自动等同于读取：方法调用可能改对象，传引用也可能间接写入。取址列同样只是词法命中，位运算也可能出现`&`；真正的别名/间接写入以B.11.1–B.11.4及所引模块为准。下面每个数字都属于`main.cpp`，括号内是完整的`文件:行号`引用。常量没有后续写入不构成遗漏。

| 名称 | 精确声明类型 | 声明 | 显式赋值/自增（词法） | 其它引用；取址候选（词法） | 所属流程 |
|---|---|---|---|---|---|
| `diagnostic_build` | `const char*` | `main.cpp:41` | `main.cpp:41,43` | `main.cpp:52,1030,1214`；取址 — | 诊断/固定连接标识/节流常量 |
| `diagnostic_exe` | `wchar_t[MAX_PATH]` | `main.cpp:46` | `main.cpp:46` | `main.cpp:47–48,66`；取址 — | 诊断/固定连接标识/节流常量 |
| `diagnostic_path` | `std::wstring` | `main.cpp:48` | `main.cpp:50–51` | `main.cpp:48–50,1236,1745`；取址 — | 诊断/固定连接标识/节流常量 |
| `diagnostic_slash` | `const auto` | `main.cpp:49` | `main.cpp:49` | `main.cpp:50`；取址 — | 诊断/固定连接标识/节流常量 |
| `serial_axis1_handle` | `constexpr DWORD` | `main.cpp:54` | `main.cpp:54` | `main.cpp:80,144,954,1122,1170`；取址 — | 诊断/固定连接标识/节流常量 |
| `serial_axis6_handle` | `constexpr DWORD` | `main.cpp:55` | `main.cpp:55` | `main.cpp:81,144,955,1130,1183`；取址 — | 诊断/固定连接标识/节流常量 |
| `ads_snapshot_wait_timeout_ms` | `constexpr DWORD` | `main.cpp:57` | `main.cpp:57` | `main.cpp:970`；取址 — | 诊断/固定连接标识/节流常量 |
| `vis_publish_rate_hz` | `constexpr std::int64_t` | `main.cpp:59` | `main.cpp:59` | `main.cpp:4685`；取址 — | 诊断/固定连接标识/节流常量 |
| `axis4_coupled_axis6_speed_mm_s` | `constexpr double` | `main.cpp:61` | `main.cpp:61` | `main.cpp:3769,3976`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `hardcoded_ads_netid` | `const char*` | `main.cpp:62` | `main.cpp:62` | —；取址 — | 诊断/固定连接标识/节流常量 |
| `cfg` | `ControlConfig` | `main.cpp:64` | `main.cpp:5246,5305` | `main.cpp:64,109–110,166–167,291–294,355,392,397,445–447,600,602,604,606,654,712,748,935,1464–1465,1469–1470,1509,1581,1675,1701,1844,1850,2100,2102,2117,2121,2123,2312,2393,2417,2535,2537,2542,2544,2549,2551,2556,2558,2811,2859,2879,2904,2992,3081,3164–3165,3170–3171,3217,3219,3221–3222,3244,3264–3266,3271,3293,3311–3313,3328,3336,3360,3375,3400–3402,3425–3430,3466–3468,3470–3475,3523,3533,3555–3557,3567–3574,3587–3591,3605–3606,3618–3619,3621,3645,3649,3651,3654,3726–3727,3729,3785,3789–3791,3793,3829,3842,3889,3900–3901,3903,3947–3948,3995,4060,4062,4071,4073,4117,4143,4149,4155–4158,4160–4161,4234,4303–4305,4307,4309,4329,4597,4894,4897,5239,5283`；取址 `main.cpp:197,1506–1507,1509` | 运行配置/电缸预设（字段见A及B.11.7） |
| `cyl` | `const CylinderPreset` | `main.cpp:65` | — | `main.cpp:65,2042–2045,2155,2595,2597,2612,2614,2627–2628,2634–2635,3110–3113,3153–3154,3325–3327,3514–3517,3553–3554,3565–3566,3585–3586,3603–3604,3616–3617,3847–3850,4132–4133,4252,4254,4394–4397,4517,4521`；取址 `main.cpp:198` | 运行配置/电缸预设（字段见A及B.11.7） |
| `run_directory` | `const std::filesystem::path` | `main.cpp:66` | `main.cpp:66` | `main.cpp:67–68,87`；取址 — | 输出路径/启动常量 |
| `homing_record_path` | `const auto` | `main.cpp:67` | `main.cpp:67` | `main.cpp:69,1960,1968`；取址 — | 寻参复用/PLC重启 |
| `homing_reuse_enabled` | `const bool` | `main.cpp:68` | `main.cpp:68` | `main.cpp:1410`；取址 — | 寻参复用/PLC重启 |
| `homing_record` | `homing::Record` | `main.cpp:69` | `main.cpp:69,1949–1952,1955–1957,1966–1967` | `main.cpp:1410,1423,1952,1960,1968`；取址 `main.cpp:1962` | 寻参复用/PLC重启 |
| `plc_application_name` | `std::string` | `main.cpp:70` | `main.cpp:1354` | `main.cpp:70,1410,1950`；取址 — | 寻参复用/PLC重启 |
| `homing_restore_submitted` | `bool` | `main.cpp:71` | `main.cpp:71,1363,1425,1444` | `main.cpp:1378,1427,1436,1892,1962,1970`；取址 — | 寻参复用/PLC重启 |
| `homing_reuse_pending` | `bool` | `main.cpp:71` | `main.cpp:71,1362,1415,1429,1445` | `main.cpp:1347,1378,1962`；取址 — | 寻参复用/PLC重启 |
| `control_before_plc_restart` | `bool` | `main.cpp:72` | `main.cpp:72,1361` | `main.cpp:1451–1452,1927`；取址 — | 寻参复用/PLC重启 |
| `homing_restore_applied` | `bool` | `main.cpp:72` | `main.cpp:72,1449,1926` | `main.cpp:1905,1925`；取址 — | 寻参复用/PLC重启 |
| `homing_record_update_ms` | `ULONGLONG` | `main.cpp:73` | `main.cpp:73,1959,1965` | `main.cpp:1963`；取址 — | 寻参复用/PLC重启 |
| `homing_record_pending` | `bool` | `main.cpp:74` | `main.cpp:74,1364,1907,1948` | `main.cpp:1945,1962`；取址 — | 寻参复用/PLC重启 |
| `homing_record_refresh_generation` | `std::uint64_t` | `main.cpp:75` | `main.cpp:75,1908` | `main.cpp:1946`；取址 — | 寻参复用/PLC重启 |
| `handle_axis1` | `Handle` | `main.cpp:80` | — | `main.cpp:80,117,167,1085,1117,1120,1160,1162,1168,1191–1192,1194,1199,1288,1505,4588,4728,4730,5421`；取址 `main.cpp:127,136,143,193,1152,4730` | 输入拓扑/连接保持/重建基准 |
| `handle_axis6` | `Handle` | `main.cpp:81` | — | `main.cpp:81,118,166,1090,1125,1128,1173,1175,1181,1191–1192,1195,1200,1289,4587,4729,4731,5422`；取址 `main.cpp:128,137,143,194,1153,4731` | 输入拓扑/连接保持/重建基准 |
| `ads` | `CADSComm` | `main.cpp:82` | — | `main.cpp:82,96`；取址 `main.cpp:190` | 后台通信服务（内部状态见A/D） |
| `tracking_controller` | `DeliveryTrackingController` | `main.cpp:83` | — | `main.cpp:83,1059,1305,1369,2160,2167,2174,2180,2197,2203,2208–2209,2211,2213,2227,2230,3699,3716,4043,4106,4245,4255,4262,4270–4271,4292–4293,4295,4406,4449,4451,4766,4768–4769,5034,5036,5072,5085,5218`；取址 — | tracking门控/PI/欠账 |
| `axis1_net_distance` | `HandleNetDistance` | `main.cpp:84` | — | `main.cpp:84,1054,2073,4515,4851`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_net_distance` | `HandleNetDistance` | `main.cpp:84` | — | `main.cpp:84,1055,2076,4518,4852`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `experiment_recorder` | `ExperimentRecorder` | `main.cpp:85` | — | `main.cpp:85,957,960,963,4402–4403,4405,4608,4611,4647,4677,4775,5054–5055,5094,5108,5114,5121,5417`；取址 — | 记录/30Hz状态发布 |
| `handle_velocity_recorder` | `HandleVelocityRecorder` | `main.cpp:86` | — | `main.cpp:86,88,5416`；取址 — | 记录/30Hz状态发布 |
| `handle_velocity_path` | `const auto` | `main.cpp:87` | `main.cpp:87` | `main.cpp:88–89`；取址 — | 记录/30Hz状态发布 |
| `zenoh_service` | `ZenohService` | `main.cpp:92` | — | `main.cpp:92,111,4864,5399,5420`；取址 `main.cpp:4871` | 后台通信服务（内部状态见A/D） |
| `axis1_handle_filter` | `HandleFilterState` | `main.cpp:93` | `main.cpp:1659` | `main.cpp:93,1252,1254,1256,1461,1661,2091–2092,4602,4604`；取址 `main.cpp:195` | 输入拓扑/连接保持/重建基准 |
| `axis6_handle_filter` | `HandleFilterState` | `main.cpp:94` | `main.cpp:1660` | `main.cpp:94,1253,1255,1257,1466,1662,1703–1704,2093–2094,4603,4605`；取址 `main.cpp:196` | 输入拓扑/连接保持/重建基准 |
| `ads_communication` | `AdsCommunicationService` | `main.cpp:96` | — | `main.cpp:96–98,609,628,676,767,850,969,979–981,1056–1057,1350,1396,1408,1425–1426,1442,1766,1908,1910,1913,1946,1976–1977,1998,2016,2687,2805,3014,3041,4522,4914,4916,5412,5414,5419`；取址 `main.cpp:207` | 后台通信服务（内部状态见A/D） |
| `arm_manual_ads` | `ArmManualAdsService` | `main.cpp:97` | — | `main.cpp:97,102,4797,5127,5130,5134,5137,5140,5144,5148,5152,5160,5418`；取址 — | 后台通信服务（内部状态见A/D） |
| `axis1_handle_ready` | `bool` | `main.cpp:117` | `main.cpp:117,1085` | `main.cpp:153,155,1092,1095,1106`；取址 — | 输入拓扑/连接保持/重建基准 |
| `axis6_handle_ready_init` | `const bool` | `main.cpp:118` | `main.cpp:118` | `main.cpp:119`；取址 — | 输入拓扑/连接保持/重建基准 |
| `axis6_handle_ready` | `bool` | `main.cpp:119` | `main.cpp:119,1090` | `main.cpp:153,155,1092,1106`；取址 `main.cpp:1095` | 输入拓扑/连接保持/重建基准 |
| `handle_startup_locked` | `bool` | `main.cpp:121` | `main.cpp:121,138,148` | `main.cpp:132,1080,1137`；取址 `main.cpp:1329` | 输入拓扑/连接保持/重建基准 |
| `remote_single_handle_since_ms` | `ULONGLONG` | `main.cpp:123` | `main.cpp:123,1097,1102,1112` | `main.cpp:1097–1098`；取址 — | 输入拓扑/连接保持/重建基准 |
| `remote_single_handle_grace_ms` | `constexpr ULONGLONG` | `main.cpp:124` | `main.cpp:124` | `main.cpp:1098`；取址 — | 输入拓扑/连接保持/重建基准 |
| `single_handle_mode` | `bool` | `main.cpp:125` | `main.cpp:125,135,142` | `main.cpp:298,877,888,893,956,1139,1189,1265,1524,1529,1534,1536,1546,1651,1666,1780,2078,2150,2187,3723,4337,4519,4705,4976,4978,5008,5107`；取址 — | 输入拓扑/连接保持/重建基准 |
| `single_handle_requested_mode` | `GuidewireMode` | `main.cpp:126` | `main.cpp:126,147,890,4977` | `main.cpp:1539`；取址 — | 输入拓扑/连接保持/重建基准 |
| `axis1_input_handle` | `Handle*` | `main.cpp:127` | `main.cpp:127,136,145` | `main.cpp:191,211,1141,1190,1252,1462–1463,1503,1659,2176,5101–5102`；取址 — | 输入拓扑/连接保持/重建基准 |
| `axis6_input_handle` | `Handle*` | `main.cpp:128` | `main.cpp:128,137,146` | `main.cpp:192,212,1253,1467–1468,1504,1660,2182,5104–5105`；取址 — | 输入拓扑/连接保持/重建基准 |
| `ff` | `ForceFeedbackState` | `main.cpp:161` | `main.cpp:175,4998` | `main.cpp:161,174,786,1368,2521–2524,3057,3061,4405,4457,4585,4641–4646,4743–4745,4753–4756,4759–4760,4840–4842,4987–4990,4993,4999,5001,5219–5220`；取址 `main.cpp:302,4841` | 力标定/力反馈/诊断 |
| `pos` | `double[7]` | `main.cpp:180` | `main.cpp:180,2087–2088,2585,2588–2590,2604,2607,2619,2621,2623,2625,2632–2633,3091,3098–3103,3151,3304–3309,3329–3331,3386–3388,3393–3398,3416–3421,3479–3482,3491–3495,3518–3519,3532,3542–3543,3569–3571,3589–3590,3675–3679,3840–3841,3845–3846,4113,4116,4130` | `main.cpp:199,1046,3311–3313,3577,3843,3888,4118,4270–4271,4477`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `plc_act_pos` | `double[7]` | `main.cpp:181` | `main.cpp:181` | `main.cpp:200,390,405–410,1319–1321,1324,1339–1341,1671,1697,1753–1755,2096–2098,2119,2179,2185,2312,2487–2490,2497,2505,2586,2605,2983,3057,3061,3098–3103,3106–3107,3251–3256,3311–3313,3407–3412,3593–3595,4270–4271,4303–4305,4327,4592–4593,4697,5208–5210`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `plc_init_pos` | `double[7]` | `main.cpp:182` | `main.cpp:182` | `main.cpp:201,390,1046,1671,1697,2096–2098,2119,2312,2587,2606,3057,3061,3151,3218,3220,3223,3407–3410,3782–3783,3840,3845,4113,4327,4592–4593`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `plc_leftlimit` | `double[7]` | `main.cpp:183` | `main.cpp:183` | `main.cpp:202,390,707,2115,3218,3220,3223,3240–3241,3653–3654,3784–3786,3879,3895,3905,4327,4707–4708`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `plc_act_pos_from_left` | `double[7]` | `main.cpp:184` | `main.cpp:184` | `main.cpp:203,4698`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `plc_v_limit` | `double[7]` | `main.cpp:185` | `main.cpp:185` | `main.cpp:204,438,440–444`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `startup_smoothing_bypass` | `bool` | `main.cpp:186` | `main.cpp:186,755,2030,4526` | —；取址 `main.cpp:205,4512` | 启动/自检 |
| `ctx` | `AppContext` | `main.cpp:189` | `main.cpp:190–205,207,211–212,297–326` | `main.cpp:189,216–218,221,225,229,233,237,328–329,332,339,345–346,361,365,370,374,376–379,433,448,461,1267,1271,1275,2408,2431,2433,2435,3245,3642,3646,3728,3888–3889,3938,4706`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis3_base_rel` | `double` | `main.cpp:240` | `main.cpp:240,3832,4085` | —；取址 `main.cpp:303` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis5_base_rel` | `double` | `main.cpp:241` | `main.cpp:241,3833,4086` | `main.cpp:3782`；取址 `main.cpp:304` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_mirror_base_rel` | `double` | `main.cpp:242` | `main.cpp:242` | `main.cpp:4130`；取址 `main.cpp:305` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis2_hold_rel` | `double` | `main.cpp:245` | `main.cpp:245,482,1319,1339,1753,3106,3843,4118` | `main.cpp:2087,2588,2622`；取址 `main.cpp:306` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis7_hold_rel` | `double` | `main.cpp:246` | `main.cpp:246,483,1320,1340,1754,3082,3107` | `main.cpp:2088,2607,2633,3089`；取址 `main.cpp:307` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis1_prev_linear_filtered` | `double` | `main.cpp:248` | `main.cpp:248,1254,1661,4273,4602` | `main.cpp:2100`；取址 `main.cpp:308` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_prev_linear_filtered` | `double` | `main.cpp:249` | `main.cpp:249,1255,1662,1703,4274,4603` | `main.cpp:2121`；取址 `main.cpp:309` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis1_prev_rot_filtered` | `double` | `main.cpp:250` | `main.cpp:250,1256,4275,4604` | —；取址 `main.cpp:310` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_prev_rot_filtered` | `double` | `main.cpp:251` | `main.cpp:251,1257,1704,4276,4605` | —；取址 `main.cpp:311` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis1_follow_cmd_abs` | `double` | `main.cpp:252` | `main.cpp:252,484,3104,3838,4114` | `main.cpp:3780,3992,4008`；取址 `main.cpp:312` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_follow_cmd_abs` | `double` | `main.cpp:253` | `main.cpp:253,485,3105,3152,3839,3966` | `main.cpp:3135,3681,3710,3781,3978`；取址 `main.cpp:313` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis4_axis6_coupling_last_ms` | `ULONGLONG` | `main.cpp:254` | `main.cpp:254,1639,3319,3669,3771,3776,3967,3973,3987,4285` | `main.cpp:3767,3972`；取址 `main.cpp:3764` | 手动点动/租约/耦合/电缸覆盖 |
| `axis4_axis6_coupling_active_prev` | `bool` | `main.cpp:255` | `main.cpp:255,1638,3318,3668,3772,3777,3983,3988,4284` | `main.cpp:3764,3963`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `axis1_delivery_mapping_active` | `bool` | `main.cpp:257` | `main.cpp:257,349,355,3830,4089` | `main.cpp:3788,4057`；取址 — | 轴1先行映射 |
| `axis1_delivery_mapping_progress_mm` | `double` | `main.cpp:258` | `main.cpp:258,350,356,3827,4083` | `main.cpp:3793,3829,4064`；取址 — | 轴1先行映射 |
| `axis1_delivery_mapping_applied_extra_mm` | `double` | `main.cpp:259` | `main.cpp:259,351,357,3828,4084` | `main.cpp:4076`；取址 — | 轴1先行映射 |
| `axis1_reverse_switch_guard_active` | `bool` | `main.cpp:261` | `main.cpp:261,1674,4145` | `main.cpp:1676,4142,4148`；取址 `main.cpp:314` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_reverse_switch_guard_active` | `bool` | `main.cpp:262` | `main.cpp:262,1700,1875,3179` | `main.cpp:1707`；取址 `main.cpp:315` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis1_prev_abs_for_trigger` | `double` | `main.cpp:264` | `main.cpp:264,4277` | `main.cpp:4152`；取址 `main.cpp:316` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_prev_abs_for_trigger` | `double` | `main.cpp:265` | `main.cpp:265,1705,4278` | —；取址 `main.cpp:317` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis1_prev_abs_valid` | `bool` | `main.cpp:266` | `main.cpp:266,487,4279` | `main.cpp:4152`；取址 `main.cpp:318` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_prev_abs_valid` | `bool` | `main.cpp:267` | `main.cpp:267,488,1706,4280` | —；取址 `main.cpp:319` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `guidewire_mode` | `GuidewireMode` | `main.cpp:270` | `main.cpp:270,1799,1807,1866,1922,4536,5214,5340` | `main.cpp:1269,1555,1579,1636,1669,1693–1694,1784,2078,2152,2157,2189,2619,2621,2623,2625,3088,3672,3958,4516,4519,4705,4714,5044,5049`；取址 `main.cpp:297,1546,2075,2150,2187,3759` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `dual_reverse_active` | `bool` | `main.cpp:271` | `main.cpp:271,1527` | `main.cpp:1534`；取址 `main.cpp:298` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `independent_axis1_hold_rel` | `double` | `main.cpp:272` | `main.cpp:272` | `main.cpp:2620,3675`；取址 `main.cpp:320` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `independent_axis2_hold_rel` | `double` | `main.cpp:273` | `main.cpp:273,1321,1341,1755` | `main.cpp:2622,3676`；取址 `main.cpp:321` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `independent_axis3_hold_rel` | `double` | `main.cpp:274` | `main.cpp:274` | `main.cpp:2624,3677`；取址 `main.cpp:322` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `independent_axis5_hold_rel` | `double` | `main.cpp:275` | `main.cpp:275` | `main.cpp:2626,3678`；取址 `main.cpp:323` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_window_locked` | `bool` | `main.cpp:277` | `main.cpp:277,1801,1809,1924,5216,5342` | `main.cpp:4705`；取址 `main.cpp:324` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_locked_window_start_abs` | `double` | `main.cpp:278` | `main.cpp:278` | `main.cpp:4703`；取址 `main.cpp:325` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_locked_window_end_abs` | `double` | `main.cpp:279` | `main.cpp:279` | `main.cpp:4704`；取址 `main.cpp:326` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `startup` | `StartupState` | `main.cpp:281` | `main.cpp:291–294,403–416,1074–1075,1336–1338,1373–1374,1451–1452,1927–1930,3438–3439,3485,3498–3501,3525–3526,3535–3536,3559,3577–3579,3593–3596,3610–3611,4531–4534,4907–4912,5222–5225,5299–5304,5344–5348` | `main.cpp:281,429,931–934,1548,1608,1823,1928,1934,1979,1982,2005,2021,2029,2065,3247,3306–3308,3393–3398,3403–3406,3416–3421,3429–3430,3444,3449,3455,3458,3460,3462,3464,3467–3468,3477,3479,3481–3482,3491,3493–3495,3511,3518–3519,3521,3523,3530,3532–3533,3540,3542–3543,3550,3557,3562,3582,3591,3600,3614,3621,4364,4717,4749–4750,5230–5232,5295,5320,5338`；取址 `main.cpp:301,1934,2021,2035,3247,4749,5295` | 启动/自检 |
| `cal_cfg` | `ForceCalibrationConfig` | `main.cpp:282` | `main.cpp:5375` | `main.cpp:282,4445,4598,4625,4629,4761,5368,5373`；取址 `main.cpp:5367` | 力标定/力反馈/诊断 |
| `cal_state` | `ForceCalibrationState` | `main.cpp:283` | `main.cpp:780–785` | `main.cpp:283,788–792,4445,4459,4599,4625,4629,4631–4632,4636–4637,4746,5377`；取址 `main.cpp:4442,4622` | 力标定/力反馈/诊断 |
| `axis1_crawl` | `CrawlState` | `main.cpp:284` | `main.cpp:289,1531–1532,3751,3952` | `main.cpp:284,2106,2111–2114,2391–2392,3634,3648,3730–3731,3747,3753,3783,3841–3842,3949,4014,4018,4116–4117,4137`；取址 `main.cpp:299` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_crawl` | `CrawlState` | `main.cpp:285` | `main.cpp:1800,1808,1923,3941–3943,5215,5341` | `main.cpp:285,1699,2127,2401–2402,3081,3131–3132,3636,3687–3688,3945–3946`；取址 `main.cpp:300` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `planned_return` | `PlannedReturnCoordinator` | `main.cpp:286` | `main.cpp:556–558,611–613,682,2306,2481–2483,2486–2490,2493,2501,2650–2651,2654–2657,2675–2677,2690–2691,2699–2701,2706,2755–2757,2780–2781,2787–2790,2799–2800,2813–2814,2846,2848,2861,2876,2895–2896,2899,2939–2941,2968,2985–2988,3007,3019–3021,3032–3034,3044–3045,4559,4567,4574,4580–4581` | `main.cpp:286,551,563,565,574,576,586,591,593,624,629,634,643,651–653,658,667,671,675,677,687,701,736,907,1031–1034,1040,1042,1223–1224,1226,1248,1314,1524,1592–1593,1599,1602,1636,1734,1737,1973,2006,2032,2067,2136,2158,2191,2193,2304,2344,2346,2354–2355,2357,2364–2365,2367,2374,2376,2384,2386,2403–2405,2428,2479–2480,2493,2501,2507,2509,2534,2536,2541,2543,2548,2550,2555,2557,2565–2566,2568–2571,2573–2577,2583,2589–2590,2592,2602,2609,2620,2624,2626,2632,2641,2643,2645,2664–2665,2667,2669–2670,2681,2684–2686,2688,2693–2694,2696,2710,2713,2715,2725,2732,2734,2744,2774,2804,2806,2810,2821,2824,2830,2832,2844,2858,2867,2878,2884,2902–2903,2909,2915,2917,2927,2953–2954,2957,2969,2971,2991,2998,3000,3011,3013,3015,3024,3026,3029–3030,3038,3040,3042,3048–3049,3056,3060,3064–3066,3182,3238,3249,3665,3891,4240,4367,4480–4482,4486,4488,4502,4542–4544,4548,4550,4562–4563,4569–4571,4576,4976,5018,5193,5317`；取址 `main.cpp:587,1242,2191,2193,2594,2596,2611,2613,4978` | 计划回退/双手柄换手/故障恢复 |
| `dual_handoff` | `DualHandoffRequest` | `main.cpp:287` | `main.cpp:3865,3869–3874` | `main.cpp:287,659,666,1599,2517,3097,3738,3740–3743,3745,3755,3758–3759,3863,3867,3870–3871,3874,3876–3878,3880–3881,3883,3903,3920,3924,4367,4502`；取址 `main.cpp:3738,3742–3743` | 计划回退/双手柄换手/故障恢复 |
| `spacing_recovery` | `SpacingRecoveryState` | `main.cpp:288` | `main.cpp:903,1559,1564–1567,1572,3232–3234,3251–3257,3284,3286–3290,3335,3338–3339,3346,3351,3356,3359,3368–3373,3377,3381,5393` | `main.cpp:288,900,1058,1306,1370,1515,1525,1540,1589–1590,1602,1629,1667,1771,1773,1916,1975,2006–2007,2032,2066,2144,3218,3220,3223,3229–3230,3238,3262,3269,3271,3274,3322,3329–3331,3333,3336,3343,3345,3362,3364,3375,3378,3386–3388,4290,4301,4303–4305,4310,4314,4322–4325,4335,4366,4381,4463,4589,4762–4764,5211,5317,5327`；取址 — | 屈曲恢复/模式覆盖 |
| `control_active` | `bool` | `main.cpp:380` | `main.cpp:380,417,1073,1335,1372,1499,1733,1897,1934,1940,1982,2021,2039,2447,3442,3448,3502,4336,4537,5226,5309,5349` | `main.cpp:1361,1582,1600,1827,2031,2063,2084,2140,3248,3504,4291,4375,4461,4475,4538,4589,4718,5317`；取址 `main.cpp:1666` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `emergency_retract_active` | `bool` | `main.cpp:421` | `main.cpp:421,4525,5228` | `main.cpp:1588,1603,1628,2007,2030,2065,2084,3297,4357,4365,4375,4381,4475,4523,5198`；取址 — | 紧急回撤 |
| `emergency_retract_axis1_hold_rel` | `double` | `main.cpp:422` | `main.cpp:422,5208` | `main.cpp:3304`；取址 — | 紧急回撤 |
| `emergency_retract_axis2_hold_rel` | `double` | `main.cpp:423` | `main.cpp:423,5209` | `main.cpp:3305`；取址 — | 紧急回撤 |
| `emergency_retract_axis7_hold_rel` | `double` | `main.cpp:424` | `main.cpp:424,5210` | `main.cpp:3309`；取址 — | 紧急回撤 |
| `emergency_retract_v_limit_backup` | `double[7]` | `main.cpp:425` | `main.cpp:425` | `main.cpp:438,461`；取址 — | 紧急回撤 |
| `emergency_retract_v_limit_scaled` | `bool` | `main.cpp:426` | `main.cpp:426,452,465` | `main.cpp:457`；取址 — | 紧急回撤 |
| `self_check_done` | `bool` | `main.cpp:490` | `main.cpp:490,1015` | `main.cpp:493,1020,1065,1072–1073,1220,1379,1894,1970,4722,4741`；取址 `main.cpp:1892,1903,1945,1963,2034,5297,5324` | 启动/自检 |
| `has_self_check_flag` | `bool` | `main.cpp:491` | `main.cpp:491,1071` | `main.cpp:1068,5297,5324`；取址 `main.cpp:1892` | 启动/自检 |
| `last_self_check_done` | `bool` | `main.cpp:493` | `main.cpp:493,1072,1450,1970` | `main.cpp:1892,1894,1903`；取址 — | 启动/自检 |
| `handle_reinit_req` | `bool` | `main.cpp:494` | `main.cpp:494,1022` | `main.cpp:1973,1985`；取址 — | 输入拓扑/连接保持/重建基准 |
| `last_handle_reinit_req` | `bool` | `main.cpp:495` | `main.cpp:495,1985` | `main.cpp:1973`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `estop_hold_req` | `bool` | `main.cpp:496` | `main.cpp:496,1023` | `main.cpp:1025,1728,1730`；取址 — | 急停保持/恢复请求节流 |
| `estop_hold_active` | `bool` | `main.cpp:497` | `main.cpp:497,1732,1752` | `main.cpp:1025,1586,1601,1627,1666,1728,1759,1770,1786,1819,1931,1978,2001,2035,2065,2083,2142,4289,4312,4362,4375,4462,4474,4591,4719,4725,5296,5322`；取址 — | 急停保持/恢复请求节流 |
| `axis1_push_rearm_after_hold` | `bool` | `main.cpp:498` | `main.cpp:498,4334` | —；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis1_delivery_stop_latched` | `bool` | `main.cpp:499` | `main.cpp:499,4004,4178,4331` | `main.cpp:3993,4000,4026`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis1_delivery_stop_prompted` | `bool` | `main.cpp:500` | `main.cpp:500,3997,4005,4182,4332` | `main.cpp:4179`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis1_reverse_pressed_prev` | `bool` | `main.cpp:501` | `main.cpp:501,1724` | `main.cpp:1033,1651,1669`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_effective_reverse_prev` | `bool` | `main.cpp:502` | `main.cpp:502,1725,1874` | `main.cpp:1033,1695`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis4_manual_error_prev` | `bool` | `main.cpp:503` | `main.cpp:503,4353` | `main.cpp:4349`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `axis4_manual_error_id_prev` | `unsigned long` | `main.cpp:504` | `main.cpp:504,4354` | `main.cpp:4349`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `axis4_ui_forward_pressed` | `bool` | `main.cpp:505` | `main.cpp:505,1623,5169` | `main.cpp:1619,1631,4855`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `axis4_ui_reverse_pressed` | `bool` | `main.cpp:506` | `main.cpp:506,1624,5170` | `main.cpp:1619,1632,4855`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `y_valve_open` | `bool` | `main.cpp:507` | `main.cpp:507,5177,5227` | `main.cpp:4357,4856,5178`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `injector_ui_direction` | `int[2]` | `main.cpp:508` | `main.cpp:508,1615,5185` | `main.cpp:1611,4854`；取址 `main.cpp:1646,1648` | 手动点动/租约/耦合/电缸覆盖 |
| `injector_ui_jog_deadline_ms` | `ULONGLONG[2]` | `main.cpp:509` | `main.cpp:509,1616,5186` | `main.cpp:1612–1613`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `axis4_ui_jog_deadline_ms` | `ULONGLONG` | `main.cpp:512` | `main.cpp:512,1625,5171` | `main.cpp:1620–1621`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `manual_jog_lease_ms` | `constexpr ULONGLONG` | `main.cpp:513` | `main.cpp:513` | `main.cpp:5173,5188`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `requested_guidewire_mode_prev` | `GuidewireMode` | `main.cpp:514` | `main.cpp:514,1889` | `main.cpp:1780`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis1_fast_return` | `bool` | `main.cpp:515` | `main.cpp:515,660,689,741,1282,1317,1487,2579,2591,2652,2897,3005` | `main.cpp:4720`；取址 `main.cpp:4478` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `axis6_fast_retract` | `bool` | `main.cpp:516` | `main.cpp:516,661,690,742,1283,1318,1488,2580,2608,2653,2898,3006` | `main.cpp:4721`；取址 `main.cpp:4479` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `return_ads_fault_hold` | `bool` | `main.cpp:517` | `main.cpp:517,1739,2020,2446` | `main.cpp:1497,1585,1601,1770,1827,1988,2031,2064,2083,2143,4287,4312,4474,4590,4724,4860,5321`；取址 — | 计划回退/双手柄换手/故障恢复 |
| `return_recovery` | `ReturnRecovery` | `main.cpp:518` | — | `main.cpp:518,1738,1994,1998–1999,2014,2016,2018,2022–2023,2445,2514,3071,3628,3630–3632,4724`；取址 `main.cpp:2010` | 计划回退/双手柄换手/故障恢复 |
| `interrupted_return_mode` | `PlannedReturnMode` | `main.cpp:519` | `main.cpp:519,2515` | `main.cpp:3637–3639,3643,3659`；取址 — | 计划回退/双手柄换手/故障恢复 |
| `interrupted_return_scope` | `PlannedReturnRebaseScope` | `main.cpp:520` | `main.cpp:520,2516` | `main.cpp:3659`；取址 — | 计划回退/双手柄换手/故障恢复 |
| `original_return_intent` | `DualHandoffRequest` | `main.cpp:521` | `main.cpp:2517–2519` | `main.cpp:521,3633,3650–3651`；取址 — | 计划回退/双手柄换手/故障恢复 |
| `resuming_return` | `bool` | `main.cpp:522` | `main.cpp:522,3658,3661` | `main.cpp:2512`；取址 — | 计划回退/双手柄换手/故障恢复 |
| `recovery_event_text` | `std::string` | `main.cpp:523` | `main.cpp:1194,1196,1284,1290,1292,1414,1430,1448,1454,2023` | `main.cpp:523,1196,1293,1322,1416,1431,1455,2024,4739`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `pending_return_error_axis` | `int` | `main.cpp:525` | `main.cpp:525,1000,1007,2453,2460` | `main.cpp:982,986,991,998`；取址 — | 计划回退/双手柄换手/故障恢复 |
| `pending_return_error_event_sequence` | `std::uint64_t` | `main.cpp:526` | `main.cpp:526,1001,1008,2454,2461` | `main.cpp:996`；取址 — | 计划回退/双手柄换手/故障恢复 |
| `pending_return_error_deadline_ms` | `ULONGLONG` | `main.cpp:527` | `main.cpp:527,1002,1009,2455,2462` | `main.cpp:1004–1005`；取址 — | 计划回退/双手柄换手/故障恢复 |
| `axis6_soft_limit_hold` | `bool` | `main.cpp:537` | `main.cpp:537,723,726,1494` | `main.cpp:1491,1594,1603,2067,2136,3125,3248,3315,4231,4367,4381,4502,4774`；取址 — | 轴6软限位 |
| `axis6_soft_limit_reason` | `Axis6SoftLimitReason` | `main.cpp:538` | `main.cpp:538,727,1495` | `main.cpp:720,1492`；取址 — | 轴6软限位 |
| `axis6_soft_limit_warning_active` | `bool` | `main.cpp:539` | `main.cpp:539,733,4236` | `main.cpp:729`；取址 `main.cpp:4231` | 轴6软限位 |
| `axis6_soft_limit_warning_reason` | `Axis6SoftLimitReason` | `main.cpp:540` | `main.cpp:540,734,4237` | `main.cpp:730`；取址 — | 轴6软限位 |
| `axis6_soft_limit_blocked_target_from_left_mm` | `double` | `main.cpp:541` | `main.cpp:541,728` | `main.cpp:747`；取址 — | 轴6软限位 |
| `force_sample` | `ForceSampleFrame` | `main.cpp:542` | `main.cpp:4423,4427,5065–5066` | `main.cpp:542,4430,4442,4445,4449,4460,4586,4751–4752,4783,5057–5060,5068`；取址 — | 力标定/力反馈/诊断 |
| `loop_count` | `int` | `main.cpp:543` | `main.cpp:543,1486` | `main.cpp:4757`；取址 — | 记录/30Hz状态发布 |
| `clean_force_monitor_enabled` | `bool` | `main.cpp:544` | `main.cpp:544,5124` | `main.cpp:4406`；取址 — | 力标定/力反馈/诊断 |
| `force_feedback_diag_reason` | `std::string` | `main.cpp:545` | `main.cpp:4469` | `main.cpp:545,4465`；取址 — | 力标定/力反馈/诊断 |
| `force_sample_diag_reason` | `std::string` | `main.cpp:546` | `main.cpp:4434` | `main.cpp:546,4433,4438`；取址 — | 力标定/力反馈/诊断 |
| `ads_snapshot_sequence` | `std::uint64_t` | `main.cpp:548` | `main.cpp:548,779,937,973` | `main.cpp:762,970,1035,2657,2895,2941,2953,5414`；取址 `main.cpp:3000` | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `remote_force_zero_ok_count` | `std::uint32_t` | `main.cpp:796` | `main.cpp:796,801` | `main.cpp:4857`；取址 — | 力标定/力反馈/诊断 |
| `remote_force_zero_fail_count` | `std::uint32_t` | `main.cpp:797` | `main.cpp:797,802` | `main.cpp:4858`；取址 — | 力标定/力反馈/诊断 |
| `initial_sync_done` | `bool` | `main.cpp:806` | `main.cpp:806,1334` | `main.cpp:1297,1309,1327,4859`；取址 `main.cpp:1247,1263,2033,2063,4701,5321` | 输入拓扑/连接保持/重建基准 |
| `plc_app_name_read` | `bool` | `main.cpp:807` | `main.cpp:807,1355,1365` | `main.cpp:1347`；取址 `main.cpp:1379,1945` | 输入拓扑/连接保持/重建基准 |
| `handle1_next_retry_ms` | `ULONGLONG` | `main.cpp:808` | `main.cpp:808,1084,1119,1152,1169` | `main.cpp:1082,1117`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `handle6_next_retry_ms` | `ULONGLONG` | `main.cpp:809` | `main.cpp:809,1089,1127,1153,1182` | `main.cpp:1087,1125`；取址 — | Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2） |
| `handle_soft_hold_active` | `bool` | `main.cpp:810` | `main.cpp:810,1189` | `main.cpp:1248,1250,1263,1297,1311,1329,1459,2004,2034,2064,2083,4311,4590,4861,5321`；取址 — | 输入拓扑/连接保持/重建基准 |
| `connection_hold_active_prev` | `bool` | `main.cpp:811` | `main.cpp:811,1300` | `main.cpp:1247,1298`；取址 `main.cpp:1299` | 输入拓扑/连接保持/重建基准 |
| `cylinder_manual_mode` | `CylinderManualMode[4]` | `main.cpp:820` | `main.cpp:820,837,3282,4934,4946,4964` | `main.cpp:835,2053,4386,4846`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `cylinder_manual_position` | `unsigned short[4]` | `main.cpp:826` | `main.cpp:826,4945` | `main.cpp:4389`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `cylinder_manual_restore` | `unsigned short[4]` | `main.cpp:827` | `main.cpp:827,856` | `main.cpp:4392`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `cylinder_manual_written_mask` | `std::uint8_t` | `main.cpp:828` | `main.cpp:828,832,857` | `main.cpp:844,4377,4391`；取址 — | 手动点动/租约/耦合/电缸覆盖 |
| `vis_reverse_override_active` | `bool` | `main.cpp:860` | `main.cpp:860,893,4323,4338` | `main.cpp:1535,1537,1546,1553,1556,1564,1815`；取址 — | 屈曲恢复/模式覆盖 |
| `vis_reverse_override_value` | `bool` | `main.cpp:861` | `main.cpp:861,895,4324` | `main.cpp:1535,1557,1565,1816`；取址 — | 屈曲恢复/模式覆盖 |
| `vis_reverse_override_target` | `int` | `main.cpp:862` | `main.cpp:862,894,4325` | `main.cpp:1538,1554,1566`；取址 `main.cpp:1815` | 屈曲恢复/模式覆盖 |
| `pending_mode_selection` | `ModeSelection` | `main.cpp:871` | `main.cpp:871,902,914,1518,1774,5212,5391` | `main.cpp:1514,1517`；取址 — | 屈曲恢复/模式覆盖 |
| `spacing_recovery_button_pressed_prev` | `bool` | `main.cpp:872` | `main.cpp:872,1575` | `main.cpp:1511`；取址 `main.cpp:1513` | 屈曲恢复/模式覆盖 |
| `pending_startup` | `PendingStartupParams` | `main.cpp:926` | `main.cpp:931–935,5260–5263,5270–5271,5275` | `main.cpp:926,5280–5289,5299–5305`；取址 — | 启动/自检 |
| `pending_selfcheck_target` | `double[7]` | `main.cpp:927` | `main.cpp:927,4880` | `main.cpp:4888–4891,4915`；取址 — | 启动/自检 |
| `pending_selfcheck_axes` | `unsigned int` | `main.cpp:928` | `main.cpp:928,4881,4898,5319` | `main.cpp:4892`；取址 — | 启动/自检 |
| `selfcheck_command_error` | `bool` | `main.cpp:929` | `main.cpp:929,1021,4882,4903,4918,4922` | `main.cpp:4741`；取址 — | 启动/自检 |
| `selfcheck_start_pending` | `bool` | `main.cpp:930` | `main.cpp:930,1019,4923` | `main.cpp:4364,4742,4899`；取址 — | 启动/自检 |
| `tracking_manual_cylinder_override` | `bool` | `main.cpp:936` | `main.cpp:936,2050,2055` | `main.cpp:2068,2145,2159`；取址 — | tracking门控/PI/欠账 |
| `handled_plc_restart_count` | `std::uint64_t` | `main.cpp:938` | `main.cpp:938,1366` | `main.cpp:1359`；取址 — | 输入拓扑/连接保持/重建基准 |
| `handled_reconnect_count` | `std::uint64_t` | `main.cpp:939` | `main.cpp:939,1053` | `main.cpp:1051`；取址 — | 输入拓扑/连接保持/重建基准 |
| `ads_soft_hold_active` | `bool` | `main.cpp:940` | `main.cpp:940,1207,1245,1279` | `main.cpp:1205,1242,1247,1278,1297,1310,1515,1584,1932,1979,2035,2064,2083,4311,4425,4590,4862,4976,5013,5296,5323`；取址 `main.cpp:5383` | 输入拓扑/连接保持/重建基准 |
| `plc_restart_recovery_latched` | `bool` | `main.cpp:941` | `main.cpp:941,1367,1935,3443,3503,5350` | —；取址 — | 输入拓扑/连接保持/重建基准 |
| `drained_ads_snapshots` | `std::vector<AdsFastSnapshot>` | `main.cpp:942` | — | `main.cpp:942,979,4612`；取址 — | 记录/30Hz状态发布 |
| `handle_record_history` | `std::deque<HandleRecordSnapshot>` | `main.cpp:949` | — | `main.cpp:949,1478,1480–1481,1483,4661`；取址 — | 记录/30Hz状态发布 |
| `vis_qpc_frequency` | `LARGE_INTEGER` | `main.cpp:950` | — | `main.cpp:950,1479,4685`；取址 `main.cpp:951` | 记录/30Hz状态发布 |
| `next_vis_publish_qpc` | `std::int64_t` | `main.cpp:952` | `main.cpp:952,4686,4689,4692` | `main.cpp:4686–4687,4690`；取址 — | 记录/30Hz状态发布 |
| `recording_info` | `ExperimentStartInfo` | `main.cpp:953` | `main.cpp:954–956` | `main.cpp:953,957`；取址 — | 记录/30Hz状态发布 |
| `next_estop_recover_req_ms` | `ULONGLONG` | `main.cpp:1761` | `main.cpp:1761,1765` | `main.cpp:1763`；取址 — | 急停保持/恢复请求节流 |

### B.11.7 状态结构体全部数据声明及类型

**已确认**：下表保留各数据声明的类型、实际字段名和默认值；同一声明中的逗号字段具有同一类型。行为、写入入口、读取入口见B.11.2–B.11.4。本表的“写/读范围”指该组对象实现范围，不表示范围内每一行都写/读每个字段。方法本身、每拍重建的局部输出帧/条件结构体不属于跨周期保存量。后台服务对象内部的线程/锁/队列字段由A/D列出，main不直接操作这些私有字段。

| 对象/流程 | 数据声明（逐项名称/类型/默认值） | 声明来源 | 写入入口/读取范围 |
|---|---|---|---|
| ControlConfig（配置） | `double k_handle_to_mm = 500.0 * (75.0 / 50.0);` | `control_types.h:81` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis_push_sign = -1.0;` | `control_types.h:82` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis_rot_scale_deg = -Rad;` | `control_types.h:84` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `int axial_force_axis = 1;` | `control_types.h:87` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double force_feedback_582_force_scale = 0.3;` | `control_types.h:89` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double force_feedback_582_torque_scale = 0.3;` | `control_types.h:90` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double force_feedback_587_force_scale = 0.3;` | `control_types.h:91` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double force_feedback_587_torque_scale = 0.3;` | `control_types.h:92` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double force_feedback_force_filter_alpha = 0.25;` | `control_types.h:94` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `unsigned char btn_b6 = 0x40;` | `control_types.h:97` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis1_window_left_from_left_mm = 5.0;` | `control_types.h:100` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis1_window_right_from_left_mm = 30.0;` | `control_types.h:101` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double catheter_axis6_window_size_mm = 32.0;` | `control_types.h:104` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double catheter_axis6_window_min_gap_from_axis5_mm = 1.0;` | `control_types.h:105` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double guidewire_axis6_window_size_mm = 32.0;` | `control_types.h:106` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double guidewire_axis6_window_min_gap_from_axis5_mm = 1.0;` | `control_types.h:107` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis6_active_delivery_gap_mm = 28.0;` | `control_types.h:109` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis6_active_retraction_gap_mm = 6.0;` | `control_types.h:110` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis6_passive_clearance_mm = 3.0;` | `control_types.h:112` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis6_passive_distance_limit_mm = 27.0;` | `control_types.h:113` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double transaction_merge_eps_mm = 5.0;` | `control_types.h:115` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD cyl1_release_wait_ms = 100;` | `control_types.h:117` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD cyl1_clamp_wait_ms = 150;` | `control_types.h:118` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD cyl2_release_wait_ms = 500;` | `control_types.h:119` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD cyl2_clamp_wait_ms = 150;` | `control_types.h:120` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD cyl3_release_wait_ms = 100;` | `control_types.h:121` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD cyl3_clamp_wait_ms = 150;` | `control_types.h:122` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD cyl4_release_wait_ms = 500;` | `control_types.h:123` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD cyl4_clamp_wait_ms = 150;` | `control_types.h:124` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_g_mm = 13.0;` | `control_types.h:126` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis56_ready_gap_mm = 15.0;` | `control_types.h:128` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis3_delivery_stop_from_left_mm = 20.0;` | `control_types.h:129` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis3_delivery_release_hysteresis_mm = 2.0;` | `control_types.h:130` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double guidewire_entry_axis6_from_left_max_mm = 667.0;` | `control_types.h:131` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis1_post_return_lead_mm = 2.0;` | `control_types.h:134` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis1_post_return_lead_limit_mm = 5.0;` | `control_types.h:135` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis1_post_return_mapping_span_mm = 10.0;` | `control_types.h:136` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis6_soft_limit_from_left_mm = 660.0;` | `control_types.h:139` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double spacing_recovery_speed_limit_mm_s = 40.0;` | `control_types.h:142` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double spacing_recovery_axis3_max_from_left_mm = 650.0;` | `control_types.h:143` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double spacing_recovery_axis5_max_from_left_mm = 665.0;` | `control_types.h:144` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double spacing_recovery_axis6_max_from_left_mm = 665.0;` | `control_types.h:145` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD spacing_recovery_clamp_settle_ms = 150;` | `control_types.h:146` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD spacing_recovery_exit_settle_ms = 100;` | `control_types.h:147` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD spacing_recovery_exit_timeout_ms = 2000;` | `control_types.h:148` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `unsigned short spacing_recovery_cyl4_release = 100;` | `control_types.h:149` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `unsigned char spacing_recovery_button_mask = 0x07;` | `control_types.h:151` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double crawl_arrive_tol_mm = 0.2;` | `control_types.h:154` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double handoff_stop_velocity_mm_s = 0.5;` | `control_types.h:155` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double linear_increment_noise_deadband_mm = 0.02;` | `control_types.h:157` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double reverse_switch_trigger_guard_mm = 2.0;` | `control_types.h:159` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis1_return_velocity_mm_s = 110.0;` | `control_types.h:162` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis1_return_acc_mm_s2 = 4800.0;` | `control_types.h:163` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis1_return_dec_mm_s2 = 4800.0;` | `control_types.h:164` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis1_return_jerk_mm_s3 = 70000.0;` | `control_types.h:165` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis6_return_velocity_mm_s = 110.0;` | `control_types.h:166` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis6_return_acc_mm_s2 = 4800.0;` | `control_types.h:167` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis6_return_dec_mm_s2 = 4800.0;` | `control_types.h:168` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double axis6_return_jerk_mm_s3 = 70000.0;` | `control_types.h:169` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD planned_return_ack_timeout_ms = 250;` | `control_types.h:171` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD planned_return_retry_clear_timeout_ms = 500;` | `control_types.h:172` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD planned_return_execution_timeout_ms = 5000;` | `control_types.h:173` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double linear_handle_alpha = 0.25;` | `control_types.h:176` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double rotational_handle_alpha = 0.20;` | `control_types.h:177` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD startup_clamp_settle_delay_ms = 300;` | `control_types.h:179` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `DWORD startup_recovery_stage_delay_ms = 2000;` | `control_types.h:180` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_motion_speed_scale = 0.005;` | `control_types.h:181` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `unsigned short startup_cyl3_open = 500;` | `control_types.h:182` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `unsigned short startup_cyl4_open = 5;` | `control_types.h:183` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `unsigned short startup_cyl3_clamp = 0;` | `control_types.h:184` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `unsigned short startup_cyl4_clamp = 1000;` | `control_types.h:185` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_loading_axis1_from_left_mm = 96.0;` | `control_types.h:187` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_loading_axis3_from_left_mm = 500.0;` | `control_types.h:188` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_loading_axis5_from_left_mm = 530.0;` | `control_types.h:189` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_loading_axis6_from_left_mm = 580.0;` | `control_types.h:190` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_loading_pose_tolerance_mm = 2.0;` | `control_types.h:191` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_axis1_ready_from_left_mm = 28.0;` | `control_types.h:193` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_axis5_ready_from_left_mm = startup_loading_axis3_from_left_mm + 10.0;` | `control_types.h:194` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_final_axis1_default_from_left_mm = 28.0;` | `control_types.h:196` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_final_axis3_default_from_left_mm = 610.0;` | `control_types.h:197` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_final_axis5_default_from_left_mm = 615.0;` | `control_types.h:198` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_final_axis6_default_from_left_mm = 628.0;` | `control_types.h:199` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| ControlConfig（配置） | `double startup_rot_arrive_tol_deg = 0.2;` | `control_types.h:200` | 构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表 |
| CylinderPreset（只读预设） | `unsigned short cyl1_open = 450;` | `control_types.h:208` | 构造后main无写；自动夹爪/换手/启动读取（`main.cpp:2042-2045,2594-2635,3325-3388,3514-3617`） |
| CylinderPreset（只读预设） | `unsigned short cyl1_clamp = 5;` | `control_types.h:209` | 构造后main无写；自动夹爪/换手/启动读取（`main.cpp:2042-2045,2594-2635,3325-3388,3514-3617`） |
| CylinderPreset（只读预设） | `unsigned short cyl2_open = 5;` | `control_types.h:210` | 构造后main无写；自动夹爪/换手/启动读取（`main.cpp:2042-2045,2594-2635,3325-3388,3514-3617`） |
| CylinderPreset（只读预设） | `unsigned short cyl2_clamp = 1000;` | `control_types.h:211` | 构造后main无写；自动夹爪/换手/启动读取（`main.cpp:2042-2045,2594-2635,3325-3388,3514-3617`） |
| CylinderPreset（只读预设） | `unsigned short cyl3_open = 550;` | `control_types.h:212` | 构造后main无写；自动夹爪/换手/启动读取（`main.cpp:2042-2045,2594-2635,3325-3388,3514-3617`） |
| CylinderPreset（只读预设） | `unsigned short cyl3_clamp = 50;` | `control_types.h:213` | 构造后main无写；自动夹爪/换手/启动读取（`main.cpp:2042-2045,2594-2635,3325-3388,3514-3617`） |
| CylinderPreset（只读预设） | `unsigned short cyl4_open = 5;` | `control_types.h:214` | 构造后main无写；自动夹爪/换手/启动读取（`main.cpp:2042-2045,2594-2635,3325-3388,3514-3617`） |
| CylinderPreset（只读预设） | `unsigned short cyl4_clamp = 1000;` | `control_types.h:215` | 构造后main无写；自动夹爪/换手/启动读取（`main.cpp:2042-2045,2594-2635,3325-3388,3514-3617`） |
| CylinderPreset（只读预设） | `unsigned short cyl3_follow_release = 400;` | `control_types.h:216` | 构造后main无写；自动夹爪/换手/启动读取（`main.cpp:2042-2045,2594-2635,3325-3388,3514-3617`） |
| CylinderPreset（只读预设） | `unsigned short cyl4_follow_release = 100;` | `control_types.h:217` | 构造后main无写；自动夹爪/换手/启动读取（`main.cpp:2042-2045,2594-2635,3325-3388,3514-3617`） |
| HandleFilterState（两柄） | `double axis0_filtered = 0.0;` | `control_types.h:54` | `update/reset`写（`control_types.h:59-75`、`main.cpp:1252-1257,1461-1470`）；增量读（`main.cpp:2090-2129`） |
| HandleFilterState（两柄） | `double axis1_filtered = 0.0;` | `control_types.h:55` | `update/reset`写（`control_types.h:59-75`、`main.cpp:1252-1257,1461-1470`）；增量读（`main.cpp:2090-2129`） |
| HandleFilterState（两柄） | `bool inited = false;` | `control_types.h:56` | `update/reset`写（`control_types.h:59-75`、`main.cpp:1252-1257,1461-1470`）；增量读（`main.cpp:2090-2129`） |
| PlannedReturnLeg（两腿） | `bool active = false;` | `control_types.h:272` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `int axis_index = -1;` | `control_types.h:273` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `double target_abs = 0.0;` | `control_types.h:274` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `double entry_rel = 0.0;` | `control_types.h:275` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `bool request_armed = false;` | `control_types.h:276` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `bool acknowledged = false;` | `control_types.h:277` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `bool started = false;` | `control_types.h:278` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `bool possibly_started = false;` | `control_types.h:280` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `bool done = false;` | `control_types.h:281` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `std::uint64_t request_event_sequence = 0;` | `control_types.h:282` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `std::uint64_t ack_event_sequence = 0;` | `control_types.h:283` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `std::uint64_t error_event_sequence = 0;` | `control_types.h:284` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `std::uint32_t reported_error_id = 0;` | `control_types.h:285` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `std::uint64_t cancel_event_sequence = 0;` | `control_types.h:286` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnLeg（两腿） | `bool cancel_event_required = false;` | `control_types.h:287` | begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`） |
| PlannedReturnCoordinator | `PlannedReturnPhase phase = PlannedReturnPhase::Idle;` | `control_types.h:297` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `PlannedReturnMode mode = PlannedReturnMode::None;` | `control_types.h:298` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `PlannedReturnRebaseScope rebase_scope = PlannedReturnRebaseScope::Axis1;` | `control_types.h:299` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `PlannedReturnLeg legs[2];` | `control_types.h:300` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `int leg_count = 0;` | `control_types.h:301` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `unsigned int retry_count = 0;` | `control_types.h:302` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `ULONGLONG phase_t0_ms = 0;` | `control_types.h:303` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `ULONGLONG request_t0_ms = 0;` | `control_types.h:304` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `ULONGLONG execution_t0_ms = 0;` | `control_types.h:305` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `ULONGLONG retry_clear_t0_ms = 0;` | `control_types.h:306` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `std::uint64_t clamp_output_generation = 0;` | `control_types.h:307` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `bool clamp_output_applied = false;` | `control_types.h:308` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `bool request_prepared = false;` | `control_types.h:309` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `std::uint64_t commit_guard_generation = 0;` | `control_types.h:310` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `bool retry_clear_issued = false;` | `control_types.h:311` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `std::uint64_t retry_clear_snapshot_sequence = 0;` | `control_types.h:312` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `PlannedReturnAdsCommandPurpose ads_command_purpose = PlannedReturnAdsCommandPurpose::None;` | `control_types.h:313` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `std::uint64_t ads_command_sequence = 0;` | `control_types.h:314` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `ULONGLONG ads_command_submit_ms = 0;` | `control_types.h:315` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `bool cancel_clear_confirmed = false;` | `control_types.h:316` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `bool completion_clear_confirmed = false;` | `control_types.h:317` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `std::uint64_t cancel_snapshot_sequence = 0;` | `control_types.h:318` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `ULONGLONG cancel_t0_ms = 0;` | `control_types.h:319` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `ULONGLONG cancel_clear_retry_after_ms = 0;` | `control_types.h:320` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `bool cancel_timeout_reported = false;` | `control_types.h:321` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `bool cancel_rebased = false;` | `control_types.h:322` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `std::uint64_t cancel_hold_generation = 0;` | `control_types.h:323` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `std::uint64_t done_snapshot_sequence = 0;` | `control_types.h:324` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `std::uint64_t handoff_generation = 0;` | `control_types.h:325` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `double hold_axis3_rel = 0.0;` | `control_types.h:326` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `double hold_axis5_rel = 0.0;` | `control_types.h:327` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `double hold_axis1_rel = 0.0;` | `control_types.h:328` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `double hold_axis6_rel = 0.0;` | `control_types.h:329` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `unsigned char clamp_stage = 0;` | `control_types.h:332` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `unsigned char restore_stage = 0;` | `control_types.h:333` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `std::uint64_t release_output_generation = 0;` | `control_types.h:334` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| PlannedReturnCoordinator | `bool release_applied = false;` | `control_types.h:335` | begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2 |
| DualHandoffRequest（含original副本） | `bool axis1 = false;` | `control_types.h:375` | 触边写/实际到位读（`main.cpp:3859-3924`）；original副本写/续做读（`main.cpp:2512-2520,3628-3663`） |
| DualHandoffRequest（含original副本） | `bool axis6 = false;` | `control_types.h:376` | 触边写/实际到位读（`main.cpp:3859-3924`）；original副本写/续做读（`main.cpp:2512-2520,3628-3663`） |
| DualHandoffRequest（含original副本） | `bool wire_caused = false;` | `control_types.h:377` | 触边写/实际到位读（`main.cpp:3859-3924`）；original副本写/续做读（`main.cpp:2512-2520,3628-3663`） |
| DualHandoffRequest（含original副本） | `bool gap_boundary = false;` | `control_types.h:378` | 触边写/实际到位读（`main.cpp:3859-3924`）；original副本写/续做读（`main.cpp:2512-2520,3628-3663`） |
| DualHandoffRequest（含original副本） | `bool reverse = false;` | `control_types.h:379` | 触边写/实际到位读（`main.cpp:3859-3924`）；original副本写/续做读（`main.cpp:2512-2520,3628-3663`） |
| DualHandoffRequest（含original副本） | `bool wire_reverse = false;` | `control_types.h:380` | 触边写/实际到位读（`main.cpp:3859-3924`）；original副本写/续做读（`main.cpp:2512-2520,3628-3663`） |
| DualHandoffRequest（含original副本） | `bool axis4_caused = false;` | `control_types.h:381` | 触边写/实际到位读（`main.cpp:3859-3924`）；original副本写/续做读（`main.cpp:2512-2520,3628-3663`） |
| SpacingRecoveryState | `bool requested = false;` | `control_types.h:397` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `SpacingRecoveryPhase phase = SpacingRecoveryPhase::Idle;` | `control_types.h:398` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `DWORD phase_t0 = 0;` | `control_types.h:399` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `DWORD last_motion_tick = 0;` | `control_types.h:400` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `double axis1_hold_rel = 0.0;` | `control_types.h:401` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `double axis2_hold_rel = 0.0;` | `control_types.h:402` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `double axis3_cmd_rel = 0.0;` | `control_types.h:403` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `double axis5_cmd_rel = 0.0;` | `control_types.h:404` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `double axis6_cmd_rel = 0.0;` | `control_types.h:405` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `double axis7_hold_rel = 0.0;` | `control_types.h:406` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `double pending_delta_mm = 0.0;` | `control_types.h:407` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `double moved_mm = 0.0;` | `control_types.h:408` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `double remaining_mm = 0.0;` | `control_types.h:409` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `bool limit_logged = false;` | `control_types.h:410` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `int restore_mode_selection = 1;` | `control_types.h:412` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `bool restore_vis_reverse_override_active = false;` | `control_types.h:413` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `bool restore_vis_reverse_override_value = false;` | `control_types.h:414` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| SpacingRecoveryState | `int restore_vis_reverse_override_target = 0;` | `control_types.h:415` | 按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`） |
| CrawlState（两轴） | `bool enabled = false;` | `control_types.h:460` | 同步写（`motion_sync.cpp:111-338`）；Follow/触边读（`main.cpp:3078-3213,3723-4227`） |
| CrawlState（两轴） | `bool window_active = false;` | `control_types.h:461` | 同步写（`motion_sync.cpp:111-338`）；Follow/触边读（`main.cpp:3078-3213,3723-4227`） |
| CrawlState（两轴） | `double handle_ref = 0.0;` | `control_types.h:462` | 同步写（`motion_sync.cpp:111-338`）；Follow/触边读（`main.cpp:3078-3213,3723-4227`） |
| CrawlState（两轴） | `double rot_ref = 0.0;` | `control_types.h:463` | 同步写（`motion_sync.cpp:111-338`）；Follow/触边读（`main.cpp:3078-3213,3723-4227`） |
| CrawlState（两轴） | `double base_rel = 0.0;` | `control_types.h:464` | 同步写（`motion_sync.cpp:111-338`）；Follow/触边读（`main.cpp:3078-3213,3723-4227`） |
| CrawlState（两轴） | `double rot_base_rel = 0.0;` | `control_types.h:465` | 同步写（`motion_sync.cpp:111-338`）；Follow/触边读（`main.cpp:3078-3213,3723-4227`） |
| CrawlState（两轴） | `double start_abs = 0.0;` | `control_types.h:466` | 同步写（`motion_sync.cpp:111-338`）；Follow/触边读（`main.cpp:3078-3213,3723-4227`） |
| CrawlState（两轴） | `double end_abs = 0.0;` | `control_types.h:467` | 同步写（`motion_sync.cpp:111-338`）；Follow/触边读（`main.cpp:3078-3213,3723-4227`） |
| ForceFeedbackState::HandoffHold（两侧） | `bool active = false;` | `control_types.h:477` | capture/complete/observe/reset（`control_types.h:545-627`）；blend读写（`force_feedback.cpp:76-112`） |
| ForceFeedbackState::HandoffHold（两侧） | `bool handoff_complete = false;` | `control_types.h:478` | capture/complete/observe/reset（`control_types.h:545-627`）；blend读写（`force_feedback.cpp:76-112`） |
| ForceFeedbackState::HandoffHold（两侧） | `bool blending = false;` | `control_types.h:479` | capture/complete/observe/reset（`control_types.h:545-627`）；blend读写（`force_feedback.cpp:76-112`） |
| ForceFeedbackState::HandoffHold（两侧） | `double force = 0.0;` | `control_types.h:480` | capture/complete/observe/reset（`control_types.h:545-627`）；blend读写（`force_feedback.cpp:76-112`） |
| ForceFeedbackState::HandoffHold（两侧） | `double torque = 0.0;` | `control_types.h:481` | capture/complete/observe/reset（`control_types.h:545-627`）；blend读写（`force_feedback.cpp:76-112`） |
| ForceFeedbackState::HandoffHold（两侧） | `double resume_origin_abs = 0.0;` | `control_types.h:482` | capture/complete/observe/reset（`control_types.h:545-627`）；blend读写（`force_feedback.cpp:76-112`） |
| ForceFeedbackState::HandoffHold（两侧） | `std::int64_t completed_ms = 0;` | `control_types.h:483` | capture/complete/observe/reset（`control_types.h:545-627`）；blend读写（`force_feedback.cpp:76-112`） |
| ForceFeedbackState::HandoffHold（两侧） | `std::int64_t blend_start_ms = 0;` | `control_types.h:484` | capture/complete/observe/reset（`control_types.h:545-627`）；blend读写（`force_feedback.cpp:76-112`） |
| ForceFeedbackState::HoldSample（两侧） | `double force = 0.0;` | `control_types.h:489` | 平滑更新（`force_feedback.cpp:63-73`）；换手capture读（`main.cpp:2521-2524`）；reset（`control_types.h:551-559`） |
| ForceFeedbackState::HoldSample（两侧） | `double torque = 0.0;` | `control_types.h:490` | 平滑更新（`force_feedback.cpp:63-73`）；换手capture读（`main.cpp:2521-2524`）；reset（`control_types.h:551-559`） |
| ForceFeedbackState::HoldSample（两侧） | `std::int64_t tick_ms = 0;` | `control_types.h:491` | 平滑更新（`force_feedback.cpp:63-73`）；换手capture读（`main.cpp:2521-2524`）；reset（`control_types.h:551-559`） |
| ForceFeedbackState::HoldSample（两侧） | `bool initialized = false;` | `control_types.h:492` | 平滑更新（`force_feedback.cpp:63-73`）；换手capture读（`main.cpp:2521-2524`）；reset（`control_types.h:551-559`） |
| ForceFeedbackState | `bool enabled_582 = false;` | `control_types.h:496` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `bool enabled_587 = false;` | `control_types.h:497` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `bool clamp_hold_enabled = true;` | `control_types.h:523` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `double force_582_f = 0.0;` | `control_types.h:525` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `double force_582_n = 0.0;` | `control_types.h:526` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `double force_582_theory_f = 0.0;` | `control_types.h:527` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `double force_582_theory_n = 0.0;` | `control_types.h:528` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `double force_587_f = 0.0;` | `control_types.h:529` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `double force_587_n = 0.0;` | `control_types.h:530` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `double force_582_filtered_f = 0.0;` | `control_types.h:531` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `double force_587_filtered_f = 0.0;` | `control_types.h:532` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `bool force_582_filter_initialized = false;` | `control_types.h:533` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `bool force_587_filter_initialized = false;` | `control_types.h:534` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `HoldSample smooth_582, smooth_587;` | `control_types.h:535` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `HandoffHold hold_582;` | `control_types.h:536` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `HandoffHold hold_587;` | `control_types.h:537` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `short last_fn_1_raw = 0;` | `control_types.h:539` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `short last_ft_1_raw = 0;` | `control_types.h:540` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `short last_fn_2_raw = 0;` | `control_types.h:541` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `short last_ft_2_raw = 0;` | `control_types.h:542` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceFeedbackState | `bool last_fast_move = false;` | `control_types.h:543` | enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`） |
| ForceSampleFrame（main缓存） | `short ft_1_value = 0;` | `control_types.h:632` | ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`） |
| ForceSampleFrame（main缓存） | `short fn_1_value = 0;` | `control_types.h:633` | ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`） |
| ForceSampleFrame（main缓存） | `short fn_2_value = 0;` | `control_types.h:634` | ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`） |
| ForceSampleFrame（main缓存） | `short ft_2_value = 0;` | `control_types.h:635` | ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`） |
| ForceSampleFrame（main缓存） | `double fn_1_value_v = 0.0;` | `control_types.h:636` | ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`） |
| ForceSampleFrame（main缓存） | `double ft_1_value_v = 0.0;` | `control_types.h:637` | ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`） |
| ForceSampleFrame（main缓存） | `double fn_2_value_v = 0.0;` | `control_types.h:638` | ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`） |
| ForceSampleFrame（main缓存） | `double ft_2_value_v = 0.0;` | `control_types.h:639` | ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`） |
| ForceSampleFrame（main缓存） | `double axis2_pos_rel = 0.0;` | `control_types.h:640` | ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`） |
| ForceSampleFrame（main缓存） | `double axis1_pos_rel = 0.0;` | `control_types.h:641` | ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`） |
| ForceSampleFrame（main缓存） | `bool valid = false;` | `control_types.h:642` | ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`） |
| ForceSampleFrame（main缓存） | `DWORD tick_ms = 0;` | `control_types.h:643` | ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`） |
| ForceSampleFrame（main缓存） | `std::int64_t qpc_ticks = 0;` | `control_types.h:644` | ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`） |
| StartupState | `StartupPhase phase = StartupPhase::WaitForEnter;` | `control_types.h:659` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `bool completed = false;` | `control_types.h:660` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `bool prompted = false;` | `control_types.h:661` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `bool direct_positioning = false;` | `control_types.h:662` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `DWORD phase_t0 = 0;` | `control_types.h:663` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double axis1_hold_rel = 0.0;` | `control_types.h:665` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double axis2_hold_rel = 0.0;` | `control_types.h:666` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double axis3_hold_rel = 0.0;` | `control_types.h:667` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double axis5_hold_rel = 0.0;` | `control_types.h:668` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double axis6_hold_rel = 0.0;` | `control_types.h:669` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double axis7_hold_rel = 0.0;` | `control_types.h:670` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double axis3_move_base_rel = 0.0;` | `control_types.h:672` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double axis5_move_base_rel = 0.0;` | `control_types.h:673` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double axis6_move_base_rel = 0.0;` | `control_types.h:674` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double direct_axis5_stage_from_left_mm = 0.0;` | `control_types.h:675` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double direct_axis6_stage_from_left_mm = 0.0;` | `control_types.h:676` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double final_axis1_from_left_mm = 20.0;` | `control_types.h:678` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double final_axis3_from_left_mm = 649.0;` | `control_types.h:679` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double final_axis5_from_left_mm = 649.0;` | `control_types.h:680` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double final_axis6_from_left_mm = 650.0;` | `control_types.h:681` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double final_axis2_deg = 0.0;` | `control_types.h:682` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double final_axis7_deg = 0.0;` | `control_types.h:683` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `bool recovery_mode = false;` | `control_types.h:686` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `bool loading_pose_match = false;` | `control_types.h:687` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `bool loading_ready_symbol_available = false;` | `control_types.h:688` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `bool loading_ready_plc = false;` | `control_types.h:689` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `double v_limit_backup[7] = {};` | `control_types.h:691` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| StartupState | `bool v_limit_scaled = false;` | `control_types.h:692` | 启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`） |
| AppContext（指针为别名，三项为自有值） | `CADSComm* ads = nullptr;` | `control_types.h:703` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `AdsCommunicationService* ads_service = nullptr;` | `control_types.h:704` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `Handle* axis1_input_handle = nullptr;` | `control_types.h:705` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `Handle* axis6_input_handle = nullptr;` | `control_types.h:706` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `Handle* handle_axis1 = nullptr;` | `control_types.h:707` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `Handle* handle_axis6 = nullptr;` | `control_types.h:708` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `HandleFilterState* axis1_handle_filter = nullptr;` | `control_types.h:709` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `HandleFilterState* axis6_handle_filter = nullptr;` | `control_types.h:710` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `const ControlConfig* cfg = nullptr;` | `control_types.h:713` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `const CylinderPreset* cyl = nullptr;` | `control_types.h:714` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* pos = nullptr;` | `control_types.h:717` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* plc_act_pos = nullptr;` | `control_types.h:718` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* plc_init_pos = nullptr;` | `control_types.h:719` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* plc_leftlimit = nullptr;` | `control_types.h:720` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* plc_act_pos_from_left = nullptr;` | `control_types.h:721` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* plc_v_limit = nullptr;` | `control_types.h:722` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double plc_act_velocity_mm_s[7] = {};` | `control_types.h:723` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `std::uint64_t plc_snapshot_sequence = 0;` | `control_types.h:724` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `std::int64_t plc_snapshot_qpc_ticks = 0;` | `control_types.h:725` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `GuidewireMode* guidewire_mode = nullptr;` | `control_types.h:728` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `bool* dual_reverse = nullptr;` | `control_types.h:729` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `CrawlState* axis1_crawl = nullptr;` | `control_types.h:730` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `CrawlState* axis6_crawl = nullptr;` | `control_types.h:731` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `StartupState* startup = nullptr;` | `control_types.h:732` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `ForceFeedbackState* ff = nullptr;` | `control_types.h:733` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis3_base_rel = nullptr;` | `control_types.h:736` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis5_base_rel = nullptr;` | `control_types.h:737` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis6_mirror_base_rel = nullptr;` | `control_types.h:738` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis2_hold_rel = nullptr;` | `control_types.h:739` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis7_hold_rel = nullptr;` | `control_types.h:740` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis1_prev_linear_filtered = nullptr;` | `control_types.h:741` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis6_prev_linear_filtered = nullptr;` | `control_types.h:742` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis1_prev_rot_filtered = nullptr;` | `control_types.h:743` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis6_prev_rot_filtered = nullptr;` | `control_types.h:744` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis1_follow_cmd_abs = nullptr;` | `control_types.h:745` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis6_follow_cmd_abs = nullptr;` | `control_types.h:746` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `bool* axis1_reverse_switch_guard_active = nullptr;` | `control_types.h:747` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `bool* axis6_reverse_switch_guard_active = nullptr;` | `control_types.h:748` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis1_prev_abs_for_trigger = nullptr;` | `control_types.h:749` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis6_prev_abs_for_trigger = nullptr;` | `control_types.h:750` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `bool* axis1_prev_abs_valid = nullptr;` | `control_types.h:751` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `bool* axis6_prev_abs_valid = nullptr;` | `control_types.h:752` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* independent_axis1_hold_rel = nullptr;` | `control_types.h:753` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* independent_axis2_hold_rel = nullptr;` | `control_types.h:754` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* independent_axis3_hold_rel = nullptr;` | `control_types.h:755` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* independent_axis5_hold_rel = nullptr;` | `control_types.h:756` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `bool* axis6_window_locked = nullptr;` | `control_types.h:757` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis6_locked_window_start_abs = nullptr;` | `control_types.h:758` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `double* axis6_locked_window_end_abs = nullptr;` | `control_types.h:759` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| AppContext（指针为别名，三项为自有值） | `bool* startup_smoothing_bypass = nullptr;` | `control_types.h:760` | 指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`） |
| ForceCalibrationConfig（配置） | `double axial_direct_n_per_v = force_direct_calibration::fn_1_slope_n_per_volt;` | `force_calibration.h:30` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double tangential_direct_n_per_v = force_direct_calibration::ft_1_slope_n_per_volt;` | `force_calibration.h:31` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double guidewire_axial_direct_n_per_v = force_direct_calibration::fn_2_slope_n_per_volt;` | `force_calibration.h:33` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double guidewire_tangential_direct_n_per_v = force_direct_calibration::ft_2_slope_n_per_volt;` | `force_calibration.h:34` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double decouple_ff =  0.996063;` | `force_calibration.h:36` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double decouple_ft =  0.0;` | `force_calibration.h:37` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double decouple_tf = -0.103597;` | `force_calibration.h:38` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double decouple_tt =  0.996063;` | `force_calibration.h:39` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double installed_axial_gain = 1.80;` | `force_calibration.h:42` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double installed_axial_offset_n = 0.183108;` | `force_calibration.h:43` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double installed_torque_nmm_per_n = 18.440851;` | `force_calibration.h:44` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double installed_torque_offset_nmm = 0.020906;` | `force_calibration.h:45` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `bool gravity_comp_enabled = false;` | `force_calibration.h:48` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `bool gravity_comp_validated = false;` | `force_calibration.h:49` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double grav_ft_a = 0.4983;` | `force_calibration.h:50` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double grav_ft_phi_deg = -53.54;` | `force_calibration.h:51` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double grav_ft_offset = -0.1119;` | `force_calibration.h:52` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double grav_fa_a = 0.3195;` | `force_calibration.h:53` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double grav_fa_phi_deg = 48.42;` | `force_calibration.h:54` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double grav_fa_offset = -0.1861;` | `force_calibration.h:55` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double rot_counts_per_deg = 2150.0;` | `force_calibration.h:56` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double f_max_n = force_direct_calibration::operational_force_limit_n;` | `force_calibration.h:59` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double t_max_nm = force_direct_calibration::operational_handle_torque_limit_nm;` | `force_calibration.h:60` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double deadband_f_n = 0.02;` | `force_calibration.h:61` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationConfig（配置） | `double deadband_t_nm = 0.00001;` | `force_calibration.h:63` | 构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表 |
| ForceCalibrationState | `double f_zero = 0.0;` | `force_calibration.h:68` | 取零写（`main.cpp:759-794`）；标定/输出/记录读（`main.cpp:4442-4459,4598-4599,4622-4637`） |
| ForceCalibrationState | `double ft_zero = 0.0;` | `force_calibration.h:69` | 取零写（`main.cpp:759-794`）；标定/输出/记录读（`main.cpp:4442-4459,4598-4599,4622-4637`） |
| ForceCalibrationState | `double fn_2_zero = 0.0;` | `force_calibration.h:70` | 取零写（`main.cpp:759-794`）；标定/输出/记录读（`main.cpp:4442-4459,4598-4599,4622-4637`） |
| ForceCalibrationState | `double ft_2_zero = 0.0;` | `force_calibration.h:71` | 取零写（`main.cpp:759-794`）；标定/输出/记录读（`main.cpp:4442-4459,4598-4599,4622-4637`） |
| ForceCalibrationState | `double theta0_deg = 0.0;` | `force_calibration.h:72` | 取零写（`main.cpp:759-794`）；标定/输出/记录读（`main.cpp:4442-4459,4598-4599,4622-4637`） |
| ForceCalibrationState | `bool zeroed = false;` | `force_calibration.h:73` | 取零写（`main.cpp:759-794`）；标定/输出/记录读（`main.cpp:4442-4459,4598-4599,4622-4637`） |
| ReturnRecovery（私有） | `bool holding_ = false, resume_pending_ = false, clear_applied_ = false;` | `return_recovery.h:64` | 只经公开方法改写（`return_recovery.h:17-62`）；main API见B.11.3.3 |
| ReturnRecovery（私有） | `bool unfinished_ = true;` | `return_recovery.h:65` | 只经公开方法改写（`return_recovery.h:17-62`）；main API见B.11.3.3 |
| ReturnRecovery（私有） | `std::uint8_t mask_ = 0;` | `return_recovery.h:66` | 只经公开方法改写（`return_recovery.h:17-62`）；main API见B.11.3.3 |
| ReturnRecovery（私有） | `unsigned int resumes_ = 0;` | `return_recovery.h:67` | 只经公开方法改写（`return_recovery.h:17-62`）；main API见B.11.3.3 |
| ReturnRecovery（私有） | `std::uint64_t next_clear_ = 0, clear_sequence_ = 0, clear_observed_sequence_ = 0;` | `return_recovery.h:68` | 只经公开方法改写（`return_recovery.h:17-62`）；main API见B.11.3.3 |
| ReturnRecovery（私有） | `std::string reason_;` | `return_recovery.h:69` | 只经公开方法改写（`return_recovery.h:17-62`）；main API见B.11.3.3 |
| HandleNetDistance（私有，两轴） | `double net_mm_ = 0.0;` | `handle_net_distance.h:29` | sample/set_following写（`handle_net_distance.h:9-25`）；net_mm读（`main.cpp:4851-4852`） |
| HandleNetDistance（私有，两轴） | `double actual_mm_ = 0.0;` | `handle_net_distance.h:30` | sample/set_following写（`handle_net_distance.h:9-25`）；net_mm读（`main.cpp:4851-4852`） |
| HandleNetDistance（私有，两轴） | `double origin_mm_ = 0.0;` | `handle_net_distance.h:31` | sample/set_following写（`handle_net_distance.h:9-25`）；net_mm读（`main.cpp:4851-4852`） |
| HandleNetDistance（私有，两轴） | `std::uint64_t sequence_ = 0;` | `handle_net_distance.h:32` | sample/set_following写（`handle_net_distance.h:9-25`）；net_mm读（`main.cpp:4851-4852`） |
| HandleNetDistance（私有，两轴） | `bool sample_valid_ = false;` | `handle_net_distance.h:33` | sample/set_following写（`handle_net_distance.h:9-25`）；net_mm读（`main.cpp:4851-4852`） |
| HandleNetDistance（私有，两轴） | `bool following_ = false;` | `handle_net_distance.h:34` | sample/set_following写（`handle_net_distance.h:9-25`）；net_mm读（`main.cpp:4851-4852`） |
| DeliveryTrackingParameters（两轴） | `double kp = 0.5;` | `delivery_tracking.h:45` | set_parameter写（`delivery_tracking.cpp:28-87`）；can_enable/PI读（`delivery_tracking.cpp:94-117,223-281`） |
| DeliveryTrackingParameters（两轴） | `double ki = 2.0;` | `delivery_tracking.h:46` | set_parameter写（`delivery_tracking.cpp:28-87`）；can_enable/PI读（`delivery_tracking.cpp:94-117,223-281`） |
| DeliveryTrackingParameters（两轴） | `double max_gain = 2.0;` | `delivery_tracking.h:47` | set_parameter写（`delivery_tracking.cpp:28-87`）；can_enable/PI读（`delivery_tracking.cpp:94-117,223-281`） |
| DeliveryTrackingParameters（两轴） | `double max_error_mm = 20.0;` | `delivery_tracking.h:49` | set_parameter写（`delivery_tracking.cpp:28-87`）；can_enable/PI读（`delivery_tracking.cpp:94-117,223-281`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `bool segment_active = false;` | `delivery_tracking.h:55` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `bool grip_assumed = false;` | `delivery_tracking.h:56` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `int invalid_reason = static_cast<int>(TrackingInvalidReason::NotForwardDelivery);` | `delivery_tracking.h:57` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `std::uint64_t segment_id = 0;` | `delivery_tracking.h:58` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double handle_raw = 0.0;` | `delivery_tracking.h:60` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double handle_filtered = 0.0;` | `delivery_tracking.h:61` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double raw_mapping_increment_axis_mm = 0.0;` | `delivery_tracking.h:63` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double nominal_forward_increment_mm = 0.0;` | `delivery_tracking.h:65` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double compensated_requested_forward_increment_mm = 0.0;` | `delivery_tracking.h:66` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double effective_forward_increment_mm = 0.0;` | `delivery_tracking.h:67` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double handover_forward_increment_mm = 0.0;` | `delivery_tracking.h:69` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double session_handover_forward_mm = 0.0;` | `delivery_tracking.h:70` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double compensation_gain = 1.0;` | `delivery_tracking.h:72` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double p_term = 0.0;` | `delivery_tracking.h:73` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double i_term = 0.0;` | `delivery_tracking.h:74` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `bool integral_limited = false;` | `delivery_tracking.h:75` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double refer_rel_mm = 0.0;` | `delivery_tracking.h:77` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double actual_rel_mm = 0.0;` | `delivery_tracking.h:78` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double actual_forward_delta_mm = 0.0;` | `delivery_tracking.h:79` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double segment_master_forward_mm = 0.0;` | `delivery_tracking.h:80` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double segment_actual_forward_mm = 0.0;` | `delivery_tracking.h:81` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double session_master_forward_mm = 0.0;` | `delivery_tracking.h:82` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double session_actual_forward_mm = 0.0;` | `delivery_tracking.h:83` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `double tracking_error_mm = 0.0;` | `delivery_tracking.h:85` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingAxisSnapshot（AxisRuntime内） | `bool tracking_error_limited = false;` | `delivery_tracking.h:86` | 控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`） |
| DeliveryTrackingController::AxisRuntime（两轴私有） | `bool active = false;` | `delivery_tracking.h:154` | 控制器方法写读（`delivery_tracking.cpp:128-331,357-470`）；main不直接访问 |
| DeliveryTrackingController::AxisRuntime（两轴私有） | `std::uint32_t grip_command_since_ms = 0;` | `delivery_tracking.h:155` | 控制器方法写读（`delivery_tracking.cpp:128-331,357-470`）；main不直接访问 |
| DeliveryTrackingController::AxisRuntime（两轴私有） | `std::uint32_t last_pi_tick_ms = 0;` | `delivery_tracking.h:156` | 控制器方法写读（`delivery_tracking.cpp:128-331,357-470`）；main不直接访问 |
| DeliveryTrackingController::AxisRuntime（两轴私有） | `double last_actual_abs_mm = 0.0;` | `delivery_tracking.h:157` | 控制器方法写读（`delivery_tracking.cpp:128-331,357-470`）；main不直接访问 |
| DeliveryTrackingController::AxisRuntime（两轴私有） | `double integral_mm_s = 0.0;` | `delivery_tracking.h:158` | 控制器方法写读（`delivery_tracking.cpp:128-331,357-470`）；main不直接访问 |
| DeliveryTrackingController::AxisRuntime（两轴私有） | `double integral_before_last_update_mm_s = 0.0;` | `delivery_tracking.h:159` | 控制器方法写读（`delivery_tracking.cpp:128-331,357-470`）；main不直接访问 |
| DeliveryTrackingController::AxisRuntime（两轴私有） | `double pending_handover_error_mm = 0.0;` | `delivery_tracking.h:160` | 控制器方法写读（`delivery_tracking.cpp:128-331,357-470`）；main不直接访问 |
| DeliveryTrackingController::AxisRuntime（两轴私有） | `double nominal_forward_outstanding_mm = 0.0;` | `delivery_tracking.h:161` | 控制器方法写读（`delivery_tracking.cpp:128-331,357-470`）；main不直接访问 |
| DeliveryTrackingController::AxisRuntime（两轴私有） | `double compensation_forward_outstanding_mm = 0.0;` | `delivery_tracking.h:162` | 控制器方法写读（`delivery_tracking.cpp:128-331,357-470`）；main不直接访问 |
| DeliveryTrackingController::AxisRuntime（两轴私有） | `bool pending_handover_error_limited = false;` | `delivery_tracking.h:163` | 控制器方法写读（`delivery_tracking.cpp:128-331,357-470`）；main不直接访问 |
| DeliveryTrackingController::AxisRuntime（两轴私有） | `DeliveryTrackingAxisSnapshot snapshot;` | `delivery_tracking.h:164` | 控制器方法写读（`delivery_tracking.cpp:128-331,357-470`）；main不直接访问 |
| DeliveryTrackingController（私有） | `AxisRuntime axis_[2];` | `delivery_tracking.h:186` | 参数/开关/段号由API写（`delivery_tracking.cpp:20-126,411-429`）；main不直接访问 |
| DeliveryTrackingController（私有） | `DeliveryTrackingParameters parameters_[2];` | `delivery_tracking.h:187` | 参数/开关/段号由API写（`delivery_tracking.cpp:20-126,411-429`）；main不直接访问 |
| DeliveryTrackingController（私有） | `bool compensation_enabled_ = false;` | `delivery_tracking.h:188` | 参数/开关/段号由API写（`delivery_tracking.cpp:20-126,411-429`）；main不直接访问 |
| DeliveryTrackingController（私有） | `std::uint64_t next_segment_id_ = 0;` | `delivery_tracking.h:189` | 参数/开关/段号由API写（`delivery_tracking.cpp:20-126,411-429`）；main不直接访问 |
| Handle（两柄） | `bool bQuit = false;` | `手柄/Handle.h:18` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `bool bForceEnable = false;` | `手柄/Handle.h:19` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `bool bPause = false;` | `手柄/Handle.h:20` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `int iID1 = -1;` | `手柄/Handle.h:21` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `int iFnt = 2;` | `手柄/Handle.h:22` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `USERData objData;` | `手柄/Handle.h:23` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `double fVels2[2], fJoints2[2];` | `手柄/Handle.h:24` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `unsigned char buttons2;` | `手柄/Handle.h:25` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `long encoders2[2];` | `手柄/Handle.h:26` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `DWORD serial_number_;` | `手柄/Handle.h:55` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `ULONGLONG last_sample_rx_ms_ = 0;` | `手柄/Handle.h:56` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `bool rebase_requested_ = false;` | `手柄/Handle.h:57` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `ULONGLONG sample_gap_ms_ = 0;` | `手柄/Handle.h:58` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `bool opened_once_ = false;` | `手柄/Handle.h:59` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `bool device_epoch_known_ = false;` | `手柄/Handle.h:60` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `std::uint64_t device_epoch_ = 0;` | `手柄/Handle.h:61` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `bool resume_settling_ = false;` | `手柄/Handle.h:62` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| Handle（两柄） | `ULONGLONG resume_since_ms_ = 0;` | `手柄/Handle.h:63` | init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`） |
| homing::Record | `std::array<double, 7> leftlimit{}, init_pos{}, last_abs{};` | `homing_record.h:15` | 读取文件构造（`homing_record.h:40-60`）；main新坐标记录写（`main.cpp:1949-1968`）；复用读取（`main.cpp:1410-1456`） |
| homing::Record | `std::int64_t completed_time_unix_ms = 0, last_abs_time_unix_ms = 0;` | `homing_record.h:16` | 读取文件构造（`homing_record.h:40-60`）；main新坐标记录写（`main.cpp:1949-1968`）；复用读取（`main.cpp:1410-1456`） |
| homing::Record | `std::string application;` | `homing_record.h:17` | 读取文件构造（`homing_record.h:40-60`）；main新坐标记录写（`main.cpp:1949-1968`）；复用读取（`main.cpp:1410-1456`） |
| homing::Record | `bool complete = false;` | `homing_record.h:18` | 读取文件构造（`homing_record.h:40-60`）；main新坐标记录写（`main.cpp:1949-1968`）；复用读取（`main.cpp:1410-1456`） |
| PendingStartupParams（main本地结构） | `double axis1_from_left_mm = 0.0;` | `main.cpp:919` | 写/读见B.11.2.3（`main.cpp:931-935,5260-5305`） |
| PendingStartupParams（main本地结构） | `double axis3_from_left_mm = 0.0;` | `main.cpp:920` | 写/读见B.11.2.3（`main.cpp:931-935,5260-5305`） |
| PendingStartupParams（main本地结构） | `double axis5_from_left_mm = 0.0;` | `main.cpp:921` | 写/读见B.11.2.3（`main.cpp:931-935,5260-5305`） |
| PendingStartupParams（main本地结构） | `double axis6_from_left_mm = 0.0;` | `main.cpp:922` | 写/读见B.11.2.3（`main.cpp:931-935,5260-5305`） |
| PendingStartupParams（main本地结构） | `double axis2_deg = 0.0;` | `main.cpp:923` | 写/读见B.11.2.3（`main.cpp:931-935,5260-5305`） |
| PendingStartupParams（main本地结构） | `double axis7_deg = 0.0;` | `main.cpp:924` | 写/读见B.11.2.3（`main.cpp:931-935,5260-5305`） |
| PendingStartupParams（main本地结构） | `double speed_scale = 0.0;` | `main.cpp:925` | 写/读见B.11.2.3（`main.cpp:931-935,5260-5305`） |
| HandleRecordSnapshot（deque元素） | `std::int64_t qpc_ticks = 0;` | `main.cpp:945` | 插入/过期淘汰（`main.cpp:1472-1484`）；按源QPC读（`main.cpp:4659-4674`） |
| HandleRecordSnapshot（deque元素） | `bool axis1_valid = false;` | `main.cpp:946` | 插入/过期淘汰（`main.cpp:1472-1484`）；按源QPC读（`main.cpp:4659-4674`） |
| HandleRecordSnapshot（deque元素） | `bool axis6_valid = false;` | `main.cpp:947` | 插入/过期淘汰（`main.cpp:1472-1484`）；按源QPC读（`main.cpp:4659-4674`） |

上表按源码物理声明行收录424项数据声明；一行多个字段保持原声明，不把成员方法算作状态。配置字段也收录，以免“长驻但通常不变”与“每拍局部量”混淆。**已确认**：`PlannedReturnCoordinator`的七个`cancel_*`保留字段没有生产代码逐字段使用，`completion_clear_confirmed`有使用；对应范围与例外已在B.11.3.2列明。
