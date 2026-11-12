from pathlib import Path
import csv
import re

ROOT = Path(__file__).resolve().parents[3]
DOC = ROOT / 'docs/arch_survey_r1/B_main解剖.md'
ADS = ROOT / '介入机器人/B数据端/工程/ADS项目'
text = DOC.read_text(encoding='utf-8-sig')
for before, after in {
    'control_types.cpp:32-107': 'control_types.cpp:29-92',
    '342-489': '334-476',
    'handle_net_distance.h:7-48': 'handle_net_distance.h:7-35',
    'return_recovery.h:14-71': 'return_recovery.h:14-70',
    'guidewire_mode.cpp:61-93,133-152': 'guidewire_mode.cpp:64-113,130-149',
    '#### B.11.3.1  ': '#### B.11.3.1 ',
    '#### B.11.3.2  ': '#### B.11.3.2 ',
    '#### B.11.3.3  ': '#### B.11.3.3 ',
}.items():
    text = text.replace(before, after)

text = text.replace('| `cancel_clear_confirmed`, `completion_clear_confirmed`, `cancel_snapshot_sequence`, `cancel_t0_ms`, `cancel_clear_retry_after_ms`, `cancel_timeout_reported`, `cancel_rebased` | 取消/完成清 Req 和故障取消过程。 | `main.cpp:664-703,2998-3076`。 | `control_types.h:328-337` |',
    '| `completion_clear_confirmed` | 完成清Req的确认；cancel可依据此字段避免重复清。 | 写 `main.cpp:682,2896,3019`；读 `main.cpp:687,4482`。 | `control_types.h:317` |\n'
    '| `cancel_clear_confirmed`, `cancel_snapshot_sequence`, `cancel_t0_ms`, `cancel_clear_retry_after_ms`, `cancel_timeout_reported`, `cancel_rebased`, `cancel_hold_generation` | 当前生产源码仅声明并随整结构reset恢复默认；没有按字段的main读写，不能把字段名当成仍在运行的取消子状态机。 | 当前main取消流程为 `main.cpp:664-703`；这些声明本身在 `control_types.h:316,318-323`。 | 已确认 |')
text = text.replace('每轴补偿参数；默认值在 header。', '每轴补偿参数；默认kp=0.5、ki=2、max_gain=2、max_error_mm=20。')
text = text.replace('| `AdsCommunicationService` | 其 snapshot/output/notification 线程内部原子字段每次实际改写顺序、队列覆盖策略。 | 需结合 `ads_communication.cpp` 全函数调用和现场时序；本表只依据公开 API/头文件。 |',
    '| `AdsCommunicationService` | **未知**：一次真实断连中快照/通知/重连线程的实际交错和到达延迟。队列策略属于可读静态实现，见A的线程/通信表；main仅通过API消费（`main.cpp:969-981,4522`）。 | 需要设备隔离的回放或现场带源时间的通信日志。 |')
text = text.replace('| `ZenohService` | 网络回调线程与 worker 的每次 lease/队列字段交错顺序。 | 需逐函数审阅 `zenoh_service.cpp` 或运行回环测试；main 只看 `poll_command`/publish。 |',
    '| `ZenohService` | **未知**：真实网络下回调/worker/main命令消费的实际交错和端到端延迟。main的消费位置为 `main.cpp:4868-5402`，静态线程边界见A。 | 需要发送端/接收端共同序号和时钟对齐的回环记录。 |')
text = text.replace('| `DeliveryTrackingController` | 每个 `AxisRuntime.snapshot` 字段最终被 UI/状态读取的时间点。 | 需结合 `delivery_tracking.cpp` 与 `main.cpp:4766-4865` 的状态填充。 |',
    '| `DeliveryTrackingController` | **已确认**：main约30Hz分支在 `main.cpp:4766-4769` 复制snapshot并于 `main.cpp:4864` 发布；`snapshot()`只返回对象副本（`delivery_tracking.cpp:342-345`）。**未知**的是网络之后UI实际接收/绘制的时间。 | 确认端到端显示延迟需主端接收/绘制时间记录。 |')
text = text.replace('| 物理键位 | `spacing_recovery_button_mask=0x07` 的 bit0/1/2 与手柄外壳丝印/固件键定义。 | 现有源码只能证明位掩码条件，不能证明三个位对应哪些物理键。 |',
    '| 物理键位 | **未知**：`spacing_recovery_button_mask=0x07`的bit0/1/2与手柄外壳丝印/固件键定义；代码只证明587上 `(buttons&0x07)==0x07`（`main.cpp:1505-1513`、`control_types.h:150-151`）。 | 需要固件按键定义或隔离手柄的按键采样记录。 |')

def flow(name):
    if name.startswith(('diagnostic_', 'serial_', 'hardcoded_', 'vis_publish_rate_', 'ads_snapshot_wait_')):
        return '诊断/固定连接标识/节流常量'
    if name in {'cfg', 'cyl'}:
        return '运行配置/电缸预设（字段见A及B.11.7）'
    if name.startswith(('homing_', 'plc_application_')) or name == 'control_before_plc_restart':
        return '寻参复用/PLC重启'
    if name.startswith(('emergency_',)):
        return '紧急回撤'
    if name.startswith(('planned_', 'dual_handoff', 'interrupted_return_', 'original_return_', 'resuming_return', 'return_', 'pending_return_')):
        return '计划回退/双手柄换手/故障恢复'
    if name.startswith(('spacing_', 'pending_mode_', 'vis_reverse_')):
        return '屈曲恢复/模式覆盖'
    if name.startswith(('axis6_soft_', 'axis6_soft_limit_')):
        return '轴6软限位'
    if name.startswith(('axis4_', 'injector_', 'manual_jog_', 'y_valve_', 'cylinder_manual_')):
        return '手动点动/租约/耦合/电缸覆盖'
    if name.startswith(('pending_startup', 'pending_selfcheck_', 'selfcheck_', 'startup')) or name in {'self_check_done', 'has_self_check_flag', 'last_self_check_done'}:
        return '启动/自检'
    if name.startswith(('estop_', 'next_estop_')):
        return '急停保持/恢复请求节流'
    if name.startswith(('force_', 'remote_force_', 'cal_', 'clean_force_')) or name == 'ff':
        return '力标定/力反馈/诊断'
    if name.startswith(('tracking_',)):
        return 'tracking门控/PI/欠账'
    if name in {'ads', 'ads_communication', 'arm_manual_ads', 'zenoh_service'}:
        return '后台通信服务（内部状态见A/D）'
    if name in {'handle_velocity_path', 'handle_velocity_recorder', 'experiment_recorder', 'drained_ads_snapshots', 'handle_record_history', 'recording_info', 'vis_qpc_frequency', 'next_vis_publish_qpc', 'loop_count'}:
        return '记录/30Hz状态发布'
    if name in {'run_directory', 'homing_record_path'}:
        return '输出路径/启动常量'
    if name.startswith(('handle_', 'remote_single_', 'single_handle_', 'axis1_handle_', 'axis6_handle_', 'axis1_input_', 'axis6_input_', 'initial_sync_', 'plc_app_', 'connection_', 'ads_soft_', 'handled_', 'plc_restart_')):
        return '输入拓扑/连接保持/重建基准'
    if name.startswith(('axis1_delivery_mapping_',)):
        return '轴1先行映射'
    return 'Follow/镜像/窗口/同步（含ctx别名；语义见B.11.2）'

inventory = list(csv.DictReader((DOC.parent / 'scratch/main_outer_inventory.tsv').open(encoding='utf-8-sig'), delimiter='\t'))
append = [
    '', '### B.11.6 main长驻声明逐项类型与词法引用索引', '',
    f'**已确认**：本表逐项列出{len(inventory)}个长生命周期声明（含只读常量、对象/数组，以及循环内唯一可变static）。声明位置、显式赋值、取址和其它引用由 `scratch/state_inventory.py` 从当前源码生成并人工核对；它是B.11.2语义表的精确类型补充。初始化包含在显式赋值列中。', '',
    '“其它引用”不能自动等同于读取：方法调用可能改对象，传引用也可能间接写入。取址列同样只是词法命中，位运算也可能出现`&`；真正的别名/间接写入以B.11.1–B.11.4及所引模块为准。下面每个数字都属于`main.cpp`，括号内是完整的`文件:行号`引用。常量没有后续写入不构成遗漏。', '',
    '| 名称 | 精确声明类型 | 声明 | 显式赋值/自增（词法） | 其它引用；取址候选（词法） | 所属流程 |',
    '|---|---|---|---|---|---|',
]

def cite(value):
    return '—' if value == '—' else '`main.cpp:' + value + '`'

for row in inventory:
    append.append('| `{name}` | `{type}` | `main.cpp:{definition}` | {write} | {read}；取址 {addr} | {flow} |'.format(
        **row, write=cite(row['explicit_assignment_lines']), read=cite(row['other_reference_lines']), addr=cite(row['address_escape_lines']), flow=flow(row['name'])))

append += ['', '### B.11.7 状态结构体全部数据声明及类型', '',
    '**已确认**：下表保留各数据声明的类型、实际字段名和默认值；同一声明中的逗号字段具有同一类型。行为、写入入口、读取入口见B.11.2–B.11.4。本表的“写/读范围”指该组对象实现范围，不表示范围内每一行都写/读每个字段。方法本身、每拍重建的局部输出帧/条件结构体不属于跨周期保存量。后台服务对象内部的线程/锁/队列字段由A/D列出，main不直接操作这些私有字段。', '',
    '| 对象/流程 | 数据声明（逐项名称/类型/默认值） | 声明来源 | 写入入口/读取范围 |',
    '|---|---|---|---|',
]

groups = [
    ('ControlConfig（配置）', 'control_types.h', [(80,200)], '构造；运行写仅 `cfg.axis1_post_return_lead_mm` 与 `cfg.startup_motion_speed_scale`（`main.cpp:5246,5305`）；读见B.11.2/A配置表'),
    ('CylinderPreset（只读预设）', 'control_types.h', [(205,217)], '构造后main无写；自动夹爪/换手/启动读取（`main.cpp:2042-2045,2594-2635,3325-3388,3514-3617`）'),
    ('HandleFilterState（两柄）', 'control_types.h', [(54,57)], '`update/reset`写（`control_types.h:59-75`、`main.cpp:1252-1257,1461-1470`）；增量读（`main.cpp:2090-2129`）'),
    ('PlannedReturnLeg（两腿）', 'control_types.h', [(272,287)], 'begin/refresh/step/reset（`main.cpp:2283-2340,2471-3076`）'),
    ('PlannedReturnCoordinator', 'control_types.h', [(297,335)], 'begin/step/cancel/output generation（`main.cpp:554-703,2471-3076,4542-4582`）；未使用cancel字段见B.11.3.2'),
    ('DualHandoffRequest（含original副本）', 'control_types.h', [(375,381)], '触边写/实际到位读（`main.cpp:3859-3924`）；original副本写/续做读（`main.cpp:2512-2520,3628-3663`）'),
    ('SpacingRecoveryState', 'control_types.h', [(397,415)], '按键/阶段写读（`main.cpp:1549-1575,3215-3388,4300-4353`）；reset（`control_types.h:422-435`）'),
    ('CrawlState（两轴）', 'control_types.h', [(460,467)], '同步写（`motion_sync.cpp:111-338`）；Follow/触边读（`main.cpp:3078-3213,3723-4227`）'),
    ('ForceFeedbackState::HandoffHold（两侧）', 'control_types.h', [(477,484)], 'capture/complete/observe/reset（`control_types.h:545-627`）；blend读写（`force_feedback.cpp:76-112`）'),
    ('ForceFeedbackState::HoldSample（两侧）', 'control_types.h', [(489,492)], '平滑更新（`force_feedback.cpp:63-73`）；换手capture读（`main.cpp:2521-2524`）；reset（`control_types.h:551-559`）'),
    ('ForceFeedbackState', 'control_types.h', [(496,497),(523,543)], 'enable/reset/capture等方法（`control_types.h:501-627`）；计算读写（`force_feedback.cpp:22-148`）'),
    ('ForceSampleFrame（main缓存）', 'control_types.h', [(632,644)], 'ADS快照转采样（`plc_io.cpp:159-225`）；main取力/门控/记录/状态读取（`main.cpp:4401-4865,5056-5068`）'),
    ('StartupState', 'control_types.h', [(659,692)], '启动/重启/命令（`main.cpp:381-420,1359-1376,3390-3626,5290-5356`）；启动模块（`startup_sequence.cpp:19-185`）'),
    ('AppContext（指针为别名，三项为自有值）', 'control_types.h', [(703,760)], '指针绑定（`main.cpp:189-214,297-326`）；自有元数据写（`plc_io.cpp:210-215,241-251`）；速度读（`main.cpp:3888-3889`）'),
    ('ForceCalibrationConfig（配置）', 'force_calibration.h', [(30,63)], '构造后仅gravity_comp_enabled命令写（`main.cpp:5364-5377`）；校准读取（`force_feedback.cpp:28-33`）；值见A配置表'),
    ('ForceCalibrationState', 'force_calibration.h', [(68,73)], '取零写（`main.cpp:759-794`）；标定/输出/记录读（`main.cpp:4442-4459,4598-4599,4622-4637`）'),
    ('ReturnRecovery（私有）', 'return_recovery.h', [(64,69)], '只经公开方法改写（`return_recovery.h:17-62`）；main API见B.11.3.3'),
    ('HandleNetDistance（私有，两轴）', 'handle_net_distance.h', [(29,34)], 'sample/set_following写（`handle_net_distance.h:9-25`）；net_mm读（`main.cpp:4851-4852`）'),
    ('DeliveryTrackingParameters（两轴）', 'delivery_tracking.h', [(45,49)], 'set_parameter写（`delivery_tracking.cpp:28-87`）；can_enable/PI读（`delivery_tracking.cpp:94-117,223-281`）'),
    ('DeliveryTrackingAxisSnapshot（AxisRuntime内）', 'delivery_tracking.h', [(55,86)], '控制器方法写/复制（`delivery_tracking.cpp:128-344,357-470`）；main约30Hz读取（`main.cpp:4766-4769`）'),
    ('DeliveryTrackingController::AxisRuntime（两轴私有）', 'delivery_tracking.h', [(154,164)], '控制器方法写读（`delivery_tracking.cpp:128-331,357-470`）；main不直接访问'),
    ('DeliveryTrackingController（私有）', 'delivery_tracking.h', [(186,189)], '参数/开关/段号由API写（`delivery_tracking.cpp:20-126,411-429`）；main不直接访问'),
    ('Handle（两柄）', '手柄/Handle.h', [(18,26),(55,63)], 'init/poll/clear_cache/close写（`手柄/手柄.cpp:19-132`）；main通过公开采样及API读（`main.cpp:1079-1200,1461-1509`）'),
    ('homing::Record', 'homing_record.h', [(15,18)], '读取文件构造（`homing_record.h:40-60`）；main新坐标记录写（`main.cpp:1949-1968`）；复用读取（`main.cpp:1410-1456`）'),
    ('PendingStartupParams（main本地结构）', 'main.cpp', [(919,925)], '写/读见B.11.2.3（`main.cpp:931-935,5260-5305`）'),
    ('HandleRecordSnapshot（deque元素）', 'main.cpp', [(945,947)], '插入/过期淘汰（`main.cpp:1472-1484`）；按源QPC读（`main.cpp:4659-4674`）'),
]

declaration_count = 0
for label, filename, ranges, usage in groups:
    lines = (ADS / filename).read_text(encoding='utf-8-sig').splitlines()
    for start, end in ranges:
        for i in range(start, end + 1):
            line = lines[i-1].split('//',1)[0].strip()
            if not line or not line.endswith(';') or ('(' in line and not ('=' in line and line.index('=') < line.index('('))) or line.startswith(('return ', 'void ', 'bool active(')):
                continue
            if not re.match(r'^(?:const\s+)?(?:bool|double|DWORD|ULONGLONG|int|short|unsigned|long|std::|[A-Z]\w*|USERData)', line):
                continue
            declaration_count += 1
            append.append(f'| {label} | `{line}` | `{filename}:{i}` | {usage} |')

append += ['', f'上表按源码物理声明行收录{declaration_count}项数据声明；一行多个字段保持原声明，不把成员方法算作状态。配置字段也收录，以免“长驻但通常不变”与“每拍局部量”混淆。**已确认**：`PlannedReturnCoordinator`的七个`cancel_*`保留字段没有生产代码逐字段使用，`completion_clear_confirmed`有使用；对应范围与例外已在B.11.3.2列明。', '']
marker = '\n### B.11.6 main长驻声明逐项类型与词法引用索引'
if marker in text:
    text = text[:text.index(marker)]
DOC.write_text(text.rstrip()+'\n'+'\n'.join(append), encoding='utf-8')
print(f'Added {len(inventory)} main declarations and {declaration_count} member declaration rows to B only.')
