# 阶段1 main.cpp 精简地图与状态索引

下文 `main.cpp` 指 `介入机器人/B数据端/工程/ADS项目/main.cpp`；结构体头文件同目录。当前文件 2,683 行，主循环始于 `main.cpp:456`。本表反映当前代码，不是下一阶段的设计。

## 1. 分块地图

| 行号区间 | 职责 | 主要状态 | 外部模块/边界 |
|---|---|---|---|
| 1–33 | 依赖与文件说明 | — | control_types、motion_sync、plc_io、ADS、Zenoh、记录器 |
| 34–436 | 初始化、长驻变量、上下文及辅助 lambda | 手柄/滤波、轴基准、先行、四态事务、恢复、手动覆盖 | ADS/定位臂服务、sync/rebase、力清零、寻参记录 |
| 437–455 | 手柄历史、发布计时、默认记录开启 | handle_record_history、next_vis_publish_qpc、事件边沿缓存 | ExperimentRecorder.start("auto") |
| 456–569 | 等待并吸收 ADS 快照、收养 PLC 事务、自检/急停反馈 | ads_snapshot_sequence、planned_return、self_check_done | wait_for_snapshot_after、stats、event_cache |
| 570–776 | 两柄重试/采样、连接保持、恢复重基准、初同步 | handle_soft_hold_active、ads_soft_hold_active、initial_sync_done | Handle、motion_sync；活动事务阻止普通恢复 rebase |
| 777–896 | PLC 重启及寻参复用 | handled_plc_restart_count、homing_*、startup | ADS 低频读取、homing_record |
| 897–1072 | 滤波、按键边沿、方向、先行清除、UI 点动租约 | spacing_recovery、dual_reverse_active、先行、注射器/轴4期限 | gate/filter、事件记录 |
| 1073–1239 | 急停、自检、手柄重初始化、故障保持解除 | estop_hold_active、startup、return_recovery | sync_all、ADS 自检/重置；活动事务保留 |
| 1240–1291 | 自动恢复控制、四缸默认值及净距离采样 | control_active、manual_cylinder_override_active、net_distance | sync_all、HandleNetDistance |
| 1292–1342 | 运动树门控、实际位置/保持值载入、手柄增量 | pos、线性/旋转滤波历史、方向增量 | read_plc_state、filter/gate |
| 1343–1499 | 事务目标安全校验、组包/请求、轴6常规跟随辅助、恢复退出申请 | planned_return.command/legs/hold_*、return_recovery、ff.hold_* | 配置、窗口/软限位、记录事件 |
| 1500–1633 | 屈曲恢复 ClampWait/Active 的夹爪与等量运动 | spacing_recovery、pos、四缸 | GetTickCount、共同剩余行程与限速 |
| 1634–1664 | 故障后按当前位置规划且只续做一次 | original_return_intent、return_recovery、interrupted_return_* | 原目标公式、begin_planned_return |
| 1665–1885 | 活动事务让出运动树；普通双柄跟随、先行、镜像、轴4耦合、换手意图/就绪 | dual_handoff、axis*_crawl、先行、跟随基准 | motion_sync、配置、边界/低速判定 |
| 1886–1938 | 更新滤波历史、屈曲 ExitSync、轴4错误边沿 | spacing_recovery、control_active、axis4_manual_error_* | sync_all、诊断 |
| 1939–2045 | Requested/Running/Takeover 推进及事务保持值 | command/status、重发时钟、done快照、rebase、generation | PLC 快照、rebase_dual_after_return、力保持完成 |
| 2046–2196 | 手动电缸、Y阀/注射器/轴4、最终输出与接管确认代次 | cylinder_*、net_distance、ads_output | publish_output；takeover要求refer和四缸同次成功 |
| 2197–2293 | 力反馈与实验记录入队、ADS/手柄历史对齐 | ff、cal_state、force_sample、handle_record_history | process_force_feedback、ExperimentRecorder |
| 2294–2476 | 按发布周期构建公共状态及恢复提示 | VisState、RemoteStateExtra | Zenoh publish_state；公共字段不变 |
| 2477–2683 | 有界消费公共命令、待接管后开始控制 | pending_start_control、手动输出、自检、startup.completed | Zenoh command FIFO、ADS、定位臂服务 |

## 2. 换手状态与交接边界

| 状态归属 | 当前字段/语义 | 声明与主要读写点 |
|---|---|---|
| HandoffCoordinator | phase；mode；command/status；legs[2]/leg_count；request_t0_ms/last_send_ms；send_token/done_snapshot_sequence/handoff_generation；rebased/request_timed_out；四个 hold_axis*_rel | `control_types.h:177`；组包 `main.cpp:1385`；推进 `main.cpp:1939`；输出 `main.cpp:2184` |
| HandoffLeg | active、axis_index、target_abs、entry_rel | `control_types.h:171`；组包 `main.cpp:1404` |
| HandoffCommand/Status | 固定布局117/61字节；请求参数及PLC阶段/结束结果 | `handoff_types.h:7`、`handoff_types.h:14`；完整接口见[验收表](04_验证与交付.md#1-最终接口) |
| DualHandoffRequest | axis1/axis6、wire_caused、gap_boundary、reverse/wire_reverse、axis4_caused | `control_types.h:207`；意图 `main.cpp:1815`；故障续做 `main.cpp:1634` |
| ReturnRecovery | 故障保持、恢复资格、腿掩码、未完成标记、一次续做计数 | `return_recovery.h:12`；解除 `main.cpp:1215`；续做 `main.cpp:1634` |
| CrawlState | 只剩Follow启用/窗口及手柄、旋转、轴、窗口端点基准 | `control_types.h:271`；基准更新在 motion_sync；双柄跟随 `main.cpp:1674` |
| SpacingRecoveryState | requested、Idle/ClampWait/Active/ExitSync；计时、保持/指令坐标、pending/moved/remaining | `control_types.h:229`；按键 `main.cpp:959`；Active `main.cpp:1501`；ExitSync `main.cpp:1897` |
| 力保持与先行 | ff.hold_587/582；先行active/progress/applied_extra | `control_types.h:288`；请求锁存 `main.cpp:1434`；接管完成 `main.cpp:1999` |

当前不再有 Prepare/Commit/Clear、可能已启动、清 Req 掩码、夹爪等待代次或 fast。数据端只在收到比 done 更新的一帧后调用一次 `rebase_dual_after_return`，等 ADS 已应用输出 generation 才离开 Takeover：`main.cpp:1991`、`main.cpp:1996`、`main.cpp:2193`。PLC内部单轴状态不在本表的数据端状态计数中。

## 3. main 外层长驻声明总表

按与阶段0相同的词法口径，共133个声明，其中117个可变、16个只读；包括外层服务、上下文、数组、配置常量和循环内静态变量，不包含 lambda 对象或每拍临时量。数组按一个声明计数，结构体字段不在这133项内展开。完整显式写入及其它引用行见[CSV索引](main_state_inventory.csv)，可用 `scratch/inventory.py` 重生成。

此索引识别赋值/初始化和词法引用，不把传引用函数或对象方法内的间接写入误称为“读取”；这些需沿 `ctx`、motion_sync、Handle 和服务实现继续追踪。完整展开两腿及事务载荷后的换手字段为43项，与阶段0同口径66项的明细见[规模JSON](handoff_size_comparison.json)。

| 名称 | 类型 | 声明位置 |
|---|---|---|
| `diagnostic_build` | `const char*` | `main.cpp:38` |
| `diagnostic_exe` | `wchar_t[MAX_PATH]` | `main.cpp:43` |
| `diagnostic_path` | `std::wstring` | `main.cpp:45` |
| `diagnostic_slash` | `const auto` | `main.cpp:46` |
| `serial_axis1_handle` | `constexpr DWORD` | `main.cpp:51` |
| `serial_axis6_handle` | `constexpr DWORD` | `main.cpp:52` |
| `ads_snapshot_wait_timeout_ms` | `constexpr DWORD` | `main.cpp:54` |
| `vis_publish_rate_hz` | `constexpr std::int64_t` | `main.cpp:56` |
| `axis4_coupled_axis6_speed_mm_s` | `constexpr double` | `main.cpp:58` |
| `hardcoded_ads_netid` | `const char*` | `main.cpp:59` |
| `cfg` | `ControlConfig` | `main.cpp:61` |
| `cyl` | `const CylinderPreset` | `main.cpp:62` |
| `run_directory` | `const std::filesystem::path` | `main.cpp:63` |
| `homing_record_path` | `const auto` | `main.cpp:64` |
| `homing_reuse_enabled` | `const bool` | `main.cpp:65` |
| `homing_record` | `homing::Record` | `main.cpp:66` |
| `plc_application_name` | `std::string` | `main.cpp:67` |
| `homing_reuse_pending` | `bool` | `main.cpp:68` |
| `homing_restore_submitted` | `bool` | `main.cpp:68` |
| `control_before_plc_restart` | `bool` | `main.cpp:69` |
| `homing_restore_applied` | `bool` | `main.cpp:69` |
| `homing_record_update_ms` | `ULONGLONG` | `main.cpp:70` |
| `homing_record_pending` | `bool` | `main.cpp:71` |
| `homing_record_refresh_generation` | `std::uint64_t` | `main.cpp:72` |
| `handle_axis1` | `Handle` | `main.cpp:75` |
| `handle_axis6` | `Handle` | `main.cpp:76` |
| `ads` | `CADSComm` | `main.cpp:77` |
| `axis1_net_distance` | `HandleNetDistance` | `main.cpp:78` |
| `axis6_net_distance` | `HandleNetDistance` | `main.cpp:78` |
| `experiment_recorder` | `ExperimentRecorder` | `main.cpp:79` |
| `ads_snapshot_sequence` | `std::uint64_t` | `main.cpp:80` |
| `handle_velocity_recorder` | `HandleVelocityRecorder` | `main.cpp:86` |
| `handle_velocity_path` | `const auto` | `main.cpp:87` |
| `zenoh_service` | `ZenohService` | `main.cpp:92` |
| `axis1_handle_filter` | `HandleFilterState` | `main.cpp:93` |
| `axis6_handle_filter` | `HandleFilterState` | `main.cpp:94` |
| `ads_communication` | `AdsCommunicationService` | `main.cpp:96` |
| `arm_manual_ads` | `ArmManualAdsService` | `main.cpp:97` |
| `axis1_input_handle` | `Handle* const` | `main.cpp:120` |
| `axis6_input_handle` | `Handle* const` | `main.cpp:121` |
| `ff` | `ForceFeedbackState` | `main.cpp:122` |
| `pos` | `double[7]` | `main.cpp:141` |
| `plc_act_pos` | `double[7]` | `main.cpp:142` |
| `plc_init_pos` | `double[7]` | `main.cpp:143` |
| `plc_leftlimit` | `double[7]` | `main.cpp:144` |
| `plc_act_pos_from_left` | `double[7]` | `main.cpp:145` |
| `ctx` | `AppContext` | `main.cpp:148` |
| `axis3_base_rel` | `double` | `main.cpp:169` |
| `axis5_base_rel` | `double` | `main.cpp:170` |
| `axis2_hold_rel` | `double` | `main.cpp:172` |
| `axis7_hold_rel` | `double` | `main.cpp:173` |
| `axis1_prev_linear_filtered` | `double` | `main.cpp:175` |
| `axis6_prev_linear_filtered` | `double` | `main.cpp:176` |
| `axis1_prev_rot_filtered` | `double` | `main.cpp:177` |
| `axis6_prev_rot_filtered` | `double` | `main.cpp:178` |
| `axis1_follow_cmd_abs` | `double` | `main.cpp:179` |
| `axis6_follow_cmd_abs` | `double` | `main.cpp:180` |
| `axis4_axis6_coupling_last_ms` | `ULONGLONG` | `main.cpp:181` |
| `axis4_axis6_coupling_active_prev` | `bool` | `main.cpp:182` |
| `axis1_delivery_mapping_active` | `bool` | `main.cpp:184` |
| `axis1_delivery_mapping_progress_mm` | `double` | `main.cpp:185` |
| `axis1_delivery_mapping_applied_extra_mm` | `double` | `main.cpp:186` |
| `dual_reverse_active` | `bool` | `main.cpp:189` |
| `startup` | `StartupState` | `main.cpp:191` |
| `cal_cfg` | `ForceCalibrationConfig` | `main.cpp:192` |
| `cal_state` | `ForceCalibrationState` | `main.cpp:193` |
| `axis1_crawl` | `CrawlState` | `main.cpp:194` |
| `axis6_crawl` | `CrawlState` | `main.cpp:195` |
| `planned_return` | `HandoffCoordinator` | `main.cpp:196` |
| `next_handoff_id` | `std::uint32_t` | `main.cpp:197` |
| `next_handoff_send_token` | `std::uint64_t` | `main.cpp:198` |
| `pending_start_control` | `bool` | `main.cpp:199` |
| `dual_handoff` | `DualHandoffRequest` | `main.cpp:200` |
| `spacing_recovery` | `SpacingRecoveryState` | `main.cpp:201` |
| `control_active` | `bool` | `main.cpp:242` |
| `self_check_done` | `bool` | `main.cpp:276` |
| `has_self_check_flag` | `bool` | `main.cpp:277` |
| `last_self_check_done` | `bool` | `main.cpp:279` |
| `handle_reinit_req` | `bool` | `main.cpp:280` |
| `last_handle_reinit_req` | `bool` | `main.cpp:281` |
| `estop_hold_req` | `bool` | `main.cpp:282` |
| `estop_hold_active` | `bool` | `main.cpp:283` |
| `axis1_reverse_pressed_prev` | `bool` | `main.cpp:284` |
| `axis6_effective_reverse_prev` | `bool` | `main.cpp:285` |
| `axis4_manual_error_prev` | `bool` | `main.cpp:286` |
| `axis4_manual_error_id_prev` | `unsigned long` | `main.cpp:287` |
| `axis4_ui_forward_pressed` | `bool` | `main.cpp:288` |
| `axis4_ui_reverse_pressed` | `bool` | `main.cpp:289` |
| `y_valve_open` | `bool` | `main.cpp:290` |
| `injector_ui_direction` | `int[2]` | `main.cpp:291` |
| `injector_ui_jog_deadline_ms` | `ULONGLONG[2]` | `main.cpp:292` |
| `axis4_ui_jog_deadline_ms` | `ULONGLONG` | `main.cpp:295` |
| `manual_jog_lease_ms` | `constexpr ULONGLONG` | `main.cpp:296` |
| `return_ads_fault_hold` | `bool` | `main.cpp:297` |
| `return_recovery` | `ReturnRecovery` | `main.cpp:298` |
| `interrupted_return_mode` | `PlannedReturnMode` | `main.cpp:299` |
| `interrupted_return_scope` | `PlannedReturnRebaseScope` | `main.cpp:300` |
| `original_return_intent` | `DualHandoffRequest` | `main.cpp:301` |
| `resuming_return` | `bool` | `main.cpp:302` |
| `recovery_event_text` | `std::string` | `main.cpp:303` |
| `force_sample` | `ForceSampleFrame` | `main.cpp:304` |
| `loop_count` | `int` | `main.cpp:305` |
| `force_feedback_diag_reason` | `std::string` | `main.cpp:306` |
| `force_sample_diag_reason` | `std::string` | `main.cpp:307` |
| `remote_force_zero_ok_count` | `std::uint32_t` | `main.cpp:362` |
| `remote_force_zero_fail_count` | `std::uint32_t` | `main.cpp:363` |
| `initial_sync_done` | `bool` | `main.cpp:372` |
| `plc_app_name_read` | `bool` | `main.cpp:373` |
| `handle1_next_retry_ms` | `ULONGLONG` | `main.cpp:374` |
| `handle6_next_retry_ms` | `ULONGLONG` | `main.cpp:375` |
| `handle_soft_hold_active` | `bool` | `main.cpp:376` |
| `connection_hold_active_prev` | `bool` | `main.cpp:377` |
| `cylinder_manual_mode` | `CylinderManualMode[4]` | `main.cpp:384` |
| `cylinder_manual_position` | `unsigned short[4]` | `main.cpp:390` |
| `cylinder_manual_restore` | `unsigned short[4]` | `main.cpp:391` |
| `cylinder_manual_written_mask` | `std::uint8_t` | `main.cpp:392` |
| `spacing_recovery_button_pressed_prev` | `bool` | `main.cpp:424` |
| `pending_selfcheck_target` | `double[7]` | `main.cpp:426` |
| `pending_selfcheck_axes` | `unsigned int` | `main.cpp:427` |
| `selfcheck_command_error` | `bool` | `main.cpp:428` |
| `selfcheck_start_pending` | `bool` | `main.cpp:429` |
| `manual_cylinder_override_active` | `bool` | `main.cpp:430` |
| `handled_plc_restart_count` | `std::uint64_t` | `main.cpp:432` |
| `handled_reconnect_count` | `std::uint64_t` | `main.cpp:433` |
| `ads_soft_hold_active` | `bool` | `main.cpp:434` |
| `drained_ads_snapshots` | `std::vector<AdsFastSnapshot>` | `main.cpp:435` |
| `handle_record_history` | `std::deque<HandleRecordSnapshot>` | `main.cpp:442` |
| `vis_qpc_frequency` | `LARGE_INTEGER` | `main.cpp:443` |
| `next_vis_publish_qpc` | `std::int64_t` | `main.cpp:445` |
| `recording_info` | `ExperimentStartInfo` | `main.cpp:446` |
| `event_hold_sources` | `int` | `main.cpp:454` |
| `event_connection_hold_active` | `bool` | `main.cpp:455` |
| `next_estop_recover_req_ms` | `ULONGLONG` | `main.cpp:1101` |
