# B main.cpp 跨周期状态总表（scratch）

> 这是给 `B_main解剖.md` 使用的证据表，不是设计建议。范围是当前 `介入机器人/B数据端/工程/ADS项目/main.cpp` 及其直接包含的状态头文件。main.cpp 的物理行数为 5424；主循环为 `while(true)`（`main.cpp:966-5402`）。除 `while` 内的 `static next_estop_recover_req_ms` 外，跨拍值均定义在主循环外并被 `[&]` lambda 引用，或封装在下面的长期对象中。有捕获的 lambda 均为引用捕获（例如 `main.cpp:216-379,549-961`），另有无捕获的 `gate_linear_increment_for_mode`（`main.cpp:470`）；没有按值捕获形成的隐藏历史副本。

## 1. 口径和别名

| 口径 | 事实与出处 |
|---|---|
| 外层局部 | `main.cpp:64-961` 定义的对象、数组、标志、计时和基线跨越每次 `while` 迭代；命令处理在 `main.cpp:4868-5402` 仍可修改这些值。 |
| 循环内 static | 只有 `next_estop_recover_req_ms`，在急停保持分支按 1 s 节流 watchdog recovery（`main.cpp:1759-1767`）。 |
| ctx 别名 | `AppContext ctx` 在 `main.cpp:189-207,297-326` 保存指针；`ctx.pos/plc_*`、`ctx.axis*_crawl`、`ctx.startup` 等不是副本。模块通过指针直接写回外层数组/变量；`motion_sync::sync_all/rebase_*` 会批量改写它们（`motion_sync.cpp:158-248,303-385`）。 |
| reset 语义 | `PlannedReturnCoordinator::reset()` 是 `*this = PlannedReturnCoordinator{}`，会清整个协调器（`control_types.h:295-371`）；`SpacingRecoveryState::reset()` 只显式清请求、阶段、计时/位移/恢复覆盖字段，不清保持坐标 `axis*_hold_rel/cmd_rel`（`control_types.h:395-438`）。`DualHandoffRequest::reset()` 是整结构清零（`control_types.h:373-385`）。 |
| 证据限制 | 下面的“写入/读取”列列出静态可见位置；服务对象（ADS、tracking、HandleNetDistance、Recorder、Zenoh）内部私有字段的逐字段写入由其类实现负责，main 只列公开 API 调用。精确词法行集另见 `scratch/main_outer_inventory.tsv`，生成脚本为 `scratch/state_inventory.py`。 |

## 2. main.cpp 外层长驻变量

### 2.1 启动、通信和外设对象

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
| `axis1_net_distance`, `axis6_net_distance` | 连续 Follow 段净位移累计；换手/无效快照时停计。 | `set_following/sample` `main.cpp:1054-1055,2071-2079,4515-4518`。 | 状态发布和命令 `main.cpp:4848-4862,5030-5038`；私有字段见 `handle_net_distance.h:7-48`。 | 已确认 |
| `experiment_recorder`, `handle_velocity_recorder` | 实验记录器和 100 Hz 手柄速度 CSV 记录器。 | 启动 `main.cpp:85-91,957-960`；各拍 enqueue/health/stop `4401-4680,5094-5121,5416`。 | 状态发布 `main.cpp:4608-4680,4775,5054-5055`；私有线程字段见 `experiment_recorder.h:134-225`。 | 已确认 |
| `zenoh_service` | Zenoh 接收手柄/命令、发布状态/力；内部有 worker、lease、结果队列。 | 构造/启动 `main.cpp:92,108-113`；主循环 `publish_state/poll_command/command_processed` `4864-5402`；停止 `5420`。 | `remote_cfg` 和命令/状态调用 `main.cpp:108-113,4681-4865`；私有字段见 `zenoh_service.h:31-143`。 | 已确认 |

### 2.2 PLC 镜像、Follow 基线和运动模式

| 名称（类型） | 含义/流程 | 写入位置 | 读取位置 | 确定度 |
|---|---|---|---|---|
| `pos[7]` | 本周期要写入 `G.refer` 的相对目标。 | `load_pos_from_actual`、各运动分支、planned/spacing/startup/手动覆盖 `main.cpp:2086-4582`；最终 `publish_output` 使用。 | `main.cpp:1045-1046,2390-2417,4490-4521`；`plc_io::write_refer`/ADS 输出。 | 已确认 |
| `plc_act_pos[7]`, `plc_init_pos[7]`, `plc_leftlimit[7]`, `plc_act_pos_from_left[7]`, `plc_v_limit[7]` | PLC 实际相对位置、零点偏置、左限位、距左限位和速度上限镜像。 | `plc_io::read_plc_state/load_pos_from_actual/read_v_limit`；`main.cpp:969-978,1051-1068,2083-2089`。 | 全运动树、限位、启动/寻参、输出（`main.cpp:2096-4329`）。 | 已确认 |
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
| `independent_axis1/2/3/5_hold_rel` | 独立导丝模式冻结导管侧轴位。 | 进入/退出/重同步 `guidewire_mode.cpp:61-93,133-152`、`motion_sync.cpp:220-285`。 | `main.cpp:2619-2626,3078-3165,3322-3388`。 | 已确认 |
| `axis6_window_locked`, `axis6_locked_window_start_abs/end_abs` | 独立导丝窗口锁存；普通重建可更新窗口。 | `motion_sync.cpp:66-91,243-248,303-385`。 | `main.cpp:2398-2417,3078-3165,3672-3722`。 | 已确认 |
| `startup (StartupState)` | 启动阶段、目标、保持位姿、恢复启动资格和速度备份。 | `main.cpp:281,3390-3626,4907-5305`；模块 `startup_sequence.cpp`。 | `main.cpp:2029-2041,3423-3626,4861-4865`。 | 已确认 |
| `control_active` | 正常手柄控制是否激活；与 startup/hold/故障标志并行门控。 | `main.cpp:380,1334-1337,1732-1734,2020-2039,3442-3503,5219-5225,5347`。 | 运动入口、tracking、状态发布 `main.cpp:2031-2089,2137-2152,4681-4865`。 | 已确认 |
| `emergency_retract_active (bool)`, `emergency_retract_axis1/2/7_hold_rel (double)`, `emergency_retract_v_limit_backup[7] (double)`, `emergency_retract_v_limit_scaled (bool)` | 极简器械撤出、固定轴 1/2/7 坐标和速度限速备份。 | 初始化/限速备份与恢复 `main.cpp:421-466`；命令启动时锁存坐标并置 active `main.cpp:5198-5228`；到位清 active 并恢复速度 `main.cpp:4523-4527`；退出再次恢复速度 `main.cpp:5404`。 | 运动和输出门控 `main.cpp:1588-1628,2007-2084,3297-3313,4357-4381,4475,4523`。 | 已确认 |

### 2.3 连接保持、急停、按键、回退故障和输出门控

| 名称（类型） | 含义/流程 | 写入位置 | 读取位置 | 确定度 |
|---|---|---|---|---|
| `self_check_done`, `has_self_check_flag`, `last_self_check_done` | PLC 自检完成缓存及边沿。 | `main.cpp:1012-1022,1337-1344,1891-1943`。 | 启动/运动有效性/状态 `main.cpp:1063-1068,1327-1344,2031-2035,4860-4865`。 | 已确认 |
| `handle_reinit_req`, `last_handle_reinit_req` | PLC 请求主机重建手柄/位置基准及边沿。 | `main.cpp:1022-1023,1972-1985`。 | 连接/同步门控 `main.cpp:1063-1066,1973-1983`。 | 已确认 |
| `estop_hold_req`, `estop_hold_active` | PLC 急停请求与主循环锁存；边沿清控制/回退/力。 | `main.cpp:1023-1050,1727-1756`。 | 全运动和恢复门控 `main.cpp:1730-1777,1987-2035,4356-4606`。 | 已确认 |
| `axis1_push_rearm_after_hold`, `axis1_delivery_stop_latched`, `axis1_delivery_stop_prompted` | 轴1支撑轴3近限位停送、恢复推入和提示状态。 | `main.cpp:498-500,3947-3995,4117-4161`。 | 单柄导管 Follow/状态输出。 | 已确认 |
| `axis1_reverse_pressed_prev`, `axis6_effective_reverse_prev` | B6 方向边沿基线。 | `main.cpp:1674-1725`。 | 方向切换和触发保护。 | 已确认 |
| `axis4_manual_error_prev`, `axis4_manual_error_id_prev` | 轴4点动故障通知去重。 | `main.cpp:503-504,1608-1664`。 | 错误提示/状态。 | 已确认 |
| `axis4_ui_forward_pressed`, `axis4_ui_reverse_pressed`, `y_valve_open` | UI 轴4按住电平与 Y 阀状态。 | 命令消费 `main.cpp:1608-1664,4868-5402`。 | 输出/状态 `main.cpp:2047-2050,4383-4400,4848-4865`。 | 已确认 |
| `injector_ui_direction[2]`, `injector_ui_jog_deadline_ms[2]`, `axis4_ui_jog_deadline_ms`, `manual_jog_lease_ms=500` | 注射器/轴4手动点动方向及 500 ms 租约。 | `main.cpp:508-513,1608-1726,4868-5402`。 | 到期清零和 ADS 输出 `main.cpp:1608-1664,4356-4400,4517-4521`。 | 已确认 |
| `axis1_fast_return`, `axis6_fast_retract` | 每拍清零，planned/急退阶段置位后写 PLC 旁路。 | `main.cpp:515-516,1486-1488,2579-2609,3078-3213`。 | ADS 输出 `main.cpp:4517-4521`。 | 已确认 |
| `return_ads_fault_hold`, `return_recovery` | 回退 ADS 故障锁存和自动清 Req/续做条件。 | `main.cpp:517-523,1734-1741,2440-2469,1987-2026`；类内部见 `return_recovery.h:14-71`。 | 全运动门控、状态/故障恢复。 | 已确认 |
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
| `tracking_manual_cylinder_override` | 本拍是否有任一手动电缸覆盖，禁用 tracking。 | `main.cpp:2049-2058`。 | `main.cpp:2068,2145,2159`。 | 已确认 |
| `handled_plc_restart_count`, `handled_reconnect_count` | 已处理 ADS 重连/PLC 重启计数。 | `main.cpp:1051-1060,1359-1376`。 | 同区间。 | 已确认 |
| `drained_ads_snapshots`, `handle_record_history` | 记录器排空的 ADS 快照临时容器、最近 100 ms 手柄有效性历史。 | `main.cpp:979,1472-1484,4608-4680`。 | 记录器。容器本身跨拍，但元素是按窗口淘汰。 | 已确认 |
| `vis_qpc_frequency`, `next_vis_publish_qpc` | 30 Hz 状态发布的 QPC 频率和下一 deadline。 | 初始化/发布 `main.cpp:950-952,4681-4865`。 | 同区间。 | 已确认 |
| `recording_info` | 自动记录会话启动信息（两手柄序列号、单柄模式）。 | `main.cpp:953-957`。 | 记录器启动，之后只作启动快照。 | 已确认 |

## 3. 直接使用的结构体字段

### 3.1 `HandleFilterState`、`CrawlState`、`StartupState`

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

### 3.2 planned return、dual handoff、spacing recovery

| 结构/字段 | 跨周期含义 | 写入/读取位置 | 出处 |
|---|---|---|---|
| `PlannedReturnCoordinator.phase/mode/rebase_scope` | 统一回退阶段、业务模式和重基准范围。 | begin/step/fail/reset `main.cpp:2471-3076`。 | `control_types.h:228-269,295-371` |
| `leg_count`, `legs[2]` | 最多轴1/轴6两条腿。 | `main.cpp:2491-2506,2283-2340`。 | 同上 |
| `retry_count`, `phase_t0_ms`, `request_t0_ms`, `execution_t0_ms`, `retry_clear_t0_ms` | 阶段、请求、运动执行、清除重试的时间基线。 | `main.cpp:2483-2489,2650-2657,2710-3076`。 | `control_types.h:304-313` |
| `clamp_output_generation`, `clamp_output_applied`, `handoff_generation`, `release_output_generation`, `release_applied` | ADS output generation 是否已应用，用于夹爪稳定/交接。 | `main.cpp:2681-2708,3011-3076,4542-4582`。 | `control_types.h:314-337` |
| `request_prepared`, `commit_guard_generation`, `retry_clear_issued`, `retry_clear_snapshot_sequence` | Prepare/Commit guard 和重试清除状态。 | `main.cpp:2710-2996`。 | 同上 |
| `ads_command_purpose/sequence/submit_ms` | 异步回退 ADS 命令跟踪。 | `main.cpp:554-648,2710-2996`。 | 同上 |
| `cancel_clear_confirmed`, `completion_clear_confirmed`, `cancel_snapshot_sequence`, `cancel_t0_ms`, `cancel_clear_retry_after_ms`, `cancel_timeout_reported`, `cancel_rebased` | 取消/完成清 Req 和故障取消过程。 | `main.cpp:664-703,2998-3076`。 | `control_types.h:328-337` |
| `done_snapshot_sequence` | Done 后等待更大 ADS 快照。 | `main.cpp:2884-3009`。 | 同上 |
| `hold_axis1/3/5/6_rel` | 回退期间保持的轴位。 | `main.cpp:2487-2490,2563-2637`。 | 同上 |
| `clamp_stage`, `restore_stage` | 持有侧/移动侧夹爪两级时序。 | `main.cpp:2529-2561,2681-2708,3024-3076`。 | 同上 |
| `PlannedReturnLeg.active/axis_index/target_abs/entry_rel` | 每腿轴号、目标和入场位置。 | `main.cpp:2491-2506`；输出/安全读取 `2563-2637,2382-2424`。 | `control_types.h:270-294` |
| `request_armed/acknowledged/started/possibly_started/done` | PLC Req 通知基线、是否接收/启动/可能启动/完成。 | `main.cpp:2283-2340,2710-2996`。 | 同上 |
| `request_event_sequence/ack_event_sequence/error_event_sequence/cancel_event_sequence` | 各通知边沿序号。 | 同上。 | 同上 |
| `reported_error_id`, `cancel_event_required` | 回退错误码和取消清理要求。 | `main.cpp:2320-2330,2440-2469`。 | 同上 |
| `DualHandoffRequest.axis1/axis6/wire_caused/gap_boundary/reverse/wire_reverse/axis4_caused` | 双柄换手意图和触发来源；`active()` 为 axis1||axis6。 | `main.cpp:3859-3924`；reset `656-667`。 | `control_types.h:373-385` |
| `SpacingRecoveryState.requested/phase` | 物理587组合键请求和 `Idle/ClampWait/Active/ExitSync` 阶段。 | `main.cpp:1508-1575,3215-3388,4300-4353`。 | `control_types.h:387-438` |
| spacing `phase_t0/last_motion_tick` | 夹爪稳定、运动 delta 和退出同步时间基线。 | 同上。 | 同上 |
| spacing `axis1/2/7_hold_rel`, `axis3/5/6_cmd_rel` | 恢复期间固定轴和三轴共同目标。 | `main.cpp:3251-3257,3322-3388`。 | 同上 |
| spacing `pending_delta_mm/moved_mm/remaining_mm/limit_logged` | 输入待移动量、已移动量、剩余安全行程和日志去重。 | `main.cpp:3215-3388`。 | 同上 |
| spacing `restore_mode_selection`, `restore_vis_reverse_override_*` | 松键后恢复的模式/方向覆盖。 | `main.cpp:1553-1566,4314-4343`。 | 同上 |

### 3.3 力、标定、恢复、tracking 和 net distance

| 结构/字段或私有状态 | 含义 | main 可见写/读 | 出处 |
|---|---|---|---|
| `cal_cfg (ForceCalibrationConfig)` `gravity_comp_enabled/validated` | 重力补偿仅在双条件下生效；两个字段默认 false；控制域旧命令分支可改 enabled，validated=false 时拒绝启用。 | 构造 `main.cpp:282`；写 `cal_cfg.gravity_comp_enabled` 在 `main.cpp:5364-5377`，不是 `cfg` 或 `cal_state`；`force_feedback.cpp:28-33` 传入标定参数。 | `force_calibration.h:27-64` |
| `ForceCalibrationState.f_zero/ft_zero/fn_2_zero/ft_2_zero/theta0_deg/zeroed` | 三帧 ADS 取零后的传感器偏置。 | `main.cpp:759-794`。 | `force_calibration.h:66-74` |
| `ForceSampleFrame` `*_value_v`, `axis1_pos_rel`, `axis2_pos_rel`, `valid`, `tick_ms`, `qpc_ticks` | 当前力样本和源时间；本地 `force_sample` 跨拍缓存。 | `main.cpp:759-794,4401-4606,5056-5066`；填充 `plc_io.cpp:159-225`。 | `control_types.h:630-645` |
| `ForceFeedbackState.enabled_582/587`, `clamp_hold_enabled` | 两侧独立力开关及换手力保持开关。 | `ff.set_enabled`/命令 `main.cpp:1368,4986-5003,5367`。 | `control_types.h:473-535` |
| `force_582/587_f/n`, `force_582/587_theory_f/n`, `force_*_filtered_f`, filter flags | 当前/理论/滤波后的反馈输出。 | `force_feedback.cpp`; main 清零 `171-176,743,786-787,4401-4606`。 | `control_types.h:536-627` |
| `smooth_582/587 (HoldSample)` | 原始平滑力/扭矩及 tick。 | `force_feedback.cpp`；planned capture `main.cpp:2521-2524`。 | `control_types.h:487-490` |
| `hold_582/587 (HandoffHold)` | 换手后保持、完成、运动2 mm/400 ms 后 blend 的状态。 | `ff.capture/complete/observe` 由 main/force 模块调用。 | `control_types.h:475-486,548-591` |
| `ReturnRecovery` 私有 `holding/resume_pending/clear_applied/unfinished/mask/resumes/next_clear/clear_sequence/clear_observed/reason` | 故障回退 Clear/新鲜帧/条件满足后仅续做一次。main 不直接写私有字段，仅调用公开方法。 | `main.cpp:1734-1741,2440-2469,1987-2026`。 | `return_recovery.h:14-71` |
| `DeliveryTrackingController` 私有 `axis_[2].active/grip_command_since_ms/last_pi_tick_ms/last_actual_abs_mm/integral*/pending_handover_error*/nominal_forward_outstanding/compensation_forward_outstanding/snapshot` | tracking 每轴分段、PI、换手欠账和实际进度；main 只通过 API。 | `main.cpp:1059,1305,1369,2160-2231,3699-4769,5030-5089`。 | `delivery_tracking.h:89-174` |
| tracking `DeliveryTrackingParameters kp/ki/max_gain/max_error_mm` | 每轴补偿参数；默认值在 header。 | UI 命令 `main.cpp:5085-5089`。 | `delivery_tracking.h:42-51` |
| `HandleNetDistance` 私有 `net_mm/actual_mm/origin_mm/sequence/sample_valid/following` | 只在连续 Follow、同 origin、新序号时累计净位移。 | main 仅 `sample/set_following` `main.cpp:1054-1055,2071-2079,4515-4518`。 | `handle_net_distance.h:7-48` |
| `Handle` 私有 `last_sample_rx_ms/rebase_requested/sample_gap_ms/opened_once/device_epoch_known/device_epoch/resume_settling/resume_since_ms` | 远程采样 5 s 失效、>1.5 s gap/epoch变化进入 500 ms 静置，再请求重建基准。 | main 仅调用 `init/poll/close/take_rebase_request/resume_settling` `1079-1200`。 | `手柄/Handle.h:10-64`、`手柄/手柄.cpp:7-105` |

## 4. 结构别名和“整结构写入”提醒

1. `ctx` 中的指针字段不是第二份状态。比如 `ctx.axis1_crawl->start_abs` 和外层 `axis1_crawl.start_abs` 是同一对象；`motion_sync::sync_all()` 还会同时清 axis6 窗口、设置滤波基准、写 `pos` 与旋转 hold（`motion_sync.cpp:303-385`）。
2. `PlannedReturnCoordinator::reset()` 会把 `phase/mode/rebase_scope/legs/计时/generation/hold/clamp` 全部恢复默认；main 的 `reset_planned_return` 还额外清 `dual_handoff`、两个 fast flag（`main.cpp:656-662`）。
3. `SpacingRecoveryState::reset()` 不清 hold/cmd 坐标，所以“退出请求”与“坐标值缓存”不是同一件事；退出完成分支随后用实际位置重同步并再 `reset()`（`main.cpp:4300-4353`）。
4. `ForceFeedbackState::reset/clear_output` 会清输出、滤波和 hold；`clear_force_output()` 还调用 `ff.clear_clamp_holds()`，因此安全保持/急停/软限位会同时清掉换手力保持（`control_types.h:604-627`、`main.cpp:171-176,743-744`）。
5. `ReturnRecovery` 的私有状态只能通过方法改变；main 不能直接把 `holding_` 或 `clear_applied_` 改回去。`restored()` 会设置 `resume_pending_ = unfinished_ && resumes_ == 0`，`consume_resume()` 才消耗一次续做机会（`return_recovery.h:47-57`）。

## 5. 不能从 main 词法直接确定的字段

| 项目 | 未知项 | 需要的证据 |
|---|---|---|
| `AdsCommunicationService` | 其 snapshot/output/notification 线程内部原子字段每次实际改写顺序、队列覆盖策略。 | 需结合 `ads_communication.cpp` 全函数调用和现场时序；本表只依据公开 API/头文件。 |
| `ZenohService` | 网络回调线程与 worker 的每次 lease/队列字段交错顺序。 | 需逐函数审阅 `zenoh_service.cpp` 或运行回环测试；main 只看 `poll_command`/publish。 |
| `DeliveryTrackingController` | 每个 `AxisRuntime.snapshot` 字段最终被 UI/状态读取的时间点。 | 需结合 `delivery_tracking.cpp` 与 `main.cpp:4766-4865` 的状态填充。 |
| 物理键位 | `spacing_recovery_button_mask=0x07` 的 bit0/1/2 与手柄外壳丝印/固件键定义。 | 现有源码只能证明位掩码条件，不能证明三个位对应哪些物理键。 |
