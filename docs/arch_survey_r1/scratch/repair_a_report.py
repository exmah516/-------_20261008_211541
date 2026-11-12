"""Finalize the engineering inventory. Writes only this survey's A attachment."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT/'docs/arch_survey_r1'
DOC = OUT/'A_工程全貌.md'
text = DOC.read_text(encoding='utf-8-sig')

def replace_row(prefix, new):
    global text
    rows = text.splitlines()
    hits = [i for i,s in enumerate(rows) if s.startswith(prefix)]
    assert len(hits) == 1, (prefix, hits)
    rows[hits[0]] = new
    text = '\n'.join(rows)+'\n'

text = text.replace('# A 工程全貌（主端与跨端协议边界）', '# A 工程全貌')
text = text.replace(
    '本文只描述现状；“已确认”来自源码/文档直接证据',
    '本文只描述现状；除逐项注明“推断/未知”或未运行外，本附件事实表各行及源码布局/协议说明均标为**已确认**；“已确认”来自源码/文档直接证据')
if '| `D/` |' not in text:
    anchor = '| `B数据端/` | `介入机器人/B数据端/` |'
    assert anchor in text
    text = text.replace(anchor, anchor+'''
| `C从端/` | `介入机器人/C从端/` |
| `D/`、`ADS项目/` | `介入机器人/B数据端/工程/ADS项目/` |
| `K/`、`PositioningArmKinematics/` | `介入机器人/B数据端/工程/PositioningArmKinematics/` |
| `P/`、`POUs/` | `介入机器人/C从端/250902/Untitled2/POUs/` |
| `Untitled2/`、`250902.tsproj` | 分别为 `介入机器人/C从端/250902/Untitled2/`、`介入机器人/C从端/250902/250902.tsproj` |
| A.10以后未加前缀的C++源/头及`ADS.vcxproj` | 一律相对于`D/`；`BuildAndTest.ps1`相对于`K/` |
'''.rstrip())

replace_row('| ADS快线程 |', '| ADS快线程 | 100Hz目标；每次读取完整快照并写快输出。 | 快照队列容量2048，互斥/条件变量；最新输出用独立mutex及generation交换；失败进入SoftHold，持续1s进入Reconnecting。 | `D/ads_communication.cpp:15-21,205-216,923-1005`；快照队列`D/ads_communication.h:322-326`，planned队列/结果容量32/128另见`D/ads_communication.h:279-281`。 |')
if '| 实验记录stop_worker |' not in text:
    anchor = next(s for s in text.splitlines() if s.startswith('| 手柄速度记录sampler |'))
    text = text.replace(anchor, anchor+'\n| 实验记录stop_worker | 按需，stop_async状态转换成功时创建；非周期线程。 | 等三个writer清队列/停止后写最终session.json，stop_and_wait会join；线程创建失败在调用方同步收尾。 | `D/experiment_recorder.h:228`、`D/experiment_recorder.cpp:568-611`。 |')

replace_row('| tracking参数 |', '| tracking参数 | Axis1/Axis6 Kp=.5、Ki=2、max_gain=2、max_error=20；数据端保留按字段校验的内部VisCommand处理。 | 当前公共Zenoh枚举及WPF命令注册没有tracking入口；不能把保留处理分支当作操作者可用配置。 | `D/delivery_tracking.h:29-50`、`D/delivery_tracking.cpp:28-82`、`D/control_messages.h:144-199`、`D/protocol/cpp/zenoh_protocol.h:14-24`。 |')
replace_row('| ZenohServiceConfig |', '| ZenohServiceConfig | endpoint=`tcp/192.168.60.1:7447`、token_path=`remote.token`、router_path空、start_router=true；startup_g=13、axis6 soft limit=660。 | main用默认构造，并复制cfg的g/soft limit后调用`zenoh_service.start(remote_cfg)`。 | `D/zenoh_service.h:17-25`、`D/main.cpp:106-115`；完整字段见A.15.5。 |')
replace_row('| ADS连接 |', '| ADS连接 | CADSComm构造默认20ms；快服务连接阶段改1000ms、运行改300ms；完整成功硬窗1s，1s重连。优先本地AMS，失败回退`169.254.119.135.1.1:851`，本地也固定端口851。 | 源码常量；main:62的hardcoded_ads_netid未被调用使用；main:98的两个nullptr只是坐标缓存初始化参数。 | `D/ADSComm.cpp:10-13,815-831,872-911`、`D/ads_communication.cpp:1015-1067,2072-2075`、`D/ads_communication.h:189`。 |')
replace_row('| `handle_velocity_<GetTickCount64>.csv` |', '| `handle_velocity_<GetTickCount64>.csv` | 100Hz | 13列：elapsed_ms、rx582_ms、valid582、rx587_ms、valid587；SDK接收速度4列；相邻关节样本差分/接收间隔得到的关节变化率4列，SDK速度单位未在代码注明。 | exe旁目录；`D/main.cpp:66-69,87-91`、`D/handle_velocity_recorder.h:22-40,64-94`。 |')

start = text.index('### A.12.1 ')
end = text.index('## A.13 ', start)
text = text[:start]+'''### A.12.1 100Hz快速读

**已确认**：本组由ADS快线程解析句柄后每拍Sum Read；所有索引按PLC 1-based展开。表内`[1..7]`表示全部七轴，`[1/3/5/6]`表示四个指定轴。下列“写方”说明正常生产责任，不把NC库内部行为当作显式POU赋值。来源：`D/ads_communication.cpp:1255-1320,1406-1460`。

| 符号/类型 | 数据端读取时机和意义 | 谁写、何时写（PLC） | 直接出处 |
|---|---|---|---|
| `TwinCAT_SystemInfoVarList._TaskInfo[1].CycleCount`、`DcTaskTime`；DWORD/ULINT缓存 | 每拍；CycleCount在Sum首尾各读一次；用于跨扫描/停滞诊断 | TwinCAT运行时按任务扫描更新 | `D/ads_communication.h:44-45`、`D/ads_communication.cpp:1255-1256,1424-1425,1459-1460,1515-1550` |
| `G.axis[1..7].NcToPlc.ActPos`；7×LREAL | 优先直接NC绝对位置，减init/left得到两个坐标 | NC/AXIS_REF回读；MAIN先调用ReadStatus | `D/ads_communication.cpp:1310-1320,1426-1435`；`P/MAIN.TcPOU:35-40` |
| `G.Act_pos`；7×LREAL | 若任一直接NC句柄缺失，用整数组回退相对位置 | handle/SelfCheck从NC重建，等待态可能不刷新 | `D/ads_communication.cpp:1257,1275-1279,1433-1435`；`P/handle.TcPOU:489-491`；`D/main.cpp:1402-1408` |
| `G.axis[1/3/5/6].NcToPlc.ActVelo`；4×LREAL | 每拍，换手停稳/恢复门 | NC/AXIS_REF | `D/ads_communication.cpp:1260-1263,1437-1440` |
| `G.ft_1_value`、`fn_1_value`、`fn_2_value`、`ft_2_value`（均带`G.`）；4×INT | 每拍，原始力/扭矩ADC，后按1000 counts/V换算 | EtherCAT输入Box7/8；非数据端写 | `D/ads_communication.cpp:1264-1267,1441-1444`；`介入机器人/C从端/250902/250902.tsproj:7630-7688` |
| `G.estop_hold_req`、`G.host_comm_timeout`；BOOL | 每拍，运动/力门控；也订阅通知 | handle/err/clear_err/reset的急停路径；MAIN/handle会话心跳/停稳恢复路径 | `D/ads_communication.cpp:1268-1269,1445-1446`；`P/MAIN.TcPOU:68-104`、`P/handle.TcPOU:108-136,321-466`、`P/err.TcPOU:32-48` |
| `G.init_pos`、`G.leftlimit`；各7×LREAL | 每拍；初始化和请求坐标刷新时也读 | init/SelfCheck寻参建立；MAIN接受homing_restore时复制 | `D/ads_communication.cpp:1258-1259,1447-1448`；`P/init.TcPOU:38-42`、`P/SelfCheck.TcPOU:118-567`、`P/MAIN.TcPOU:149-151` |
| `G.return_cmd[1/6].Req`、`Busy`；各BOOL | 每拍，恢复/取消观察；仅Req/Busy不能独立确认回退Done | Req由数据端Prepare/Commit/Clear写，PLC故障/复位亦会清；Busy由handle回退状态更新 | `D/ads_communication.cpp:1294-1300,1449-1452`；`P/handle.TcPOU:250-319,579-709` |
| `G.handle_reinit_done`；BOOL | 每拍且通知，重新允许Follow的资格 | handle重新初始化完成/保持路径写 | `D/ads_communication.cpp:1296,1453`；`P/handle.TcPOU:108-136,321-466` |
| `G.err_backoff_active`、`G.err_backoff_failed`；各7×BOOL | 每拍，故障恢复与续做门 | err POU限位退离流程 | `D/ads_communication.cpp:1296,1454-1455`；`P/err.TcPOU:32-263` |
| `G.axis2_sync_error`/`error_id`；BOOL/UDINT | 每拍，同步轴故障门 | Axis2Sync每扫描按Gear/Reset故障锁存和清理 | `D/ads_communication.cpp:1297,1456-1457`；`P/Axis2Sync.TcPOU:19-89` |
| `G.self_check_done`；BOOL | 每拍且通知，自检/寻参资格 | init/SelfCheck置位/清零；MAIN寻参复用可置位 | `D/ads_communication.cpp:1297-1298,1458`；`P/SelfCheck.TcPOU:548-567`、`P/MAIN.TcPOU:149-156` |

### A.12.2 100Hz快速写

**已确认**：数据端main生产`AdsOutputCommand`，快线程执行Sum Write。每拍实际子项由motion_enabled/dirty/mask及planned队列决定，心跳/会话每次追加。PLC不保证一个Sum子项列表在同一扫描原子生效，代码也明确不作此保证：`D/ads_communication.cpp:1752-1761`。

| 符号/类型 | 谁写、何时写（数据端） | PLC使用/回写方 | 出处 |
|---|---|---|---|
| `G.host_session_id`、`G.host_heartbeat_sequence`；UDINT | ADS快线程每拍追加，会话在服务连接生命周期保持，heartbeat递增 | MAIN观察变化喂2s看门狗；handle处理恢复 | `D/ads_communication.cpp:1619-1635,1757-1761`；`P/MAIN.TcPOU:68-104` |
| `G.host_recover_req`；BOOL | watchdog_recovery_pending.exchange后写true；不每拍true | PLC停稳后清req并开始重初始化 | `D/ads_communication.cpp:1759-1761`；`P/handle.TcPOU:321-466` |
| `G.refer`；7×LREAL | main本拍pos，motion_enabled时快写 | handle读取相对init目标，平滑/限速/软限位；自检/急停路径也可重建refer | `D/main.cpp:4471-4494`、`D/ads_communication.cpp:1636-1648`；`P/handle.TcPOU:489-571` |
| `G.axis1_fast_return`、`G.axis6_fast_retract`；BOOL | 随motion输出写；从enabled变disabled补false | handle平滑/限速分支；PLC初始化亦清 | `D/ads_communication.cpp:1638-1647`；`P/handle.TcPOU:491-545` |
| `G.cylinder1_value`…`G.cylinder4_value`；UINT | discrete_dirty且cylinder_valid时按4-bit mask写对应项 | 映射物理电缸输出；PLC自检/回退/急停路径也写，非单写所有权 | `D/ads_communication.cpp:1650-1660`；`P/handle.TcPOU:250-319,321-466`；`介入机器人/C从端/250902/250902.tsproj:7630-7688` |
| `G.cylinder5_press_req`；BOOL | 离散dirty时随同批写 | handle按true=500/false=2000映射cylinder5_value | `D/ads_communication.cpp:1662`；`P/handle.TcPOU:475-479` |
| `G.axis4_fwd_req`、`G.axis4_rev_req`；BOOL | 离散dirty时写；main按方向/500ms租约门控 | handle轴4状态机，冲突/超时路径清理 | `D/ads_communication.cpp:1663-1664`、`D/main.cpp:1609-1639`；`P/handle.TcPOU:706-884` |
| `G.inject_push_req`、`G.inject_pull_req`；各2×BOOL | 离散dirty时写；每个注射器方向经500ms租约 | InjectorManual每拍读；host超时清请求 | `D/ads_communication.cpp:1665-1666`；`P/InjectorManual.TcPOU:23-109` |
| `G.startup_smoothing_bypass`；BOOL | 离散dirty时写启动旁路 | handle平滑分支，PLC初始化/故障清理也改 | `D/ads_communication.cpp:1667`；`P/handle.TcPOU:491-545` |
| `G.return_cmd[1/6].TargetAbs/Velocity/Acc/Dec/Jerk`；LREAL | 专用planned队列Prepare：先Req=false，填两腿参数；一拍至多取一个排队命令 | handle请求锁存→MC_MoveAbsolute | `D/ads_communication.cpp:1692-1701`；`P/handle.TcPOU:579-644` |
| `G.return_cmd[1/6].Req`；BOOL | Prepare=false；Commit=true；Clear=false；PublishHandoff/AwaitHandoffApplied的planned_return_clear_mask另可带false | handle按Req/Busy/Done/Error阶段执行/收尾，故障可清 | `D/ads_communication.cpp:1692-1750`；`P/handle.TcPOU:579-709` |

### A.12.3 ADS通知（全部20项）

**已确认**：注册类型`ADSTRANS_SERVERONCHA`、cycle参数原值10000、maxDelay=0；回调更新受锁事件缓存与事件序号；注册后主动Sum Read同20项初值。10000的物理单位未由本轮在线验证。出处：`D/ads_communication.cpp:1090-1138,1907-1960`。

| 符号 | 类型 | 谁写、何时更新（PLC） |
|---|---|---|
| `G.self_check_done`、`G.selfcheck_status` | BOOL、DINT | init/SelfCheck与MAIN接受复用/新自检时；`P/MAIN.TcPOU:120-180`、`P/SelfCheck.TcPOU:118-567` |
| `G.handle_reinit_req`、`G.handle_reinit_done` | BOOL各一 | 自检/急停恢复请求与handle初始化完成；数据端同步后清req；`P/handle.TcPOU:108-136,321-466`、`D/plc_io.cpp:672-675` |
| `G.estop_hold_req`、`G.host_comm_timeout` | BOOL各一 | 急停/失使能/心跳超时与停稳恢复；`P/MAIN.TcPOU:68-104`、`P/handle.TcPOU:108-136,321-466` |
| `G.startup_loading_ready` | BOOL | SelfCheck完成置true，数据端开始控制前消费false；`P/SelfCheck.TcPOU:548-567`、`D/plc_io.cpp:358-360` |
| `G.axis4_manual_busy`、`done`、`error`、`error_id`（完整前缀均`G.axis4_manual_`） | BOOL×3、UDINT | handle轴4手动状态每扫描；`P/handle.TcPOU:706-884` |
| `G.gen_state` | state/INT16缓存 | MAIN/init/SelfCheck/handle/err/clear_err/reset状态切换；`P/MAIN.TcPOU:120-203`，全部PLC写方逐项见C.6.2 |
| `G.return_cmd[1].Busy/Done/Error/ErrorId` | BOOL×3、UDINT | handle轴1回退状态，每扫描按FB/阶段更新；`P/handle.TcPOU:579-709` |
| `G.return_cmd[6].Busy/Done/Error/ErrorId` | BOOL×3、UDINT | handle轴6回退状态，同上；`P/handle.TcPOU:579-709` |

### A.12.4 低频、初始化和兼容读写

**已确认**：本组经单独AMS端口的低频请求队列；不是每100Hz快事务都写。API调用方timeout参数不构成排队上限，见`D/ads_communication.cpp:559-586,2044-2225`。

| 符号/类型 | 谁读/写、何时 | PLC对端/用途 | 证据 |
|---|---|---|---|
| `TwinCAT_SystemInfoVarList._AppInfo.AppName`；数据端char[64] | 连接初始化读；用于识别应用重载和寻参记录校验 | TwinCAT运行时 | `D/ads_communication.cpp:1070-1077` |
| 通知20项（上表） | 每次连接注册后主动读一次 | 避免等待下一次变量变化 | `D/ads_communication.cpp:1090-1138` |
| `G.init_pos`、`G.leftlimit`；7×LREAL各一 | 连接初始化读；寻参完成/复用后请求低频刷新；此外每拍也快读 | init/SelfCheck/MAIN写基准 | `D/ads_communication.cpp:1391-1403,2078-2103` |
| `G.v_limit`；7×LREAL | startup/emergency先read备份、write缩放，完成/取消再write还原 | handle读取每扫描位置增量上限；PLC init给默认 | `D/plc_io.cpp:336-344`、`D/startup_sequence.cpp:138-165,236-248`、`D/main.cpp:427-467` |
| `G.startup_loading_ready`；BOOL | read封装生产从通知取值；write用于消费资格false | SelfCheck置true | `D/plc_io.cpp:347-360`、`D/main.cpp:5325-5329` |
| `G.handle_reinit_req`；BOOL | main成功同步后低频write=false | PLC自检/恢复置true，handle判断资格 | `D/plc_io.cpp:672-675`、`D/main.cpp:1911-1985` |
| `G.selfcheck_target_from_left`、`G.selfcheck_start_req`；7×LREAL、BOOL | PreparePosition转换的内部命令校验通过后先写target，再写start=true；不自动重试失败的启动 | MAIN自检请求校验/消费，SelfCheck执行 | `D/main.cpp:4881-4924`、`P/MAIN.TcPOU:164-180` |
| `G.power_output[1..7].Done/Error`、`G.reset_output[1..7].Error`、`G.axis[1..7].Status.Error/DriveDeviceError`；35×BOOL | 寻参复用前read_sum检查七轴健康 | MAIN FBD MC输出/NC状态 | `D/main.cpp:1380-1398`；`P/MAIN.TcPOU:207-2438,2439-3809` |
| `G.axis[1..7].NcToPlc.ActPos`；7×LREAL | 健康门通过后复用前单独低频读，不能使用等待态未刷新的Act_pos | NC实际绝对位置 | `D/main.cpp:1401-1409` |
| `G.homing_restore_leftlimit`、`G.homing_restore_init_pos`；各7×LREAL；`G.homing_restore_status` DINT；`G.homing_restore_req` BOOL | main先写两坐标数组和status=0，成功后单独req=true | MAIN复用校验后复制、设置状态并清req | `D/main.cpp:1420-1426`、`P/MAIN.TcPOU:124-162` |
| `G.homing_restore_status`、`G.homing_restore_reject_reason`；DINT各一 | request提交后每拍低频读，status!=0才结束等待 | MAIN写接受1/拒绝状态与原因 | `D/main.cpp:1436-1455`、`P/MAIN.TcPOU:124-162` |
| `G.return_cmd[1/3/5/6].Busy/Done/Error/ErrorId` | 兼容plc_io read封装，缓存100ms；当前协调器主要用新快照+通知 | handle写回退状态 | `D/plc_io.cpp:394-437` |
| `G.return_cmd[1/3/5/6].Req/TargetAbs/Velocity/Acc/Dec/Jerk` | 兼容prepare/commit/clear封装；axis1组清Req一次清1/3/5/6 | 当前正常计划运动腿只有1/6，3/5符号不等于本轮它们在执行回退 | `D/plc_io.cpp:439-652`、`D/main.cpp:664-703` |
| `G.refer`、四电缸值、`G.startup_smoothing_bypass`、`G.axis4_fwd_req/rev_req` | 存在低频写封装；main正常每拍输出走快通道，启动/同步/退出可调用封装 | handle与自检/停稳对端同上 | `D/plc_io.cpp:330-385,664-669`；当前调用关系见B.2/B.11 |
| `G.Act_pos`、`G.init_pos`、`G.leftlimit`、四轴ActVelo、四力输入 | plc_io保留无ads_service时直连read_sum分支；生产main绑定服务后读最新快照 | 兼容路径不另构成生产100Hz线程 | `D/plc_io.cpp:197-327`、`D/main.cpp:197-206` |

### A.12.5 定位臂慢通道：全部符号族

**已确认**：专用worker目标50ms，dirty改变才写；读状态每轮进行。5轴数组是PLC的1..5。PLC读写方为ArmManual/ArmCartesian/ArmCartJog及它们的FB。下面列出动态生成的Power/Reset字段，避免只列一个总称遗漏实际符号。

| 符号 | 数据端读写方/时机 | PLC侧职责 | 证据 |
|---|---|---|---|
| `G.arm_manual_enable`、`G.arm_enable_req`、`G.arm_reset_req`、`G.arm_jog_pos_req`、`G.arm_jog_neg_req` | worker根据desired_* dirty写；读manual/enable回显；Reset成功后清本地脉冲 | ArmManual读请求，做Power/Reset/MoveVelocity/Stop，host timeout时清方向 | `D/arm_manual_ads_service.cpp:19-27,621-633,648-655,678-679`、`P/ArmManual.TcPOU:30-254` |
| `G.arm_jog_velocity`、`G.arm_jog_acc`、`G.arm_jog_dec`、`G.arm_jog_jerk` | 参数dirty时写5维数组；每次snapshot读回；速度先于方向项加入sum | ArmManual和Cartesian读取运动参数 | `D/arm_manual_ads_service.cpp:624-633,680-683` |
| `G.arm_act_pos`、`G.arm_act_vel`、`G.arm_motion_busy`、`G.arm_motion_done`、`G.arm_motion_error`、`G.arm_motion_error_id`、`G.arm_cmd_dir`、`G.arm_cmd_conflict` | worker每50ms读 | ArmManual/NC每扫描更新 | `D/arm_manual_ads_service.cpp:28-35,684-691`、`P/ArmManual.TcPOU:30-254` |
| `G.arm_power_output[1..5].Done/Busy/Active/Error/ErrorID`、`G.arm_reset_output[1..5].Done/Busy/Active/Error/ErrorID` | 每次snapshot动态展开50个BOOL/UDINT读项 | ArmManual的MC_Power/MC_Reset输出 | `D/arm_manual_ads_service.cpp:37-84,703-715` |
| `G.arm_curve_data`（17×LREAL）、`G.arm_curve_cancel`、`G.arm_curve_request`、`G.arm_curve_heartbeat` | 程序归零先data+cancel=false，成功后单独request；运行每轮heartbeat递增；abort写cancel=true | ArmCartesian在request变化时锁存曲线，500ms监测heartbeat | `D/arm_manual_ads_service.cpp:319-321,348-350,383-398`、`P/ArmCartesian.TcPOU:46-269` |
| `G.arm_curve_ack`、`G.arm_curve_state`、`G.arm_curve_error`、`G.arm_curve_owned`、`G.arm_curve_progress` | 每50ms读，推进请求ACK/执行/完成/错误 | ArmCartesian每扫描写 | `D/arm_manual_ads_service.cpp:353-364,692-696`、`P/ArmCartesian.TcPOU:46-269` |
| `G.arm_cjog_mode`、`G.arm_cjog_dir`、`G.arm_cjog_speed`、`G.arm_cjog_alive` | dirty时一起写；mode非0则alive递增；输入租约到期清mode/dir/speed | ArmCartJog读、500ms保活；ArmCartJogGen生成p/v/a | `D/arm_manual_ads_service.cpp:483-508,608-638`、`P/ArmCartJog.TcPOU:29-146` |
| `G.arm_cjog_owned`、`G.arm_cjog_state`、`G.arm_cjog_error`、`G.arm_cjog_limited`、`G.arm_cjog_path_speed`、`G.arm_cjog_if_version` | 每50ms读；if_version期望1，仅日志警告不作为强制版本闸门 | ArmCartJog更新状态/所有权与诊断 | `D/arm_manual_ads_service.cpp:697-724`、`P/ArmCartJog.TcPOU:29-146` |

**已确认（边界）**：`plc_io.cpp`还声明`G.act_pos_from_left`、`G.refer_from_left`、`G.cylinder5_cmd`、`G.cylinder5_value`等名字，但当前生产C++未见这些常量的实际ADS读写调用；不能把符号声明等同接口正在使用。声明证据`D/plc_io.cpp:14-53`，实际封装为`D/plc_io.cpp:197-677`。完整PLC所有150项变量、初始化值及PLC读写索引在C.6；其中包含数据端当前不访问的内部状态。

'''+text[end:]

# Clarify ambiguous paths introduced by the appended data/PLC material.
before, after = text.split('## A.10 ', 1)
after = after.replace('`工程/build-and-run.ps1:', '`B数据端/工程/build-and-run.ps1:')
text = before+'## A.10 '+after
text = text.replace('`D/ads_communication.h:189`', '`D/ads_communication.h:186-193`')
text = text.replace('`K/include/positioning_arm/kinematics.h:137-138`', '`K/include/positioning_arm/kinematics.h:129-130`')
text = text.replace('`D/arm_manual_ads_service.cpp:662-663`', '`D/arm_manual_ads_service.cpp:375-376`')

marker = '\n## A.18 数据端模块目录与固定常量'
text = text.split(marker)[0]
text += marker+'''

**已确认**：下列为当前数据端工程到主要文件的目录地图；非生产tests、SDK和输出单列。编译项依据`D/ADS.vcxproj:156-220`，库依据`K/CMakeLists.txt:1-51`。

| 目录（相对于`介入机器人/B数据端/工程/`） | 主要文件 | 用途/输出 |
|---|---|---|
| `ADS项目/` 入口/状态 | main.cpp、control_types.h/.cpp、control_messages.h、return_recovery.h、homing_record.h | 主循环、公共类型、内部命令和可靠性状态 |
| `ADS项目/` 通信 | ADSComm.cpp、ads_communication.h/.cpp、plc_io.h/.cpp、zenoh_service.h/.cpp | CADSComm底层；快/慢服务；PLC符号；Zenoh服务 |
| `ADS项目/protocol/cpp/` | zenoh_protocol.h | MessagePack/HMAC线格式 |
| `ADS项目/手柄/` | Handle.h、手柄.cpp、remote_handle_bridge.h | 远程手柄缓存、采样新鲜度/epoch/静置 |
| `ADS项目/` 运动业务 | motion_sync、guidewire_mode、startup_sequence、delivery_tracking各h/cpp；arm_manual_ads_service.h/.cpp、arm_cartesian_motion.h | 同步/导丝/启动/PI/定位臂 |
| `ADS项目/` 力/记录 | force_feedback.h/.cpp、force_calibration.h、experiment_recorder.h/.cpp、async_csv_writer.h、handle_velocity_recorder.h、handle_net_distance.h | 标定、触觉保持、记录和净距离 |
| `PositioningArmKinematics/include/positioning_arm/`、`src/` | kinematics.h、kinematics.cpp、program_zero.cpp | 数学库；ADS项目直接编译两个源 |
| `ADS项目/ADS/`、`PositioningArmKinematics/.deps/`、`Runtime/VC142/` | ADS SDK/库、Eigen、VC运行库 | 第三方依赖；实际复制见`D/ADS.vcxproj:222-225` |
| `ADS项目/tools/`、`tests/zenoh/`、`PositioningArmKinematics/tests/` | 原生抽取/协议/模型测试及runner | 离线测试，不是现场应用；清单与结果见F |
| `ADS项目/x64/Release`、`Debug` | ADS.exe及依赖DLL、可选token/reliability配置和日志 | 默认生产输出；本轮构建重定向scratch，未启动exe |

以下逐项为**已确认**的固定边界值/协议常量；属于源码配置，改值需重编译。与超时有关的到期动作/恢复条件见D（此处不省略数值）。引用的使用行由同文件标识符匹配给出，既可有读也可有声明；公式中间量不作为额外配置。

| 常量 | 当前表达式 | 声明/引用 |
|---|---|---|
'''
ADS = ROOT/'介入机器人/B数据端/工程/ADS项目'
fixed_files = ['ADSComm.cpp','ads_communication.cpp','ads_communication.h',
    'zenoh_service.cpp','delivery_tracking.h','delivery_tracking.cpp',
    'force_feedback.cpp','plc_io.cpp','return_recovery.h','main.cpp',
    '手柄/Handle.h','手柄/手柄.cpp','手柄/remote_handle_bridge.h',
    'async_csv_writer.h','protocol/cpp/zenoh_protocol.h']
fixed_count = 0
for name in fixed_files:
    lines = (ADS/name).read_text(encoding='utf-8-sig').splitlines()
    for i,s in enumerate(lines,1):
        m = re.match(r'\s*(?:(?:static|inline)\s+)*constexpr\s+[\w:<>, ]+?\s+(\w+)\s*=\s*(.+?);', s)
        if not m or m[1].startswith('kNotify'):
            continue
        field, value = m.groups()
        refs = [str(n) for n,l in enumerate(lines,1) if n != i and re.search(r'\b'+re.escape(field)+r'\b',l.split('//')[0])]
        ref_text = ','.join(refs) if refs else '声明外未检得字面引用'
        value = value.replace('|', r'\|')
        text += f'| `{field}` | `{value}` | `D/{name}:{i}`；同文件使用 {ref_text}。 |\n'
        fixed_count += 1
assert fixed_count >= 35, fixed_count
text += '''

**已确认**：未采用命名常量的固定控制阈值也参与运行：力保持捕获100ms、释放2mm或400ms、blend300ms、扭矩平滑时间常数100ms（`D/control_types.h:540-603`、`D/force_feedback.cpp:21,65-66,93-110`）；屈曲Active dt封顶50ms（`D/main.cpp:3349-3360`）；程序归零snapshot年龄200ms与ACK2s（`D/arm_manual_ads_service.cpp:333-355`）。PLC几何/profile常量见A.15.7；GVL初值和其余POU运动参数、TON等待见C.4/C.6、D.5，均有逐项PLC源码出处。

**未知**：仓库外配置、PLC在线改参和现场启动参数无法由磁盘源码穷尽；本附件没有把软件默认值替代设备实值。具体缺证据项见99。
'''
assert '运行时Zenoh命令可改参数' not in text
assert len(text.splitlines()) > 800
DOC.write_text(text, encoding='utf-8')
print(f'A finalized: {len(text.splitlines())} lines; fixed constants={fixed_count}')
