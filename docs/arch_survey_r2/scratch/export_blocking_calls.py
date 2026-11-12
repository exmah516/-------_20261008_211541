from pathlib import Path
import csv
import re
from collections import Counter

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / '介入机器人/B数据端/工程/ADS项目'
OUT = ROOT / 'docs/arch_survey_r2'
rows = []


def ref(file, line):
    return f'{(SRC / file).as_posix()}:{line}'


def add(file, line, call, basis):
    rows.append({'位置': ref(file, line), '调用': call, '最坏阻塞时长的依据': basis})


def main(line, call, basis):
    phase = '初始化或本线程lambda' if line < 966 else '退出' if line >= 5404 else '主循环'
    add('main.cpp', line, f'[{phase}] {call}', basis)


LF = ('已确认：可能等待且未设主线程等待上界。read/write/read_sum/write_sum 将请求排队，'
      'timeout_ms 被丢弃，随后 request->cv.wait 无超时；完成、断连清失败队列或停止可唤醒。'
      f'依据 {ref("ads_communication.cpp", 559)}、{ref("ads_communication.cpp", 579)}。'
      '后台单次 ADS 的300ms配置不等于排队完成上界。')
LOCK = ('已确认：无超时 mutex/lock_guard；锁竞争和调度的最坏等待未设上界，'
        '本调用只访问本地共享状态，不直接等待网络。')
IO = ('已确认：同步文件/文件系统 I/O；源码未提供完成截止时间，最坏阻塞时长未知。')
JOIN = ('已确认：std::thread::join 等待目标线程退出，未设超时；调度、目标线程中 I/O/锁等待的最坏时长未知。')
CONSOLE = ('已确认：同步控制台/iostream 输出；标准流及其目标（控制台、管道或重定向文件）未设完成截止时间，'
           '最坏阻塞时长未知；std::endl 还会 flush。')
SAMPLE = ('已确认：每个样本都调用 Handle::poll 后 Sleep(10)，正常走完整个循环的请求睡眠总量为 samples×10ms，'
          '不是硬上界；poll 取得本地共享缓存的无超时 mutex，另有调度延迟。'
          f'依据 {ref("control_types.cpp", 29)}、{ref("control_types.cpp", 54)}、'
          f'{ref("手柄/remote_handle_bridge.h", 81)}。')

# 主线程直接调用和包装调用；底层原语另列，二者不可相加成延迟预算。
main(68, 'homing::enabled(reliability_config.json)', IO + f' 读取实现 {ref("homing_record.h", 54)}。')
main(69, 'homing::load(homing_record.json)', IO + f' 读取实现 {ref("homing_record.h", 40)}。')
main(88, 'HandleVelocityRecorder::start', IO + ' 同步打开CSV、写表头并flush，再创建writer和sampler线程；'
     f'{ref("handle_velocity_recorder.h", 22)}、{ref("async_csv_writer.h", 34)}。')
main(98, 'AdsCommunicationService::start', LOCK + ' 还创建两个后台线程；创建失败后会join已创建线程；'
     f'{ref("ads_communication.cpp", 174)}。')
main(102, 'ArmManualAdsService::start', LOCK + ' 持mutex创建worker；'
     f'{ref("arm_manual_ads_service.cpp", 108)}。')
main(111, 'ZenohService::start', '已确认：同步令牌文件I/O、Winsock/BCrypt初始化、可选router启动、z_open/声明实体、创建线程；'
     '源码没有整次start的最坏上界；router循环不能简化为4s总上界；'
     f'{ref("zenoh_service.cpp", 144)}、{ref("zenoh_service.cpp", 231)}、{ref("zenoh_service.cpp", 270)}。')

for line in [117, 118, 1085, 1090, 1120, 1128]:
    main(line, 'Handle::init → remote_handle_bridge::get_sample', LOCK + ' 成功首次接入还调用printf；'
         f'{ref("手柄/手柄.cpp", 19)}、{ref("手柄/remote_handle_bridge.h", 85)}。')
for line in [1144, 1162, 1175]:
    main(line, 'Handle::poll → remote_handle_bridge::get_sample', LOCK +
         f' {ref("手柄/手柄.cpp", 44)}、{ref("手柄/remote_handle_bridge.h", 85)}。')
for line in [1151, 1168, 1181, 5421, 5422]:
    main(line, 'Handle::close → remote_handle_bridge::set_output', LOCK +
         f' {ref("手柄/手柄.cpp", 125)}、{ref("手柄/remote_handle_bridge.h", 106)}。')
for line in [166, 167]:
    main(line, 'apply_force_output内 Handle::setforce_axis → set_output', LOCK +
         f' 所有clear_force_output和正常力输出均经此处；{ref("手柄/手柄.cpp", 140)}、{ref("手柄/remote_handle_bridge.h", 106)}。')

main(767, 'zero_force_sensor_impl: 连续3次 wait_for_snapshot(...,500)',
     '已确认：每次条件变量相对等待请求500ms，成功取得3帧时最多提出3次等待；任一失败立即返回。'
     'CV等待请求合计至多1500ms，另含初始mutex获取、唤醒后重取mutex和线程调度，故整次调用无严格1500ms墙钟上界；'
     f'{ref("main.cpp", 759)}、{ref("ads_communication.cpp", 247)}。')
main(969, 'wait_for_snapshot(...,ads_snapshot_wait_timeout_ms=300)',
     '已确认：CV等待请求300ms；另有snapshot_mutex获取/重取与调度，整次调用不保证300ms硬上界；'
     f'{ref("main.cpp", 56)}、{ref("ads_communication.cpp", 247)}。')
main(5414, 'wait_for_snapshot(...,50)',
     '已确认：退出CV等待请求50ms；另有mutex和调度开销，无整次调用硬上界；'
     f'{ref("ads_communication.cpp", 247)}。')

for line, name in [
    (850, 'read(cylinder原位置,...,100ms)'),
    (1350, 'read(PLC AppName,...,50ms)'),
    (1396, 'read_sum(35个符号，timeout省略=250ms)'),
    (1408, 'read_sum(7个NC绝对位置，timeout省略=250ms)'),
    (1425, 'write_sum(3个homing恢复符号，timeout省略=250ms)'),
    (1426, 'write(homing_restore_req，timeout省略=250ms)'),
    (1442, 'read_sum(2个homing状态符号，timeout省略=250ms)'),
    (1910, 'refresh_coordinates(timeout省略=250ms) → read_sum'),
    (1976, 'refresh_coordinates(timeout省略=250ms) → read_sum'),
    (4914, 'write(selfcheck_target_from_left，timeout省略=250ms)'),
    (4916, 'write(selfcheck_start_req，timeout省略=250ms)')
]:
    main(line, 'AdsCommunicationService::' + name, LF + f' 默认参数 {ref("ads_communication.h", 199)}。')

for line, name in [(433, 'plc_io::read_v_limit'), (448, 'plc_io::write_v_limit'),
                   (461, 'restore_emergency_retract_v_limit内plc_io::write_v_limit'),
                   (3278, 'clear_axis1_group_return_requests'), (3279, 'clear_axis_return_request(axis6)')]:
    main(line, name, LF + f' 经plc_io适配 {ref("plc_io.cpp", 122)}。')

SYNCALL = (SAMPLE + ' sync_all还依次清四轴Req、清axis6 Req、清axis4手动请求、两次写refer；'
           '这些写入均走无超时低频请求等待；两次read_plc_state在当前ctx下仅锁快照。'
           f'依据 {ref("motion_sync.cpp", 406)}。')
for line, samples in [(1332,30),(1932,30),(1980,30),(2018,20),(2037,20),(3440,30),
                       (3502,20),(4315,20),(4537,20),(5336,20)]:
    main(line, f'sync_all({samples})', f'请求Sleep总量{samples*10}ms；' + SYNCALL)
for line, axis in [(1271,6),(1275,1)]:
    main(line, f'motion_sync::sync_axis{axis}(ctx,1,...)', '请求Sleep总量10ms；' + SAMPLE + LF +
         f' 清Req及写refer；{ref("motion_sync.cpp", 343 if axis == 6 else 111)}。')

for line in [383, 976, 1331, 2084]:
    main(line, 'read_plc_state → latest_snapshot', LOCK + ' 当前ctx.ads_service始终非空，读取内存快照；'
         f'{ref("main.cpp", 207)}、{ref("plc_io.cpp", 197)}、{ref("ads_communication.cpp", 258)}。')
main(5064, 'read_force_sample → latest_snapshot', LOCK + ' 当前运行路径读取缓存；'
     f'{ref("plc_io.cpp", 256)}、{ref("ads_communication.cpp", 258)}。')
for line, name, impl in [(979,'drain_snapshots',266),(980,'stats',592),(981,'event_state',586),
                         (4522,'publish_output',278),(5412,'publish_output',278)]:
    main(line, 'AdsCommunicationService::' + name, LOCK + f' {ref("ads_communication.cpp", impl)}。')

main(1788, 'exit_guidewire_mode_to_normal → sync_axis1(ctx,20)',
     '当前公共入口限制见R2-3；内部路径可调用。请求Sleep总量200ms；' + SAMPLE + LF +
     f' {ref("guidewire_mode.cpp", 130)}。')
main(1862, 'enter_independent_guidewire_mode → get_average_handle_pose(...,20)',
     '当前公共入口限制见R2-3；内部路径可调用。请求Sleep总量200ms；' + SAMPLE + LF +
     f' {ref("guidewire_mode.cpp", 64)}。')
for line in [429,1918,3434,5328]:
    main(line, 'restore_startup_v_limit（scaled=true时写ADS）', LF + f' {ref("startup_sequence.cpp", 236)}。')
main(5306, 'start_startup_sequence', '内部ExecuteStartup分支，公共入口限制见R2-3；' + LF +
     ' 可能读取v_limit、写缩放v_limit/夹爪/装卸位标志/平滑旁路/refer，失败清理再写；'
     f'{ref("startup_sequence.cpp", 69)}。')
main(5332, 'consume_startup_loading_ready', LF + f' {ref("startup_sequence.cpp", 53)}。')
for line in [1960,1968]:
    main(line, 'homing::save', IO + ' 临时文件写、flush、close，随后MoveFileExW(MOVEFILE_WRITE_THROUGH)；'
         f'{ref("homing_record.h", 61)}。')
for line in [1236,1745]:
    main(line, 'diagnostic_log ofstream构造/写入/作用域析构关闭', IO + f' 写入紧随此行；{ref("main.cpp", line+1)}。')

main(957, 'ExperimentRecorder::start("auto")',
     IO + JOIN + ' 可能join上次stop线程；创建session目录、打开三个CSV/写表头flush、写session.json并创建线程；'
     f'{ref("experiment_recorder.cpp", 457)}、{ref("experiment_recorder.cpp", 399)}、{ref("experiment_recorder.cpp", 524)}。')
main(5108, 'ExperimentRecorder::start(UI名称)', IO + JOIN +
     f' 同步启动路径与自动start相同；{ref("experiment_recorder.cpp", 457)}。')
for line in [960,4775,5094,5114]:
    main(line, 'ExperimentRecorder::snapshot', LOCK + f' ads_stats_mutex；{ref("experiment_recorder.cpp", 826)}。')
main(4402, 'ExperimentRecorder::update_ads_communication_stats', LOCK +
     f' ads_stats_mutex；{ref("experiment_recorder.cpp", 770)}。')
main(4403, 'ExperimentRecorder::poll_health → 失败时stop_async',
     '已确认：正常健康检查仅原子load；写盘错误时进入stop_async，其mutex/join和线程创建失败同步收尾可能阻塞；'
     f'{ref("experiment_recorder.cpp", 621)}、{ref("experiment_recorder.cpp", 563)}。')
main(5121, 'ExperimentRecorder::stop_async', LOCK + JOIN +
     ' 正常新建stop线程后返回，但若已有stop线程会先join；创建线程失败就在调用线程执行stop_worker '
     '(writer join/flush/最终JSON文件I/O)，因此API名不构成不阻塞保证；'
     f'{ref("experiment_recorder.cpp", 563)}。')
main(5416, 'HandleVelocityRecorder::stop', JOIN + IO +
     ' sampler循环请求sleep_until为10ms节拍，但调度/共享缓存锁与writer排空无整体上界；还输出统计；'
     f'{ref("handle_velocity_recorder.h", 45)}、{ref("async_csv_writer.h", 90)}。')
main(5417, 'ExperimentRecorder::stop_and_wait', JOIN + IO +
     f' 等三个writer排空、flush/close以及最终JSON；{ref("experiment_recorder.cpp", 593)}。')
main(5418, 'ArmManualAdsService::stop', JOIN +
     ' 先取mutex设置stop，再等待worker；worker退出前仍会尝试write_dirty_request/update_cartesian，可能在低频ADS等待。'
     f'{ref("arm_manual_ads_service.cpp", 125)}、{ref("arm_manual_ads_service.cpp", 525)}。')
main(5419, 'AdsCommunicationService::stop', JOIN +
     ' 取lifecycle锁、失败低频排队请求、join两个worker、注销通知；后台同步ADS调用完成时间以及回调退出时间无整体上界。'
     f'{ref("ads_communication.cpp", 228)}、{ref("ads_communication.cpp", 1965)}。')
main(5420, 'ZenohService::stop', JOIN +
     ' 之后z_drop实体/会话、关闭router（终止后WaitForSingleObject请求2000ms）、取state/command锁、清桥缓存；'
     f'{ref("zenoh_service.cpp", 194)}、{ref("zenoh_service.cpp", 311)}。')
main(5404, 'restore_emergency_retract_v_limit', 'scaled=true时才走写入；' + LF)
main(4527, 'restore_emergency_retract_v_limit', 'scaled=true时才走写入；' + LF)
main(5202, 'apply_emergency_retract_v_limit', LF + ' 可能先恢复startup limit，再读/写limit；'
     f'{ref("main.cpp", 427)}。')

for line, name, impl in [(4797,'snapshot',141),(5127,'set_manual_enable',147),(5130,'set_cartesian_jog',236),
                         (5134,'request_program_zero',265),(5137,'stop_cartesian',282),(5140,'keep_cartesian_alive',297),
                         (5144,'set_axis_enable',162),(5148,'request_reset',187),(5152,'set_jog_direction',201),
                         (5160,'set_jog_parameter',408)]:
    main(line, 'ArmManualAdsService::' + name, LOCK +
         f' setter只改dirty/request并唤醒worker；{ref("arm_manual_ads_service.cpp", impl)}。')
for line, name, impl in [(4864,'publish_state',376),(4871,'poll_command',459),(5399,'command_processed',512)]:
    main(line, 'ZenohService::' + name, LOCK + ' state_mutex/command_mutex；本线程不调用z_publisher_put；'
         f'{ref("zenoh_service.cpp", impl)}。')

# 被上述主线程调用到的底层等待原语，便于直接定位。仅worker线程中的睡眠/I/O不作为独立主线程行。
for line, call in [(176,'lifecycle_mutex lock/start'),(189,'snapshot_mutex lock/start'),
                   (195,'output_mutex lock/start'),(203,'coordinate_mutex lock/start'),(210,'coordinate_mutex lock/start'),
                   (230,'lifecycle_mutex lock/stop'),(247,'snapshot_mutex unique_lock'),
                   (260,'snapshot_mutex lock/latest_snapshot'),(268,'snapshot_mutex lock/drain_snapshots'),
                   (280,'output_mutex lock/publish_output'),(427,'coordinate_mutex lock/refresh_coordinates'),
                   (565,'stats_mutex lock/低频提交'),(570,'low_frequency_mutex lock/提交'),
                   (581,'LowFrequencyRequest::mutex unique_lock'),(588,'event_mutex lock/event_state'),
                   (596,'stats_mutex lock/stats'),(617,'coordinate_mutex lock/coordinate_cache'),
                   (1969,'g_notification_registry_mutex lock/注销通知')]:
    add('ads_communication.cpp',line,'[底层原语] '+call,LOCK)
add('ads_communication.cpp',248,'[底层原语] snapshot_cv_.wait_for(timeout_ms)',
    '已确认：请求相对超时由main给500/300/50ms；等待返回仍需重新取得同一mutex，且不保证调度墙钟上界。')
add('ads_communication.cpp',582,'[底层原语] request->cv.wait(predicate)',LF)
for line, name in [(222,'启动异常回收worker_.join'),(235,'worker_.join'),(236,'low_frequency_worker_.join')]:
    add('ads_communication.cpp',line,'[底层原语] '+name,JOIN)
add('ads_communication.cpp',1983,'[底层原语] 注销通知 ads_.ADSDeleteNotification',
    '已确认：取得CADSComm递归mutex后同步调用AdsSyncDelDeviceNotificationReqEx；未设整次注销上界。'
    '回调退出通过注册表同一把mutex同步，源码没有轮询回调计数。'+
    ref('ads_communication.cpp',112)+'、'+ref('ads_communication.cpp',1969)+'、'+ref('ADSComm.cpp',738)+'。')

for file, line, call in [('control_types.cpp',44,'get_average_handle_pose: Sleep(10)'),
                         ('control_types.cpp',82,'get_average_dual_pos: Sleep(10)')]:
    add(file,line,'[底层原语] '+call,SAMPLE)
for line,call in [(85,'get_sample mutex'),(106,'set_output mutex'),(96,'clear_samples mutex/Zenoh stop'),
                  (121,'clear_outputs mutex/Zenoh stop')]:
    add('手柄/remote_handle_bridge.h',line,'[底层原语] '+call,LOCK)
add('手柄/手柄.cpp',40,'[底层原语] Handle::init printf',CONSOLE)

for file,line,call,basis in [
    ('zenoh_service.cpp',104,'getaddrinfo(host,port)', '已确认：DNS/名字解析同步调用，源码未设超时；最坏阻塞未知。'),
    ('zenoh_service.cpp',119,'select(endpoint_ready)', '已确认：每个解析地址一次select，timeout_ms为150/100/250ms；地址数不固定，另有getaddrinfo等开销，整次endpoint_ready无硬上界。'),
    ('zenoh_service.cpp',251,'候选token ifstream读取至EOF',IO+' 先读取整个文件，随后才判16..4096字节，读取量不受4096预先限制。'),
    ('zenoh_service.cpp',280,'filesystem::exists(router)',IO),
    ('zenoh_service.cpp',288,'CreateProcessW(zenohd)', '已确认：同步创建进程，源码未设调用截止时间；等待新进程开放端点另行处理。'),
    ('zenoh_service.cpp',300,'40轮router启动轮询中的Sleep(100)', '已确认：最多40次请求100ms睡眠，另有每轮endpoint_ready(...,100)及首150/末250ms探测；Sleep/名字解析与进程创建调度未设整体上界。'),
    ('zenoh_service.cpp',317,'WaitForSingleObject(router_process_,2000)', '已确认：终止managed router后请求最多2000ms内核等待；不是整个ZenohService::stop上界。'),
    ('zenoh_service.cpp',331,'z_open', '已确认：同步调用库会话打开；此处无传入超时，仓库代码不能证明最坏时长。'),
    ('zenoh_service.cpp',345,'z_declare_publisher(2次)', '已确认：同步库调用，此处没有等待上界。'),
    ('zenoh_service.cpp',352,'z_declare_subscriber(handle)', '已确认：同步库调用，此处没有等待上界。'),
    ('zenoh_service.cpp',356,'z_declare_subscriber(command)', '已确认：同步库调用，此处没有等待上界。'),
    ('zenoh_service.cpp',197,'worker_.join',JOIN),
    ('zenoh_service.cpp',365,'close_zenoh z_drop(subscriber/publisher/key/session)', '已确认：同步库资源释放，无项目定义的完成上界。'),
    ('arm_manual_ads_service.cpp',136,'worker_.join',JOIN),
    ('handle_velocity_recorder.h',49,'sampler_.join',JOIN),
    ('async_csv_writer.h',42,'_wfopen_s CSV',IO),
    ('async_csv_writer.h',52,'CSV表头fwrite/fflush',IO),
    ('async_csv_writer.h',75,'std::thread(writer_loop)', '已确认：同步线程创建操作无项目定义的最坏耗时，异常路径同步fclose。'),
    ('async_csv_writer.h',95,'writer_thread_.join',JOIN),
    ('async_csv_writer.h',99,'CSV fflush/fclose',IO),
    ('experiment_recorder.cpp',467,'上次stop_thread_.join/start',JOIN),
    ('experiment_recorder.cpp',440,'CreateDirectoryW session（最多10000候选）',IO+' 目录重名循环限制候选数，不限制单次I/O时长。'),
    ('experiment_recorder.cpp',553,'rollback_failed_start: writer.stop + 删除文件/目录',JOIN+IO),
    ('experiment_recorder.cpp',580,'stop_async内上次stop_thread_.join',JOIN),
    ('experiment_recorder.cpp',584,'std::thread(stop_worker)', '已确认：线程创建无项目定义耗时上界；抛异常时调用线程同步执行stop_worker，包含writer.join和文件I/O。'),
    ('experiment_recorder.cpp',598,'stop_and_wait stop_thread_.join',JOIN),
    ('experiment_recorder.cpp',1025,'write_session_json: _wfopen_s/fwrite/fflush/fclose',IO),
    ('homing_record.h',43,'load ifstream + JSON格式解析',IO),
    ('homing_record.h',56,'enabled ifstream + JSON配置读取',IO),
    ('homing_record.h',64,'save ofstream/flush/close',IO),
    ('homing_record.h',82,'MoveFileExW(...MOVEFILE_WRITE_THROUGH)',IO),
]:
    add(file,line,'[底层原语] '+call,basis)

for file,lines in [('arm_manual_ads_service.cpp',[110,128,137,143,150,166,191,206,241,267,285,299,412]),
                   ('zenoh_service.cpp',[201,214,378,464,466,487,514,515]),
                   ('experiment_recorder.cpp',[510,568,772,843,882])]:
    for line in lines:
        add(file,line,'[底层原语] 无超时mutex获取（调用路径见主线程行）',LOCK)

# 辅助模块中会在main线程执行的显式调用。当前ctx下的位置/力读取只锁快照；不标成网络等待。
for file, line, call in [
    ('motion_sync.cpp',113,'sync_axis1: clear_axis1_group_return_requests'),
    ('motion_sync.cpp',155,'sync_axis1: write_refer'),
    ('motion_sync.cpp',350,'sync_axis6: clear_axis_return_request'),
    ('motion_sync.cpp',401,'sync_axis6: write_refer'),
    ('motion_sync.cpp',410,'sync_all: clear_axis1_group_return_requests'),
    ('motion_sync.cpp',411,'sync_all: clear_axis_return_request'),
    ('motion_sync.cpp',412,'sync_all: write_axis4_manual_requests'),
    ('motion_sync.cpp',425,'sync_all: write_refer'),
    ('motion_sync.cpp',456,'sync_all: write_refer'),
    ('guidewire_mode.cpp',19,'prepare_axis6_guidewire_handoff: clear_axis_return_request'),
    ('guidewire_mode.cpp',112,'enter_independent_guidewire_mode: write_refer'),
    ('startup_sequence.cpp',47,'write_recovery_grip_commands: write_cylinder_values'),
    ('startup_sequence.cpp',60,'consume_startup_loading_ready: write_startup_loading_ready'),
    ('startup_sequence.cpp',140,'start_startup_sequence: read_v_limit'),
    ('startup_sequence.cpp',161,'start_startup_sequence: write_v_limit'),
    ('startup_sequence.cpp',168,'start_startup_sequence: write_recovery_grip_commands'),
    ('startup_sequence.cpp',171,'start_startup_sequence: restore_startup_v_limit'),
    ('startup_sequence.cpp',178,'start_startup_sequence: consume_startup_loading_ready'),
    ('startup_sequence.cpp',180,'start_startup_sequence: restore_startup_v_limit'),
    ('startup_sequence.cpp',189,'start_startup_sequence: write_startup_smoothing_bypass'),
    ('startup_sequence.cpp',192,'start_startup_sequence: restore_startup_v_limit'),
    ('startup_sequence.cpp',199,'start_startup_sequence: write_refer'),
    ('startup_sequence.cpp',202,'start_startup_sequence: write_startup_smoothing_bypass'),
    ('startup_sequence.cpp',203,'start_startup_sequence: restore_startup_v_limit'),
    ('startup_sequence.cpp',243,'restore_startup_v_limit: write_v_limit'),
    ('plc_io.cpp',126,'ads_service->read'),
    ('plc_io.cpp',135,'ads_service->write'),
    ('plc_io.cpp',150,'ads_service->read_sum'),
    ('plc_io.cpp',164,'ads_service->write_sum'),
    ('plc_io.cpp',333,'write_refer: ads_write'),
    ('plc_io.cpp',339,'read_v_limit: ads_read'),
    ('plc_io.cpp',344,'write_v_limit: ads_write'),
    ('plc_io.cpp',360,'write_startup_loading_ready: ads_write'),
    ('plc_io.cpp',376,'write_cylinder_values: ads_write_sum'),
    ('plc_io.cpp',381,'write_startup_smoothing_bypass: ads_write'),
    ('plc_io.cpp',445,'clear_axis_return_request: ads_write'),
    ('plc_io.cpp',652,'clear_axis1_group_return_requests: ads_write_sum'),
    ('plc_io.cpp',669,'write_axis4_manual_requests: ads_write_sum'),
]:
    add(file,line,'[同步辅助调用] '+call,LF)
for file, line, call in [('motion_sync.cpp',131,'get_average_handle_pose(samples)'),
                         ('motion_sync.cpp',371,'get_average_handle_pose(samples)'),
                         ('motion_sync.cpp',430,'get_average_dual_pos(samples)'),
                         ('guidewire_mode.cpp',85,'get_average_handle_pose(20)'),
                         ('guidewire_mode.cpp',141,'sync_axis1(ctx,20)')]:
    add(file,line,'[同步辅助调用] '+call,SAMPLE+(LF if line==141 and file=='guidewire_mode.cpp' else ''))
for file,line,call in [('motion_sync.cpp',115,'sync_axis1: read_plc_state'),
                       ('motion_sync.cpp',352,'sync_axis6: read_plc_state'),
                       ('motion_sync.cpp',414,'sync_all: read_plc_state'),
                       ('motion_sync.cpp',448,'sync_all: read_plc_state'),
                       ('guidewire_mode.cpp',72,'enter: read_plc_state'),
                       ('guidewire_mode.cpp',119,'entry_gate: read_plc_state'),
                       ('guidewire_mode.cpp',133,'exit: read_plc_state'),
                       ('startup_sequence.cpp',84,'start: read_plc_state'),
                       ('plc_io.cpp',202,'read_plc_state: latest_snapshot'),
                       ('plc_io.cpp',261,'read_force_sample: latest_snapshot')]:
    add(file,line,'[同步辅助调用] '+call,LOCK+' 当前ctx.ads_service非空，读取内存快照。')
add('plc_io.cpp',172,'[同步辅助调用] ads_error_text: CADSComm::GetLastErrorCopy',
    LOCK+' 仅复制错误文本，但锁可能被执行ADS同步I/O的其它线程持有；'+ref('ADSComm.cpp',159)+'。')
add('ADSComm.cpp',120,'[退出析构] CADSComm::~CADSComm → CloseComm',
    '已确认：main局部对象析构仍在main线程，CloseComm持递归mutex；已关闭时快速返回，'
    '仍有端口时同步注销通知/释放句柄/关闭端口，没有整次退出上界；'+ref('ADSComm.cpp',928)+'。')
add('ADSComm.cpp',931,'[底层原语] CloseComm递归mutex及ADS资源释放',
    '已确认：递归mutex获取无超时，持锁执行同步ADS通知删除、句柄释放及端口关闭，整次关闭未设上界；'+
    ref('ADSComm.cpp',955)+'、'+ref('ADSComm.cpp',969)+'。')
main(1838,'check_axis6_guidewire_entry_gate → read_plc_state',LOCK+' 当前ctx为内存快照；'+ref('guidewire_mode.cpp',117)+'。')

# main显式控制台输出逐条保留；模块仅保留可能由main同步进入的函数范围。
console_ranges = {
    'main.cpp':[(1,10000)],
    'motion_sync.cpp':[(81,110)],
    'guidewire_mode.cpp':[(16,148)],
    'startup_sequence.cpp':[(1,260)],
    'plc_io.cpp':[(394,491)],
    'zenoh_service.cpp':[(144,375)],
    'handle_velocity_recorder.h':[(45,60)],
}
for file,ranges in console_ranges.items():
    for line,text in enumerate((SRC/file).read_text(encoding='utf-8-sig').splitlines(),1):
        if any(a<=line<=b for a,b in ranges) and re.search(r'\bstd::(?:cout|cerr)\b',text):
            add(file,line,'[同步输出] '+text.strip(),CONSOLE)

# 位置信息应准确，不让文件总行数或编码错误流入CSV。
for row in rows:
    path,line=row['位置'].rsplit(':',1)
    p=Path(path)
    count=len(p.read_text(encoding='utf-8-sig').splitlines())
    assert 1<=int(line)<=count,(row['位置'],count)
    assert any(k in row['最坏阻塞时长的依据'] for k in ['已确认','推断','未知']),row
rows.sort(key=lambda r:(r['位置'].rsplit(':',1)[0],int(r['位置'].rsplit(':',1)[1]),r['调用']))
with (OUT/'blocking_calls.csv').open('w',encoding='utf-8-sig',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=['位置','调用','最坏阻塞时长的依据'])
    writer.writeheader()
    writer.writerows(rows)
print(f'rows={len(rows)}, columns=3; files='+str(Counter(Path(r['位置'].rsplit(':',1)[0]).name for r in rows)))
