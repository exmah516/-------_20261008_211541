# C 时间敏感动作与 PLC 能力

路径引用约定：D/ 唯一展开为 介入机器人/B数据端/工程/ADS项目/；U/ 为 介入机器人/C从端/250902/Untitled2/；P/ 为 U/POUs/；G 是唯一文件别名 U/GVLs/G.TcGVL；T 是唯一文件别名 介入机器人/C从端/250902/250902.tsproj。所有文件:行号均为当前磁盘物理行号；同一引用里的逗号表示同文件的多个证据区间。本附件只描述现状。“已确认”指静态源码、离线记录或测试直接证明；不等于现场动作验证。

## C.1 时间敏感动作序列

| 序列 | 发起者 | 当前步骤和输出 | 等待条件/时钟 | 延迟/丢拍敏感性 | 确定度与出处 |
|---|---|---|---|---|---|
| 普通Follow | 数据端main每拍 | Handle过滤差分→axis1/6窗口裁剪→写refer、四电缸、fast flag | 下一ADS快写；PLC handle每1ms平滑/NC执行 | 漏采可能改变瞬时增量及窗口/模式边界处应用量（推断）；绝对手柄位置的差分不能直接等同永久丢失全部运动量；实测抖动未知 | 已确认：D/main.cpp:2083-2245,3665-4279；P/handle.TcPOU:489-571 |
| 双手柄一次共同换手 | 数据端先识别轴1/g端，待1/3/5/6实际到位低速，再begin planned | 持有者1/3夹→150ms→移动者2/4松→500ms→PLC Prepare/Commit→PLC MoveAbsolute→停稳→移动者夹→150ms→持有者松→100ms→重建refer | 上位机GetTickCount/64 phase；ADS generation/通知；PLC状态10/20/31各2s，状态30无阶段timeout | 需要本次请求后的新通知；只漏过Busy边沿时可由新Done通知+实际到目标兼容确认；Req/Busy快照只辅助恢复/清理，不能独立判Done | 已确认：`D/main.cpp:2283-2339,2529-3076,3876-3924`；`P/handle.TcPOU:579-703` |
| 单手柄轴1换手 | inline Follow在轴1端点且同向输入 | 轴1计划返回另一端；换手阶段轴3/5保持，轴6按本次leg集合处理；同一协调器夹爪时序 | 同上；当前没有独立的run_axis1_crawl_state实现 | 端点判断依赖实际位置+增量；丢拍可能延后触发 | 已确认：`D/main.cpp:2581-2635,3927-4227` |
| 导丝轴6独立换手 | `run_axis6_crawl_state`计算非零同向触边；可被axis4方向命令辅助触发 | axis6移到窗口另一端，轴1/3/5冻结；夹3开4夹；planned Axis6 | `.2mm`、`.5mm/s`；PLC回退2s执行/2s启用 | 只要后续同向增量仍在可触发；慢快照延迟不改变PLC内部执行 | 已确认：`D/main.cpp:3121-3213`；`P/handle.TcPOU:629-703` |
| 屈曲/间距恢复 | 物理587低三位组合上升沿 | 1/2/7保持；四电缸450/1000/550/100；150ms后把axis1输入正增量累计，3/5/6等量向远离左限位方向移动；松键进入ExitSync | GetTickCount；dt最多50ms；步长≤40mm/s×dt；退出至少100ms，2s后允许按实际位置重同步，仍需同步成功 | ClampWait从阶段进入计时，未等夹爪输出generation；长拍被50ms截断；当前输入来自axis1，日志“582”与双柄587映射不符 | 已确认：D/main.cpp:1503-1575,3215-3388,4300-4353；D/control_types.h:142-151；功能核查见E |
| 公共开始控制 | Zenoh StartControl→SelectDirectControl | 当前位置sync_all(20)；清启动资格，startup Done/completed，control_active=true；没有夹爪300ms阶段 | 同步阻塞等待；公共命令结果期限3000ms | 同步成功前不置control_active；没有调用begin_direct_positioning | 已确认：D/zenoh_service.cpp:841-850；D/main.cpp:5315-5356 |
| 内部标准装卸启动 | 内部ExecuteStartup→startup_sequence入口；装卸姿态及资格位均满足 | 旋转2/7先启动→打开夹爪→1/5/6准备点→3/4夹→1/3/5/6最终→2夹→sync | 三个夹爪阶段各300ms；目标误差.2mm/deg；运动到位无总timeout | GetTickCount阶段计时；不等待输出generation；当前公共Zenoh PreparePosition实际启动PLC自检，此内部序列未被公共分发 | 已确认：D/main.cpp:5277-5313,3550-3625,3432-3453；D/startup_sequence.cpp:103-135,213-216；D/zenoh_service.cpp:816-850 |
| 内部启动中断恢复 | 同一内部ExecuteStartup入口，装卸姿态或资格位不满足 | 全程1/3开2/4夹；1/2/6/7先运动→2s后5运动→再2s后3运动；所有最终目标满足后完成 | 两个2s固定等待；不是1/2/6/7先保持；最后阶段才查全部到位 | 主循环阻塞/丢拍推迟阶段边界；PLC/ADS重连本身不等于已调用此入口 | 已确认：D/startup_sequence.cpp:123-135,168-216；D/main.cpp:3511-3547 |
| 紧急回撤 | 内部EmergencyRetractDevice，planned active时拒绝；当前公共Zenoh未分发 | 备份限速；3/5/6用四平移原限速最小值×startup_motion_speed_scale（默认.005）；1/2/7保持；3/5/6到startup.final（默认610/615/628mm）；四电缸1000/0/100/1000，Y阀请求true | 3/5/6实际误差均≤.2mm；无固定总timeout；输出发布后还原限速并sync_all(20) | 主循环暂停延迟完成观察；还原失败有日志，control_active仍取决于sync结果 | 已确认：D/main.cpp:427-467,3297-3313,4357,4523-4540,5191-5232；D/control_types.h:181,195-199；D/zenoh_service.cpp:814-956 |
| 力传感器取零 | UI/内部ForceZero；main lambda | 等3个新的force_valid ADS快照，平均4路电压和axis2位置 | 每帧wait最多500ms；快照wait预算至多3×500ms，失败即返回；调用总耗时未由该预算单独界定 | 依赖新序号；快照缺失直接失败；调用在消费命令的main线程 | 已确认：`D/main.cpp:759-804` |
| 定位臂关节点动 | 主端UI每100ms重发；数据端arm线程写sum | G.arm_* request/params→PLC ArmManual状态0/10/20/30 | 数据端读周期50ms；方向租约300ms；ADS调用传250ms参数（低频API忽略调用者期限、底层300ms、队列等待无总上限） | 100ms保活覆盖300ms租约；线程/ADS失败保留dirty请求 | 已确认：介入机器人/A主端/工程/master/VI.Module.PositioningArm/PositioningArmPanel.xaml.cs:23-24,160-169；D/arm_manual_ads_service.cpp:13-17,483-556,558-663；P/ArmManual.TcPOU:145-254 |
| 定位臂末端点动 | UI每100ms重发完整ArmCartesianJog；数据端arm线程写PLC alive；PLC ArmCartJog | mode/dir/speed/alive→stage10/25→失alive减速→40/50/60 | 输入lease500ms；PLC独立alive500ms，实际周期要求1ms；数据端读50ms；ArmCartesianAlive命令只续程序归零 | 超过500ms未更新PLC alive后进入释放路径；定时器丢拍数量不能单独界定网络端到端租约年龄 | 已确认：介入机器人/A主端/工程/master/VI.Module.PositioningArm/PositioningArmPanel.xaml.cs:23-24,160-169；D/arm_manual_ads_service.cpp:15-17,236-251,297-301,608-638,692-703；P/ArmCartJog.TcPOU:38-146 |
| 轴4手动 | 主端control帧持续携带方向；数据端500ms租约；离散值变更后ADS写G.axis4_fwd/rev | PLC state0→10 disable→20 MoveVelocity→30 stop→39保留→40恢复 | GetTickCount64租约；PLC state10/30/40各2s；数据端主循环到期清零 | 网络中断后需主循环观察到租约到期再清请求；PLC内部1ms扫描 | 已确认：D/main.cpp:510-513,1609-1639,5163-5173；D/ads_communication.cpp:1650-1667；P/handle.TcPOU:706-884 |
| 注射器 | UI持续方向，主循环500ms租约 | G.inject_push/pull→PLC InjectorManual NC13/14 | GetTickCount64租约；PLC没有本地阶段timeout；松开/换向由状态0/20/30处理 | 主循环观察到500ms到期后清请求；PLC停止等待无上限 | 已确认：D/main.cpp:1609-1648,5180-5188；P/InjectorManual.TcPOU:23-109 |
| 电缸5/Y阀 | 主端写press req；PLC每拍映射 | press=true→500，false→2000 | handle POU超时分支前RETURN，保持上次输出 | 数据端更新press请求后，需handle走到475-479才更新该物理输出 | 已确认：`P/handle.TcPOU:321-479`；`G:139-145` |

### C.1.1 一次完整换手的逐步证据

**已确认**：当前统一协调器最多有轴1/6两腿，轴3/5不作为本次运动腿。默认普通电缸值为1开/夹=450/5，2开/夹=5/1000，3开/夹=550/50，4开/夹=5/1000。下表若某腿缺席，该侧保持普通抓持，不执行该侧开夹动作。出处：D/control_types.h:203-215,270-335；D/main.cpp:2581-2635。

| 次序/阶段 | 发起和输出 | 等待条件与起算点 | 时钟/默认值 | 延迟或缺拍影响；确定程度与出处 |
|---|---|---|---|---|
| 0 dual_handoff待运动停稳 | 数据端触到轴1或g边界后冻结本次换手意图 | 实际边界合格，1/3/5/6目标误差均≤.2mm，速度均≤.5mm/s | 无独立计时上限 | 慢反馈延迟发起；已确认：D/main.cpp:3855-3924 |
| 1 ClampSettle，stage0 | 持有者cyl1/cyl3先夹；移动者cyl2/cyl4仍夹；轴1/3/5/6保持入场值 | 发布输出generation，等applied_motion_output_generation达到该代次后重置phase_t0 | GetTickCount now_ms；max(所含腿持有者夹稳)=150ms | ADS排队时间不消耗这150ms；输出确认无本阶段期限；已确认：D/main.cpp:2532-2537,2581-2614,2681-2703,4562-4567 |
| 2 ClampSettle，stage1 | 保持持有者夹，移动者松 | 新输出代次实际应用后重新计时，满release等待→SubmitRequest | 同上；max(cyl2/cyl4 release)=500ms | 代码残留“50ms”注释不等于当前配置；已确认：D/control_types.h:117-124；D/main.cpp:2539-2544,2696-2707,3208,4566 |
| 3 SubmitRequest/Prepare | 通信队列清Req、写目标/速度/加减速/jerk；不置Req | 命令结果成功；失败且尚未启动可RetryClear | 提交时间GetTickCount64；调用超时比较用运动树DWORD now_ms；250ms | 长拍延后轮询；32/64位混用见D；已确认：D/main.cpp:582-654,2732-2769；D/ads_communication.cpp:1692-1712 |
| 4 提交保护输出 | 保持refer入场值，置参与腿fast旁路；保留双侧夹持交接组合 | 输出generation已应用，PLC !Busy/!Done/!Error；然后锁存本次通知基线 | GetTickCount now_ms；250ms | 不满足时不入Commit；已确认：D/main.cpp:2804-2837,4542-4560 |
| 5 Commit / AwaitAccepted | 队列最后置Req=true；尚未ACK仍保持entry refer | 命令成功后等本次新Busy=true或Done=false；也可观察PLC ACK后跳过迟到写回执 | ACK 250ms；phase用GetTickCount | Commit超时保守置possibly_started，禁止按未启动整组重发；已确认：D/main.cpp:2669-2677,2774-2801,2839-2882 |
| 6 AwaitDone | ACK后参与轴refer为return target，fast=true；轴3/5保持；PLC执行C.3各状态 | 每腿新的Done=true通知且!Busy；已ACK时Done序号须晚于ACK | 首个ACK起execution 5000ms；GetTickCount | 快读Req/Busy不能替代Done通知；PLC内部2s等待另计；已确认：D/main.cpp:2283-2339,2583-2609,2884-2906 |
| 7 RetryClear（仅未启动支路） | clear请求入通信队列，再等较新有效快照且!Busy/!Error；重新校验目标 | 只允许一次；任何started/possibly_started转故障；回到SubmitRequest | 清命令250ms，清状态500ms；GetTickCount与提交GetTickCount64 | 超时进入故障保持；已确认：D/main.cpp:2639-2659,2909-2995 |
| 8 AwaitFreshSnapshot | Done后记录快照序号；保持实际位置，关闭fast；移动者重新夹，持有者仍夹 | 下一有效快照序号更大，rebase_planned_return成功 | 无本阶段独立期限 | 慢快照/重基准失败延长双侧夹紧；已确认：D/main.cpp:2567-2571,2583-2614,2893-2900,2998-3009 |
| 9 PublishHandoff→AwaitHandoffApplied | 同一输出批次携带清Req、实际位置保持、fast=false、恢复夹持 | 发布后捕获handoff_generation；等该代次实际应用 | 无本阶段独立期限 | 这是ADS结果确认，代码不宣称SUM子项相对PLC扫描原子；已确认：D/main.cpp:3011-3022,4481-4491,4576-4581；D/ads_communication.cpp:1755-1756 |
| 10 PostHandoffClampSettle，restore0 | 移动者已夹；持有者仍夹 | 上一步应用确认后计时 | GetTickCount；cyl2/4夹稳150ms | 主循环慢拍只延长保持；已确认：D/main.cpp:2546-2551,3024-3035 |
| 11 restore1 | 松持有者，移动者继续夹；捕获release_output_generation | 先等release输出应用，再等cyl1/3松稳100ms和有效快照 | GetTickCount；100ms | 输出确认无独立期限；已确认：D/main.cpp:2553-2558,3038-3050,4569-4574 |
| 12 最终重基准/恢复Follow | rebase成功，启动力反馈释放距离/时间原点；符合导管正向条件时arm轴1先行；reset planned | rebase失败留在原阶段重试 | 力保持完成时间steady_clock；流程无额外固定等待 | 换手自动运动不消耗力保持的2mm；已确认：D/main.cpp:3052-3072 |

### C.1.2 其它按时间或按拍编排

| 序列 | 当前编排、输入输出 | 时钟/等待 | 延迟或缺拍影响；确定程度与出处 |
|---|---|---|---|
| 轴4→轴6附加位移 | 轴4UI方向同时让axis6按±7.8016217564mm/s累加目标；首次只建立时间基准；仍经g窗口与计划回退 | GetTickCount64两拍差；这里没有elapsed封顶 | 长拍产生更大本拍请求，随后受窗口裁剪；已确认：D/main.cpp:61,3759-3777,3955-3988 |
| tracking夹稳与PI | 夹紧命令持续150ms才开段；当前有正向输入才增加PI补偿；每拍额外≤.25mm、总增益默认≤2 | GetTickCount DWORD；积分dt封顶100ms；无欠账时积分每次×.75 | 150ms从命令持续起算，未用夹爪反馈；.25mm与.75是按调用次数，非按实测时间；已确认：D/delivery_tracking.h:42-50,92,167-172；D/delivery_tracking.cpp:128-169,223-280 |
| 轴1先行 | arm后前10mm名义递送按lead/10加额外轴1位移，默认lead2mm；axis3/5保持名义镜像关系 | 按输入距离，无自主定时动作 | 只有当前递送增量才应用；持有/模式/重基准可清零；已确认：D/main.cpp:347-363,1576-1596,3064-3069,3787-3844,4053-4090 |
| 力反馈换手保持与释放 | begin planned锁存力/扭矩；完成后实际运动2mm或400ms开始blend，300ms完成；取样锁存平滑时间常数100ms | steady_clock毫秒；换手完成后起400ms，blend另起300ms | force样本无效且满足保留条件时继续持有并重设blend原点；这不是机器人运动指令；已确认：D/control_types.h:561-591；D/force_feedback.cpp:21,62-108；D/main.cpp:3056-3063,4584-4596 |
| 同步/重基准采样 | sync_all(20/30)等调用对主循环内手柄取均值；每样本都Sleep(10)，无“必须新sample_sequence”检查 | 20/30样本含至少200/300ms显式Sleep；OS调度时间另加 | 此期间main不推进自身状态，ADS后台仍运行；重复缓存可被计为不同poll样本；已确认：D/control_types.cpp:29-92；D/motion_sync.cpp:406-439；D/main.cpp:3440,3502,4315,4537,5336 |
| 连接保持后恢复 | 手柄断流/重开先进入resume_settling；新帧连续500ms后请求重基准，再恢复Follow；活动planned走保留/恢复条件 | 新帧间隔≤250ms；断流>1500ms或epoch变更需静置；超过5000ms缓存失效；详见D | 缓存poll不会自行推进新帧连续时间；已确认：D/手柄/remote_handle_bridge.h:61-84；D/手柄/手柄.cpp:27-104；D/main.cpp:1079-1325 |
| 内部标准启动阶段值 | ReleaseClamps四电缸450/5/500/5→1/5/6到28/510/525→3/4设0/1000→1/3/5/6及2/7最终→2夹1000→完成 | 三个夹稳各300ms；各运动段位置/角度容差.2；PLC二阶旁路，前级斜坡/均值仍在 | 此分支只由内部ExecuteStartup选中，不能等同公共“准备位置”；已确认：D/control_types.h:179-200,208-215；D/main.cpp:3550-3625；D/startup_sequence.cpp:213-216 |
| 残留direct_positioning | begin_direct_positioning设PositionAxis156，随后PositionAxis356→Done，只有目标到位等待 | .2mm/deg，无独立总期限 | 当前只有定义和内部赋true，没有调用点；可达现场动作未知；已确认代码：D/main.cpp:381-420,3455-3508,5315-5356 |

## C.2 PLC任务、POU和调用顺序

**已确认**：当前tsproj只引用U/Untitled2.plcproj，ADS端口851；PlcTask源码注释明确1000µs=1ms、优先级20、入口MAIN（U/PlcTask.TcTTO:3-9；T:3594-3598）。旧Untitled1任务声明10000µs=10ms，但不在当前tsproj的PLC项目引用中（介入机器人/C从端/250902/Untitled1/PlcTask.TcTTO:3-8；T:3594-3781）。

| 当前任务 | 周期原值/优先级/端口 | 单位和确定度 | 出处 |
|---|---|---|---|
| PlcTask | TcTTO=1000；tsproj Task=10000；Instance Context=1000000；priority20；AMS350 | TcTTO明确µs，实际配置目标1ms已确认；三个XML字段采用不同尺度 | U/PlcTask.TcTTO:3-9；T:1443-1446,3766-3778 |
| NC-Task 1 SAF | CycleTime=10000；priority4；AMS501 | 数值已确认；与PlcTask的Task字段同尺度，按此关系推断1ms；运行周期未知 | T:1451-1462 |
| NC-Task 1 SVB | CycleTime=50000；priority8；AMS511 | 数值已确认；按相同尺度推断5ms；运行周期未知 | T:1463-1465 |

**已确认**：MAIN每扫描顺序为7业务AXIS_REF刷新→2注射AXIS_REF刷新→FirstCycle初始化→session/heartbeat watchdog→ArmManual→ArmCartesian→ArmCartJog→Axis2Sync→InjectorManual→轴6负向自检许可→POWERED→非_handle时SETPOSITIONGEN→RESETED→自检请求清理→CASE分发→保存本次dispatched_state；_handle分支先handle1_再SETPOSITIONGEN。出处：P/MAIN.TcPOU:35-204。当前CASE只真正分发_init、_self_check、_handle、_clear_err、_err、_reset；DUT仍枚举jog/trans/trans_wait，但没有对应业务分支（U/DUTs/state.TcDUT:4-17；P/MAIN.TcPOU:120-203）。

| POU | 状态变量/阶段 | 输入 | 输出、副作用 | 确定程度与出处 |
|---|---|---|---|---|
| MAIN | G.gen_state、host_watchdog、restore_pending/restore_ready、last_dispatched_state | session/heartbeat/recover；selfcheck_start_req/targets、homing_restore_*；轴反馈 | 调度；FBD FB；自检启动校验；寻参复用 | 已确认；P/MAIN.TcPOU:5-33,35-204 |
| init | step 0/1、init_watchdog | power_output、轴状态 | 7轴MC_Power；记录init_pos；_self_check或_err | 已确认；P/init.TcPOU:9-14,27-83 |
| SelfCheck | 每轴stu 0/1/2/3/4/5/55/56/6/7/71/72/8/10；扫描与错开timer、动作exec | init_target_from_left、target_axes、轴/Power/Reset反馈 | leftlimit/init_pos；refer基准；电缸；self_check_done/startup_loading_ready/handle_reinit_req | 已确认；P/SelfCheck.TcPOU:8-51,118-567 |
| handle | init_done；return_state[7] 0/10/20/30/31；axis4_manual_state 0/10/20/30/39/40；host_timeout_latched | refer、v_limit、return_cmd、fast flags、axis4 req、心跳 | 平滑feed；回退MC_MoveAbsolute；axis4点动；软限位；Y阀；超时冻结 | 已确认；P/handle.TcPOU:9-104,108-957 |
| err | step 0/1/2/3/10/11/20；backoff_attempts/exec/distance、pulse/retry_timer | 轴错误、Power、左限位、恢复常量 | estop_hold、disable；MC_Reset/Power循环；限位退离；_clear_err | 已确认；P/err.TcPOU:10-29,32-263 |
| clear_err | step 0/1、settle_timer | has_err、all_powered、self_check_done | estop_hold；重初始化handle或回_init | 已确认；P/clear_err.TcPOU:9-15,19-85 |
| reset | step 0/1/2、reset_timer | reset/axis/power错误 | disable、MC_Reset；回_init或_err | 已确认；P/reset.TcPOU:9-14,17-77 |
| ArmManual | axis_state[5] 0/10/20/30、current_dir/pending_dir、move/stop/reset_exec | arm_manual_enable、enable/reset/jog req、velocity/acc/dec/jerk、curve/cjog_owned | MC_Power/Reset/MoveVelocity/Stop；arm状态数组 | 已确认；P/ArmManual.TcPOU:5-27,30-254 |
| ArmCartesian | stage 0/10/20/30/40/50/60、elapsed/phase_elapsed、heartbeat_age、finish_state、curve/p/v/a | curve_data/request/cancel/heartbeat、arm驱动状态/参数 | ack、owned、state/error/progress；曲线feed/使能/禁用/停止 | 已确认；P/ArmCartesian.TcPOU:5-44,46-269 |
| ArmCartJog | stage 0/10/25/40/50/60、alive_age/enable_age、zero_cycles、fault_stop、gen | mode/dir/speed/alive、arm驱动状态 | cjog_owned/state/error/limited/path_speed；feed/FB握手 | 已确认；P/ArmCartJog.TcPOU:5-27,29-146 |
| ArmCartJogGen | latched_mode/dir、anchor、path_v/v0/v1/tau/duration/sigma、branch | start/held/mode/dir/speed/seed/cycle_dt | p/v/a/path_speed/target_speed/limited | 已确认；P/ArmCartJogGen.TcPOU:5-54,56-216 |
| Axis2Sync | gear_execute/active/done/failed、reset_execute/had_axis_error、reset timers | axis2_sync_axis、业务轴2使能/反馈 | Axis15 MC_Power/Reset/GearIn、sync_error/id | 已确认；P/Axis2Sync.TcPOU:5-17,19-89 |
| InjectorManual | axis_state[2] 0/10/20/30、current/pending_dir、move/stop_exec | inject_push/pull_req、host_comm_timeout | 自动MC_Power、MoveVelocity、MC_Stop | 已确认；P/InjectorManual.TcPOU:5-21,23-109 |

**已确认**：以上13个POU均在 `U/Untitled2.plcproj:33-70`的Compile清单。MAIN另内嵌FBD动作POWERED、RESETED、SETPOSITIONGEN，分别承载7轴Power、Reset、外部给定Enable/Disable的库调用及输出映射，调用次序如上（P/MAIN.TcPOU:207-2438,2439-3809,3810-7063）。目录还存在两份未编入当前工程的POU，不与正式能力混算：

| 未编入文件 | 职责、状态和GVL接口 | 确定程度与出处 |
|---|---|---|
| move_edge.TcPOU | prevRef/rt_cmd/moveAbs/moveOut；参考差>.01且非Busy/Active的上升沿MoveAbsolute；读refer/init_pos并操作axis，vel50、acc/dec200 | 已确认源码；P/move_edge.TcPOU:4-48；不在 `U/Untitled2.plcproj:33-70` |
| handle(备份).TcPOU | 旧handle FB，init_done/hold_active、平滑与回退缓存；读写旧G.axis1/3/5/6_return_cmd等名字 | 已确认源码；P/handle(备份).TcPOU:3-35,71-185,259-265；不在 `U/Untitled2.plcproj:33-70` |

## C.3 PLC计划回退10/20/30/31

**已确认**：`return_enable`只有轴1/3/5/6有效，当前数据端只发轴1、轴6专用Req；轴3/5的旧符号仍在plc_io中但不是当前planned queue使用对象。`P/handle.TcPOU:58-66,574-577`、`D/plc_io.cpp:56-106`。

| PLC状态 | 动作 | 等待/转移 | 数值和缺口 | 确定度 |
|---|---|---|---|---|
| 0 | Busy=false；Req false时清Done/Error；锁存目标后进10 | Req且无Done/Error | 无独立timeout | 已确认：`P/handle.TcPOU:579-597` |
| 10 | Disable执行 | Disable.Done或非Enabled→20；Req清/Disable.Error→30 | 2s；超时错误收尾 | 已确认：`P/handle.TcPOU:599-627`、`G:181` |
| 20 | MC_MoveAbsolute执行；refer_from_left=TargetAbs-leftlimit | Done→30；Error/Aborted→30错误 | 2s；只认Done，不以接近目标替代 | 已确认：`P/handle.TcPOU:629-643` |
| 30 | 等ActVelo≤.5且stop未激活，试图清traj参数 | 满足→31 | **无阶段timeout**；CASE内IF return_state[i] <> 30 THEN在状态30恒假，653-663锁存/重建块不可达；494-499又保持traj_pos | 已确认代码矛盾：P/handle.TcPOU:494-499,645-668；引入df776f3691ed15e936b590bec128c0b0d5fc404d；实际物理效果未知 |
| 31 | Enable重新执行 | Enable.Done且无Error→0/Done；错误最多3次；超时err/estop | 2s | 已确认：`P/handle.TcPOU:670-703`、`G:181-182` |

数据端在planned中另有ACK 250ms、retry clear500ms、execution5000ms；这三项不是PLC状态10/20/31的替代值，而是上位机对ADS/通知的观察窗口。`D/control_types.h:170-173`，已确认。


**已确认：回退完成的通知依赖。** Commit前捕获每腿事件序号并武装request_armed；ACK来自本次请求后的Busy=TRUE或Done=FALSE事件。已ACK时，完成要求新的Done=TRUE事件且!Busy；没有ACK时，仅允许“本次请求后新Done=TRUE通知、!Busy、!Error、有效位置快照且实际绝对位置在目标容差内”的兼容确认。Req/Busy快速快照参与清理、恢复观察，不会独立制造Done事件。出处：D/main.cpp:2283-2339,2830-2837；通知边沿序号由 D/ads_communication.cpp:704-782 写入；快读只包含Req/Busy，D/ads_communication.cpp:1295-1308。

## C.4 平滑、限速、软限位和输出

| 能力 | 当前事实 | 出处/确定度 |
|---|---|---|
| 坐标 | Act_pos=NC.ActPos-init_pos；from_left=NC.ActPos-leftlimit；refer使用相对init_pos | `P/handle.TcPOU:489-492`；已确认 |
| 快退旁路 | fast flag或return Busy时平均窗=1、ramp清零、跳过平滑部分 | `P/handle.TcPOU:502-547`；已确认 |
| 速度坡 | 每扫描按v_limit限制ref_slow增量；代码未按dt换算 | `P/handle.TcPOU:502-532`；已确认 |
| 平均滤波 | 缓存20点，窗口逐拍增至20 | `P/handle.TcPOU:510-516,534-545`；已确认 |
| 二阶轨迹 | wn=60；acc=wn²(refer-traj_pos)-2wn·traj_vel，限amax后按固定.001s积分，再限vmax | `P/handle.TcPOU:547-571`；已确认 |
| 软限位 | 轴1/3/5/6右限=leftlimit+rslimit；实际到右限且refer继续增时冻结 | `P/handle.TcPOU:58-60,298-300,518-522`、`G:180`；已确认 |
| rslimit | [99,0,666,0,688,688,0] | `G:180`；已确认 |
| 电缸1–4 | `WORD AT %Q*`，当前主循环直接写G.cylinder*_value；SelfCheck/MAIN也会初始化 | `G:139-145`、`P/SelfCheck.TcPOU:148-154,541-545`；已确认 |
| 电缸5 | press req true=500，false=2000；handle超时分支前RETURN使输出保持上次值 | `P/handle.TcPOU:321-479`；已确认 |
| 力输入 | 四路INT AT %I*；PLC POU不解算滤波 | `G:148-151`；已确认 |


### C.4.1 轴4手动各阶段

方向请求解码：fwd单独TRUE为1，rev单独TRUE为-1，冲突或全FALSE为0。速度/加速度/减速度/jerk=60/200/200/500（P/handle.TcPOU:85-91,706-729）。**已确认**。

| 阶段 | 输出与转移 | 确定程度与出处 |
|---|---|---|
| 0 | 待请求；无方向时清done/error；有方向→10 | 已确认；P/handle.TcPOU:713-729 |
| 10 | Disable外部给定；完成→20；错误最多3次→40 | 已确认；P/handle.TcPOU:752-771 |
| 20 | MoveVelocity；松开或换向→30；Error→40；CommandAborted→30 | 已确认；P/handle.TcPOU:773-800 |
| 30 | MC_Stop；Done或ActVelo≤0.05且无StopError→39 | 已确认；P/handle.TcPOU:802-824 |
| 39 | 保留MC_Stop Execute=FALSE完整扫描；仍有方向且无错→20，否则→40 | 已确认；P/handle.TcPOU:826-837 |
| 40 | 锁存当前位置到参考/缓存；Enable外部给定；Done→0并置done/error | 已确认；P/handle.TcPOU:839-884 |

10/30/40每阶段2s；40超时或恢复失败进入_err；10/30超时先转40收尾（P/handle.TcPOU:731-750）。该状态机对数据端10ms请求延迟的响应在PLC1ms扫描内执行；实际延迟分布**未知**。

### C.4.2 自检/寻参与复用

- **已确认**：目标平移轴1/3/5/6；扫描与回退速度均40，扫描acc/dec/jerk=50/100/100，回退=100/100/100（P/SelfCheck.TcPOU:17-22,510-528）。
- **已确认**：每轴0禁用→1错开→2扫描→3停稳→4/5清错或55/56重上电→6等所有平移轴达到限位且Power ready→7/71同步SetPos=ActPos→72错开回退→8等Done→10完成（P/SelfCheck.TcPOU:140-448）。
- **已确认**：扫描错开轴3/5/6=300/600/900ms；100ms堵转后备；电子限位停稳后再等300ms（P/SelfCheck.TcPOU:156-181,183-237）。
- **已确认**：Power重新使能脉冲200ms、等待3s，超时循环重试；无最大总次数（P/SelfCheck.TcPOU:285-306）。
- **已确认**：轴6先启动回退并看到Busy/Done/ActVelo>0.5才启动stagger；轴5/3/1从该起点等1/2/3s（P/SelfCheck.TcPOU:388-420）。
- **已确认**：stagger_timer的PT=10s，但实际放行只比较ET与各轴delay，没有把Q作为错误退出条件（P/SelfCheck.TcPOU:129,411）。
- **已确认**：旋转轴2/7回绝对0，轴4保持当前ActPos；前述注释称2/4/7回0与实现不符（P/SelfCheck.TcPOU:60-62,450-507）。
- **已确认**：全7轴stu=10后电缸1/3=1000、2/4=150；self_check_done/startup_loading_ready=TRUE，handle_reinit_req=TRUE；旋转轴2/7 init_pos=0、轴4=current ActPos；刷新参考转_handle（P/SelfCheck.TcPOU:532-567）。
- **已确认**：寻参复用在MAIN的_self_check分支：接收homing_restore_req后等所有外部给定禁用且Power/轴/Reset无错，复制leftlimit/init_pos缓存、设置标准电缸值、刷新参考，置homing_restore_status=1并转_handle；拒绝status=2，reason1=未就绪，2=Disable错误（P/MAIN.TcPOU:124-175）。本分支无独立超时。

### C.4.3 定位臂、注射器、同步轴

| 链路 | 输入/门控/执行 | 确定程度与出处 |
|---|---|---|
| 定位臂关节点动 | host timeout或总使能FALSE清总使能/单轴enable/jog；参数非正用5/50/50/500；正反冲突停止 | 已确认；P/ArmManual.TcPOU:30-59,105-127 |
| 定位臂关节复位 | 先MC_Stop完成再MC_Reset；复位完成或Error后清请求；curve/cjog所有权在时先取消曲线 | 已确认；P/ArmManual.TcPOU:73-103 |
| 关节点动阶段 | 每轴0空闲，10运动，20常规停，30换向停；停止阈值ActVel<0.01且Stop不Busy，或Stop.Done | 已确认；P/ArmManual.TcPOU:145-222 |
| 归零曲线输入 | 17个LREAL；索引0..4起点，5..9终点，10..14弯曲量，15时长0.02..120s，16必须0；数据先锁存再ack | 已确认；P/ArmCartesian.TcPOU:72-163 |
| 归零曲线门控 | PLC实际周期须1ms，全部5轴已使能无错、没有关节点动、heartbeat_age<0.5s；cjog不得持有 | 已确认；P/ArmCartesian.TcPOU:46-70,91-127 |
| 归零曲线阶段 | 10外部给定enable→20五次平滑时间参数曲线→30等待到位(0.05,0.1)至少2拍→40disable→50错误时Stop→60等Busy释放→0 | 已确认；P/ArmCartesian.TcPOU:167-269 |
| 末端点动请求 | mode1..5、dir±1、speed(0,100]；alive0.5s；drive_ready同归零曲线 | 已确认；P/ArmCartJog.TcPOU:38-73 |
| 末端点动阶段 | 10使能→25运行；失保活gen减速，连续零速2拍→40；故障40→50→60，正常40→60；60释放所有权 | 已确认；P/ArmCartJog.TcPOU:74-146 |
| 末端点动轨迹 | mode1/2=X/Y平面逆解，3=Z(轴1减)，4=俯仰(轴5减)，5=偏航且保持TCP；T_ACC=1s/T_DEC=.5s；关节速度帽[24,30,30,30,30]；12次二分限步 | 已确认；P/ArmCartJogGen.TcPOU:22-34,56-216 |
| 注射器 | NC13/14每拍Power Enable=TRUE；push单TRUE正向，pull单TRUE负向，冲突=0；速度180、acc/dec100、jerk500 | 已确认；P/InjectorManual.TcPOU:8-11,23-42 |
| 注射器阶段 | 0空闲→10运动；松开20、换向30；Stop.Done或速度≤.05后0或反向10；无本地阶段超时 | 已确认；P/InjectorManual.TcPOU:44-109 |
| Axis15同步 | 跟随业务轴2使能；双方ActVelo≤.01且Power ready时GearIn 1:1同向；acc/dec100、jerk500；InGear完成，Error/Abort锁gear_failed | 已确认；P/Axis2Sync.TcPOU:50-89 |
| Axis15恢复 | 错误初次立即Reset脉冲，此后每2s再触发200ms脉冲；错误清除重置gear状态；无最大重试次数 | 已确认；P/Axis2Sync.TcPOU:19-44；G:186-187 |

## C.5 AdsOutputCommand及反馈依赖

| 字段 | 数据端填写/含义 | ADS符号及PLC用法 | 推进/门控依赖 | 确定程度与出处 |
|---|---|---|---|---|
| `motion_enabled` | 是否写refer/fast；false时若上一帧motion true只补写两个fast=false | `G.refer`、`G.axis1_fast_return`、`G.axis6_fast_retract` | `ads_motion_cycle_valid`、控制树 | 已确认：`D/ads_communication.cpp:1636-1648` |
| `refer[7]` | 七轴相对init目标 | `G.refer`；PLC handle平滑/NC | 每帧输出，实际执行后ActPos反馈 | 已确认：`D/ads_communication.h:65-70`、`P/handle.TcPOU:489-571` |
| fast flags | 旁路轴1/6回退平滑 | G flags | planned request/commit/输出generation | 已确认：`D/ads_communication.h:69-70` |
| cylinder[4],valid,write_mask | 四电缸目标；自动0x0f，手动只选bit | G.cylinder1..4_value | connection/estop/自动接管门控 | 已确认：`D/ads_communication.h:71-74`、`D/main.cpp:4356-4399` |
| cylinder5_press_req | Y阀/电缸5压合请求 | `G.cylinder5_press_req` | 数据端离散输出变化时写，PLC映射500/2000 | 已确认：`D/ads_communication.h:75`；`P/handle.TcPOU:475-479` |
| axis4 fwd/rev | UI轴4方向 | G.axis4_fwd_req/rev_req | 500ms上位机租约，PLC 0/10/20/30/39/40 | 已确认：`D/ads_communication.h:76-77` |
| inject push/pull[2] | 两注射器方向 | G.inject_push_req/pull_req | 500ms上位机租约，PLC无本地deadline | 已确认：`D/ads_communication.h:78-79`；`P/InjectorManual.TcPOU:23-109` |
| startup_smoothing_bypass | startup/emergency旁路标志 | G.startup_smoothing_bypass | PLC在547处用其旁路二阶轨迹；仍有前级斜坡/均值 | 已确认：`D/ads_communication.h:80`、`P/handle.TcPOU:547-571` |
| planned_return_clear_mask | 内部bit0轴1、bit1轴6清Req | `G.return_cmd[1/6].Req`清写 | 取消/完成/重试clear；不属于普通16符号快写 | 已确认：`D/ads_communication.h:81-82`、`D/ads_communication.cpp:1553-1618` |
| 通信服务附加：host session/heartbeat/recover | 这些不是AdsOutputCommand成员；服务给快写追加会话和递增心跳，pending时追加恢复请求 | G.host_session_id、G.host_heartbeat_sequence、G.host_recover_req | PLC MAIN 2s watchdog；host timeout反馈 | 已确认：D/ads_communication.h:65-83；D/ads_communication.cpp:1755-1761；P/MAIN.TcPOU:68-97 |

**已确认**：快速读符号完整基础表为`TwinCAT_SystemInfoVarList._TaskInfo[1].CycleCount`、`DcTaskTime`；7轴NC ActPos（若全有则direct，否则fallback G.Act_pos）、G.init_pos、G.leftlimit、轴1/3/5/6 ActVelo、四力输入、estop、host timeout、轴1/6 Req/Busy、handle_reinit_done、err_backoff_active/failed[7]、axis2_sync_error/id、self_check_done。句柄解析在 `D/ads_communication.cpp:1231-1320`。通知注册gen_state、selfcheck、reinit、estop、host timeout、startup ready、axis4、axis1/6 return Busy/Done/Error/ErrorID，SERVERONCHA 10000。`D/ads_communication.cpp:1900-1962`。低频包括应用名、坐标刷新、寻参复用、selfcheck target/start、定位臂各接口；低频服务单独重连，见 `D/ads_communication.cpp:2060-2225`。

## C.6 PLC GVL分组与读写方

150项完整逐变量声明表直接列于C.6.2，声明来源为G:7-188；逐行交叉引用在docs/arch_survey_r1/scratch/plc_gvl_refs.tsv:1（1169条）。下表是分组索引；PLC写/调用、PLC读是对编译清单内13个POU的词法检索，外部ADS读写另列C.6.1。PLC侧未见字面写不等于没有写入方；host_*、homing_restore_*、selfcheck_target、手动请求由数据端写入，见C.6.1的逐族调用出处。

| GVL行组 | 变量族 | 写入方 | 读取方 |
|---|---|---|---|
| G:7–15 | err_backoff、axis2_sync、homing_restore | PLC err/Axis2Sync/MAIN；数据端写homing request | MAIN、数据端快读/低频 |
| G:17–29 | gen_state、axis、power/reset/output、SetPointGen | PLC各FB/FBD | MAIN、handle、err/reset |
| G:31–47 | init_pos/refer/Act_pos/leftlimit/from_left、自检/启动/reinit | PLC SelfCheck/MAIN；数据端写refer/请求 | handle、main、ADS快读 |
| G:50–58 | estop、host session/heartbeat/recover/timeout | 数据端/MAIN | MAIN/handle |
| G:60–69 | fast flags、startup bypass、return_cmd、axis4 | 数据端写，handle写状态 | handle、ADS快读 |
| G:73–83 | v_limit/ref_slow/PID | 数据端v_limit；handle ref_slow | handle；PID当前无现用POU引用 |
| G:86–126 | arm_*、曲线、cjog | 数据端arm service；PLC POU写状态 | ArmManual/Cartesian/CartJog/主端状态 |
| G:130–136 | axis2_sync、inject_axis/req | 数据端注射/PLC Axis2Sync | NC、InjectorManual |
| G:139–151 | cylinder1..5、四力输入 | 数据端/MAIN/SelfCheck；物理I/O | handle、记录器、主端 |
| G:156–177 | estop诊断 | handle故障入口锁存；正常恢复不清 | 当前数据端未见读取这些G.estop_diag_*符号的调用；不等于现场调试器无人读取 |
| G:180–188 | rslimit、recovery timeout/retry、clear error、axis15 reset | 常量/FB | handle/err/Axis2Sync |

**已确认**：接口文档早期章节写watchdog100ms，而末尾已更正当前为2s，MAIN也为2s；Y阀文档写输出0，而handle为500/2000；SelfCheck注释称轴4回0而实现保持当前；文档描述CASE30锁存位置而代码锁存块恒假。证据：介入机器人/C从端/PLC全局变量与ADS接口.md:117,159-168,276,352,374；P/MAIN.TcPOU:68-97；P/handle.TcPOU:645-668,475-479；P/SelfCheck.TcPOU:450-461。


### C.6.1 数据端与I/O的外部写读注记

本表补足PLC字面引用表不包含的外部访问。以下为当前数据端已确认的符号调用；仅定义符号名不当作已发生读写。PLC内的AXIS_REF/MC功能块还由NC与库调用更新，本表不把其内部数据流当作可穷尽的PLC赋值语句。

| GVL字段族 | 数据端外部写入 | 数据端外部读取/订阅 | 确定程度与出处 |
|---|---|---|---|
| host_session_id、host_heartbeat_sequence、host_recover_req、host_comm_timeout | 通信服务每拍补会话和递增心跳；pending恢复时写recover；PLC清请求 | 快读和通知读取timeout | 已确认；D/ads_communication.cpp:1322-1339,1620,1757-1761,1268-1269,1909-1913 |
| refer、axis1_fast_return、axis6_fast_retract、startup_smoothing_bypass | motion_enabled时写refer/fast；上次motion启用而当前关闭时补写fast=false；bypass在离散输出变更时写 | 快读不读refer/fast；位置读来自NC或Act_pos | 已确认；D/ads_communication.cpp:1636-1667,1255-1319 |
| cylinder1..4_value、cylinder5_press_req、axis4_fwd/rev_req、inject_push/pull_req | 离散输出dirty时按cylinder_valid/write_mask写1..4，其余请求随同批写 | axis4 Busy/Done/Error/ErrorId通知；本组其余不在当前快读基础表 | 已确认；D/ads_communication.cpp:1650-1667,1914-1917；D/ads_communication.h:71-82 |
| return_cmd[1/6] | Prepare先清Req并写TargetAbs/Velocity/Acc/Dec/Jerk；Commit写Req=true；Clear写false | 快读Req/Busy；通知Busy/Done/Error/ErrorId；低频status读取封装也存在 | 已确认；D/ads_communication.cpp:1295-1308,1692-1712,1919-1926；D/plc_io.cpp:394-424 |
| init_pos、leftlimit、axis[*].NcToPlc.ActPos/ActVelo、Act_pos、四力输入 | 当前快写不写基准/反馈 | 快读基准/位置/四轴速度/力；7个直接NC位置句柄不齐则fallback Act_pos | 已确认；D/ads_communication.cpp:1255-1319；D/plc_io.cpp:197-327 |
| v_limit、startup_loading_ready、handle_reinit_req | 启动/恢复缩放与还原v_limit；消费startup_loading_ready=false；同步后清reinit_req | v_limit低频读取；启动资格/reinit通过通知或封装读取 | 已确认；D/plc_io.cpp:336-360,672-675；D/startup_sequence.cpp:53-60,138-165,236-248 |
| homing_restore_leftlimit/init_pos/status/req/reject_reason | main先写缓存leftlimit/init_pos和status，再写req=true | 低频读取status/reject_reason | 已确认；D/main.cpp:1421-1426,1436-1444 |
| selfcheck_target_from_left、selfcheck_start_req、self_check_done、selfcheck_status | UI命令处理写target与start_req | self_check_done快读及通知；status通知 | 已确认；D/main.cpp:4914-4916；D/ads_communication.cpp:1295-1298,1907-1908 |
| err_backoff_active/failed、axis2_sync_error/id、handle_reinit_done、gen_state、estop_hold_req | 当前快写不写这些诊断/状态 | 快读与通知按字段分别读取 | 已确认；D/ads_communication.cpp:1268-1298,1907-1926 |
| arm_manual_enable、enable/reset/jog请求、velocity/acc/dec/jerk | arm服务dirty请求用write_sum写；失败保留dirty | 50ms状态读取manual/enable/参数、act_pos/vel、motion状态、dir/conflict、Power/Reset输出 | 已确认；D/arm_manual_ads_service.cpp:621-643,667-721 |
| arm_curve_data/cancel/request/heartbeat、arm_cjog_mode/dir/speed/alive | 曲线先data/cancel后递增request；单独保活；cjog dirty递增alive并写4字段 | 读取曲线ack/state/error/owned/progress及cjog owned/state/error/limited/path_speed/if_version | 已确认；D/arm_manual_ads_service.cpp:321,349,385-398,608-638,692-702 |
| cylinder*_value（物理输出）、ft/fn_*（物理输入）、AXIS_REF | PLC/数据端写输出；EtherCAT输入和NC回读不是POU内显式赋值 | 物理Box7/8、NC1..15链接 | 已确认；T:7616-7688；G:139-151 |
| estop_diag_* | handle故障路径写快照；当前数据端未检得对应ADS符号读取代码 | 数据端当前PLC-ESTOP文本取快照/事件镜像，不能等同读取这组GVL诊断 | 已确认代码边界；P/handle.TcPOU:190-249；D/main.cpp:1024-1055 |

### C.6.2 全部150项GVL声明与PLC字面引用

确定程度：以下声明和字面引用为已确认；类型/初值保留源码写法，未显式赋初值记作类型原文。W=赋值左值，R=其余字面读取，RW/FB=功能块调用，RW/Axis=AXIS_REF的IN_OUT调用，W/FBD=XML左值。每个单元格只给该POU/角色的首行与出现次数。它不证明被条件挡住的赋值会执行（例如计划回退CASE30），也不覆盖第三方MC库内部或外部ADS写入。“PLC侧未见”限定于当前13个编译POU。

|变量|类型/初值（原样）|说明注释|PLC 写/调用方|PLC 读取方|声明出处|
|---|---|---|---|---|---|
|G.err_backoff_active|ARRAY[1..7] OF BOOL|—|err [W] P/err.TcPOU:73（3处）|err [R] P/err.TcPOU:123（1处）|G:7|
|G.err_backoff_failed|ARRAY[1..7] OF BOOL|—|err [W] P/err.TcPOU:72（2处）|PLC侧未见字面读取|G:8|
|G.axis2_sync_error|BOOL|—|Axis2Sync [W] P/Axis2Sync.TcPOU:21（1处）|Axis2Sync [R] P/Axis2Sync.TcPOU:23（3处）|G:9|
|G.axis2_sync_error_id|UDINT|—|Axis2Sync [W] P/Axis2Sync.TcPOU:22（1处）|PLC侧未见字面读取|G:10|
|G.homing_restore_req|BOOL|—|MAIN [W] P/MAIN.TcPOU:125（1处）|MAIN [R] P/MAIN.TcPOU:124（1处）|G:11|
|G.homing_restore_leftlimit|ARRAY[1..7] OF LREAL|—|PLC侧未见字面写/调用|MAIN [R] P/MAIN.TcPOU:150（1处）|G:12|
|G.homing_restore_init_pos|ARRAY[1..7] OF LREAL|—|PLC侧未见字面写/调用|MAIN [R] P/MAIN.TcPOU:151（1处）|G:13|
|G.homing_restore_status|DINT|—|MAIN [W] P/MAIN.TcPOU:126（3处）|PLC侧未见字面读取|G:14|
|G.homing_restore_reject_reason|DINT|—|MAIN [W] P/MAIN.TcPOU:127（4处）|PLC侧未见字面读取|G:15|
|G.gen_state|state := _init|—|MAIN [W] P/MAIN.TcPOU:45（2处）<br>SelfCheck [W] P/SelfCheck.TcPOU:566（1处）<br>clear_err [W] P/clear_err.TcPOU:59（3处）<br>err [W] P/err.TcPOU:160（3处）<br>handle [W] P/handle.TcPOU:251（5处）<br>init [W] P/init.TcPOU:52（3处）<br>reset [W] P/reset.TcPOU:71（2处）|MAIN [R] P/MAIN.TcPOU:91（7处）|G:17|
|G.axis|ARRAY[1..7] OF axis_REF|—|MAIN [RW/Axis] P/MAIN.TcPOU:138（2处）<br>MAIN [RW/FB] P/MAIN.TcPOU:36（1处）<br>SelfCheck [RW/Axis] P/SelfCheck.TcPOU:87（34处）<br>err [RW/Axis] P/err.TcPOU:77（1处）<br>handle [RW/Axis] P/handle.TcPOU:398（11处）|Axis2Sync [R] P/Axis2Sync.TcPOU:63（2处）<br>MAIN [R] P/MAIN.TcPOU:107（4处）<br>MAIN [R/FBD] P/MAIN.TcPOU:348（7处）<br>SelfCheck [R] P/SelfCheck.TcPOU:159（21处）<br>clear_err [R] P/clear_err.TcPOU:29（1处）<br>err [R] P/err.TcPOU:50（11处）<br>handle [R] P/handle.TcPOU:200（23处）<br>init [R] P/init.TcPOU:49（2处）<br>reset [R] P/reset.TcPOU:54（2处）|G:18|
|G.power_|ARRAY[1..7] OF MC_Power|—|MAIN [RW/FB] P/MAIN.TcPOU:255（7处）<br>SelfCheck [W] P/SelfCheck.TcPOU:286（3处）<br>clear_err [W] P/clear_err.TcPOU:72（1处）<br>err [W] P/err.TcPOU:167（5处）<br>init [W] P/init.TcPOU:34（2处）|Axis2Sync [R] P/Axis2Sync.TcPOU:52（2处）<br>MAIN [R/FBD] P/MAIN.TcPOU:367（7处）<br>handle [R] P/handle.TcPOU:213（3处）|G:19|
|G.power_output|ARRAY[1..7] OF ST_McOutputs|—|MAIN [W/FBD] P/MAIN.TcPOU:230（35处）|Axis2Sync [R] P/Axis2Sync.TcPOU:57（2处）<br>MAIN [R] P/MAIN.TcPOU:135（2处）<br>SelfCheck [R] P/SelfCheck.TcPOU:120（17处）<br>clear_err [R] P/clear_err.TcPOU:29（2处）<br>err [R] P/err.TcPOU:46（6处）<br>handle [R] P/handle.TcPOU:192（4处）<br>init [R] P/init.TcPOU:49（2处）<br>reset [R] P/reset.TcPOU:70（1处）|G:20|
|G.axis_Reset|ARRAY[1..7] OF MC_Reset|—|MAIN [RW/FB] P/MAIN.TcPOU:2487（7处）<br>MAIN [W] P/MAIN.TcPOU:56（1处）<br>SelfCheck [RW/FB] P/SelfCheck.TcPOU:88（11处）<br>clear_err [W] P/clear_err.TcPOU:50（1处）<br>err [W] P/err.TcPOU:147（7处）<br>handle [W] P/handle.TcPOU:889（1处）<br>init [W] P/init.TcPOU:33（1处）<br>reset [W] P/reset.TcPOU:32（3处）|Axis2Sync [R] P/Axis2Sync.TcPOU:47（1处）<br>MAIN [R/FBD] P/MAIN.TcPOU:2583（7处）<br>SelfCheck [R] P/SelfCheck.TcPOU:266（4处）|G:21|
|G.reset_output|ARRAY[1..7] OF ST_McOutputs|—|MAIN [W/FBD] P/MAIN.TcPOU:2462（28处）|MAIN [R] P/MAIN.TcPOU:136（1处）<br>clear_err [R] P/clear_err.TcPOU:29（1处）<br>err [R] P/err.TcPOU:50（3处）<br>handle [R] P/handle.TcPOU:200（2处）<br>reset [R] P/reset.TcPOU:54（2处）|G:22|
|G.SetPointGenEnable|ARRAY[1..7] OF MC_ExtSetPointGenEnable|—|MAIN [RW/FB] P/MAIN.TcPOU:3858（7处）<br>MAIN [W] P/MAIN.TcPOU:54（2处）<br>clear_err [W] P/clear_err.TcPOU:48（1处）<br>err [W] P/err.TcPOU:91（3处）<br>handle [W] P/handle.TcPOU:265（4处）<br>init [W] P/init.TcPOU:31（1处）<br>reset [W] P/reset.TcPOU:33（1处）|MAIN [R/FBD] P/MAIN.TcPOU:3970（21处）<br>handle [R] P/handle.TcPOU:221（8处）|G:23|
|G.SetPointGenDisable|ARRAY[1..7] OF MC_ExtSetPointGenDisable|—|MAIN [RW/FB] P/MAIN.TcPOU:138（9处）<br>MAIN [W] P/MAIN.TcPOU:55（2处）<br>SelfCheck [RW/FB] P/SelfCheck.TcPOU:87（7处）<br>clear_err [W] P/clear_err.TcPOU:49（1处）<br>err [W] P/err.TcPOU:92（4处）<br>handle [W] P/handle.TcPOU:234（3处）<br>init [W] P/init.TcPOU:32（1处）<br>reset [W] P/reset.TcPOU:34（2处）|MAIN [R] P/MAIN.TcPOU:139（2处）<br>MAIN [R/FBD] P/MAIN.TcPOU:4221（7处）<br>SelfCheck [R] P/SelfCheck.TcPOU:143（3处）<br>err [R] P/err.TcPOU:87（1处）<br>handle [R] P/handle.TcPOU:223（9处）|G:24|
|G.SetPointGenEnable_output|ARRAY[1..7] OF ST_McOutputs|—|MAIN [W/FBD] P/MAIN.TcPOU:3877（21处）|PLC侧未见字面读取|G:25|
|G.SetPointGenDisable_output|ARRAY[1..7] OF ST_McOutputs|—|MAIN [W/FBD] P/MAIN.TcPOU:3833（28处）|PLC侧未见字面读取|G:25|
|G.move_jog|ARRAY [1..7] OF MC_Jog|—|PLC侧未见字面写/调用|PLC侧未见字面读取|G:26|
|G.enable|BOOL|跟随运动变量|MAIN [W/FBD] P/MAIN.TcPOU:3925（1处）|PLC侧未见字面读取|G:27|
|G.disable|BOOL|跟随运动变量|MAIN [W/FBD] P/MAIN.TcPOU:4176（1处）|PLC侧未见字面读取|G:27|
|G.flag|INT:=0|—|PLC侧未见字面写/调用|PLC侧未见字面读取|G:28|
|G.flagpos|LREAL:=0|遗留调试量，当前无业务读取|PLC侧未见字面写/调用|PLC侧未见字面读取|G:29|
|G.init_pos|ARRAY[1..7] OF LREAL := [30, 9, 0, 0, 0, 30, 13]|—|MAIN [W] P/MAIN.TcPOU:151（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:447（4处）<br>init [W] P/init.TcPOU:77（1处）|MAIN [R] P/MAIN.TcPOU:161（1处）<br>SelfCheck [R] P/SelfCheck.TcPOU:558（1处）<br>handle [R] P/handle.TcPOU:229（13处）|G:31|
|G.refer|ARRAY[1..7] OF LREAL|—|MAIN [W] P/MAIN.TcPOU:163（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:560（1处）<br>handle [W] P/handle.TcPOU:237（7处）|handle [R] P/handle.TcPOU:229（16处）|G:32|
|G.Act_pos|ARRAY[1..7] OF LREAL|—|MAIN [W] P/MAIN.TcPOU:161（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:558（1处）<br>handle [W] P/handle.TcPOU:235（6处）|MAIN [R] P/MAIN.TcPOU:163（4处）<br>SelfCheck [R] P/SelfCheck.TcPOU:560（4处）<br>handle [R] P/handle.TcPOU:237（28处）|G:32|
|G.act_h|ARRAY[1..7] OF LREAL|—|MAIN [W] P/MAIN.TcPOU:165（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:562（1处）<br>handle [W] P/handle.TcPOU:239（8处）|handle [R] P/handle.TcPOU:543（6处）|G:32|
|G.act_hf|ARRAY[1..7] OF LREAL|—|MAIN [W] P/MAIN.TcPOU:166（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:563（1处）<br>handle [W] P/handle.TcPOU:240（7处）|PLC侧未见字面读取|G:32|
|G.leftlimit|ARRAY[1..7] OF LREAL|—|MAIN [W] P/MAIN.TcPOU:150（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:185（2处）|MAIN [R] P/MAIN.TcPOU:162（1处）<br>SelfCheck [R] P/SelfCheck.TcPOU:361（3处）<br>err [R] P/err.TcPOU:100（2处）<br>handle [R] P/handle.TcPOU:236（11处）|G:35|
|G.act_pos_from_left|ARRAY[1..7] OF LREAL|—|MAIN [W] P/MAIN.TcPOU:162（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:559（1处）<br>handle [W] P/handle.TcPOU:236（6处）|MAIN [R] P/MAIN.TcPOU:167（1处）<br>SelfCheck [R] P/SelfCheck.TcPOU:564（1处）<br>handle [R] P/handle.TcPOU:241（4处）|G:36|
|G.refer_from_left|ARRAY[1..7] OF LREAL|—|MAIN [W] P/MAIN.TcPOU:167（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:564（1处）<br>handle [W] P/handle.TcPOU:241（8处）|PLC侧未见字面读取|G:37|
|G.self_check_done|BOOL := FALSE|—|MAIN [W] P/MAIN.TcPOU:46（2处）<br>SelfCheck [W] P/SelfCheck.TcPOU:546（1处）<br>init [W] P/init.TcPOU:36（1处）|clear_err [R] P/clear_err.TcPOU:63（1处）<br>err [R] P/err.TcPOU:83（1处）|G:38|
|G.selfcheck_start_req|BOOL := FALSE|—|MAIN [W] P/MAIN.TcPOU:47（3处）|MAIN [R] P/MAIN.TcPOU:131（2处）|G:39|
|G.selfcheck_status|DINT := 0|0=未就绪，1=等待启动，2=运行中，3=参数拒绝|MAIN [W] P/MAIN.TcPOU:48（5处）|MAIN [R] P/MAIN.TcPOU:177（1处）|G:40|
|G.selfcheck_target_from_left|ARRAY[1..7] OF LREAL := [96,0,500,0,530,580,0]|—|PLC侧未见字面写/调用|MAIN [R] P/MAIN.TcPOU:180（8处）|G:41|
|G.startup_loading_ready|BOOL := FALSE|TRUE=自检刚完成且仍允许按标准器械装卸姿态启动；上位机开始控制前消费为 FALSE|MAIN [W] P/MAIN.TcPOU:157（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:69（2处）<br>init [W] P/init.TcPOU:37（1处）|PLC侧未见字面读取|G:42|
|G.handle_reinit_req|BOOL := FALSE|—|MAIN [W] P/MAIN.TcPOU:49（2处）<br>SelfCheck [W] P/SelfCheck.TcPOU:548（1处）<br>clear_err [W] P/clear_err.TcPOU:82（1处）<br>handle [W] P/handle.TcPOU:145（2处）|handle [R] P/handle.TcPOU:138（1处）|G:43|
|G.handle_reinit_done|BOOL := FALSE|—|MAIN [W] P/MAIN.TcPOU:50（1处）<br>handle [W] P/handle.TcPOU:144（3处）|PLC侧未见字面读取|G:44|
|G.selfcheck_reset_req|BOOL := FALSE|—|MAIN [W] P/MAIN.TcPOU:52（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:110（1处）<br>init [W] P/init.TcPOU:38（1处）|SelfCheck [R] P/SelfCheck.TcPOU:68（1处）|G:45|
|G.selfcheck_stu|ARRAY[1..7] OF INT|—|SelfCheck [W] P/SelfCheck.TcPOU:82（2处）|PLC侧未见字面读取|G:46|
|G.selfcheck_all_reach_limit|BOOL := FALSE|—|SelfCheck [W] P/SelfCheck.TcPOU:96（2处）|PLC侧未见字面读取|G:47|
|G.estop_hold_req|BOOL := FALSE|—|MAIN [W] P/MAIN.TcPOU:51（1处）<br>clear_err [W] P/clear_err.TcPOU:43（1处）<br>err [W] P/err.TcPOU:67（1处）<br>handle [W] P/handle.TcPOU:232（6处）|ArmCartJog [R] P/ArmCartJog.TcPOU:51（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:63（1处）|G:50|
|G.host_session_id|UDINT := 0|—|PLC侧未见字面写/调用|MAIN [R] P/MAIN.TcPOU:69（3处）<br>handle [R] P/handle.TcPOU:108（3处）|G:55|
|G.host_heartbeat_sequence|UDINT := 0|—|PLC侧未见字面写/调用|MAIN [R] P/MAIN.TcPOU:71（2处）<br>handle [R] P/handle.TcPOU:110（2处）|G:56|
|G.host_recover_req|BOOL := FALSE|—|MAIN [W] P/MAIN.TcPOU:95（1处）<br>handle [W] P/handle.TcPOU:460（2处）|MAIN [R] P/MAIN.TcPOU:93（1处）<br>handle [R] P/handle.TcPOU:456（1处）|G:57|
|G.host_comm_timeout|BOOL := TRUE|—|MAIN [W] P/MAIN.TcPOU:75（3处）<br>handle [W] P/handle.TcPOU:136（2处）|ArmCartJog [R] P/ArmCartJog.TcPOU:51（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:63（1处）<br>ArmManual [R] P/ArmManual.TcPOU:35（1处）<br>InjectorManual [R] P/InjectorManual.TcPOU:24（1处）<br>handle [R] P/handle.TcPOU:133（5处）|G:58|
|G.axis6_fast_retract|BOOL := FALSE|—|handle [W] P/handle.TcPOU:345（1处）|handle [R] P/handle.TcPOU:505（1处）|G:60|
|G.axis1_fast_return|BOOL := FALSE|—|handle [W] P/handle.TcPOU:344（1处）|handle [R] P/handle.TcPOU:504（1处）|G:61|
|G.startup_smoothing_bypass|BOOL := FALSE|—|handle [W] P/handle.TcPOU:348（1处）|handle [R] P/handle.TcPOU:547（2处）|G:62|
|G.return_cmd|ARRAY[1..7] OF ST_AxisPlannedReturnCmd|—|SelfCheck [W] P/SelfCheck.TcPOU:98（5处）<br>handle [W] P/handle.TcPOU:165（27处）|handle [R] P/handle.TcPOU:217（14处）|G:63|
|G.axis4_fwd_req|BOOL := FALSE|—|SelfCheck [W] P/SelfCheck.TcPOU:104（1处）<br>handle [W] P/handle.TcPOU:182（2处）|handle [R] P/handle.TcPOU:707（2处）|G:64|
|G.axis4_rev_req|BOOL := FALSE|—|SelfCheck [W] P/SelfCheck.TcPOU:105（1处）<br>handle [W] P/handle.TcPOU:183（2处）|handle [R] P/handle.TcPOU:707（2处）|G:65|
|G.axis4_manual_busy|BOOL := FALSE|—|SelfCheck [W] P/SelfCheck.TcPOU:106（1处）<br>handle [W] P/handle.TcPOU:184（10处）|PLC侧未见字面读取|G:66|
|G.axis4_manual_done|BOOL := FALSE|—|SelfCheck [W] P/SelfCheck.TcPOU:107（1处）<br>handle [W] P/handle.TcPOU:185（7处）|PLC侧未见字面读取|G:67|
|G.axis4_manual_error|BOOL := FALSE|—|SelfCheck [W] P/SelfCheck.TcPOU:108（1处）<br>handle [W] P/handle.TcPOU:186（6处）|handle [R] P/handle.TcPOU:719（1处）|G:68|
|G.axis4_manual_error_id|UDINT := 0|—|SelfCheck [W] P/SelfCheck.TcPOU:109（1处）<br>handle [W] P/handle.TcPOU:187（7处）|PLC侧未见字面读取|G:69|
|G.v_limit|ARRAY[1..7] OF LREAL := [7.5,7.5,1.5,1.5,1.5,7.5,1.5]|—|PLC侧未见字面写/调用|handle [R] P/handle.TcPOU:502（1处）|G:73|
|G.ref_slow|ARRAY[1..7] OF LREAL := [0,0,0,0,0,0,0]|—|MAIN [W] P/MAIN.TcPOU:164（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:561（1处）<br>handle [W] P/handle.TcPOU:238（9处）|handle [R] P/handle.TcPOU:519（9处）|G:74|
|G.Kp|ARRAY[1..7] OF LREAL := [0.14,0.14,0.14,0.14,0.14,0.14,0.14]|—|PLC侧未见字面写/调用|PLC侧未见字面读取|G:78|
|G.Ki|ARRAY[1..7] OF LREAL := [0.016,0.016,0.016,0.016,0.016,0.016,0.016]|—|PLC侧未见字面写/调用|PLC侧未见字面读取|G:79|
|G.Kd|ARRAY[1..7] OF LREAL := [0.1,0.1,0.1,0.1,0.1,0.1,0.1]|—|PLC侧未见字面写/调用|PLC侧未见字面读取|G:80|
|G.pid_i|ARRAY[1..7] OF LREAL := [0,0,0,0,0,0,0]|—|PLC侧未见字面写/调用|PLC侧未见字面读取|G:81|
|G.pid_e_prev|ARRAY[1..7] OF LREAL := [0,0,0,0,0,0,0]|—|PLC侧未见字面写/调用|PLC侧未见字面读取|G:82|
|G.pid_i_limit|ARRAY[1..7] OF LREAL := [100,100,100,100,100,100,100]|—|PLC侧未见字面写/调用|PLC侧未见字面读取|G:83|
|G.arm_axis|ARRAY[1..5] OF AXIS_REF|定位臂轴对象，链接到 NC Axis 1..5|ArmCartJog [RW/Axis] P/ArmCartJog.TcPOU:102（4处）<br>ArmCartesian [RW/Axis] P/ArmCartesian.TcPOU:222（4处）<br>ArmManual [RW/Axis] P/ArmManual.TcPOU:62（4处）<br>ArmManual [RW/FB] P/ArmManual.TcPOU:32（1处）|ArmCartJog [R] P/ArmCartJog.TcPOU:55（4处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:67（4处）<br>ArmManual [R] P/ArmManual.TcPOU:33（5处）|G:86|
|G.arm_manual_enable|BOOL := FALSE|定位臂手动控制总使能，FALSE 时所有点动请求无效|ArmManual [W] P/ArmManual.TcPOU:38（1处）<br>MAIN [W] P/MAIN.TcPOU:58（1处）|ArmCartJog [R] P/ArmCartJog.TcPOU:51（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:63（1处）<br>ArmManual [R] P/ArmManual.TcPOU:35（3处）|G:87|
|G.arm_enable_req|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|TRUE=对应轴保持上电|ArmManual [W] P/ArmManual.TcPOU:39（1处）<br>MAIN [W] P/MAIN.TcPOU:61（1处）|ArmCartJog [R] P/ArmCartJog.TcPOU:54（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:66（1处）<br>ArmManual [R] P/ArmManual.TcPOU:63（1处）|G:88|
|G.arm_reset_req|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|TRUE=请求清错，PLC 完成后自动清零|ArmManual [W] P/ArmManual.TcPOU:102（1处）<br>MAIN [W] P/MAIN.TcPOU:62（1处）|ArmCartJog [R] P/ArmCartJog.TcPOU:55（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:67（1处）<br>ArmManual [R] P/ArmManual.TcPOU:73（1处）|G:89|
|G.arm_jog_pos_req|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|UI 正向点动按钮，按下 TRUE、松开 FALSE|ArmManual [W] P/ArmManual.TcPOU:40（1处）<br>MAIN [W] P/MAIN.TcPOU:63（1处）|ArmCartJog [R] P/ArmCartJog.TcPOU:56（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:68（1处）<br>ArmManual [R] P/ArmManual.TcPOU:105（2处）|G:90|
|G.arm_jog_neg_req|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|UI 反向点动按钮，按下 TRUE、松开 FALSE|ArmManual [W] P/ArmManual.TcPOU:41（1处）<br>MAIN [W] P/MAIN.TcPOU:64（1处）|ArmCartJog [R] P/ArmCartJog.TcPOU:56（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:68（1处）<br>ArmManual [R] P/ArmManual.TcPOU:105（2处）|G:91|
|G.arm_jog_velocity|ARRAY[1..5] OF LREAL := [5.0,5.0,5.0,5.0,5.0]|—|PLC侧未见字面写/调用|ArmCartesian [R] P/ArmCartesian.TcPOU:97（3处）<br>ArmManual [R] P/ArmManual.TcPOU:44（1处）|G:92|
|G.arm_jog_acc|ARRAY[1..5] OF LREAL := [50.0,50.0,50.0,50.0,50.0]|—|PLC侧未见字面写/调用|ArmCartesian [R] P/ArmCartesian.TcPOU:98（3处）<br>ArmManual [R] P/ArmManual.TcPOU:48（1处）|G:93|
|G.arm_jog_dec|ARRAY[1..5] OF LREAL := [50.0,50.0,50.0,50.0,50.0]|—|PLC侧未见字面写/调用|ArmCartJog [R] P/ArmCartJog.TcPOU:109（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:99（2处）<br>ArmManual [R] P/ArmManual.TcPOU:52（1处）|G:94|
|G.arm_jog_jerk|ARRAY[1..5] OF LREAL := [500.0,500.0,500.0,500.0,500.0]|—|PLC侧未见字面写/调用|ArmCartJog [R] P/ArmCartJog.TcPOU:109（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:100（2处）<br>ArmManual [R] P/ArmManual.TcPOU:56（1处）|G:95|
|G.arm_power_output|ARRAY[1..5] OF ST_McOutputs|—|ArmManual [W] P/ArmManual.TcPOU:67（5处）|ArmCartJog [R] P/ArmCartJog.TcPOU:54（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:66（1处）<br>ArmManual [R] P/ArmManual.TcPOU:119（4处）|G:96|
|G.arm_reset_output|ARRAY[1..5] OF ST_McOutputs|—|ArmManual [W] P/ArmManual.TcPOU:87（5处）|ArmManual [R] P/ArmManual.TcPOU:124（3处）|G:97|
|G.arm_act_pos|ARRAY[1..5] OF LREAL|—|ArmManual [W] P/ArmManual.TcPOU:33（1处）|ArmCartJog [R] P/ArmCartJog.TcPOU:64（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:199（2处）|G:98|
|G.arm_act_vel|ARRAY[1..5] OF LREAL|—|ArmManual [W] P/ArmManual.TcPOU:34（1处）|ArmCartesian [R] P/ArmCartesian.TcPOU:236（1处）<br>ArmManual [R] P/ArmManual.TcPOU:185（2处）|G:99|
|G.arm_motion_busy|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|—|ArmManual [W] P/ArmManual.TcPOU:253（1处）|ArmCartesian [R] P/ArmCartesian.TcPOU:125（1处）|G:100|
|G.arm_motion_done|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|—|ArmManual [W] P/ArmManual.TcPOU:96（5处）|PLC侧未见字面读取|G:101|
|G.arm_motion_error|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|—|ArmManual [W] P/ArmManual.TcPOU:94（7处）|PLC侧未见字面读取|G:102|
|G.arm_motion_error_id|ARRAY[1..5] OF UDINT := [0,0,0,0,0]|—|ArmManual [W] P/ArmManual.TcPOU:95（9处）|PLC侧未见字面读取|G:103|
|G.arm_cmd_dir|ARRAY[1..5] OF SINT := [0,0,0,0,0]|-1=反向，0=停止，1=正向|ArmManual [W] P/ArmManual.TcPOU:252（1处）|PLC侧未见字面读取|G:104|
|G.arm_cmd_conflict|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|正反向同时请求|ArmManual [W] P/ArmManual.TcPOU:105（1处）|ArmManual [R] P/ArmManual.TcPOU:106（2处）|G:105|
|G.arm_curve_data|ARRAY[0..16] OF LREAL|—|PLC侧未见字面写/调用|ArmCartesian [R] P/ArmCartesian.TcPOU:82（1处）|G:107|
|G.arm_curve_request|UDINT|—|PLC侧未见字面写/调用|ArmCartJog [R] P/ArmCartJog.TcPOU:59（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:49（4处）|G:108|
|G.arm_curve_ack|UDINT|—|ArmCartesian [W] P/ArmCartesian.TcPOU:49（3处）|ArmCartJog [R] P/ArmCartJog.TcPOU:59（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:72（1处）|G:109|
|G.arm_curve_cancel|BOOL := TRUE|—|ArmCartesian [W] P/ArmCartesian.TcPOU:50（7处）<br>ArmManual [W] P/ArmManual.TcPOU:75（1处）|ArmCartesian [R] P/ArmCartesian.TcPOU:91（2处）|G:110|
|G.arm_curve_heartbeat|UDINT|—|PLC侧未见字面写/调用|ArmCartesian [R] P/ArmCartesian.TcPOU:56（2处）|G:111|
|G.arm_curve_owned|BOOL|—|ArmCartesian [W] P/ArmCartesian.TcPOU:51（3处）|ArmCartJog [R] P/ArmCartJog.TcPOU:58（1处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:221（2处）<br>ArmManual [R] P/ArmManual.TcPOU:74（3处）|G:112|
|G.arm_curve_state|UDINT|0空闲，1执行，2完成，3取消，4错误|ArmCartesian [W] P/ArmCartesian.TcPOU:52（4处）|PLC侧未见字面读取|G:113|
|G.arm_curve_error|UDINT|—|ArmCartesian [W] P/ArmCartesian.TcPOU:53（10处）|ArmCartesian [R] P/ArmCartesian.TcPOU:75（7处）|G:114|
|G.arm_curve_progress|LREAL|—|ArmCartesian [W] P/ArmCartesian.TcPOU:150（2处）|PLC侧未见字面读取|G:115|
|G.arm_cjog_mode|INT|—|PLC侧未见字面写/调用|ArmCartJog [R] P/ArmCartJog.TcPOU:45（4处）|G:117|
|G.arm_cjog_dir|INT|—|PLC侧未见字面写/调用|ArmCartJog [R] P/ArmCartJog.TcPOU:46（3处）|G:118|
|G.arm_cjog_speed|LREAL|—|PLC侧未见字面写/调用|ArmCartJog [R] P/ArmCartJog.TcPOU:47（3处）|G:119|
|G.arm_cjog_alive|UDINT|—|PLC侧未见字面写/调用|ArmCartJog [R] P/ArmCartJog.TcPOU:38（2处）|G:120|
|G.arm_cjog_owned|BOOL|—|ArmCartJog [W] P/ArmCartJog.TcPOU:32（3处）|ArmCartJog [R] P/ArmCartJog.TcPOU:101（2处）<br>ArmCartesian [R] P/ArmCartesian.TcPOU:93（1处）<br>ArmManual [R] P/ArmManual.TcPOU:74（2处）|G:121|
|G.arm_cjog_state|UDINT|0空闲，1使能，2运动，3减速，4释放|ArmCartJog [W] P/ArmCartJog.TcPOU:33（6处）|PLC侧未见字面读取|G:122|
|G.arm_cjog_error|UDINT|—|ArmCartJog [W] P/ArmCartJog.TcPOU:34（9处）|ArmCartJog [R] P/ArmCartJog.TcPOU:49（5处）|G:123|
|G.arm_cjog_limited|BOOL|—|ArmCartJog [W] P/ArmCartJog.TcPOU:35（2处）|PLC侧未见字面读取|G:124|
|G.arm_cjog_path_speed|LREAL|—|ArmCartJog [W] P/ArmCartJog.TcPOU:36（2处）|PLC侧未见字面读取|G:125|
|G.arm_cjog_if_version|UDINT := 1|—|PLC侧未见字面写/调用|PLC侧未见字面读取|G:126|
|G.axis2_sync_axis|AXIS_REF|—|Axis2Sync [RW/Axis] P/Axis2Sync.TcPOU:46（2处）<br>Axis2Sync [RW/FB] P/Axis2Sync.TcPOU:19（1处）|Axis2Sync [R] P/Axis2Sync.TcPOU:21（4处）|G:130|
|G.inject_axis|ARRAY[1..2] OF AXIS_REF|—|InjectorManual [RW/Axis] P/InjectorManual.TcPOU:30（3处）<br>MAIN [RW/FB] P/MAIN.TcPOU:41（1处）|InjectorManual [R] P/InjectorManual.TcPOU:74（1处）|G:134|
|G.inject_push_req|ARRAY[1..2] OF BOOL := [FALSE,FALSE]|—|InjectorManual [W] P/InjectorManual.TcPOU:25（1处）|InjectorManual [R] P/InjectorManual.TcPOU:36（2处）|G:135|
|G.inject_pull_req|ARRAY[1..2] OF BOOL := [FALSE,FALSE]|—|InjectorManual [W] P/InjectorManual.TcPOU:26（1处）|InjectorManual [R] P/InjectorManual.TcPOU:36（2处）|G:136|
|G.cylinder1_value|WORD|电缸1控制值 (安装在夹持机构上)|MAIN [W] P/MAIN.TcPOU:152（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:150（2处）|PLC侧未见字面读取|G:139|
|G.cylinder2_value|WORD|电缸2控制值 (安装在轴1机构上)|MAIN [W] P/MAIN.TcPOU:153（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:151（2处）|PLC侧未见字面读取|G:140|
|G.cylinder3_value|WORD|电缸3控制值 (安装在轴5机构上)|MAIN [W] P/MAIN.TcPOU:154（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:152（2处）|PLC侧未见字面读取|G:141|
|G.cylinder4_value|WORD|电缸4控制值 (安装在轴6机构上)|MAIN [W] P/MAIN.TcPOU:155（1处）<br>SelfCheck [W] P/SelfCheck.TcPOU:153（2处）|PLC侧未见字面读取|G:142|
|G.cylinder5_value|WORD|电缸5控制值 (安装在轴3机构上控制y阀)|SelfCheck [W] P/SelfCheck.TcPOU:114（1处）<br>handle [W] P/handle.TcPOU:476（2处）|PLC侧未见字面读取|G:143|
|G.cylinder5_cmd|WORD := 2000|历史兼容缓存，当前正式控制不写此变量|SelfCheck [W] P/SelfCheck.TcPOU:115（1处）|PLC侧未见字面读取|G:144|
|G.cylinder5_press_req|BOOL := FALSE|Y阀请求（TRUE=打开，输出500；FALSE=关闭，输出2000）|SelfCheck [W] P/SelfCheck.TcPOU:116（1处）|handle [R] P/handle.TcPOU:475（1处）|G:145|
|G.ft_1_value|INT|—|PLC侧未见字面写/调用|PLC侧未见字面读取|G:148|
|G.fn_1_value|INT|—|PLC侧未见字面写/调用|PLC侧未见字面读取|G:149|
|G.fn_2_value|INT|—|PLC侧未见字面写/调用|PLC侧未见字面读取|G:150|
|G.ft_2_value|INT|—|PLC侧未见字面写/调用|PLC侧未见字面读取|G:151|
|G.estop_diag_count|UDINT|—|handle [W] P/handle.TcPOU:231（1处）|handle [R] P/handle.TcPOU:231（1处）|G:156|
|G.estop_diag_host_timeout|BOOL|—|handle [W] P/handle.TcPOU:208（1处）|PLC侧未见字面读取|G:157|
|G.estop_diag_axis|ARRAY[1..7] OF ST_AxisStatus|—|handle [W] P/handle.TcPOU:210（1处）|PLC侧未见字面读取|G:158|
|G.estop_diag_power|ARRAY[1..7] OF ST_McOutputs|—|handle [W] P/handle.TcPOU:211（1处）|PLC侧未见字面读取|G:159|
|G.estop_diag_reset|ARRAY[1..7] OF ST_McOutputs|—|handle [W] P/handle.TcPOU:212（1处）|PLC侧未见字面读取|G:160|
|G.estop_diag_power_enable|ARRAY[1..7] OF BOOL|—|handle [W] P/handle.TcPOU:213（1处）|PLC侧未见字面读取|G:161|
|G.estop_diag_power_positive|ARRAY[1..7] OF BOOL|—|handle [W] P/handle.TcPOU:214（1处）|PLC侧未见字面读取|G:162|
|G.estop_diag_power_negative|ARRAY[1..7] OF BOOL|—|handle [W] P/handle.TcPOU:215（1处）|PLC侧未见字面读取|G:163|
|G.estop_diag_return_state|ARRAY[1..7] OF INT|—|handle [W] P/handle.TcPOU:216（1处）|PLC侧未见字面读取|G:164|
|G.estop_diag_return_cmd|ARRAY[1..7] OF ST_AxisPlannedReturnCmd|—|handle [W] P/handle.TcPOU:217（1处）|PLC侧未见字面读取|G:165|
|G.estop_diag_move_error|ARRAY[1..7] OF BOOL|—|handle [W] P/handle.TcPOU:218（1处）|PLC侧未见字面读取|G:166|
|G.estop_diag_move_aborted|ARRAY[1..7] OF BOOL|—|handle [W] P/handle.TcPOU:219（1处）|PLC侧未见字面读取|G:167|
|G.estop_diag_enable_error|ARRAY[1..7] OF BOOL|—|handle [W] P/handle.TcPOU:221（1处）|PLC侧未见字面读取|G:168|
|G.estop_diag_disable_error|ARRAY[1..7] OF BOOL|—|handle [W] P/handle.TcPOU:223（1处）|PLC侧未见字面读取|G:169|
|G.estop_diag_move_error_id|ARRAY[1..7] OF UDINT|—|handle [W] P/handle.TcPOU:220（1处）|PLC侧未见字面读取|G:170|
|G.estop_diag_enable_error_id|ARRAY[1..7] OF UDINT|—|handle [W] P/handle.TcPOU:222（1处）|PLC侧未见字面读取|G:171|
|G.estop_diag_disable_error_id|ARRAY[1..7] OF UDINT|—|handle [W] P/handle.TcPOU:224（1处）|PLC侧未见字面读取|G:172|
|G.estop_diag_act_pos|ARRAY[1..7] OF LREAL|—|handle [W] P/handle.TcPOU:225（1处）|PLC侧未见字面读取|G:173|
|G.estop_diag_act_vel|ARRAY[1..7] OF LREAL|—|handle [W] P/handle.TcPOU:226（1处）|PLC侧未见字面读取|G:174|
|G.estop_diag_feed_pos|ARRAY[1..7] OF LREAL|—|handle [W] P/handle.TcPOU:227（1处）|PLC侧未见字面读取|G:175|
|G.estop_diag_feed_vel|ARRAY[1..7] OF LREAL|—|handle [W] P/handle.TcPOU:228（1处）|PLC侧未见字面读取|G:176|
|G.estop_diag_refer_abs|ARRAY[1..7] OF LREAL|—|handle [W] P/handle.TcPOU:229（1处）|PLC侧未见字面读取|G:177|
|G.rslimit_distance|ARRAY[1..7] OF LREAL := [99.0, 0.0, 666.0, 0.0, 688.0, 688.0, 0.0]|—|PLC侧未见字面写/调用|err [R] P/err.TcPOU:101（1处）<br>handle [R] P/handle.TcPOU:299（1处）|G:180|
|G.recovery_wait_timeout|TIME := T#2S|—|PLC侧未见字面写/调用|handle [R] P/handle.TcPOU:256（3处）|G:181|
|G.recovery_max_retries|UINT := 3|—|PLC侧未见字面写/调用|err [R] P/err.TcPOU:99（1处）<br>handle [R] P/handle.TcPOU:269（5处）|G:182|
|G.err_backoff_distance|LREAL := 2.0|—|PLC侧未见字面写/调用|err [R] P/err.TcPOU:102（2处）|G:183|
|G.err_backoff_velocity|LREAL := 5.0|—|PLC侧未见字面写/调用|err [R] P/err.TcPOU:78（1处）|G:184|
|G.err_backoff_acc|LREAL := 50.0|—|PLC侧未见字面写/调用|err [R] P/err.TcPOU:79（1处）|G:185|
|G.axis15_reset_pulse|TIME := T#200MS|—|PLC侧未见字面写/调用|Axis2Sync [R] P/Axis2Sync.TcPOU:31（1处）|G:186|
|G.axis15_reset_interval|TIME := T#2S|—|PLC侧未见字面写/调用|Axis2Sync [R] P/Axis2Sync.TcPOU:24（1处）|G:187|
|G.return_clear_error_id|UDINT := 16#80005001|—|PLC侧未见字面写/调用|handle [R] P/handle.TcPOU:613（1处）|G:188|

## C.7 现有记录和时序统计

调查日期：2026-11-12；路径相对仓库根。只读取既有记录，没有启动数据端或连接 PLC。统计脚本：`docs/arch_survey_r1/scratch/analyze_existing_records.py:1`；完整统计：`docs/arch_survey_r1/scratch/existing_records_stats.json:1`。分位数采用 nearest-rank，median 为常规定义；单位在表内列明。

### C.7.1 四套历史 session

**已确认**：仓库当前运行工程的 records 目录共找到以下四套 session；全部 `utc_end=null`、`stop_reason=null`、`metrics_available=false`、`baseline_available=false`。它们没有可用的会话结束 ADS 统计，JSON 中的 null 不能解释为 0。当前记录器只在 final 写结束时间、结束原因，RTT 和计数也有可用性条件；因此“这些文件没有正常写出最终汇总”是**推断**，“为什么没有写完”是**未知**。证据：`D/x64/Release/records/20261102/165217_auto/session.json:6`、`D/records/20261108/092851_auto/session.json:6`、`D/records/20261108/120537_auto/session.json:6`、`D/records/20261110/084418_auto/session.json:6`、下表所列 ADS 元数据行；`介入机器人/B数据端/工程/ADS项目/experiment_recorder.cpp:884`、`介入机器人/B数据端/工程/ADS项目/experiment_recorder.cpp:907`、`介入机器人/B数据端/工程/ADS项目/experiment_recorder.cpp:934`、`介入机器人/B数据端/工程/ADS项目/experiment_recorder.cpp:947`、`介入机器人/B数据端/工程/ADS项目/experiment_recorder.cpp:951`、`介入机器人/B数据端/工程/ADS项目/experiment_recorder.cpp:1018`。

| 编号 | session 目录（前缀均为 `介入机器人/B数据端/工程/ADS项目/`） | schema、UTC 开始 | 力/运动行数 | 数据有效性 | 原始证据 |
|---|---|---|---:|---|---|
| R1 | `x64/Release/records/20261102/165217_auto/` | 6；2026-11-02 08:52:17.015Z | 261 / 130 | sample_valid/calibrated_valid/position_valid与两手柄有效位均0 | `D/x64/Release/records/20261102/165217_auto/session.json:2`、`D/x64/Release/records/20261102/165217_auto/session.json:5`、`D/x64/Release/records/20261102/165217_auto/session.json:23`；同目录 `D/x64/Release/records/20261102/165217_auto/force.csv:1`、`D/x64/Release/records/20261102/165217_auto/force.csv:262`，`D/x64/Release/records/20261102/165217_auto/motion.csv:1`、`D/x64/Release/records/20261102/165217_auto/motion.csv:131` |
| R2 | `records/20261108/092851_auto/` | 7；2026-11-08 01:28:51.091Z | 476,992 / 238,464 | sample_valid/calibrated_valid/position_valid均0；两个handle_valid 各54,905行1、183,559行0 | `D/records/20261108/092851_auto/session.json:2`、`D/records/20261108/092851_auto/session.json:5`、`D/records/20261108/092851_auto/session.json:24`；同目录 `D/records/20261108/092851_auto/force.csv:1`、`D/records/20261108/092851_auto/force.csv:476993`，`D/records/20261108/092851_auto/motion.csv:1`、`D/records/20261108/092851_auto/motion.csv:238465` |
| R3 | `records/20261108/120537_auto/` | 7；2026-11-08 04:05:37.984Z | 169 / 85 | sample_valid/calibrated_valid/position_valid与两手柄有效位均0 | `D/records/20261108/120537_auto/session.json:2`、`D/records/20261108/120537_auto/session.json:5`、`D/records/20261108/120537_auto/session.json:24`；同目录 `D/records/20261108/120537_auto/force.csv:1`、`D/records/20261108/120537_auto/force.csv:170`，`D/records/20261108/120537_auto/motion.csv:1`、`D/records/20261108/120537_auto/motion.csv:86` |
| R4 | `records/20261110/084418_auto/` | 8；2026-11-10 00:44:18.291Z | 2,203 / 1,101 | sample_valid/calibrated_valid/position_valid与两手柄有效位均0；新增4列 ff 输出全0 | `D/records/20261110/084418_auto/session.json:2`、`D/records/20261110/084418_auto/session.json:5`、`D/records/20261110/084418_auto/session.json:24`；同目录 `D/records/20261110/084418_auto/force.csv:1`、`D/records/20261110/084418_auto/force.csv:2204`，`D/records/20261110/084418_auto/motion.csv:1`、`D/records/20261110/084418_auto/motion.csv:1102` |

表内原始证据路径为避免超宽省略同一工程前缀；完整路径同时保存在 `docs/arch_survey_r1/scratch/existing_records_stats.json:1`。**已确认**：脚本扫描了全部 CSV 行而非只看文件头；四套所有力原始/清零/清洁力/扭矩列及七轴位置列都没有有限数值（NaN）；R4 的四列反馈输出为有限0。证据：`docs/arch_survey_r1/scratch/analyze_existing_records.py:43`（完整 reader）、`docs/arch_survey_r1/scratch/analyze_existing_records.py:45`（有效位计数）、`docs/arch_survey_r1/scratch/analyze_existing_records.py:50`（逐列数值扫描）；各原始 CSV 引用见上表。

### C.7.2 可统计的记录间隔（不是 main 周期）

**已确认**：以下数值是 CSV 相邻 `elapsed_us` 相减后转换为 ms 的分布。R1–R4 都声明力100Hz、运动50Hz；当前代码逐 ADS 记录快照写 force，仅选择偶数 attempt_sequence 写 motion，并将 `record_snapshot.qpc_ticks` 传给记录器。故它们至多可用于描述文件内记录的时间间隔；不能当作主循环 wait→publish 整拍耗时，也不能将间隔中的约1s空洞直接定性为线程丢拍。证据：`D/x64/Release/records/20261102/165217_auto/session.json:9-10`、`D/records/20261108/092851_auto/session.json:9-10`、`D/records/20261108/120537_auto/session.json:9-10`、`D/records/20261110/084418_auto/session.json:9-10`；`介入机器人/B数据端/工程/ADS项目/main.cpp:4608`、`介入机器人/B数据端/工程/ADS项目/main.cpp:4615`、`介入机器人/B数据端/工程/ADS项目/main.cpp:4616`、`介入机器人/B数据端/工程/ADS项目/main.cpp:4650`、`介入机器人/B数据端/工程/ADS项目/main.cpp:4653`；`介入机器人/B数据端/工程/ADS项目/experiment_recorder.cpp:711`、`介入机器人/B数据端/工程/ADS项目/experiment_recorder.cpp:739`。历史 schema 对应旧版源代码的确切构建哈希未记录，故旧文件具体采样实现是**未知**。

| 会话/文件 | 相邻间隔数 | 最小ms | 中位数ms | p95 ms | p99 ms | 最大ms | 均值ms | 原始时间覆盖（首/末elapsed，s） |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| R1 force | 260 | 0.006 | 10 | 10 | 10.053 | 1012.126 | 17.562 | 0.789781 / 5.355776 |
| R1 motion | 129 | 15.979 | 20 | 20 | 1010.873 | 1012.132 | 35.318 | 0.789797 / 5.345776 |
| R2 force | 476,991 | 0.007 | 10 | 10.258 | 13.610 | 1894.202 | 19.710 | 0.809929 / 9402.325303 |
| R2 motion | 238,463 | 0.241 | 20 | 20.280 | 1013.594 | 1894.225 | 39.423 | 0.809952 / 9401.685303 |
| R3 force | 168 | 0.014 | 10 | 10.004 | 10.059 | 1015.527 | 15.893 | 0.002088 / 2.672088 |
| R3 motion | 84 | 14.459 | 20 | 20.028 | 1015.541 | 1015.541 | 31.786 | 0.002088 / 2.672088 |
| R4 force | 2,202 | 0.011 | 10 | 10.003 | 10.594 | 1021.647 | 19.446 | 0.409329 / 43.229240 |
| R4 motion | 1,100 | 10.771 | 20 | 20.003 | 1015.819 | 1021.665 | 38.918 | 0.409355 / 43.219238 |

统计依据是上表对应全部 CSV 行；计算实现 `docs/arch_survey_r1/scratch/analyze_existing_records.py:23`、`docs/arch_survey_r1/scratch/analyze_existing_records.py:44`、`docs/arch_survey_r1/scratch/analyze_existing_records.py:64`，机器可读统计 `docs/arch_survey_r1/scratch/existing_records_stats.json:1`。不报告主循环周期分布，原因不是“没有任何日志”，而是所找到日志不含每轮主循环的起止时间，也未保存可识别的成功 ADS 快照序列。

### C.7.3 三份既有 ADS 诊断日志

| 文件 | 总行数/CONNECT行数 | 具体失败 | 有 RTT 字段 | 确定程度/证据 |
|---|---:|---|---:|---|
| `介入机器人/B数据端/工程/ADS项目/x64/Release/ads_fault_diagnostics.log` | 3 / 3 | 每行 local192.168.0.160.1.1:851 read-state错误6；fallback169.254.119.135.1.1:851错误1861；timeout1000ms | 0 | **已确认**：`介入机器人/B数据端/工程/ADS项目/x64/Release/ads_fault_diagnostics.log:1,3`；扫描脚本 `docs/arch_survey_r1/scratch/analyze_existing_records.py:84` |
| `介入机器人/B数据端/工程/ADS项目/x64/Debug/ads_fault_diagnostics.log` | 4,687 / 4,687 | 每行同上两项错误与1000ms | 0 | **已确认**：`介入机器人/B数据端/工程/ADS项目/x64/Debug/ads_fault_diagnostics.log:1,4687`；扫描脚本 `docs/arch_survey_r1/scratch/analyze_existing_records.py:84` |
| `ADS项目/ADS项目/x64/Debug/ads_fault_diagnostics.log` | 40 / 40 | 前8行同上；后32行包含 AdsPortOpenEx failed | 0 | **已确认**：`ADS项目/ADS项目/x64/Debug/ads_fault_diagnostics.log:1,8,9,40`；扫描脚本 `docs/arch_survey_r1/scratch/analyze_existing_records.py:84` |

**已确认**：这里的 `tick_ms` / `failure_tick_ms` / `seq` 是失败事件序列；日志里没有正常成功通信周期的 RTT 序列，不能将它的约2s重试间隔称为 ADS 往返耗时。`timeout_ms=1000` 是请求配置值，也不是实测 RTT。各文件原始行见上表；完整文件哈希和分类计数保存在 `docs/arch_survey_r1/scratch/existing_records_stats.json:1`。

### C.7.4 四项时序问题的结论

| 要求 | 本轮结果 | 确定程度与缺失材料 |
|---|---|---|
| 主循环周期分布 | 未取得；上表仅记录文件间隔 | **未知**：需要包含每拍主循环起止时刻或等价测量的日志；当前CSV头没有这些字段，见 `D/records/20261110/084418_auto/motion.csv:1`、`D/records/20261110/084418_auto/force.csv:1`（其余三套文件头见C.7.1） |
| 正常 ADS 往返时间分布 | 未取得 | **未知**：session RTT 为null，三份diagnostic只有CONNECT失败；需要成功快通道的RTT样本；`D/x64/Release/records/20261102/165217_auto/session.json:28`、`D/records/20261108/092851_auto/session.json:29`、`D/records/20261108/120537_auto/session.json:29`、`D/records/20261110/084418_auto/session.json:29`、各diagnostic上述行 |
| 快照缺失次数 | 未取得；不能把所有 position_valid=0 直接算成缺失次数 | **未知**：没有最终 ADS 计数/基线；session failed_cycles和snapshot_queue字段为null；例如 `介入机器人/B数据端/工程/ADS项目/records/20261110/084418_auto/session.json:30`、`介入机器人/B数据端/工程/ADS项目/records/20261110/084418_auto/session.json:37` |
| 一次换手各阶段实际耗时 | 未取得 | **未知**：R2/R3/R4仅有events表头（`D/records/20261108/092851_auto/events.csv:1`、`D/records/20261108/120537_auto/events.csv:1`、`D/records/20261110/084418_auto/events.csv:1`），R1无events文件，位置全无效；需要带planned_return/dual_handoff阶段时间和对应PLC反馈的完整实际记录 |

**已确认**：仓库 `介入机器人/docs/`、`验证记录/` 还有大量明确为 `target=offline` 的注入故障日志，本轮离线测试也可生成虚拟时钟换手周期数。它们证明模型在指定输入下的行为，不是上述四项现场时序实测；例如 `docs/arch_survey_r1/scratch/tests_native/ads_diagnostics_test.log:1`、`docs/arch_survey_r1/scratch/tests/v5_st/results.log:2`。未将这些数值混入实际时序分布。
