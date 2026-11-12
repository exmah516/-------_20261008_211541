# C：PLC 已有能力与完整接口（供主报告合并）

路径约定：P/ = 介入机器人/C从端/250902/Untitled2/POUs/；G = 介入机器人/C从端/250902/Untitled2/GVLs/G.TcGVL；T = 介入机器人/C从端/250902/250902.tsproj。本页 P/文件:行号、G:行号、T:行号均按此前缀展开。结论以当前磁盘为准。仅做只读核查。

## 活跃工程、任务和调用顺序

| 确定程度 | 事实 | 出处 |
|---|---|---|
| 已确认 | 系统项目只引用 Untitled2，AmsPort=851，TMC=Untitled2.tmc；Untitled1不属于当前系统实例 | T:3594-3596 |
| 已确认 | PlcTask为1 ms，优先级20，入口MAIN | 介入机器人/C从端/250902/Untitled2/PlcTask.TcTTO:3-9；T:3766-3778 |
| 已确认 | NC SAF任务CycleTime原值10000、优先级4，NC SVB原值50000、优先级8；按现用POU读取_TaskInfo.CycleTime的100ns换算对应1ms/5ms（该NC字段单位仍需TwinCAT工程属性确认） | T:1443-1465 |
| 已确认 | Untitled2实际编译13个POU、2个DUT、1个GVL和1个任务；handle(备份)和move_edge不在编译清单 | 介入机器人/C从端/250902/Untitled2/Untitled2.plcproj:20-70 |
| 已确认 | state枚举含_init=0,_jog1=1,_jog2=2,_jog3=3,_trans=4,_trans_wait=5,_handle=6,_self_check=7,_clear_err=8,_err=9,_reset=10 | 介入机器人/C从端/250902/Untitled2/DUTs/state.TcDUT:4-17 |
| 已确认 | 当前CASE只分发_init、_self_check、_handle、_clear_err、_err、_reset；其它枚举没有业务分支 | P/MAIN.TcPOU:120-203 |

| 每拍次序 | 内容 | 出处 |
|---|---|---|
| 1 | 刷新G.axis[1..7]与G.inject_axis[1..2] | P/MAIN.TcPOU:35-42 |
| 2 | FirstCycle初始化全局状态、清定位臂请求 | P/MAIN.TcPOU:44-66 |
| 3 | 主机session/heartbeat watchdog，2s心跳不变置host_comm_timeout | P/MAIN.TcPOU:68-97 |
| 4 | ArmManual → ArmCartesian → ArmCartJog → Axis2Sync → InjectorManual | P/MAIN.TcPOU:100-104 |
| 5 | 轴6自检负向许可 → POWERED → 非_handle时SETPOSITIONGEN → RESETED | P/MAIN.TcPOU:106-112 |
| 6 | 非_self_check清自检启动标志；CASE分发；保存last_dispatched_state | P/MAIN.TcPOU:114-204 |
| _handle分支 | handle1_()后立刻SETPOSITIONGEN()，使该拍handle写入的Execute用于FB | P/MAIN.TcPOU:198-199 |

以上顺序均为**已确认**。单PLC任务内普通变量共享，不存在源码中的线程锁。是否按此源码下载到现场PLC、实际任务抖动和部署二进制一致性均**未知**，需要在线配置和版本指纹。

## 各POU职责、状态和G接口

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

## PLC计划回退10/20/30/31

return_enable掩码是[T,F,T,F,T,T,F]，只有轴1/3/5/6处理return_cmd；其它轴Req不会处理。**已确认**，P/handle.TcPOU:58-66,574-577。

| 阶段 | 输出 | 等待与转移 | 时间/错误 | 确定程度与出处 |
|---|---|---|---|---|
| 0 | Busy=FALSE，Req为FALSE时清Done/Error | 收到Req且未Done/Error，一次锁存TargetAbs/Velocity/Acc/Dec/Jerk→10 | 无独立超时 | 已确认；P/handle.TcPOU:579-597 |
| 10 | Busy=TRUE；spg_disable_exec=TRUE | Disable.Done或NOT Enabled→20 | 2s；Req清除/Disable.Error/超时→30错误收尾 | 已确认；P/handle.TcPOU:599-627；G:181 |
| 20 | Busy=TRUE；return_exec=TRUE；refer_from_left=目标绝对-leftlimit | MoveAbsolute.Error/CommandAborted→30错误；Done→30成功 | 2s；只认Done、不以接近目标替代 | 已确认；P/handle.TcPOU:629-643 |
| 30 | Busy=TRUE；等待ActVelo≤0.5且return_clear_stop_active=FALSE；清traj_vel/acc | 满足→31 | 没有此阶段计时；下文基准重建条件矛盾 | 已确认；P/handle.TcPOU:645-668 |
| 31 | Busy=TRUE、spg_enable_exec=TRUE | Enable.Done且无Error→0；Busy=FALSE，置Done或Error | 2s；Enable错误最多重试3次；超时直接_err/estop_hold | 已确认；P/handle.TcPOU:670-703；G:181-182 |

重要代码事实：CASE 30内P/handle.TcPOU:652写成IF return_state[i] <> 30 THEN，因而653-663的“锁存当前ActPos、同步refer/ref_slow/act_h/traj_pos”赋值块不会执行。状态30/31前置循环又会用traj_pos冻结refer（P/handle.TcPOU:494-500）。**已确认**条件矛盾；是否产生运动尖峰**未知**，需要实际阶段记录。git blame将该条件归于df776f3，1d1614e只改了停稳条件P/handle.TcPOU:647。

Req被清时用G.return_clear_error_id=16#80005001标识；movement被清会激活MC_Stop，等其Done/Error再释放（P/handle.TcPOU:608-618,922-938；G:188）。state30不设时间上限，Stop在该路径Done或Error均视为清理结束（P/handle.TcPOU:925-929）。

## 平滑、限速、软限位、电缸

| 项目 | 当前动作 | 确定程度与出处 |
|---|---|---|
| 坐标基准 | Act_pos=NC.ActPos-init_pos，from_left=NC.ActPos-leftlimit，refer是相对init_pos | 已确认；P/handle.TcPOU:489-492 |
| 快退旁路 | 轴1/6 fast flag或对应return Busy→avg_window=1、ramp_counter=0、跳过斜坡/均值及二阶滤波 | 已确认；P/handle.TcPOU:502-512,524-547 |
| 斜坡 | 每PLC扫描按G.v_limit对ref_slow作±v_step增量限制；并非代码中除以dt的速度单位转换 | 已确认；P/handle.TcPOU:502,524-532；G:73 |
| 均值 | 缓存20点；首次逐拍增大平均窗，最大20 | 已确认；P/handle.TcPOU:510-516,534-545 |
| 二阶轨迹 | acc=wn²*(refer-traj_pos)-2wn*traj_vel；限amax后以固定0.001s积分；再限vmax | 已确认；P/handle.TcPOU:547-571 |
| 参数 | wn=60；traj_vmax=[80,0,80,0,80,80,0]；traj_amax=[2000,0,2000,0,2000,2000,0]；只在轴1/3/5/6开启 | 已确认；P/handle.TcPOU:95-103 |
| 右软限位 | 仅轴1/3/5/6；right_slimit=leftlimit+rslimit_distance；Actual≥右限且refer增加时冻结refer到ref_slow | 已确认；P/handle.TcPOU:58-60,298-300,518-522；G:180 |
| rslimit | [99,0,666,0,688,688,0] | 已确认；G:180 |
| 最终feed | ext_setpoint_enabled、return_state31或本拍enable时feed；trajectory路径用traj_pos/vel/acc，其余路径用act_h和差分速度 | 已确认；P/handle.TcPOU:893-920 |
| 电缸1..4 | WORD AT %Q*，正常跟随由数据端直接写；SelfCheck/寻参复用时PLC也写初始化值 | 已确认；G:139-143；P/SelfCheck.TcPOU:148-154,541-545；P/MAIN.TcPOU:150-155 |
| 电缸5 | cylinder5_press_req TRUE→500，FALSE→2000；超时分支前RETURN，超时保持上次输出 | 已确认；P/handle.TcPOU:321-466,475-479 |
| 力传感器 | ft_1/fn_1/fn_2/ft_2是INT AT %I*；当前POU不解算滤波 | 已确认；G:148-151；T:3662-3676,7618-7625 |

## 轴4手动

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

## 自检/寻参与复用

- **已确认**：目标平移轴1/3/5/6；扫描与回退速度均40，扫描acc/dec/jerk=50/100/100，回退=100/100/100（P/SelfCheck.TcPOU:17-22,510-528）。
- **已确认**：每轴0禁用→1错开→2扫描→3停稳→4/5清错或55/56重上电→6等所有平移轴达到限位且Power ready→7/71同步SetPos=ActPos→72错开回退→8等Done→10完成（P/SelfCheck.TcPOU:140-448）。
- **已确认**：扫描错开轴3/5/6=300/600/900ms；100ms堵转后备；电子限位停稳后再等300ms（P/SelfCheck.TcPOU:156-181,183-237）。
- **已确认**：Power重新使能脉冲200ms、等待3s，超时循环重试；无最大总次数（P/SelfCheck.TcPOU:285-306）。
- **已确认**：轴6先启动回退并看到Busy/Done/ActVelo>0.5才启动stagger；轴5/3/1从该起点等1/2/3s（P/SelfCheck.TcPOU:388-420）。
- **已确认**：stagger_timer的PT=10s，但实际放行只比较ET与各轴delay，没有把Q作为错误退出条件（P/SelfCheck.TcPOU:129,411）。
- **已确认**：旋转轴2/7回绝对0，轴4保持当前ActPos；前述注释称2/4/7回0与实现不符（P/SelfCheck.TcPOU:60-62,450-507）。
- **已确认**：全7轴stu=10后电缸1/3=1000、2/4=150；self_check_done/startup_loading_ready=TRUE，handle_reinit_req=TRUE；旋转轴2/7 init_pos=0、轴4=current ActPos；刷新参考转_handle（P/SelfCheck.TcPOU:532-567）。
- **已确认**：寻参复用在MAIN的_self_check分支：接收homing_restore_req后等所有外部给定禁用且Power/轴/Reset无错，复制leftlimit/init_pos缓存、设置标准电缸值、刷新参考，置homing_restore_status=1并转_handle；拒绝status=2，reason1=未就绪，2=Disable错误（P/MAIN.TcPOU:124-175）。本分支无独立超时。

## 定位臂、注射器、同步轴

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

## GVL全量分组

完整逐变量表见 scratch/C_GVL全量索引.md（150变量/常量，含类型、默认值、注释、PLC写读POU及行号）；逐行交叉引用见 scratch/plc_gvl_refs.tsv（1169条）。下面的组索引对应全部声明，没有把遗留变量删去。

| 声明行组 | 变量 | 主要写方/读方 |
|---|---|---|
| G:7-15 | err_backoff_active/failed，axis2_sync_error/id，homing_restore_req/leftlimit/init_pos/status/reject_reason | err、Axis2Sync、MAIN；数据端读诊断/写复用请求 |
| G:17-29 | gen_state、axis/power_/power_output/axis_Reset/reset_output/SetPointGenEnable/Disable及outputs、move_jog、enable/disable/flag/flagpos | MAIN FBD；init/SelfCheck/handle/err/reset/clear_err改Execute/Enable；move_jog/flag/flagpos无现用POU读写 |
| G:31-47 | init_pos/refer/Act_pos/act_h/act_hf/leftlimit/from_left；self_check_done/selfcheck_*、startup_loading_ready、handle_reinit_* | 数据端写refer/target/start请求；PLC状态POU写基准和握手；handle读 |
| G:50-58 | estop_hold_req、host_session_id/heartbeat/recover_req/comm_timeout | 数据端写心跳/恢复；MAIN/handle维护timeout；多POU读门控 |
| G:60-69 | fast flags、startup_smoothing_bypass、return_cmd、axis4请求和状态 | 数据端写请求/参数；handle写Busy/Done/Error |
| G:73-83 | v_limit/ref_slow、Kp/Ki/Kd/pid_i/e_prev/i_limit | v_limit数据端可写；handle读写ref_slow；PID数组在现用POU无引用 |
| G:86-126 | 全部arm_*轴对象、请求、参数、输出、曲线、cjog字段 | 数据端写控制字段；ArmManual/Cartesian/CartJog写状态；MC库/NC写AXIS_REF |
| G:130-136 | axis2_sync_axis、inject_axis、inject_push/pull_req | Axis2Sync/InjectorManual/NC；数据端写注射请求 |
| G:139-151 | cylinder1..5_value、cylinder5_cmd/press_req、四路力输入 | 数据端写1..4及press_req；SelfCheck/MAIN初始化；handle映射5；I/O写力输入 |
| G:156-177 | 全部estop_diag_*计数/快照 | handle故障入口写，正常恢复不清；数据端按需读 |
| G:180-188 | rslimit_distance、recovery_wait_timeout/max_retries、err_backoff_*、axis15_reset_*、return_clear_error_id | 常量；handle/err/Axis2Sync读 |

## 与旧文档的冲突

| 当前事实 | 旧描述 | 判定/证据 |
|---|---|---|
| 完整PLC源码/工程存在且系统引用Untitled2 | C从端README称未提供PLC源码 | 已确认README过时；介入机器人/C从端/README.md:5；T:3594 |
| MAIN与handle心跳PT=2s | 接口文档中段称100ms | 已确认旧值残留；P/MAIN.TcPOU:87；P/handle.TcPOU:128；介入机器人/C从端/PLC全局变量与ADS接口.md:117,276,374 |
| Y阀打开输出500 | 接口文档写0 | 已确认代码优先；P/handle.TcPOU:475-479；介入机器人/C从端/PLC全局变量与ADS接口.md:159-168 |
| 轴4自检保持当前位置 | SelfCheck注释写轴2/4/7回绝对0 | 已确认注释不一致；P/SelfCheck.TcPOU:60-62,450-461 |
| CASE30内条件return_state<>30恒假 | 注释和维护说明称在30锁存 | 已确认条件矛盾；P/handle.TcPOU:650-663；介入机器人/C从端/PLC全局变量与ADS接口.md:352 |

未运行TwinCAT、未连接PLC，未对现场下载程序或机械动作作“正常”判定。


