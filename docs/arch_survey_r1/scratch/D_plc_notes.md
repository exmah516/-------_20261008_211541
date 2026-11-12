# D：PLC 全部时间与恢复项

路径约定：P/ = 介入机器人/C从端/250902/Untitled2/POUs/；G = 介入机器人/C从端/250902/Untitled2/GVLs/G.TcGVL。以下所有事实以当前磁盘为准。TON计时在1ms PlcTask内；LREAL年龄以cycle_seconds累加。缺少现场下载版本和实时任务统计，不能据此确认现场时延。

| 项目/位置 | 数值、时钟 | 保护对象 | 到期动作/恢复条件 | 关联与确定程度、证据 |
|---|---|---|---|---|
| MAIN主机watchdog | T#2S，TON | host_session非0且heartbeat不变 | 置G.host_comm_timeout；非_handle状态收到recover且心跳新鲜才清 | 已确认；P/MAIN.TcPOU:68-97 |
| handle主机watchdog | T#2S，TON | 正常业务运动 | 锁存host_timeout_latched；只有心跳新鲜、显式recover且所有受控运动停稳才解除，随后重初始化 | 已确认；P/handle.TcPOU:108-136,321-466 |
| session替换 | 无计时；已有非零session被改变 | 上位机进程接管 | MAIN置timeout；handle锁存timeout；需显式恢复 | 已确认；P/MAIN.TcPOU:69-78；P/handle.TcPOU:112-118 |
| handle初始化 | G.recovery_wait_timeout=2s；TON | 全7轴SetPointGenEnable | 超时_err、estop_hold；EnableError最多3次拉低一拍再试 | 已确认；P/handle.TcPOU:255-317；G:181-182 |
| return state10 | 2s；return_wait[i] TON，切state复位timer | 关闭外部给定 | Req清/DisableError/超时→30错误收尾 | 已确认；P/handle.TcPOU:599-627 |
| return state20 | 2s；return_wait[i] TON | MC_MoveAbsolute完成 | 超时→30错误收尾；FB Error/Aborted同样错误 | 已确认；P/handle.TcPOU:603-643 |
| return state30 | 无独立超时 | 停稳/clear-stop释放 | ActVelo≤0.5且clear_stop inactive→31，否则保持Busy | 已确认；P/handle.TcPOU:645-668 |
| return state31 | 2s；return_wait TON | 重新Enable外部给定 | 超时_err；EnableError最多3次；Done后Busy=FALSE、置Done/Error并回0 | 已确认；P/handle.TcPOU:670-703；G:181-182 |
| return清Req | 无计时；布尔握手 | Done/Error生命周期 | state0且Req=FALSE清Done/Error；Req被清导致未完运动停止并收尾 | 已确认；P/handle.TcPOU:579-597,608-618,922-938 |
| axis4 state10/30/40 | 每阶段2s；axis4_wait TON，状态变化重置 | Disable/Stop/Enable | 10/30超时→40错误收尾；40超时_err；每FB错误最多3次拉低一拍重试 | 已确认；P/handle.TcPOU:731-750,751-885；G:181-182 |
| axis4 state39 | 一完整PLC扫描，1ms配置 | MC_Stop释放 | Execute=FALSE一个完整周期后再启MoveVelocity或SetPointGenEnable | 已确认；P/handle.TcPOU:821-837 |
| init上电watchdog | 5s；TON | 7轴PowerReady | 错误/超时_err；PowerReady后记录init_pos转_self_check | 已确认；P/init.TcPOU:40-82 |
| reset清错 | 3s；TON | 7轴MC_Reset | Done/Error/已无轴错或Q后收尾；残错_err，无错_init | 已确认；P/reset.TcPOU:39-76 |
| clear_err稳定 | 200ms；TON | 无错误且全轴Power | 稳定后handle_reinit_req、转_handle；selfcheck未完成则_init；Power不齐不计时 | 已确认；P/clear_err.TcPOU:43-84 |
| err复位脉冲 | 200ms；TON | MC_Reset.Execute与Power循环 | 拉低形成边沿；随后进入等待阶段 | 已确认；P/err.TcPOU:164-197,215-237 |
| err普通错误retry | 2s；TON | Reset后错误恢复 | 超时回step1再次Reset；错误清且PowerReady、无压限位→clear_err | 已确认；P/err.TcPOU:189-213 |
| err Power-not-ready retry | 3s；TON | MC_Power/DriveDeviceError | 200msPower off/on后等待；超时/残错回step1；无总上限 | 已确认；P/err.TcPOU:215-263 |
| err限位退离 | 无独立PT；最多3次 | 轴1/3/5/6压限位 | 相对退离±2.0，速度5/acc50；Done或Error后再判断；3次仍压限位置failed | 已确认；P/err.TcPOU:67-139；G:181-185 |
| SelfCheck扫描错开 | 轴1=0，轴3=300ms，轴5=600ms，轴6=900ms；TON | 多轴扫描加速 | Q或delay=0→MoveVelocity | 已确认；P/SelfCheck.TcPOU:20-23,148-164 |
| SelfCheck堵转后备 | 100ms；wait_timer TON | 寻左限位，电子限位失效后备 | 速度近0且set/act条件连续成立→记录leftlimit、Stop | 已确认；P/SelfCheck.TcPOU:166-212 |
| SelfCheck限位稳定 | 300ms；wait_timer TON | DriveLimitActive后Power/Disabled延迟 | 停稳后等300ms；轴错→Reset、PowerReady→stu6、其余PowerPulse | 已确认；P/SelfCheck.TcPOU:214-250 |
| SelfCheck Power pulse | 200ms；TON | Power重新初始化 | Enable FALSE一个脉冲后TRUE，进56 | 已确认；P/SelfCheck.TcPOU:285-292 |
| SelfCheck Power wait | 3s；TON | Power恢复 | 超时回55再次脉冲；无最大次数 | 已确认；P/SelfCheck.TcPOU:294-306 |
| SelfCheck回退错开 | 轴6先动，轴5/3/1依次等1/2/3s；stagger_timer PT=10s | 回退启动排序 | 轴6Busy/Done/ActVelo>0.5启动stagger；ET达delay后放行；Q未作失败转移 | 已确认；P/SelfCheck.TcPOU:20-23,129,388-420 |
| SelfCheck其余FB | 没有独立总超时 | Disable/Stop/Reset/SetPosition/MoveAbsolute | 等反馈；Error进入Reset或PowerPulse循环 | 已确认；P/SelfCheck.TcPOU:140-146,214-284,320-448,450-507 |
| 寻参复用 | 无TON或时长上限 | MAIN restore_pending等待禁用 | ready失败立即reject；外部给定禁用完成后复制基准；若一直Enabled且无Error则继续等 | 已确认；P/MAIN.TcPOU:124-175 |
| Axis15 reset pulse | G.axis15_reset_pulse=200ms；TON | Axis15错误 | 首次错误立即Reset脉冲；Q清Execute | 已确认；P/Axis2Sync.TcPOU:23-44；G:186 |
| Axis15 retry interval | G.axis15_reset_interval=2s；TON | Axis15持续错误 | 每2s再触发200msReset；无最大次数；错误清后重建gear状态 | 已确认；P/Axis2Sync.TcPOU:23-44；G:187 |
| Axis15 GearIn | 无独立超时 | 1:1电子齿轮耦合 | 等InGear或Error/CommandAborted；失败锁gear_failed，直到掉使能或错误恢复重置 | 已确认；P/Axis2Sync.TcPOU:57-89 |
| ArmCartesian heartbeat | 0.5s；heartbeat_age+=cycle_seconds | 5轴归零曲线 | allowed=FALSE；运行stage<40转40，错误2003/cancel；heartbeat新鲜+其它ready才可重新启动 | 已确认；P/ArmCartesian.TcPOU:46-70,167-184 |
| ArmCartesian enable/arrival | phase_elapsed>2s，stage10/30；cycle_seconds | Enable/到位等待 | 错误2007、cancel、转40释放 | 已确认；P/ArmCartesian.TcPOU:178-184 |
| ArmCartesian曲线时长 | curve[15]在0.02..120s；elapsed+=cycle_seconds | 曲线执行 | u≥1进入stage30等待到位；无另设总超时 | 已确认；P/ArmCartesian.TcPOU:87-89,186-211 |
| ArmCartesian到位稳定 | 至少2*cycle_seconds；位置≤0.05、速度≤0.1 | 末端曲线完成 | stage30条件满足转40，disable后成功60/错误50→60 | 已确认；P/ArmCartesian.TcPOU:235-266 |
| ArmCartesian释放 | stage40/50/60无独立超时 | Disable/Stop/Busy释放 | Done/Error/轴Error视为已完成，等所有Stop不Busy释放owned | 已确认；P/ArmCartesian.TcPOU:219-266 |
| ArmCartJog alive | ALIVE_TIMEOUT=.5s；alive_age+=cycle_seconds | 末端点动按住保活 | held=FALSE；gen正常减速，零速连续2周期后40；新alive可恢复held，但当前stage释放中不抢占 | 已确认；P/ArmCartJog.TcPOU:5-8,38-49,87-96 |
| ArmCartJog Enable | ENABLE_TIMEOUT=2s；enable_age+=cycle_seconds | 外部给定使能 | 错误3003、fault_stop、stage40 | 已确认；P/ArmCartJog.TcPOU:79-85 |
| ArmCartJog释放 | 40/50/60无独立超时 | Disable/Stop/Busy释放 | Done/Error/轴Error视为完成，等Stop不Busy→owned FALSE | 已确认；P/ArmCartJog.TcPOU:97-146 |
| ArmCartJogGen加减速 | T_ACC=1s、T_DEC=.5s；tau+=cycle_dt | 路径速度连续性 | smoothstep加减速；模式或方向变化先零速再重新锚定 | 已确认；P/ArmCartJogGen.TcPOU:22-34,56-95 |
| ArmManual停止/复位 | 无本地PT | 5轴关节点动 | Stop.Done或ActVel<.01且不Busy退出；Reset.Done/Error清请求；host timeout撤使能和请求 | 已确认；P/ArmManual.TcPOU:35-42,73-103,182-214 |
| InjectorManual停止 | 无本地PT | 注射器按住/换向 | Stop.Done或速度≤.05后0或反向；host timeout清push/pull，但Power始终TRUE | 已确认；P/InjectorManual.TcPOU:23-42,71-109 |

## 谁监视谁（PLC范围）

| 监视方 | 被监视对象 | 异常后影响 | 证据 |
|---|---|---|---|
| MAIN watchdog | 数据端ADS写入session/heartbeat | host_comm_timeout；先于独立Arm/Injector FB调用生效 | 已确认；P/MAIN.TcPOU:68-104 |
| handle watchdog | 数据端心跳/显式recover | 7轴锁存绝对位置；停止快退/axis4；清请求；等待显式恢复 | 已确认；P/handle.TcPOU:321-466 |
| handle轴状态门控 | Power.Done、Axis/Error、ResetError | estop快照，gen_state=_err，重建参考 | 已确认；P/handle.TcPOU:190-253 |
| return/axis4 timer | PLC MC FB握手、运动完成 | 收尾、重试或_err | 已确认；P/handle.TcPOU:599-703,731-885 |
| SelfCheck | 轴限位、速度、PowerReady、MC FB | 建立leftlimit；Stop/Reset/Power循环 | 已确认；P/SelfCheck.TcPOU:140-448 |
| ArmCartesian / ArmCartJog | 专项curve heartbeat/cjog_alive及ArmReady | 500ms保活到期先局部退出，host 2s后全局冻结 | 已确认数值；连锁时序为推断；P/ArmCartesian.TcPOU:56-70；P/ArmCartJog.TcPOU:38-56 |
| Axis2Sync | Axis15Error、业务轴2Power、双方速度 | Reset / GearIn 1:1；不直接切gen_state | 已确认；P/Axis2Sync.TcPOU:19-89 |
| err | 7轴错误/限位/Power | 复位、重上电、局部退离；成功→clear_err | 已确认；P/err.TcPOU:40-263 |

## 数值冲突和连锁事实

- 已确认：MAIN与handle都2s，同拍先MAIN再handle；handle还会把MAIN的timeout锁进自身flag。session变更即使心跳变化也要恢复握手（P/MAIN.TcPOU:68-104；P/handle.TcPOU:112-136）。
- 已确认：接口文档中段100ms是历史残留，当前MAIN/handle均2s；文档末尾也写当前正式工程2s（介入机器人/C从端/PLC全局变量与ADS接口.md:117,276,374）。
- 推断：Arm的500ms保活会先于host 2s watchdog；连接同一数据端时，专项心跳停发但主host心跳继续，可只触发局部退出（P/ArmCartesian.TcPOU:56-70；P/ArmCartJog.TcPOU:38-56）。
- 已确认：return state10/20/31各2s，但30无时长；Disable/Move超时进入30后可能被停稳条件卡住（P/handle.TcPOU:603-677）。
- 已确认：SelfCheck Power 3s超时无限重试；err普通2s/Power3s无限阶段循环；Axis15每2s复位也无总上限。不能把局部超时等同于“有限时间完成恢复”（P/SelfCheck.TcPOU:294-306；P/err.TcPOU:189-263；P/Axis2Sync.TcPOU:23-44）。
- 已确认：stagger_timer虽然有10s PT，程序不以Q判失败（P/SelfCheck.TcPOU:129,411），故不能计为10s总保护。
- 未知：上述超时与数据端专项等待/主端恢复静置数值的跨端碰撞，需与主报告D数据端表合并判断；本笔记不替主端/数据端作结论。

