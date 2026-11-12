# 删除后 main.cpp 分块与状态索引

版本：阶段0事件源码`45929f2`（后续仅测试适配和交付文档）。D/表示`介入机器人/B数据端/工程/ADS项目/`；下表`main.cpp`、`control_types.h`均相对此目录。标为“已确认”的是静态源码事实，不表示设备验收。完整历史语义保留在R1 B和R2附件；这里仅给当前导航。

## B0.1 当前分块地图

| 行区间（main.cpp） | 功能 | 主要长驻状态 | 外部模块 |
|---|---|---|---|
| 1–33 | 头文件/职责 | 类型 | 保留模块 |
| 34–124 | 路径、配置、服务、固定双柄 | homing、handle587/582、filters、ff、ads序号 | ADS、Zenoh、Arm、记录器 |
| 125–250 | 力出口、PLC镜像、AppContext与Follow基准/先行helper | pos/plc_*、crawl、基准/prev、mapping、dual_reverse | plc_io、motion_sync |
| 251–328 | 公共启动、控制/自检/急停/租约/故障/力状态 | control、startup、return_recovery、original_intent、pending_error | 保留状态类型 |
| 329–509 | 回退异步提交/轮询/可能启动/取消、软限位谓词 | planned_return、dual_handoff、cancel记录序号 | ADS专用队列 |
| 510–556 | 取零与结果计数 | cal_state、force_sample、remote_force_zero_* | force_calibration |
| 557–640 | 初同步/保持、电缸覆盖、待自检目标、记录历史、事件观察量 | hold、cylinder_manual_*、pending_selfcheck_*、记录器 | ADS/CSV |
| 641–773 | 等快照/排空、异步结果补记、重连、自检有效性 | snapshot序号、planned/error、selfcheck、spacing | ADS快照/通知 |
| 774–966 | 双柄重试/采样/静置、基准重建、连接保持进退 | handle、filters、hold、net distance | Handle、motion_sync |
| 967–1103 | 初同步、应用名、PLC重启/寻参复用 | initial_sync、startup、homing_*、control | ADS低频、homing_record |
| 1104–1281 | 滤波、历史、按键、方向、先行门控、手动租约/换向 | prev、dual_reverse、spacing请求、mapping、manual | Handle/记录 |
| 1282–1431 | 急停、自检边沿、寻参记录事件、重同步 | estop/control/startup/spacing、homing | ADS、motion_sync |
| 1432–1531 | 故障清Req/恢复、自动重同步、缺省夹爪与净距离 | ReturnRecovery、control、net distance | return_recovery |
| 1532–1581 | 运动门控、线性/旋转增量、窗口 | filtered/prev、crawl、位置 | cfg |
| 1582–2437 | 回退通知视图、目标校验、阶段/输出/重试/交接 | planned_return、legs、generation、fast、ff保持 | ADS、motion_sync |
| 2438–2640 | 轴7旋转、恢复余量/进入/ClampWait/Active、故障续做 | spacing、original_intent、pos/夹爪 | cfg、ReturnRecovery |
| 2641–2873 | planned优先；否则双柄Follow、g裁剪/镜像/先行/换手 | handoff、follow_cmd、mapping、bases、pos | motion_sync |
| 2874–2956 | ExitSync、轴4错误、电缸最终门控/覆盖 | spacing、control、manual状态 | sync_all、plc_io |
| 2957–3136 | 力有效性/诊断、ADS帧与代次、力输出、更新prev | force/cal/ff、planned generations、pos/prev | ADS publish、force_feedback |
| 3137–3210 | 排空快照写force/motion | handle_record_history、recorder | CSV（100/50Hz源序号） |
| 3211–3392 | 约30Hz状态填充/发布 | VisState、诊断/恢复/记录状态 | Zenoh |
| 3393–3597 | 每拍最多64条公共映射内部命令 | 自检参数、manual、力、StartControl | Zenoh队列/ADS低频 |

以上各行区间均已确认，出处即`main.cpp:区间首行`。主循环从`main.cpp:641`至3595，没有外层缺帧continue，也没有循环后的正常清理。运动树门控`main.cpp:1532`；最终motion_enabled门控`main.cpp:3021`。注释中的“15Hz”与“50ms”等旧文字未作为事实：实际发布常量30Hz（`main.cpp:56`），夹爪等待读取`ControlConfig`（`control_types.h:116`），本阶段保留原运算。

## B0.2 长驻状态语义总表（分组）

| 流程 | 变量/类型 | 主要写入与读取 |
|---|---|---|
| 双柄与保持 | Handle两对象、Filter两对象；initial_sync_done/ads_soft_hold_active/handle_soft_hold_active/connection_hold_active_prev(bool)、两next_retry(ULONGLONG) | 采样/重建/保持`main.cpp:774`；门控`main.cpp:914`、1532、3021 |
| Follow/镜像/旋转 | plc_*与pos(double[7])；axis3/5_base_rel、axis2/7_hold_rel、4个prev、2个follow_cmd(double)；crawl两对象 | 初始化绑定`main.cpp:141`、181；运动`main.cpp:1532`、2650；更新prev3131 |
| 方向/按键 | dual_reverse_active、axis1_reverse_pressed_prev、axis6_effective_reverse_prev、spacing_recovery_button_pressed_prev(bool) | `main.cpp:1141`、1154、1257 |
| 先行 | axis1_delivery_mapping_active(bool)、progress_mm/applied_extra_mm(double) | clear/arm`main.cpp:233`；资格1190；计算2712；完成2753 |
| 公共启动/自检 | StartupState；self_check_done/has_self_check_flag/last_self_check_done、handle_reinit_req/last_*；pending_selfcheck_target(double[7])、axes(unsigned int)、两selfcheck bool | 有效观察`main.cpp:700`；初同步967；边沿1339；命令3400/3547 |
| 换手/回退 | PlannedReturnCoordinator、DualHandoffRequest、original_return_intent；interrupted mode/scope、resuming_return(bool)、fast两bool | helpers329；状态机1582；意图2794；续做2600；代次3069 |
| 故障/急停 | ReturnRecovery、return_ads_fault_hold/estop_hold_req/active(bool)、pending_error轴/事件序号/deadline、static next_estop_recover_req_ms | 故障1785；急停1282；恢复1432；补报669 |
| 手动与租约 | axis4_UI两bool、y_valve_open(bool)、injector_ui_direction(int[2])、deadline(ULONGLONG[2])、axis4_deadline；cylinder_manual_mode/position/restore[4]、written_mask | 过期1224；输出2918/3049；命令3393 |
| 轴4→6耦合 | last_ms(ULONGLONG)、active_prev(bool) | `main.cpp:193`、2687；planned接管时2644清零 |
| 力/标定 | ForceCalibrationConfig/State、ForceSampleFrame、ForceFeedbackState；zero_ok/fail_count(uint32)；两诊断string | 取零510；实时力2957；反馈3114；公共命令 |
| PLC重启/寻参 | homing::Record、pending/submitted/applied/之前control/record_pending(bool)、update_ms/refresh_generation、应用名与read标志 | `main.cpp:63`；重启998；校验/回执1035；记录1387 |
| 记录/诊断/发布 | ExperimentRecorder、HandleVelocityRecorder；drained_snapshots(vector)、handle_history(deque)、QPC频率/截止、loop_count(int)、各路径/string | 记录启动631；快照排空654；CSV3137；状态3211 |
| 新增仅记录状态 | event_cancel_clear_sequence(uint64)、event_hold_sources(int)、event_connection_hold_active(bool) | `main.cpp:455`、639；轮询结果657；保持边沿916 |

所有表项已确认。逐项精确类型/声明/显式写入/其它词法引用见B0.4与同名CSV；“其它引用”包括取址和方法调用，不能一概等同只读。AppContext保存别名，不复制Follow/回退状态。

## B0.3 直接状态结构体（字段组）

| 结构 | 保留字段/语义 | 声明与读写入口 |
|---|---|---|
| HandleFilterState | axis0_filtered/axis1_filtered(double)、inited(bool) | `control_types.h:52`；采样/重建/运动 |
| CrawlState ×2 | enabled/window_active(bool)、handle_ref/rot_ref/base_rel/rot_base_rel/start_abs/end_abs(double)；已无独立crawl阶段 | `control_types.h:391`；motion_sync与Follow |
| StartupState | phase(WaitForEnter=0/Done=11)、completed、loading_ready_symbol_available(bool) | `control_types.h:385`、590；公共StartControl |
| PlannedReturnLeg ×2 | active/index、target_abs/entry_rel、request_armed/acknowledged/started/possibly_started/done、request/ack/error/cancel事件序号、reported_error_id、cancel_required | `control_types.h:224`；refresh1632、begin1820、step2012 |
| PlannedReturnCoordinator | phase/mode/scope、legs/count/retry；phase/request/execution/retry清除计时；clamp/commit/handoff/release代次与applied；ADS命令purpose/seq/submit；done_snapshot；hold_axis1/3/5/6；clamp/restore子阶段 | `control_types.h:249`；提交329、输出1915、推进2012、发布3069 |
| Coordinator保留旧字段 | cancel_clear_confirmed/snapshot/t0/retry_after/timeout/rebased/hold_generation仍只随结构reset；未在本轮新增取消流程 | `control_types.h:269`；R1 B.11.3对这些原无读者字段的结论，声明未因本轮删除才失效 |
| DualHandoffRequest | axis1/axis6/wire_caused/gap_boundary/reverse/wire_reverse/axis4_caused(bool) | `control_types.h:327`；意图2794、回退进入/完成reset |
| SpacingRecoveryState | requested、phase；phase_t0/last_motion_tick(DWORD)；1/2/7保持与3/5/6命令(double)；pending_delta/moved/remaining、limit_logged | `control_types.h:349`；进入/Active2451、ExitSync2874、保持914 |
| ForceFeedbackState | 两侧enabled/输出/filtered/initialized；HandoffHold两份(active/completed/blend、锁存force/torque/origin/时刻)；HoldSample两份；历史/低通/hold开关 | `control_types.h:408`；force_feedback.cpp、回退1820/2416、3114 |
| ForceCalibrationState | 四个传感零点、theta0_deg(double)、zeroed(bool) | `force_calibration.h:66`；取零510、力2957 |
| ForceSampleFrame | 四原始short/电压double、轴1/2位置、valid、DWORD tick、int64 QPC | `control_types.h:565`；ADS快照构建2957 |
| ReturnRecovery私有状态 | holding/resume_pending/clear_applied/unfinished、mask/resumes、next_clear/clear_seq/observed_seq、reason | `return_recovery.h:63`；main故障/清Req/恢复/续做 |
| HandleNetDistance ×2 | net/actual/origin(double)、sequence(uint64)、sample_valid/following(bool) | `handle_net_distance.h:28`；main1510/3061 |
| homing::Record | 坐标/最后绝对位置/完整性/应用名/时间；通过load/保存更新 | `homing_record.h:1`；main63、998、1387 |
| AppContext | 设备/服务/配置指针、PLC数组别名、snapshot序号/QPC/velocity、Follow/ff/crawl/方向别名 | `control_types.h:597`；main148绑定、plc_io与motion_sync间接写 |

已确认：后台ADS/Zenoh/记录器/手柄对象的私有线程队列不在本表展开，仍以R1 A/D及其实现为准；这些模块除记录器schema以外未改变。所有状态字段精确声明保存在所引头文件，表中字段组避免把对象展开计数与main声明数混用。

## B0.4 每项长驻声明的精确引用

统计方式：`scratch/inventory.py:1`。当前136项中120项非const/constexpr；数组/对象各计1，静态节流量计入，lambda及其形参、每拍局部量不计。常量和服务对象一并列出以便核对口径。下表来自实际源码，不能把“显式写入”当成包含方法内或通过ctx别名的全部写入。

| 名称 | 类型 | 声明 | 显式写入/初始化 | 其它引用 |
|---|---|---|---|---|
| `diagnostic_build` | `const char*` | `main.cpp:38` | `main.cpp:38,40` | `main.cpp:49,717,844` |
| `diagnostic_exe` | `wchar_t[MAX_PATH]` | `main.cpp:43` | `main.cpp:43` | `main.cpp:44,45,63` |
| `diagnostic_path` | `std::wstring` | `main.cpp:45` | `main.cpp:45,47,48` | `main.cpp:45,46,47,866,1303` |
| `diagnostic_slash` | `const auto` | `main.cpp:46` | `main.cpp:46` | `main.cpp:47` |
| `serial_axis1_handle` | `constexpr DWORD` | `main.cpp:51` | `main.cpp:51` | `main.cpp:75,632,780,804` |
| `serial_axis6_handle` | `constexpr DWORD` | `main.cpp:52` | `main.cpp:52` | `main.cpp:76,633,788,817` |
| `ads_snapshot_wait_timeout_ms` | `constexpr DWORD` | `main.cpp:54` | `main.cpp:54` | `main.cpp:645` |
| `vis_publish_rate_hz` | `constexpr std::int64_t` | `main.cpp:56` | `main.cpp:56` | `main.cpp:3215` |
| `axis4_coupled_axis6_speed_mm_s` | `constexpr double` | `main.cpp:58` | `main.cpp:58` | `main.cpp:2694` |
| `hardcoded_ads_netid` | `const char*` | `main.cpp:59` | `main.cpp:59` | — |
| `cfg` | `ControlConfig` | `main.cpp:61` | `main.cpp:61` | `main.cpp:61,109,110,127,128,156,242,385,387,389,391,442,506,1110,1111,1115,1116,1145,1146,1148,1191,1206,1549,1551,1566,1570,1572,1662,1745,1767,1887,1889,1894,1896,1901,1903,1908,1910,2166,2214,2235,2261,2350,2441,2453,2455,2457,2458,2481,2503,2504,2505,2512,2537,2547,2555,2580,2595,2622,2626,2628,2631,2651,2652,2654,2710,2714,2715,2716,2718,2754,2770,2823,2834,2835,2837,2876,2877,2878,2880,2882,3127,3419,3422` |
| `cyl` | `const CylinderPreset` | `main.cpp:62` | `main.cpp:62` | `main.cpp:62,157,1492,1493,1494,1495,1947,1949,1964,1966,1975,1976,1982,1983,2544,2545,2546,2775,2776,2777,2778,3064,3068` |
| `run_directory` | `const std::filesystem::path` | `main.cpp:63` | `main.cpp:63` | `main.cpp:64,65,87` |
| `homing_record_path` | `const auto` | `main.cpp:64` | `main.cpp:64` | `main.cpp:66,1403,1411` |
| `homing_reuse_enabled` | `const bool` | `main.cpp:65` | `main.cpp:65` | `main.cpp:1050,1055` |
| `homing_record` | `homing::Record` | `main.cpp:66` | `main.cpp:66,1392,1393,1394,1395,1398,1399,1400,1409,1410` | `main.cpp:1050,1056,1057,1067,1395,1403,1405,1411` |
| `plc_application_name` | `std::string` | `main.cpp:67` | `main.cpp:67,993` | `main.cpp:67,1050,1057,1393` |
| `homing_reuse_pending` | `bool` | `main.cpp:68` | `main.cpp:68,1002,1059,1074,1090` | `main.cpp:986,1018,1405` |
| `homing_restore_submitted` | `bool` | `main.cpp:68` | `main.cpp:68,1003,1069,1089` | `main.cpp:1018,1071,1072,1081,1340,1405,1413` |
| `control_before_plc_restart` | `bool` | `main.cpp:69` | `main.cpp:69,1001` | `main.cpp:1097,1098,1371` |
| `homing_restore_applied` | `bool` | `main.cpp:69` | `main.cpp:69,1095,1370` | `main.cpp:1354,1369` |
| `homing_record_update_ms` | `ULONGLONG` | `main.cpp:70` | `main.cpp:70,1402,1408` | `main.cpp:1406` |
| `homing_record_pending` | `bool` | `main.cpp:71` | `main.cpp:71,1004,1356,1391` | `main.cpp:1388,1405` |
| `homing_record_refresh_generation` | `std::uint64_t` | `main.cpp:72` | `main.cpp:72,1357` | `main.cpp:1389` |
| `handle_axis1` | `Handle` | `main.cpp:75` | `main.cpp:75` | `main.cpp:75,117,120,128,152,775,778,794,796,802,821,822,824,829,907,919,1144,3118,3257,3259` |
| `handle_axis6` | `Handle` | `main.cpp:76` | `main.cpp:76` | `main.cpp:76,118,121,127,153,783,786,807,809,815,821,822,825,830,908,920,3117,3258,3260` |
| `ads` | `CADSComm` | `main.cpp:77` | `main.cpp:77` | `main.cpp:77,96,149` |
| `axis1_net_distance` | `HandleNetDistance` | `main.cpp:78` | `main.cpp:78` | `main.cpp:78,741,1522,3063,3376` |
| `axis6_net_distance` | `HandleNetDistance` | `main.cpp:78` | `main.cpp:78` | `main.cpp:78,742,1525,3065,3377` |
| `experiment_recorder` | `ExperimentRecorder` | `main.cpp:79` | `main.cpp:79` | `main.cpp:79,83,84,634,637,2958,2959,2961,3138,3141,3177,3207,3300` |
| `ads_snapshot_sequence` | `std::uint64_t` | `main.cpp:80` | `main.cpp:80,530,616,648` | `main.cpp:83,513,645,722,2006,2251,2298,2310,2358` |
| `handle_velocity_recorder` | `HandleVelocityRecorder` | `main.cpp:86` | `main.cpp:86` | `main.cpp:86,88` |
| `handle_velocity_path` | `const auto` | `main.cpp:87` | `main.cpp:87` | `main.cpp:88,89` |
| `zenoh_service` | `ZenohService` | `main.cpp:92` | `main.cpp:92` | `main.cpp:92,111,3389,3396,3592` |
| `axis1_handle_filter` | `HandleFilterState` | `main.cpp:93` | `main.cpp:93,1273` | `main.cpp:93,154,882,884,886,1107,1275,1540,1541,3132,3134` |
| `axis6_handle_filter` | `HandleFilterState` | `main.cpp:94` | `main.cpp:94,1274` | `main.cpp:94,155,883,885,887,1112,1276,1542,1543,3133,3135` |
| `ads_communication` | `AdsCommunicationService` | `main.cpp:96` | `main.cpp:96` | `main.cpp:96,97,98,164,394,414,468,518,599,644,654,655,656,661,743,744,989,1036,1048,1069,1070,1087,1324,1357,1359,1362,1389,1420,1421,1443,1463,2039,2160,2373,2403,3069,3433,3435` |
| `arm_manual_ads` | `ArmManualAdsService` | `main.cpp:97` | `main.cpp:97` | `main.cpp:97,102,3322,3495,3498,3502,3505,3508,3512,3516,3520` |
| `axis1_input_handle` | `Handle* const` | `main.cpp:120` | `main.cpp:120` | `main.cpp:150,882,1108,1109,1142,1273` |
| `axis6_input_handle` | `Handle* const` | `main.cpp:121` | `main.cpp:121` | `main.cpp:151,883,1113,1114,1143,1274` |
| `ff` | `ForceFeedbackState` | `main.cpp:122` | `main.cpp:122,136,3486` | `main.cpp:122,135,217,537,1007,1872,1873,1874,1875,2419,2423,2961,3005,3115,3171,3172,3173,3174,3175,3176,3272,3273,3274,3282,3283,3284,3285,3288,3289,3365,3366,3367,3481,3487,3489` |
| `pos` | `double[7]` | `main.cpp:141` | `main.cpp:141,1536,1537,1937,1940,1941,1942,1956,1959,1971,1972,1973,1974,1980,1981,2449,2548,2549,2550,2606,2607,2608,2768,2769,2773,2774` | `main.cpp:158,733,2771,2822,3025` |
| `plc_act_pos` | `double[7]` | `main.cpp:142` | `main.cpp:142` | `main.cpp:159,960,961,964,979,980,1312,1313,1545,1546,1547,1568,1662,1837,1838,1839,1840,1847,1855,1938,1957,2340,2419,2423,2488,2489,2490,2491,2492,2493,2876,2877,2878,3122,3123,3227` |
| `plc_init_pos` | `double[7]` | `main.cpp:143` | `main.cpp:143` | `main.cpp:160,733,1545,1546,1547,1568,1662,1939,1958,2419,2423,2454,2456,2459,2707,2708,2768,2773,3122,3123` |
| `plc_leftlimit` | `double[7]` | `main.cpp:144` | `main.cpp:144` | `main.cpp:161,501,1564,2454,2456,2459,2477,2478,2630,2631,2709,2710,2711,2813,2829,2839,3236,3237` |
| `plc_act_pos_from_left` | `double[7]` | `main.cpp:145` | `main.cpp:145` | `main.cpp:162,3228` |
| `ctx` | `AppContext` | `main.cpp:148` | `main.cpp:148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,164,214,215,216,217,218,219,220,221,222,223,224,225,226,227` | `main.cpp:148,166,167,170,174,178,229,230,231,232,249,258,895,1758,1778,2482,2619,2623,2653,2822,2823,3235` |
| `axis3_base_rel` | `double` | `main.cpp:181` | `main.cpp:181,2760` | `main.cpp:218` |
| `axis5_base_rel` | `double` | `main.cpp:182` | `main.cpp:182,2761` | `main.cpp:219,2707` |
| `axis2_hold_rel` | `double` | `main.cpp:184` | `main.cpp:184,279,960,979,1312,2771` | `main.cpp:220,1536,1940,1972` |
| `axis7_hold_rel` | `double` | `main.cpp:185` | `main.cpp:185,280,961,980,1313,2442` | `main.cpp:221,1537,1959,1981,2448` |
| `axis1_prev_linear_filtered` | `double` | `main.cpp:187` | `main.cpp:187,884,1275,2862,3132` | `main.cpp:222,1549` |
| `axis6_prev_linear_filtered` | `double` | `main.cpp:188` | `main.cpp:188,885,1276,2863,3133` | `main.cpp:223,1570` |
| `axis1_prev_rot_filtered` | `double` | `main.cpp:189` | `main.cpp:189,886,2864,3134` | `main.cpp:224` |
| `axis6_prev_rot_filtered` | `double` | `main.cpp:190` | `main.cpp:190,887,2865,3135` | `main.cpp:225` |
| `axis1_follow_cmd_abs` | `double` | `main.cpp:191` | `main.cpp:191,281,2766` | `main.cpp:226,2705` |
| `axis6_follow_cmd_abs` | `double` | `main.cpp:192` | `main.cpp:192,282,2767` | `main.cpp:227,2706` |
| `axis4_axis6_coupling_last_ms` | `ULONGLONG` | `main.cpp:193` | `main.cpp:193,1253,2645,2696,2701,2870` | `main.cpp:2689,2692` |
| `axis4_axis6_coupling_active_prev` | `bool` | `main.cpp:194` | `main.cpp:194,1252,2644,2697,2702,2869` | `main.cpp:2689` |
| `axis1_delivery_mapping_active` | `bool` | `main.cpp:196` | `main.cpp:196,236,242,2757` | `main.cpp:235,243,2713,2756` |
| `axis1_delivery_mapping_progress_mm` | `double` | `main.cpp:197` | `main.cpp:197,237,244,2752` | `main.cpp:2718,2754` |
| `axis1_delivery_mapping_applied_extra_mm` | `double` | `main.cpp:198` | `main.cpp:198,238,245,2753` | — |
| `dual_reverse_active` | `bool` | `main.cpp:201` | `main.cpp:201,1158` | `main.cpp:214,1162` |
| `startup` | `StartupState` | `main.cpp:203` | `main.cpp:203,771,977,978,1013,1014,1097,1098,1371,1372,3571,3572` | `main.cpp:203,254,1223,1372,1376,1377,1426,1427,1471,1472,1484,2484,2498,3246,3278,3279,3567` |
| `cal_cfg` | `ForceCalibrationConfig` | `main.cpp:204` | `main.cpp:204` | `main.cpp:204,3000,3128,3155,3159,3290` |
| `cal_state` | `ForceCalibrationState` | `main.cpp:205` | `main.cpp:205,531,532,533,534,535,536` | `main.cpp:205,539,540,541,542,543,2997,3000,3007,3129,3152,3155,3159,3161,3162,3166,3167,3275` |
| `axis1_crawl` | `CrawlState` | `main.cpp:206` | `main.cpp:206,211,1160,1161,2676` | `main.cpp:206,215,1555,1560,1561,1562,1563,1743,1744,2616,2625,2655,2656,2672,2678,2708,2769,2770` |
| `axis6_crawl` | `CrawlState` | `main.cpp:207` | `main.cpp:207,1368,3569` | `main.cpp:207,216,1576,1753,1754,2441,2618` |
| `planned_return` | `PlannedReturnCoordinator` | `main.cpp:208` | `main.cpp:208,337,338,339,397,398,399,474,1656,1830,1831,1832,1836,1837,1838,1839,1840,1843,1851,1998,1999,2002,2004,2005,2006,2026,2027,2028,2042,2043,2051,2053,2054,2059,2109,2110,2111,2134,2135,2141,2142,2143,2144,2154,2155,2168,2169,2201,2203,2216,2231,2251,2252,2255,2296,2297,2298,2342,2343,2344,2345,2365,2379,2380,2381,2393,2395,2396,2406,2407,3087,3095,3102,3109,3110` | `main.cpp:208,326,344,346,355,357,371,372,376,378,410,415,420,425,430,434,439,440,441,443,448,449,459,463,467,469,479,493,494,495,718,719,720,721,727,729,853,854,856,872,878,955,1155,1201,1202,1215,1218,1250,1291,1294,1416,1453,1481,1516,1654,1696,1698,1706,1707,1709,1716,1717,1719,1726,1728,1736,1738,1755,1780,1828,1829,1843,1851,1857,1859,1860,1886,1888,1893,1895,1900,1902,1907,1909,1917,1918,1920,1921,1922,1923,1925,1926,1927,1928,1929,1935,1941,1942,1944,1946,1948,1954,1961,1963,1965,1971,1973,1974,1980,1989,1991,1993,2014,2015,2017,2019,2020,2023,2033,2036,2037,2038,2040,2045,2046,2048,2064,2067,2069,2079,2086,2088,2098,2128,2159,2161,2165,2176,2179,2185,2187,2199,2213,2222,2234,2240,2259,2260,2266,2272,2274,2284,2310,2311,2314,2325,2326,2328,2349,2356,2358,2370,2372,2374,2385,2387,2390,2391,2400,2402,2404,2410,2411,2418,2422,2426,2475,2486,2641,2825,2926,3028,3029,3030,3034,3036,3050,3070,3071,3072,3076,3078,3090,3091,3097,3098,3099,3104,3549` |
| `dual_handoff` | `DualHandoffRequest` | `main.cpp:209` | `main.cpp:209,2793,2797,2798,2799,2800,2801,2802` | `main.cpp:209,450,458,1215,1868,2663,2665,2666,2667,2668,2670,2680,2683,2684,2791,2795,2798,2799,2802,2803,2805,2806,2807,2810,2811,2812,2814,2815,2817,2837,2854,2858,2926,3050` |
| `spacing_recovery` | `SpacingRecoveryState` | `main.cpp:210` | `main.cpp:210,1177,1183,2468,2470,2471,2488,2489,2490,2491,2492,2493,2494,2527,2530,2531,2532,2533,2534,2554,2557,2559,2566,2571,2576,2579,2588,2589,2590,2591,2592,2593,2597,2601` | `main.cpp:210,745,747,749,751,939,942,944,946,1008,1009,1156,1163,1198,1199,1211,1218,1243,1329,1331,1332,1365,1366,1418,1419,1453,1454,1481,1515,2454,2456,2459,2465,2466,2475,2500,2501,2509,2510,2512,2516,2517,2541,2548,2549,2550,2552,2555,2563,2565,2582,2584,2595,2598,2606,2607,2608,2874,2876,2877,2878,2883,2887,2894,2895,2925,2940,3011,3119,3291,3292,3293,3549,3559,3560` |
| `control_active` | `bool` | `main.cpp:251` | `main.cpp:251,769,976,1012,1138,1290,1346,1377,1383,1427,1472,1489,1795,2897,3574` | `main.cpp:768,975,1001,1011,1137,1171,1192,1210,1216,1289,1345,1376,1382,1426,1471,1480,1488,1512,1533,1794,2485,2896,2934,3009,3023,3119,3247,3549,3573` |
| `self_check_done` | `bool` | `main.cpp:285` | `main.cpp:285,702` | `main.cpp:288,707,758,765,766,850,1019,1340,1342,1352,1388,1406,1413,1483,3251,3270,3556` |
| `has_self_check_flag` | `bool` | `main.cpp:286` | `main.cpp:286,764` | `main.cpp:761,1340,3556` |
| `last_self_check_done` | `bool` | `main.cpp:288` | `main.cpp:288,765,1096,1413` | `main.cpp:1340,1342,1352` |
| `handle_reinit_req` | `bool` | `main.cpp:289` | `main.cpp:289,709` | `main.cpp:1416,1430` |
| `last_handle_reinit_req` | `bool` | `main.cpp:290` | `main.cpp:290,1430` | `main.cpp:1416` |
| `estop_hold_req` | `bool` | `main.cpp:291` | `main.cpp:291,710` | `main.cpp:712,1283,1285` |
| `estop_hold_active` | `bool` | `main.cpp:292` | `main.cpp:292,1288,1311` | `main.cpp:712,1171,1174,1196,1209,1217,1242,1283,1317,1328,1334,1373,1422,1449,1484,1514,1532,2885,2921,2934,3010,3022,3121,3248,3254,3554` |
| `axis1_reverse_pressed_prev` | `bool` | `main.cpp:293` | `main.cpp:293,1279` | `main.cpp:720,1265` |
| `axis6_effective_reverse_prev` | `bool` | `main.cpp:294` | `main.cpp:294,1280` | `main.cpp:720` |
| `axis4_manual_error_prev` | `bool` | `main.cpp:295` | `main.cpp:295,2912` | `main.cpp:2908` |
| `axis4_manual_error_id_prev` | `unsigned long` | `main.cpp:296` | `main.cpp:296,2913` | `main.cpp:2908` |
| `axis4_ui_forward_pressed` | `bool` | `main.cpp:297` | `main.cpp:297,1238,3525` | `main.cpp:1234,1245,3380` |
| `axis4_ui_reverse_pressed` | `bool` | `main.cpp:298` | `main.cpp:298,1239,3526` | `main.cpp:1234,1246,3380` |
| `y_valve_open` | `bool` | `main.cpp:299` | `main.cpp:299,3533` | `main.cpp:2916,3381,3534` |
| `injector_ui_direction` | `int[2]` | `main.cpp:300` | `main.cpp:300,1230,3541` | `main.cpp:1226,1260,1262,3379` |
| `injector_ui_jog_deadline_ms` | `ULONGLONG[2]` | `main.cpp:301` | `main.cpp:301,1231,3542` | `main.cpp:1227,1228` |
| `axis4_ui_jog_deadline_ms` | `ULONGLONG` | `main.cpp:304` | `main.cpp:304,1240,3527` | `main.cpp:1235,1236` |
| `manual_jog_lease_ms` | `constexpr ULONGLONG` | `main.cpp:305` | `main.cpp:305` | `main.cpp:3529,3544` |
| `axis1_fast_return` | `bool` | `main.cpp:306` | `main.cpp:306,451,481,901,958,1133,1931,1943,2000,2253,2363` | `main.cpp:3026,3249` |
| `axis6_fast_retract` | `bool` | `main.cpp:307` | `main.cpp:307,452,482,902,959,1134,1932,1960,2001,2254,2364` | `main.cpp:3027,3250` |
| `return_ads_fault_hold` | `bool` | `main.cpp:308` | `main.cpp:308,1297,1470,1793` | `main.cpp:1135,1171,1175,1195,1208,1217,1295,1328,1433,1480,1513,1532,1791,2885,3022,3120,3253,3385,3553` |
| `return_recovery` | `ReturnRecovery` | `main.cpp:309` | `main.cpp:309` | `main.cpp:309,1296,1439,1443,1446,1457,1461,1465,1467,1473,1474,1792,1865,2431,2610,2612,2613,2614,2637,3253` |
| `interrupted_return_mode` | `PlannedReturnMode` | `main.cpp:310` | `main.cpp:310,1866` | `main.cpp:2620,2634` |
| `interrupted_return_scope` | `PlannedReturnRebaseScope` | `main.cpp:311` | `main.cpp:311,1867` | `main.cpp:2634` |
| `original_return_intent` | `DualHandoffRequest` | `main.cpp:312` | `main.cpp:312,1868,1869,1870` | `main.cpp:312,2615,2627,2628` |
| `resuming_return` | `bool` | `main.cpp:313` | `main.cpp:313,2633,2636` | `main.cpp:1863` |
| `recovery_event_text` | `std::string` | `main.cpp:314` | `main.cpp:314,824,826,903,909,911,1058,1075,1094,1100,1474` | `main.cpp:314,826,912,962,1060,1076,1101,1475,3268` |
| `pending_return_error_axis` | `int` | `main.cpp:316` | `main.cpp:316,687,694,1802,1809` | `main.cpp:669,673,678,685` |
| `pending_return_error_event_sequence` | `std::uint64_t` | `main.cpp:317` | `main.cpp:317,688,695,1803,1810` | `main.cpp:683` |
| `pending_return_error_deadline_ms` | `ULONGLONG` | `main.cpp:318` | `main.cpp:318,689,696,1804,1811` | `main.cpp:691,692` |
| `force_sample` | `ForceSampleFrame` | `main.cpp:319` | `main.cpp:319,2978,2982` | `main.cpp:319,2985,2997,3000,3008,3116,3280,3281,3308` |
| `loop_count` | `int` | `main.cpp:320` | `main.cpp:320` | `main.cpp:1132,3286` |
| `force_feedback_diag_reason` | `std::string` | `main.cpp:321` | `main.cpp:321,3017` | `main.cpp:321,3013` |
| `force_sample_diag_reason` | `std::string` | `main.cpp:322` | `main.cpp:322,2989` | `main.cpp:322,2988,2993` |
| `event_cancel_clear_sequence` | `std::uint64_t` | `main.cpp:455` | `main.cpp:455,494,666` | `main.cpp:657,661` |
| `remote_force_zero_ok_count` | `std::uint32_t` | `main.cpp:547` | `main.cpp:547` | `main.cpp:552,3382` |
| `remote_force_zero_fail_count` | `std::uint32_t` | `main.cpp:548` | `main.cpp:548` | `main.cpp:553,3383` |
| `initial_sync_done` | `bool` | `main.cpp:557` | `main.cpp:557,974` | `main.cpp:877,893,916,920,950,967,1482,1512,3231,3384,3553` |
| `plc_app_name_read` | `bool` | `main.cpp:558` | `main.cpp:558,994,1005` | `main.cpp:986,1019,1388` |
| `handle1_next_retry_ms` | `ULONGLONG` | `main.cpp:559` | `main.cpp:559,777,803` | `main.cpp:775` |
| `handle6_next_retry_ms` | `ULONGLONG` | `main.cpp:560` | `main.cpp:560,785,816` | `main.cpp:783` |
| `handle_soft_hold_active` | `bool` | `main.cpp:561` | `main.cpp:561,821` | `main.cpp:878,880,893,916,952,969,1105,1452,1483,1513,1532,2884,3120,3386,3553` |
| `connection_hold_active_prev` | `bool` | `main.cpp:562` | `main.cpp:562,935` | `main.cpp:877,933,934` |
| `cylinder_manual_mode` | `CylinderManualMode[4]` | `main.cpp:569` | `main.cpp:569,586,2525,3453,3465` | `main.cpp:584,1502,2944,3371` |
| `cylinder_manual_position` | `unsigned short[4]` | `main.cpp:575` | `main.cpp:575,3464` | `main.cpp:2945` |
| `cylinder_manual_restore` | `unsigned short[4]` | `main.cpp:576` | `main.cpp:576,605` | `main.cpp:2948` |
| `cylinder_manual_written_mask` | `std::uint8_t` | `main.cpp:577` | `main.cpp:577,581,606` | `main.cpp:593,2936,2947` |
| `spacing_recovery_button_pressed_prev` | `bool` | `main.cpp:609` | `main.cpp:609,1186` | `main.cpp:1150,1152` |
| `pending_selfcheck_target` | `double[7]` | `main.cpp:611` | `main.cpp:611,3405` | `main.cpp:3413,3414,3415,3416,3434` |
| `pending_selfcheck_axes` | `unsigned int` | `main.cpp:612` | `main.cpp:612,3406,3423,3551` | `main.cpp:3417` |
| `selfcheck_command_error` | `bool` | `main.cpp:613` | `main.cpp:613,708,3407,3428,3437,3441` | `main.cpp:3270` |
| `selfcheck_start_pending` | `bool` | `main.cpp:614` | `main.cpp:614,706,3442` | `main.cpp:2923,3271,3424` |
| `manual_cylinder_override_active` | `bool` | `main.cpp:615` | `main.cpp:615,1499,1504` | `main.cpp:1517` |
| `handled_plc_restart_count` | `std::uint64_t` | `main.cpp:617` | `main.cpp:617,1006` | `main.cpp:998` |
| `handled_reconnect_count` | `std::uint64_t` | `main.cpp:618` | `main.cpp:618,740` | `main.cpp:738` |
| `ads_soft_hold_active` | `bool` | `main.cpp:619` | `main.cpp:619,837,875,898` | `main.cpp:835,872,877,897,916,917,951,1194,1207,1374,1423,1484,1513,1532,2884,2980,3120,3387,3555` |
| `drained_ads_snapshots` | `std::vector<AdsFastSnapshot>` | `main.cpp:620` | `main.cpp:620` | `main.cpp:620,654,3142` |
| `handle_record_history` | `std::deque<HandleRecordSnapshot>` | `main.cpp:627` | `main.cpp:627` | `main.cpp:627,1124,1126,1127,1129,3191` |
| `vis_qpc_frequency` | `LARGE_INTEGER` | `main.cpp:628` | `main.cpp:628` | `main.cpp:628,629,1125,3215` |
| `next_vis_publish_qpc` | `std::int64_t` | `main.cpp:630` | `main.cpp:630,3216,3219,3222` | `main.cpp:3216,3217,3220` |
| `recording_info` | `ExperimentStartInfo` | `main.cpp:631` | `main.cpp:631,632,633` | `main.cpp:631,634` |
| `event_hold_sources` | `int` | `main.cpp:639` | `main.cpp:639,930` | `main.cpp:924,926,929` |
| `event_connection_hold_active` | `bool` | `main.cpp:640` | `main.cpp:640,925` | `main.cpp:921,923` |
| `next_estop_recover_req_ms` | `ULONGLONG` | `main.cpp:1319` | `main.cpp:1319,1323` | `main.cpp:1321` |
