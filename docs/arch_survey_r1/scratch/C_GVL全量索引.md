# GVL 全量声明与 PLC 字面读写索引（自动抽取）

范围：介入机器人/C从端/250902/Untitled2/Untitled2.plcproj:21 编译项中的 13 个 POU。P/ 统一展开为 介入机器人/C从端/250902/Untitled2/；GVLs/ 也相对此路径。
确定程度：本表为已确认的声明与字面引用；W=赋值左值，R=其余字面读取，RW/FB=功能块实例调用，W/FBD=FBD XML 的 LValue。不是对第三方 MC 库内部行为的验证。
每个单元格给该 POU/角色的首个行号和引用数量；逐行索引见 plc_gvl_refs.tsv。

|变量|类型/初值（原样）|说明注释|PLC 写/调用方|PLC 读取方|声明出处|
|---|---|---|---|---|---|
|G.err_backoff_active|ARRAY[1..7] OF BOOL|—|err [W] P/POUs/err.TcPOU:73（3处）|err [R] P/POUs/err.TcPOU:123（1处）|GVLs/G.TcGVL:7|
|G.err_backoff_failed|ARRAY[1..7] OF BOOL|—|err [W] P/POUs/err.TcPOU:72（2处）|无字面读取|GVLs/G.TcGVL:8|
|G.axis2_sync_error|BOOL|—|Axis2Sync [W] P/POUs/Axis2Sync.TcPOU:21（1处）|Axis2Sync [R] P/POUs/Axis2Sync.TcPOU:23（3处）|GVLs/G.TcGVL:9|
|G.axis2_sync_error_id|UDINT|—|Axis2Sync [W] P/POUs/Axis2Sync.TcPOU:22（1处）|无字面读取|GVLs/G.TcGVL:10|
|G.homing_restore_req|BOOL|—|MAIN [W] P/POUs/MAIN.TcPOU:125（1处）|MAIN [R] P/POUs/MAIN.TcPOU:124（1处）|GVLs/G.TcGVL:11|
|G.homing_restore_leftlimit|ARRAY[1..7] OF LREAL|—|无字面写/调用|MAIN [R] P/POUs/MAIN.TcPOU:150（1处）|GVLs/G.TcGVL:12|
|G.homing_restore_init_pos|ARRAY[1..7] OF LREAL|—|无字面写/调用|MAIN [R] P/POUs/MAIN.TcPOU:151（1处）|GVLs/G.TcGVL:13|
|G.homing_restore_status|DINT|—|MAIN [W] P/POUs/MAIN.TcPOU:126（3处）|无字面读取|GVLs/G.TcGVL:14|
|G.homing_restore_reject_reason|DINT|—|MAIN [W] P/POUs/MAIN.TcPOU:127（4处）|无字面读取|GVLs/G.TcGVL:15|
|G.gen_state|state := _init|—|MAIN [W] P/POUs/MAIN.TcPOU:45（2处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:566（1处）<br>clear_err [W] P/POUs/clear_err.TcPOU:59（3处）<br>err [W] P/POUs/err.TcPOU:160（3处）<br>handle [W] P/POUs/handle.TcPOU:251（5处）<br>init [W] P/POUs/init.TcPOU:52（3处）<br>reset [W] P/POUs/reset.TcPOU:71（2处）|MAIN [R] P/POUs/MAIN.TcPOU:91（7处）|GVLs/G.TcGVL:17|
|G.axis|ARRAY[1..7] OF axis_REF|—|MAIN [RW/Axis] P/POUs/MAIN.TcPOU:138（2处）<br>MAIN [RW/FB] P/POUs/MAIN.TcPOU:36（1处）<br>SelfCheck [RW/Axis] P/POUs/SelfCheck.TcPOU:87（34处）<br>err [RW/Axis] P/POUs/err.TcPOU:77（1处）<br>handle [RW/Axis] P/POUs/handle.TcPOU:398（11处）|Axis2Sync [R] P/POUs/Axis2Sync.TcPOU:63（2处）<br>MAIN [R] P/POUs/MAIN.TcPOU:107（4处）<br>MAIN [R/FBD] P/POUs/MAIN.TcPOU:348（7处）<br>SelfCheck [R] P/POUs/SelfCheck.TcPOU:159（21处）<br>clear_err [R] P/POUs/clear_err.TcPOU:29（1处）<br>err [R] P/POUs/err.TcPOU:50（11处）<br>handle [R] P/POUs/handle.TcPOU:200（23处）<br>init [R] P/POUs/init.TcPOU:49（2处）<br>reset [R] P/POUs/reset.TcPOU:54（2处）|GVLs/G.TcGVL:18|
|G.power_|ARRAY[1..7] OF MC_Power|—|MAIN [RW/FB] P/POUs/MAIN.TcPOU:255（7处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:286（3处）<br>clear_err [W] P/POUs/clear_err.TcPOU:72（1处）<br>err [W] P/POUs/err.TcPOU:167（5处）<br>init [W] P/POUs/init.TcPOU:34（2处）|Axis2Sync [R] P/POUs/Axis2Sync.TcPOU:52（2处）<br>MAIN [R/FBD] P/POUs/MAIN.TcPOU:367（7处）<br>handle [R] P/POUs/handle.TcPOU:213（3处）|GVLs/G.TcGVL:19|
|G.power_output|ARRAY[1..7] OF ST_McOutputs|—|MAIN [W/FBD] P/POUs/MAIN.TcPOU:230（35处）|Axis2Sync [R] P/POUs/Axis2Sync.TcPOU:57（2处）<br>MAIN [R] P/POUs/MAIN.TcPOU:135（2处）<br>SelfCheck [R] P/POUs/SelfCheck.TcPOU:120（17处）<br>clear_err [R] P/POUs/clear_err.TcPOU:29（2处）<br>err [R] P/POUs/err.TcPOU:46（6处）<br>handle [R] P/POUs/handle.TcPOU:192（4处）<br>init [R] P/POUs/init.TcPOU:49（2处）<br>reset [R] P/POUs/reset.TcPOU:70（1处）|GVLs/G.TcGVL:20|
|G.axis_Reset|ARRAY[1..7] OF MC_Reset|—|MAIN [RW/FB] P/POUs/MAIN.TcPOU:2487（7处）<br>MAIN [W] P/POUs/MAIN.TcPOU:56（1处）<br>SelfCheck [RW/FB] P/POUs/SelfCheck.TcPOU:88（11处）<br>clear_err [W] P/POUs/clear_err.TcPOU:50（1处）<br>err [W] P/POUs/err.TcPOU:147（7处）<br>handle [W] P/POUs/handle.TcPOU:889（1处）<br>init [W] P/POUs/init.TcPOU:33（1处）<br>reset [W] P/POUs/reset.TcPOU:32（3处）|Axis2Sync [R] P/POUs/Axis2Sync.TcPOU:47（1处）<br>MAIN [R/FBD] P/POUs/MAIN.TcPOU:2583（7处）<br>SelfCheck [R] P/POUs/SelfCheck.TcPOU:266（4处）|GVLs/G.TcGVL:21|
|G.reset_output|ARRAY[1..7] OF ST_McOutputs|—|MAIN [W/FBD] P/POUs/MAIN.TcPOU:2462（28处）|MAIN [R] P/POUs/MAIN.TcPOU:136（1处）<br>clear_err [R] P/POUs/clear_err.TcPOU:29（1处）<br>err [R] P/POUs/err.TcPOU:50（3处）<br>handle [R] P/POUs/handle.TcPOU:200（2处）<br>reset [R] P/POUs/reset.TcPOU:54（2处）|GVLs/G.TcGVL:22|
|G.SetPointGenEnable|ARRAY[1..7] OF MC_ExtSetPointGenEnable|—|MAIN [RW/FB] P/POUs/MAIN.TcPOU:3858（7处）<br>MAIN [W] P/POUs/MAIN.TcPOU:54（2处）<br>clear_err [W] P/POUs/clear_err.TcPOU:48（1处）<br>err [W] P/POUs/err.TcPOU:91（3处）<br>handle [W] P/POUs/handle.TcPOU:265（4处）<br>init [W] P/POUs/init.TcPOU:31（1处）<br>reset [W] P/POUs/reset.TcPOU:33（1处）|MAIN [R/FBD] P/POUs/MAIN.TcPOU:3970（21处）<br>handle [R] P/POUs/handle.TcPOU:221（8处）|GVLs/G.TcGVL:23|
|G.SetPointGenDisable|ARRAY[1..7] OF MC_ExtSetPointGenDisable|—|MAIN [RW/FB] P/POUs/MAIN.TcPOU:138（9处）<br>MAIN [W] P/POUs/MAIN.TcPOU:55（2处）<br>SelfCheck [RW/FB] P/POUs/SelfCheck.TcPOU:87（7处）<br>clear_err [W] P/POUs/clear_err.TcPOU:49（1处）<br>err [W] P/POUs/err.TcPOU:92（4处）<br>handle [W] P/POUs/handle.TcPOU:234（3处）<br>init [W] P/POUs/init.TcPOU:32（1处）<br>reset [W] P/POUs/reset.TcPOU:34（2处）|MAIN [R] P/POUs/MAIN.TcPOU:139（2处）<br>MAIN [R/FBD] P/POUs/MAIN.TcPOU:4221（7处）<br>SelfCheck [R] P/POUs/SelfCheck.TcPOU:143（3处）<br>err [R] P/POUs/err.TcPOU:87（1处）<br>handle [R] P/POUs/handle.TcPOU:223（9处）|GVLs/G.TcGVL:24|
|G.SetPointGenEnable_output|ARRAY[1..7] OF ST_McOutputs|—|MAIN [W/FBD] P/POUs/MAIN.TcPOU:3877（21处）|无字面读取|GVLs/G.TcGVL:25|
|G.SetPointGenDisable_output|ARRAY[1..7] OF ST_McOutputs|—|MAIN [W/FBD] P/POUs/MAIN.TcPOU:3833（28处）|无字面读取|GVLs/G.TcGVL:25|
|G.move_jog|ARRAY [1..7] OF MC_Jog|—|无字面写/调用|无字面读取|GVLs/G.TcGVL:26|
|G.enable|BOOL|跟随运动变量|MAIN [W/FBD] P/POUs/MAIN.TcPOU:3925（1处）|无字面读取|GVLs/G.TcGVL:27|
|G.disable|BOOL|跟随运动变量|MAIN [W/FBD] P/POUs/MAIN.TcPOU:4176（1处）|无字面读取|GVLs/G.TcGVL:27|
|G.flag|INT:=0|—|无字面写/调用|无字面读取|GVLs/G.TcGVL:28|
|G.flagpos|LREAL:=0|遗留调试量，当前无业务读取|无字面写/调用|无字面读取|GVLs/G.TcGVL:29|
|G.init_pos|ARRAY[1..7] OF LREAL := [30, 9, 0, 0, 0, 30, 13]|—|MAIN [W] P/POUs/MAIN.TcPOU:151（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:447（4处）<br>init [W] P/POUs/init.TcPOU:77（1处）|MAIN [R] P/POUs/MAIN.TcPOU:161（1处）<br>SelfCheck [R] P/POUs/SelfCheck.TcPOU:558（1处）<br>handle [R] P/POUs/handle.TcPOU:229（13处）|GVLs/G.TcGVL:31|
|G.refer|ARRAY[1..7] OF LREAL|—|MAIN [W] P/POUs/MAIN.TcPOU:163（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:560（1处）<br>handle [W] P/POUs/handle.TcPOU:237（7处）|handle [R] P/POUs/handle.TcPOU:229（16处）|GVLs/G.TcGVL:32|
|G.Act_pos|ARRAY[1..7] OF LREAL|—|MAIN [W] P/POUs/MAIN.TcPOU:161（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:558（1处）<br>handle [W] P/POUs/handle.TcPOU:235（6处）|MAIN [R] P/POUs/MAIN.TcPOU:163（4处）<br>SelfCheck [R] P/POUs/SelfCheck.TcPOU:560（4处）<br>handle [R] P/POUs/handle.TcPOU:237（28处）|GVLs/G.TcGVL:32|
|G.act_h|ARRAY[1..7] OF LREAL|—|MAIN [W] P/POUs/MAIN.TcPOU:165（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:562（1处）<br>handle [W] P/POUs/handle.TcPOU:239（8处）|handle [R] P/POUs/handle.TcPOU:543（6处）|GVLs/G.TcGVL:32|
|G.act_hf|ARRAY[1..7] OF LREAL|—|MAIN [W] P/POUs/MAIN.TcPOU:166（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:563（1处）<br>handle [W] P/POUs/handle.TcPOU:240（7处）|无字面读取|GVLs/G.TcGVL:32|
|G.leftlimit|ARRAY[1..7] OF LREAL|—|MAIN [W] P/POUs/MAIN.TcPOU:150（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:185（2处）|MAIN [R] P/POUs/MAIN.TcPOU:162（1处）<br>SelfCheck [R] P/POUs/SelfCheck.TcPOU:361（3处）<br>err [R] P/POUs/err.TcPOU:100（2处）<br>handle [R] P/POUs/handle.TcPOU:236（11处）|GVLs/G.TcGVL:35|
|G.act_pos_from_left|ARRAY[1..7] OF LREAL|—|MAIN [W] P/POUs/MAIN.TcPOU:162（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:559（1处）<br>handle [W] P/POUs/handle.TcPOU:236（6处）|MAIN [R] P/POUs/MAIN.TcPOU:167（1处）<br>SelfCheck [R] P/POUs/SelfCheck.TcPOU:564（1处）<br>handle [R] P/POUs/handle.TcPOU:241（4处）|GVLs/G.TcGVL:36|
|G.refer_from_left|ARRAY[1..7] OF LREAL|—|MAIN [W] P/POUs/MAIN.TcPOU:167（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:564（1处）<br>handle [W] P/POUs/handle.TcPOU:241（8处）|无字面读取|GVLs/G.TcGVL:37|
|G.self_check_done|BOOL := FALSE|—|MAIN [W] P/POUs/MAIN.TcPOU:46（2处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:546（1处）<br>init [W] P/POUs/init.TcPOU:36（1处）|clear_err [R] P/POUs/clear_err.TcPOU:63（1处）<br>err [R] P/POUs/err.TcPOU:83（1处）|GVLs/G.TcGVL:38|
|G.selfcheck_start_req|BOOL := FALSE|—|MAIN [W] P/POUs/MAIN.TcPOU:47（3处）|MAIN [R] P/POUs/MAIN.TcPOU:131（2处）|GVLs/G.TcGVL:39|
|G.selfcheck_status|DINT := 0|0=未就绪，1=等待启动，2=运行中，3=参数拒绝|MAIN [W] P/POUs/MAIN.TcPOU:48（5处）|MAIN [R] P/POUs/MAIN.TcPOU:177（1处）|GVLs/G.TcGVL:40|
|G.selfcheck_target_from_left|ARRAY[1..7] OF LREAL := [96,0,500,0,530,580,0]|—|无字面写/调用|MAIN [R] P/POUs/MAIN.TcPOU:180（8处）|GVLs/G.TcGVL:41|
|G.startup_loading_ready|BOOL := FALSE|TRUE=自检刚完成且仍允许按标准器械装卸姿态启动；上位机开始控制前消费为 FALSE|MAIN [W] P/POUs/MAIN.TcPOU:157（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:69（2处）<br>init [W] P/POUs/init.TcPOU:37（1处）|无字面读取|GVLs/G.TcGVL:42|
|G.handle_reinit_req|BOOL := FALSE|—|MAIN [W] P/POUs/MAIN.TcPOU:49（2处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:548（1处）<br>clear_err [W] P/POUs/clear_err.TcPOU:82（1处）<br>handle [W] P/POUs/handle.TcPOU:145（2处）|handle [R] P/POUs/handle.TcPOU:138（1处）|GVLs/G.TcGVL:43|
|G.handle_reinit_done|BOOL := FALSE|—|MAIN [W] P/POUs/MAIN.TcPOU:50（1处）<br>handle [W] P/POUs/handle.TcPOU:144（3处）|无字面读取|GVLs/G.TcGVL:44|
|G.selfcheck_reset_req|BOOL := FALSE|—|MAIN [W] P/POUs/MAIN.TcPOU:52（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:110（1处）<br>init [W] P/POUs/init.TcPOU:38（1处）|SelfCheck [R] P/POUs/SelfCheck.TcPOU:68（1处）|GVLs/G.TcGVL:45|
|G.selfcheck_stu|ARRAY[1..7] OF INT|—|SelfCheck [W] P/POUs/SelfCheck.TcPOU:82（2处）|无字面读取|GVLs/G.TcGVL:46|
|G.selfcheck_all_reach_limit|BOOL := FALSE|—|SelfCheck [W] P/POUs/SelfCheck.TcPOU:96（2处）|无字面读取|GVLs/G.TcGVL:47|
|G.estop_hold_req|BOOL := FALSE|—|MAIN [W] P/POUs/MAIN.TcPOU:51（1处）<br>clear_err [W] P/POUs/clear_err.TcPOU:43（1处）<br>err [W] P/POUs/err.TcPOU:67（1处）<br>handle [W] P/POUs/handle.TcPOU:232（6处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:51（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:63（1处）|GVLs/G.TcGVL:50|
|G.host_session_id|UDINT := 0|—|无字面写/调用|MAIN [R] P/POUs/MAIN.TcPOU:69（3处）<br>handle [R] P/POUs/handle.TcPOU:108（3处）|GVLs/G.TcGVL:55|
|G.host_heartbeat_sequence|UDINT := 0|—|无字面写/调用|MAIN [R] P/POUs/MAIN.TcPOU:71（2处）<br>handle [R] P/POUs/handle.TcPOU:110（2处）|GVLs/G.TcGVL:56|
|G.host_recover_req|BOOL := FALSE|—|MAIN [W] P/POUs/MAIN.TcPOU:95（1处）<br>handle [W] P/POUs/handle.TcPOU:460（2处）|MAIN [R] P/POUs/MAIN.TcPOU:93（1处）<br>handle [R] P/POUs/handle.TcPOU:456（1处）|GVLs/G.TcGVL:57|
|G.host_comm_timeout|BOOL := TRUE|—|MAIN [W] P/POUs/MAIN.TcPOU:75（3处）<br>handle [W] P/POUs/handle.TcPOU:136（2处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:51（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:63（1处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:35（1处）<br>InjectorManual [R] P/POUs/InjectorManual.TcPOU:24（1处）<br>handle [R] P/POUs/handle.TcPOU:133（5处）|GVLs/G.TcGVL:58|
|G.axis6_fast_retract|BOOL := FALSE|—|handle [W] P/POUs/handle.TcPOU:345（1处）|handle [R] P/POUs/handle.TcPOU:505（1处）|GVLs/G.TcGVL:60|
|G.axis1_fast_return|BOOL := FALSE|—|handle [W] P/POUs/handle.TcPOU:344（1处）|handle [R] P/POUs/handle.TcPOU:504（1处）|GVLs/G.TcGVL:61|
|G.startup_smoothing_bypass|BOOL := FALSE|—|handle [W] P/POUs/handle.TcPOU:348（1处）|handle [R] P/POUs/handle.TcPOU:547（2处）|GVLs/G.TcGVL:62|
|G.return_cmd|ARRAY[1..7] OF ST_AxisPlannedReturnCmd|—|SelfCheck [W] P/POUs/SelfCheck.TcPOU:98（5处）<br>handle [W] P/POUs/handle.TcPOU:165（27处）|handle [R] P/POUs/handle.TcPOU:217（14处）|GVLs/G.TcGVL:63|
|G.axis4_fwd_req|BOOL := FALSE|—|SelfCheck [W] P/POUs/SelfCheck.TcPOU:104（1处）<br>handle [W] P/POUs/handle.TcPOU:182（2处）|handle [R] P/POUs/handle.TcPOU:707（2处）|GVLs/G.TcGVL:64|
|G.axis4_rev_req|BOOL := FALSE|—|SelfCheck [W] P/POUs/SelfCheck.TcPOU:105（1处）<br>handle [W] P/POUs/handle.TcPOU:183（2处）|handle [R] P/POUs/handle.TcPOU:707（2处）|GVLs/G.TcGVL:65|
|G.axis4_manual_busy|BOOL := FALSE|—|SelfCheck [W] P/POUs/SelfCheck.TcPOU:106（1处）<br>handle [W] P/POUs/handle.TcPOU:184（10处）|无字面读取|GVLs/G.TcGVL:66|
|G.axis4_manual_done|BOOL := FALSE|—|SelfCheck [W] P/POUs/SelfCheck.TcPOU:107（1处）<br>handle [W] P/POUs/handle.TcPOU:185（7处）|无字面读取|GVLs/G.TcGVL:67|
|G.axis4_manual_error|BOOL := FALSE|—|SelfCheck [W] P/POUs/SelfCheck.TcPOU:108（1处）<br>handle [W] P/POUs/handle.TcPOU:186（6处）|handle [R] P/POUs/handle.TcPOU:719（1处）|GVLs/G.TcGVL:68|
|G.axis4_manual_error_id|UDINT := 0|—|SelfCheck [W] P/POUs/SelfCheck.TcPOU:109（1处）<br>handle [W] P/POUs/handle.TcPOU:187（7处）|无字面读取|GVLs/G.TcGVL:69|
|G.v_limit|ARRAY[1..7] OF LREAL := [7.5,7.5,1.5,1.5,1.5,7.5,1.5]|—|无字面写/调用|handle [R] P/POUs/handle.TcPOU:502（1处）|GVLs/G.TcGVL:73|
|G.ref_slow|ARRAY[1..7] OF LREAL := [0,0,0,0,0,0,0]|—|MAIN [W] P/POUs/MAIN.TcPOU:164（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:561（1处）<br>handle [W] P/POUs/handle.TcPOU:238（9处）|handle [R] P/POUs/handle.TcPOU:519（9处）|GVLs/G.TcGVL:74|
|G.Kp|ARRAY[1..7] OF LREAL := [0.14,0.14,0.14,0.14,0.14,0.14,0.14]|—|无字面写/调用|无字面读取|GVLs/G.TcGVL:78|
|G.Ki|ARRAY[1..7] OF LREAL := [0.016,0.016,0.016,0.016,0.016,0.016,0.016]|—|无字面写/调用|无字面读取|GVLs/G.TcGVL:79|
|G.Kd|ARRAY[1..7] OF LREAL := [0.1,0.1,0.1,0.1,0.1,0.1,0.1]|—|无字面写/调用|无字面读取|GVLs/G.TcGVL:80|
|G.pid_i|ARRAY[1..7] OF LREAL := [0,0,0,0,0,0,0]|—|无字面写/调用|无字面读取|GVLs/G.TcGVL:81|
|G.pid_e_prev|ARRAY[1..7] OF LREAL := [0,0,0,0,0,0,0]|—|无字面写/调用|无字面读取|GVLs/G.TcGVL:82|
|G.pid_i_limit|ARRAY[1..7] OF LREAL := [100,100,100,100,100,100,100]|—|无字面写/调用|无字面读取|GVLs/G.TcGVL:83|
|G.arm_axis|ARRAY[1..5] OF AXIS_REF|定位臂轴对象，链接到 NC Axis 1..5|ArmCartJog [RW/Axis] P/POUs/ArmCartJog.TcPOU:102（4处）<br>ArmCartesian [RW/Axis] P/POUs/ArmCartesian.TcPOU:222（4处）<br>ArmManual [RW/Axis] P/POUs/ArmManual.TcPOU:62（4处）<br>ArmManual [RW/FB] P/POUs/ArmManual.TcPOU:32（1处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:55（4处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:67（4处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:33（5处）|GVLs/G.TcGVL:86|
|G.arm_manual_enable|BOOL := FALSE|定位臂手动控制总使能，FALSE 时所有点动请求无效|ArmManual [W] P/POUs/ArmManual.TcPOU:38（1处）<br>MAIN [W] P/POUs/MAIN.TcPOU:58（1处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:51（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:63（1处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:35（3处）|GVLs/G.TcGVL:87|
|G.arm_enable_req|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|TRUE=对应轴保持上电|ArmManual [W] P/POUs/ArmManual.TcPOU:39（1处）<br>MAIN [W] P/POUs/MAIN.TcPOU:61（1处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:54（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:66（1处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:63（1处）|GVLs/G.TcGVL:88|
|G.arm_reset_req|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|TRUE=请求清错，PLC 完成后自动清零|ArmManual [W] P/POUs/ArmManual.TcPOU:102（1处）<br>MAIN [W] P/POUs/MAIN.TcPOU:62（1处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:55（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:67（1处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:73（1处）|GVLs/G.TcGVL:89|
|G.arm_jog_pos_req|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|UI 正向点动按钮，按下 TRUE、松开 FALSE|ArmManual [W] P/POUs/ArmManual.TcPOU:40（1处）<br>MAIN [W] P/POUs/MAIN.TcPOU:63（1处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:56（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:68（1处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:105（2处）|GVLs/G.TcGVL:90|
|G.arm_jog_neg_req|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|UI 反向点动按钮，按下 TRUE、松开 FALSE|ArmManual [W] P/POUs/ArmManual.TcPOU:41（1处）<br>MAIN [W] P/POUs/MAIN.TcPOU:64（1处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:56（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:68（1处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:105（2处）|GVLs/G.TcGVL:91|
|G.arm_jog_velocity|ARRAY[1..5] OF LREAL := [5.0,5.0,5.0,5.0,5.0]|—|无字面写/调用|ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:97（3处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:44（1处）|GVLs/G.TcGVL:92|
|G.arm_jog_acc|ARRAY[1..5] OF LREAL := [50.0,50.0,50.0,50.0,50.0]|—|无字面写/调用|ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:98（3处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:48（1处）|GVLs/G.TcGVL:93|
|G.arm_jog_dec|ARRAY[1..5] OF LREAL := [50.0,50.0,50.0,50.0,50.0]|—|无字面写/调用|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:109（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:99（2处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:52（1处）|GVLs/G.TcGVL:94|
|G.arm_jog_jerk|ARRAY[1..5] OF LREAL := [500.0,500.0,500.0,500.0,500.0]|—|无字面写/调用|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:109（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:100（2处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:56（1处）|GVLs/G.TcGVL:95|
|G.arm_power_output|ARRAY[1..5] OF ST_McOutputs|—|ArmManual [W] P/POUs/ArmManual.TcPOU:67（5处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:54（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:66（1处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:119（4处）|GVLs/G.TcGVL:96|
|G.arm_reset_output|ARRAY[1..5] OF ST_McOutputs|—|ArmManual [W] P/POUs/ArmManual.TcPOU:87（5处）|ArmManual [R] P/POUs/ArmManual.TcPOU:124（3处）|GVLs/G.TcGVL:97|
|G.arm_act_pos|ARRAY[1..5] OF LREAL|—|ArmManual [W] P/POUs/ArmManual.TcPOU:33（1处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:64（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:199（2处）|GVLs/G.TcGVL:98|
|G.arm_act_vel|ARRAY[1..5] OF LREAL|—|ArmManual [W] P/POUs/ArmManual.TcPOU:34（1处）|ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:236（1处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:185（2处）|GVLs/G.TcGVL:99|
|G.arm_motion_busy|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|—|ArmManual [W] P/POUs/ArmManual.TcPOU:253（1处）|ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:125（1处）|GVLs/G.TcGVL:100|
|G.arm_motion_done|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|—|ArmManual [W] P/POUs/ArmManual.TcPOU:96（5处）|无字面读取|GVLs/G.TcGVL:101|
|G.arm_motion_error|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|—|ArmManual [W] P/POUs/ArmManual.TcPOU:94（7处）|无字面读取|GVLs/G.TcGVL:102|
|G.arm_motion_error_id|ARRAY[1..5] OF UDINT := [0,0,0,0,0]|—|ArmManual [W] P/POUs/ArmManual.TcPOU:95（9处）|无字面读取|GVLs/G.TcGVL:103|
|G.arm_cmd_dir|ARRAY[1..5] OF SINT := [0,0,0,0,0]|-1=反向，0=停止，1=正向|ArmManual [W] P/POUs/ArmManual.TcPOU:252（1处）|无字面读取|GVLs/G.TcGVL:104|
|G.arm_cmd_conflict|ARRAY[1..5] OF BOOL := [FALSE,FALSE,FALSE,FALSE,FALSE]|正反向同时请求|ArmManual [W] P/POUs/ArmManual.TcPOU:105（1处）|ArmManual [R] P/POUs/ArmManual.TcPOU:106（2处）|GVLs/G.TcGVL:105|
|G.arm_curve_data|ARRAY[0..16] OF LREAL|—|无字面写/调用|ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:82（1处）|GVLs/G.TcGVL:107|
|G.arm_curve_request|UDINT|—|无字面写/调用|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:59（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:49（4处）|GVLs/G.TcGVL:108|
|G.arm_curve_ack|UDINT|—|ArmCartesian [W] P/POUs/ArmCartesian.TcPOU:49（3处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:59（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:72（1处）|GVLs/G.TcGVL:109|
|G.arm_curve_cancel|BOOL := TRUE|—|ArmCartesian [W] P/POUs/ArmCartesian.TcPOU:50（7处）<br>ArmManual [W] P/POUs/ArmManual.TcPOU:75（1处）|ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:91（2处）|GVLs/G.TcGVL:110|
|G.arm_curve_heartbeat|UDINT|—|无字面写/调用|ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:56（2处）|GVLs/G.TcGVL:111|
|G.arm_curve_owned|BOOL|—|ArmCartesian [W] P/POUs/ArmCartesian.TcPOU:51（3处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:58（1处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:221（2处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:74（3处）|GVLs/G.TcGVL:112|
|G.arm_curve_state|UDINT|0空闲，1执行，2完成，3取消，4错误|ArmCartesian [W] P/POUs/ArmCartesian.TcPOU:52（4处）|无字面读取|GVLs/G.TcGVL:113|
|G.arm_curve_error|UDINT|—|ArmCartesian [W] P/POUs/ArmCartesian.TcPOU:53（10处）|ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:75（7处）|GVLs/G.TcGVL:114|
|G.arm_curve_progress|LREAL|—|ArmCartesian [W] P/POUs/ArmCartesian.TcPOU:150（2处）|无字面读取|GVLs/G.TcGVL:115|
|G.arm_cjog_mode|INT|—|无字面写/调用|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:45（4处）|GVLs/G.TcGVL:117|
|G.arm_cjog_dir|INT|—|无字面写/调用|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:46（3处）|GVLs/G.TcGVL:118|
|G.arm_cjog_speed|LREAL|—|无字面写/调用|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:47（3处）|GVLs/G.TcGVL:119|
|G.arm_cjog_alive|UDINT|—|无字面写/调用|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:38（2处）|GVLs/G.TcGVL:120|
|G.arm_cjog_owned|BOOL|—|ArmCartJog [W] P/POUs/ArmCartJog.TcPOU:32（3处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:101（2处）<br>ArmCartesian [R] P/POUs/ArmCartesian.TcPOU:93（1处）<br>ArmManual [R] P/POUs/ArmManual.TcPOU:74（2处）|GVLs/G.TcGVL:121|
|G.arm_cjog_state|UDINT|0空闲，1使能，2运动，3减速，4释放|ArmCartJog [W] P/POUs/ArmCartJog.TcPOU:33（6处）|无字面读取|GVLs/G.TcGVL:122|
|G.arm_cjog_error|UDINT|—|ArmCartJog [W] P/POUs/ArmCartJog.TcPOU:34（9处）|ArmCartJog [R] P/POUs/ArmCartJog.TcPOU:49（5处）|GVLs/G.TcGVL:123|
|G.arm_cjog_limited|BOOL|—|ArmCartJog [W] P/POUs/ArmCartJog.TcPOU:35（2处）|无字面读取|GVLs/G.TcGVL:124|
|G.arm_cjog_path_speed|LREAL|—|ArmCartJog [W] P/POUs/ArmCartJog.TcPOU:36（2处）|无字面读取|GVLs/G.TcGVL:125|
|G.arm_cjog_if_version|UDINT := 1|—|无字面写/调用|无字面读取|GVLs/G.TcGVL:126|
|G.axis2_sync_axis|AXIS_REF|—|Axis2Sync [RW/Axis] P/POUs/Axis2Sync.TcPOU:46（2处）<br>Axis2Sync [RW/FB] P/POUs/Axis2Sync.TcPOU:19（1处）|Axis2Sync [R] P/POUs/Axis2Sync.TcPOU:21（4处）|GVLs/G.TcGVL:130|
|G.inject_axis|ARRAY[1..2] OF AXIS_REF|—|InjectorManual [RW/Axis] P/POUs/InjectorManual.TcPOU:30（3处）<br>MAIN [RW/FB] P/POUs/MAIN.TcPOU:41（1处）|InjectorManual [R] P/POUs/InjectorManual.TcPOU:74（1处）|GVLs/G.TcGVL:134|
|G.inject_push_req|ARRAY[1..2] OF BOOL := [FALSE,FALSE]|—|InjectorManual [W] P/POUs/InjectorManual.TcPOU:25（1处）|InjectorManual [R] P/POUs/InjectorManual.TcPOU:36（2处）|GVLs/G.TcGVL:135|
|G.inject_pull_req|ARRAY[1..2] OF BOOL := [FALSE,FALSE]|—|InjectorManual [W] P/POUs/InjectorManual.TcPOU:26（1处）|InjectorManual [R] P/POUs/InjectorManual.TcPOU:36（2处）|GVLs/G.TcGVL:136|
|G.cylinder1_value|WORD|电缸1控制值 (安装在夹持机构上)|MAIN [W] P/POUs/MAIN.TcPOU:152（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:150（2处）|无字面读取|GVLs/G.TcGVL:139|
|G.cylinder2_value|WORD|电缸2控制值 (安装在轴1机构上)|MAIN [W] P/POUs/MAIN.TcPOU:153（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:151（2处）|无字面读取|GVLs/G.TcGVL:140|
|G.cylinder3_value|WORD|电缸3控制值 (安装在轴5机构上)|MAIN [W] P/POUs/MAIN.TcPOU:154（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:152（2处）|无字面读取|GVLs/G.TcGVL:141|
|G.cylinder4_value|WORD|电缸4控制值 (安装在轴6机构上)|MAIN [W] P/POUs/MAIN.TcPOU:155（1处）<br>SelfCheck [W] P/POUs/SelfCheck.TcPOU:153（2处）|无字面读取|GVLs/G.TcGVL:142|
|G.cylinder5_value|WORD|电缸5控制值 (安装在轴3机构上控制y阀)|SelfCheck [W] P/POUs/SelfCheck.TcPOU:114（1处）<br>handle [W] P/POUs/handle.TcPOU:476（2处）|无字面读取|GVLs/G.TcGVL:143|
|G.cylinder5_cmd|WORD := 2000|历史兼容缓存，当前正式控制不写此变量|SelfCheck [W] P/POUs/SelfCheck.TcPOU:115（1处）|无字面读取|GVLs/G.TcGVL:144|
|G.cylinder5_press_req|BOOL := FALSE|Y阀请求（TRUE=打开，输出500；FALSE=关闭，输出2000）|SelfCheck [W] P/POUs/SelfCheck.TcPOU:116（1处）|handle [R] P/POUs/handle.TcPOU:475（1处）|GVLs/G.TcGVL:145|
|G.ft_1_value|INT|—|无字面写/调用|无字面读取|GVLs/G.TcGVL:148|
|G.fn_1_value|INT|—|无字面写/调用|无字面读取|GVLs/G.TcGVL:149|
|G.fn_2_value|INT|—|无字面写/调用|无字面读取|GVLs/G.TcGVL:150|
|G.ft_2_value|INT|—|无字面写/调用|无字面读取|GVLs/G.TcGVL:151|
|G.estop_diag_count|UDINT|—|handle [W] P/POUs/handle.TcPOU:231（1处）|handle [R] P/POUs/handle.TcPOU:231（1处）|GVLs/G.TcGVL:156|
|G.estop_diag_host_timeout|BOOL|—|handle [W] P/POUs/handle.TcPOU:208（1处）|无字面读取|GVLs/G.TcGVL:157|
|G.estop_diag_axis|ARRAY[1..7] OF ST_AxisStatus|—|handle [W] P/POUs/handle.TcPOU:210（1处）|无字面读取|GVLs/G.TcGVL:158|
|G.estop_diag_power|ARRAY[1..7] OF ST_McOutputs|—|handle [W] P/POUs/handle.TcPOU:211（1处）|无字面读取|GVLs/G.TcGVL:159|
|G.estop_diag_reset|ARRAY[1..7] OF ST_McOutputs|—|handle [W] P/POUs/handle.TcPOU:212（1处）|无字面读取|GVLs/G.TcGVL:160|
|G.estop_diag_power_enable|ARRAY[1..7] OF BOOL|—|handle [W] P/POUs/handle.TcPOU:213（1处）|无字面读取|GVLs/G.TcGVL:161|
|G.estop_diag_power_positive|ARRAY[1..7] OF BOOL|—|handle [W] P/POUs/handle.TcPOU:214（1处）|无字面读取|GVLs/G.TcGVL:162|
|G.estop_diag_power_negative|ARRAY[1..7] OF BOOL|—|handle [W] P/POUs/handle.TcPOU:215（1处）|无字面读取|GVLs/G.TcGVL:163|
|G.estop_diag_return_state|ARRAY[1..7] OF INT|—|handle [W] P/POUs/handle.TcPOU:216（1处）|无字面读取|GVLs/G.TcGVL:164|
|G.estop_diag_return_cmd|ARRAY[1..7] OF ST_AxisPlannedReturnCmd|—|handle [W] P/POUs/handle.TcPOU:217（1处）|无字面读取|GVLs/G.TcGVL:165|
|G.estop_diag_move_error|ARRAY[1..7] OF BOOL|—|handle [W] P/POUs/handle.TcPOU:218（1处）|无字面读取|GVLs/G.TcGVL:166|
|G.estop_diag_move_aborted|ARRAY[1..7] OF BOOL|—|handle [W] P/POUs/handle.TcPOU:219（1处）|无字面读取|GVLs/G.TcGVL:167|
|G.estop_diag_enable_error|ARRAY[1..7] OF BOOL|—|handle [W] P/POUs/handle.TcPOU:221（1处）|无字面读取|GVLs/G.TcGVL:168|
|G.estop_diag_disable_error|ARRAY[1..7] OF BOOL|—|handle [W] P/POUs/handle.TcPOU:223（1处）|无字面读取|GVLs/G.TcGVL:169|
|G.estop_diag_move_error_id|ARRAY[1..7] OF UDINT|—|handle [W] P/POUs/handle.TcPOU:220（1处）|无字面读取|GVLs/G.TcGVL:170|
|G.estop_diag_enable_error_id|ARRAY[1..7] OF UDINT|—|handle [W] P/POUs/handle.TcPOU:222（1处）|无字面读取|GVLs/G.TcGVL:171|
|G.estop_diag_disable_error_id|ARRAY[1..7] OF UDINT|—|handle [W] P/POUs/handle.TcPOU:224（1处）|无字面读取|GVLs/G.TcGVL:172|
|G.estop_diag_act_pos|ARRAY[1..7] OF LREAL|—|handle [W] P/POUs/handle.TcPOU:225（1处）|无字面读取|GVLs/G.TcGVL:173|
|G.estop_diag_act_vel|ARRAY[1..7] OF LREAL|—|handle [W] P/POUs/handle.TcPOU:226（1处）|无字面读取|GVLs/G.TcGVL:174|
|G.estop_diag_feed_pos|ARRAY[1..7] OF LREAL|—|handle [W] P/POUs/handle.TcPOU:227（1处）|无字面读取|GVLs/G.TcGVL:175|
|G.estop_diag_feed_vel|ARRAY[1..7] OF LREAL|—|handle [W] P/POUs/handle.TcPOU:228（1处）|无字面读取|GVLs/G.TcGVL:176|
|G.estop_diag_refer_abs|ARRAY[1..7] OF LREAL|—|handle [W] P/POUs/handle.TcPOU:229（1处）|无字面读取|GVLs/G.TcGVL:177|
|G.rslimit_distance|ARRAY[1..7] OF LREAL := [99.0, 0.0, 666.0, 0.0, 688.0, 688.0, 0.0]|—|无字面写/调用|err [R] P/POUs/err.TcPOU:101（1处）<br>handle [R] P/POUs/handle.TcPOU:299（1处）|GVLs/G.TcGVL:180|
|G.recovery_wait_timeout|TIME := T#2S|—|无字面写/调用|handle [R] P/POUs/handle.TcPOU:256（3处）|GVLs/G.TcGVL:181|
|G.recovery_max_retries|UINT := 3|—|无字面写/调用|err [R] P/POUs/err.TcPOU:99（1处）<br>handle [R] P/POUs/handle.TcPOU:269（5处）|GVLs/G.TcGVL:182|
|G.err_backoff_distance|LREAL := 2.0|—|无字面写/调用|err [R] P/POUs/err.TcPOU:102（2处）|GVLs/G.TcGVL:183|
|G.err_backoff_velocity|LREAL := 5.0|—|无字面写/调用|err [R] P/POUs/err.TcPOU:78（1处）|GVLs/G.TcGVL:184|
|G.err_backoff_acc|LREAL := 50.0|—|无字面写/调用|err [R] P/POUs/err.TcPOU:79（1处）|GVLs/G.TcGVL:185|
|G.axis15_reset_pulse|TIME := T#200MS|—|无字面写/调用|Axis2Sync [R] P/POUs/Axis2Sync.TcPOU:31（1处）|GVLs/G.TcGVL:186|
|G.axis15_reset_interval|TIME := T#2S|—|无字面写/调用|Axis2Sync [R] P/POUs/Axis2Sync.TcPOU:24（1处）|GVLs/G.TcGVL:187|
|G.return_clear_error_id|UDINT := 16#80005001|—|无字面写/调用|handle [R] P/POUs/handle.TcPOU:613（1处）|GVLs/G.TcGVL:188|
