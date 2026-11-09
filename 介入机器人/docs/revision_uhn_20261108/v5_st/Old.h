#pragma once
struct Oldhandle {
Node& G;
Node i;
Node j;
Node k;
Node v_step;
Node avg_window;
Node avg_window_size;
Node return_handoff_bypass;
Node act_hh;
Node pos_abs;
Node legacy_vel;
Node init_done;
Node all_enabled;
Node all_powered;
Node has_axis_error;
Node hold_active;
Node host_watchdog_timer;
Node host_last_session_id;
Node host_last_heartbeat_sequence;
Node host_heartbeat_seen;
Node host_session_changed;
Node host_heartbeat_changed;
Node host_timeout_latched;
Node host_timeout_prev;
Node host_hold_abs;
Node host_hold_captured;
Node host_all_stopped;
Node host_return_stop_active;
Node host_return_stop_exec;
Node host_axis4_stop_active;
Node fb_host_return_stop;
Node right_slimit;
Node rslimit_distance;
Node slimit_enable;
Node ext_setpoint_enabled;
Node spg_enable_exec;
Node spg_disable_exec;
Node return_enable;
Node return_state;
Node return_exec;
Node return_target_abs;
Node return_velocity;
Node return_acc;
Node return_dec;
Node return_jerk;
Node return_finish_error;
Node return_error_id_pending;
Node fb_return_move_abs;
Node axis4_manual_state;
Node axis4_request_dir_now;
Node axis4_requested_dir;
Node axis4_pending_dir;
Node axis4_move_exec;
Node axis4_stop_exec;
Node axis4_finish_error;
Node axis4_error_id_pending;
Node axis4_manual_velocity;
Node axis4_manual_acc;
Node axis4_manual_dec;
Node axis4_manual_jerk;
Node axis4_manual_direction;
Node fb_axis4_move_velocity;
Node fb_axis4_stop;
Node ramp_counter;
Node act_h_prev;
Node traj_pos;
Node traj_vel;
Node traj_acc;
Node acc_raw;
Node traj_wn;
Node traj_vmax;
Node traj_amax;
Node traj_enable;
explicit Oldhandle(Node& global) : G(global) {
avg_window_size = 20;
init_done = false;
hold_active = false;
host_watchdog_timer.kind = "TON";
host_last_session_id = 0;
host_last_heartbeat_sequence = 0;
host_heartbeat_seen = false;
host_timeout_latched = false;
host_timeout_prev = false;
host_hold_captured = false;
host_axis4_stop_active = false;
fb_host_return_stop[1].kind = "MC_Stop";
fb_host_return_stop[2].kind = "MC_Stop";
fb_host_return_stop[3].kind = "MC_Stop";
fb_host_return_stop[4].kind = "MC_Stop";
fb_host_return_stop[5].kind = "MC_Stop";
fb_host_return_stop[6].kind = "MC_Stop";
fb_host_return_stop[7].kind = "MC_Stop";
rslimit_distance[1] = 99.0;
rslimit_distance[2] = 0.0;
rslimit_distance[3] = 666.0;
rslimit_distance[4] = 0.0;
rslimit_distance[5] = 688.0;
rslimit_distance[6] = 688.0;
rslimit_distance[7] = 0.0;
slimit_enable[1] = true;
slimit_enable[2] = false;
slimit_enable[3] = true;
slimit_enable[4] = false;
slimit_enable[5] = true;
slimit_enable[6] = true;
slimit_enable[7] = false;
ext_setpoint_enabled[1] = false;
ext_setpoint_enabled[2] = false;
ext_setpoint_enabled[3] = false;
ext_setpoint_enabled[4] = false;
ext_setpoint_enabled[5] = false;
ext_setpoint_enabled[6] = false;
ext_setpoint_enabled[7] = false;
return_enable[1] = true;
return_enable[2] = false;
return_enable[3] = true;
return_enable[4] = false;
return_enable[5] = true;
return_enable[6] = true;
return_enable[7] = false;
fb_return_move_abs[1].kind = "MC_MoveAbsolute";
fb_return_move_abs[2].kind = "MC_MoveAbsolute";
fb_return_move_abs[3].kind = "MC_MoveAbsolute";
fb_return_move_abs[4].kind = "MC_MoveAbsolute";
fb_return_move_abs[5].kind = "MC_MoveAbsolute";
fb_return_move_abs[6].kind = "MC_MoveAbsolute";
fb_return_move_abs[7].kind = "MC_MoveAbsolute";
axis4_manual_state = 0;
axis4_request_dir_now = 0;
axis4_requested_dir = 0;
axis4_pending_dir = 0;
axis4_move_exec = false;
axis4_stop_exec = false;
axis4_finish_error = false;
axis4_error_id_pending = 0;
axis4_manual_velocity = 60;
axis4_manual_acc = 200;
axis4_manual_dec = 200;
axis4_manual_jerk = 500.0;
axis4_manual_direction = 1;
axis4_manual_direction.kind = "MC_Direction";
fb_axis4_move_velocity.kind = "MC_MoveVelocity";
fb_axis4_stop.kind = "MC_Stop";
traj_wn = 60.0;
traj_vmax[1] = 80;
traj_vmax[2] = 0;
traj_vmax[3] = 80;
traj_vmax[4] = 0;
traj_vmax[5] = 80;
traj_vmax[6] = 80;
traj_vmax[7] = 0;
traj_amax[1] = 2000;
traj_amax[2] = 0;
traj_amax[3] = 2000;
traj_amax[4] = 0;
traj_amax[5] = 2000;
traj_amax[6] = 2000;
traj_amax[7] = 0;
traj_enable[1] = true;
traj_enable[2] = false;
traj_enable[3] = true;
traj_enable[4] = false;
traj_enable[5] = true;
traj_enable[6] = true;
traj_enable[7] = false;
}
void cycle() {
host_session_changed = G ["host_session_id"] != host_last_session_id;
host_heartbeat_changed = host_session_changed || ( G ["host_heartbeat_sequence"] != host_last_heartbeat_sequence );
if (host_session_changed) {
if (host_last_session_id != 0) {
host_timeout_latched = true;
}
host_last_session_id = G ["host_session_id"];
}
if (host_heartbeat_changed) {
host_last_heartbeat_sequence = G ["host_heartbeat_sequence"];
}
host_heartbeat_seen = G ["host_session_id"] != 0;
call(host_watchdog_timer, {{"IN", host_heartbeat_seen && ! host_heartbeat_changed}, {"PT", 2000}});
if (( ! host_heartbeat_seen ) || host_watchdog_timer ["Q"]) {
host_timeout_latched = true;
}
if (G ["host_comm_timeout"]) {
host_timeout_latched = true;
}
G ["host_comm_timeout"] = host_timeout_latched;
if (G ["handle_reinit_req"]) {
init_done = false;
G ["handle_reinit_done"] = false;
G ["handle_reinit_req"] = false;
for (i = 1; double(i) <= 7; ++i) {
ext_setpoint_enabled [ i ] = false;
spg_enable_exec [ i ] = false;
spg_disable_exec [ i ] = false;
return_state [ i ] = 0;
return_exec [ i ] = false;
return_finish_error [ i ] = false;
return_error_id_pending [ i ] = 0;
traj_pos [ i ] = 0.0;
traj_vel [ i ] = 0.0;
traj_acc [ i ] = 0.0;
host_hold_abs [ i ] = 0.0;
G ["return_cmd"] [ i ] ["Req"] = false;
G ["return_cmd"] [ i ] ["Busy"] = false;
G ["return_cmd"] [ i ] ["Done"] = false;
G ["return_cmd"] [ i ] ["Error"] = false;
G ["return_cmd"] [ i ] ["ErrorId"] = 0;
}
host_hold_captured = false;
host_timeout_prev = false;
axis4_manual_state = 0;
axis4_request_dir_now = 0;
axis4_requested_dir = 0;
axis4_pending_dir = 0;
axis4_move_exec = false;
axis4_stop_exec = false;
axis4_finish_error = false;
axis4_error_id_pending = 0;
G ["axis4_fwd_req"] = false;
G ["axis4_rev_req"] = false;
G ["axis4_manual_busy"] = false;
G ["axis4_manual_done"] = false;
G ["axis4_manual_error"] = false;
G ["axis4_manual_error_id"] = 0;
}
if (! init_done) {
all_enabled = true;
for (i = 1; double(i) <= 7; ++i) {
G ["SetPointGenEnable"] [ i ] ["Execute"] = true;
if (! G ["SetPointGenEnable"] [ i ] ["Done"]) {
all_enabled = false;
}
}
if (all_enabled) {
for (j = 1; double(j) <= 7; ++j) {
G ["Act_pos"] [ j ] = G ["axis"] [ j ] ["NcToPlc"] ["ActPos"] - G ["init_pos"] [ j ];
G ["act_pos_from_left"] [ j ] = G ["axis"] [ j ] ["NcToPlc"] ["ActPos"] - G ["leftlimit"] [ j ];
G ["ref_slow"] [ j ] = G ["Act_pos"] [ j ];
G ["act_h"] [ j ] = G ["Act_pos"] [ j ];
G ["act_hf"] [ j ] = G ["Act_pos"] [ j ];
G ["refer"] [ j ] = G ["Act_pos"] [ j ];
G ["refer_from_left"] [ j ] = G ["act_pos_from_left"] [ j ];
ext_setpoint_enabled [ j ] = true;
for (k = 1; double(k) <= 30; ++k) {
act_hh [ j ] [ k ] = G ["Act_pos"] [ j ];
}
act_h_prev [ j ] = G ["Act_pos"] [ j ];
traj_pos [ j ] = G ["Act_pos"] [ j ];
traj_vel [ j ] = 0.0;
traj_acc [ j ] = 0.0;
if (slimit_enable [ j ]) {
right_slimit [ j ] = G ["leftlimit"] [ j ] + rslimit_distance [ j ];
}
G ["return_cmd"] [ j ] ["Busy"] = false;
G ["return_cmd"] [ j ] ["Done"] = false;
G ["return_cmd"] [ j ] ["Error"] = false;
G ["return_cmd"] [ j ] ["ErrorId"] = 0;
}
G ["axis4_manual_busy"] = false;
G ["axis4_manual_done"] = false;
G ["axis4_manual_error"] = false;
G ["axis4_manual_error_id"] = 0;
init_done = true;
G ["handle_reinit_done"] = ! G ["host_comm_timeout"];
}
return;
}
all_powered = true;
for (i = 1; double(i) <= 7; ++i) {
if (! G ["power_output"] [ i ] ["Done"]) {
all_powered = false;
break;
}
}
has_axis_error = false;
for (i = 1; double(i) <= 7; ++i) {
if (G ["axis"] [ i ] ["Status"] ["Error"] || G ["power_output"] [ i ] ["Error"] || G ["reset_output"] [ i ] ["Error"]) {
has_axis_error = true;
break;
}
}
if (( ! all_powered ) || has_axis_error) {
G ["estop_diag_host_timeout"] = G ["host_comm_timeout"];
for (j = 1; double(j) <= 7; ++j) {
G ["estop_diag_axis"] [ j ] = G ["axis"] [ j ] ["Status"];
G ["estop_diag_power"] [ j ] = G ["power_output"] [ j ];
G ["estop_diag_reset"] [ j ] = G ["reset_output"] [ j ];
G ["estop_diag_power_enable"] [ j ] = G ["power_"] [ j ] ["Enable"];
G ["estop_diag_power_positive"] [ j ] = G ["power_"] [ j ] ["Enable_Positive"];
G ["estop_diag_power_negative"] [ j ] = G ["power_"] [ j ] ["Enable_Negative"];
G ["estop_diag_return_state"] [ j ] = return_state [ j ];
G ["estop_diag_return_cmd"] [ j ] = G ["return_cmd"] [ j ];
G ["estop_diag_move_error"] [ j ] = fb_return_move_abs [ j ] ["Error"];
G ["estop_diag_move_aborted"] [ j ] = fb_return_move_abs [ j ] ["CommandAborted"];
G ["estop_diag_move_error_id"] [ j ] = fb_return_move_abs [ j ] ["ErrorID"];
G ["estop_diag_enable_error"] [ j ] = G ["SetPointGenEnable"] [ j ] ["Error"];
G ["estop_diag_enable_error_id"] [ j ] = G ["SetPointGenEnable"] [ j ] ["ErrorID"];
G ["estop_diag_disable_error"] [ j ] = G ["SetPointGenDisable"] [ j ] ["Error"];
G ["estop_diag_disable_error_id"] [ j ] = G ["SetPointGenDisable"] [ j ] ["ErrorID"];
G ["estop_diag_act_pos"] [ j ] = G ["axis"] [ j ] ["NcToPlc"] ["ActPos"];
G ["estop_diag_act_vel"] [ j ] = G ["axis"] [ j ] ["NcToPlc"] ["ActVelo"];
G ["estop_diag_feed_pos"] [ j ] = G ["axis"] [ j ] ["PlcToNc"] ["ExtSetPos"];
G ["estop_diag_feed_vel"] [ j ] = G ["axis"] [ j ] ["PlcToNc"] ["ExtSetVelo"];
G ["estop_diag_refer_abs"] [ j ] = G ["refer"] [ j ] + G ["init_pos"] [ j ];
}
G ["estop_diag_count"] = G ["estop_diag_count"] + 1;
G ["estop_hold_req"] = true;
for (j = 1; double(j) <= 7; ++j) {
G ["SetPointGenDisable"] [ j ] ["Execute"] = false;
G ["Act_pos"] [ j ] = G ["axis"] [ j ] ["NcToPlc"] ["ActPos"] - G ["init_pos"] [ j ];
G ["act_pos_from_left"] [ j ] = G ["axis"] [ j ] ["NcToPlc"] ["ActPos"] - G ["leftlimit"] [ j ];
G ["refer"] [ j ] = G ["Act_pos"] [ j ];
G ["ref_slow"] [ j ] = G ["Act_pos"] [ j ];
G ["act_h"] [ j ] = G ["Act_pos"] [ j ];
G ["act_hf"] [ j ] = G ["Act_pos"] [ j ];
G ["refer_from_left"] [ j ] = G ["act_pos_from_left"] [ j ];
for (k = 1; double(k) <= 30; ++k) {
act_hh [ j ] [ k ] = G ["Act_pos"] [ j ];
}
act_h_prev [ j ] = G ["Act_pos"] [ j ];
traj_pos [ j ] = G ["Act_pos"] [ j ];
traj_vel [ j ] = 0.0;
traj_acc [ j ] = 0.0;
}
G ["gen_state"] = _err;
return;
}
if (G ["host_comm_timeout"]) {
G ["handle_reinit_done"] = false;
if (( ! host_timeout_prev ) || ( ! host_hold_captured )) {
for (i = 1; double(i) <= 7; ++i) {
host_hold_abs [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"];
}
host_hold_captured = true;
}
if (! host_timeout_prev) {
for (i = 1; double(i) <= 7; ++i) {
host_return_stop_active [ i ] = return_enable [ i ] && ( ( return_state [ i ] != 0 ) || G ["return_cmd"] [ i ] ["Busy"] || fb_return_move_abs [ i ] ["Busy"] );
host_return_stop_exec [ i ] = false;
}
host_axis4_stop_active = ( axis4_manual_state != 0 ) || fb_axis4_move_velocity ["Busy"];
}
G ["axis1_fast_return"] = false;
G ["axis6_fast_retract"] = false;
G ["axis4_fwd_req"] = false;
G ["axis4_rev_req"] = false;
G ["startup_smoothing_bypass"] = false;
axis4_move_exec = false;
host_all_stopped = true;
for (i = 1; double(i) <= 7; ++i) {
spg_enable_exec [ i ] = false;
spg_disable_exec [ i ] = false;
return_exec [ i ] = false;
G ["return_cmd"] [ i ] ["Req"] = false;
G ["return_cmd"] [ i ] ["Done"] = false;
G ["Act_pos"] [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"] - G ["init_pos"] [ i ];
G ["act_pos_from_left"] [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"] - G ["leftlimit"] [ i ];
G ["refer"] [ i ] = host_hold_abs [ i ] - G ["init_pos"] [ i ];
G ["ref_slow"] [ i ] = G ["refer"] [ i ];
G ["act_h"] [ i ] = G ["refer"] [ i ];
G ["act_hf"] [ i ] = G ["refer"] [ i ];
G ["refer_from_left"] [ i ] = host_hold_abs [ i ] - G ["leftlimit"] [ i ];
for (k = 1; double(k) <= 30; ++k) {
act_hh [ i ] [ k ] = G ["refer"] [ i ];
}
act_h_prev [ i ] = G ["refer"] [ i ];
traj_pos [ i ] = G ["refer"] [ i ];
traj_vel [ i ] = 0.0;
traj_acc [ i ] = 0.0;
if (host_return_stop_active [ i ]) {
if (fb_host_return_stop [ i ] ["Done"]) {
host_return_stop_exec [ i ] = false;
host_return_stop_active [ i ] = false;
} else if (fb_host_return_stop [ i ] ["Error"]) {
host_return_stop_exec [ i ] = false;
host_all_stopped = false;
} else {
host_return_stop_exec [ i ] = true;
host_all_stopped = false;
}
} else {
host_return_stop_exec [ i ] = false;
}
G ["return_cmd"] [ i ] ["Busy"] = host_return_stop_active [ i ];
if (ext_setpoint_enabled [ i ]) {
feed(G ["axis"] [ i ], {{"Position", host_hold_abs [ i ]}, {"Velocity", 0.0}, {"Acceleration", 0.0}, {"Direction", 1}});
}
G ["SetPointGenEnable"] [ i ] ["Execute"] = false;
G ["SetPointGenDisable"] [ i ] ["Execute"] = false;
call(fb_return_move_abs [ i ], {{"Position", return_target_abs [ i ]}, {"Velocity", return_velocity [ i ]}, {"Acceleration", return_acc [ i ]}, {"Deceleration", return_dec [ i ]}, {"Jerk", return_jerk [ i ]}, {"Execute", false}});
call(fb_host_return_stop [ i ], {{"Execute", host_return_stop_exec [ i ]}, {"Deceleration", 4000.0}, {"Jerk", 10000.0}});
}
if (host_axis4_stop_active) {
if (fb_axis4_stop ["Done"]) {
axis4_stop_exec = false;
host_axis4_stop_active = false;
} else if (fb_axis4_stop ["Error"]) {
axis4_stop_exec = false;
host_all_stopped = false;
} else {
axis4_stop_exec = true;
host_all_stopped = false;
}
} else {
axis4_stop_exec = false;
}
G ["axis4_manual_busy"] = host_axis4_stop_active;
G ["axis4_manual_done"] = false;
call(fb_axis4_move_velocity, {{"Execute", false}, {"Velocity", axis4_manual_velocity}, {"Acceleration", axis4_manual_acc}, {"Deceleration", axis4_manual_dec}, {"Jerk", axis4_manual_jerk}, {"Direction", axis4_manual_direction}});
call(fb_axis4_stop, {{"Execute", axis4_stop_exec}, {"Deceleration", axis4_manual_dec}, {"Jerk", axis4_manual_jerk}});
if (G ["host_recover_req"] && host_heartbeat_seen && ( ! host_watchdog_timer ["Q"] ) && host_all_stopped) {
host_timeout_latched = false;
G ["host_comm_timeout"] = false;
G ["host_recover_req"] = false;
G ["handle_reinit_req"] = true;
}
host_timeout_prev = G ["host_comm_timeout"];
return;
}
host_timeout_prev = false;
host_hold_captured = false;
G ["host_recover_req"] = false;
G ["estop_hold_req"] = false;
if (G ["cylinder5_press_req"]) {
G ["cylinder5_value"] = 500;
} else {
G ["cylinder5_value"] = 2000;
}
for (i = 1; double(i) <= 7; ++i) {
spg_enable_exec [ i ] = false;
spg_disable_exec [ i ] = false;
return_exec [ i ] = false;
}
axis4_move_exec = false;
axis4_stop_exec = false;
for (j = 1; double(j) <= 7; ++j) {
G ["Act_pos"] [ j ] = G ["axis"] [ j ] ["NcToPlc"] ["ActPos"] - G ["init_pos"] [ j ];
G ["act_pos_from_left"] [ j ] = G ["axis"] [ j ] ["NcToPlc"] ["ActPos"] - G ["leftlimit"] [ j ];
G ["refer_from_left"] [ j ] = ( G ["refer"] [ j ] + G ["init_pos"] [ j ] ) - G ["leftlimit"] [ j ];
if (return_enable [ j ] && ( ( return_state [ j ] >= 30 ) || G ["return_cmd"] [ j ] ["Done"] || G ["return_cmd"] [ j ] ["Error"] )) {
G ["refer"] [ j ] = traj_pos [ j ];
G ["refer_from_left"] [ j ] = traj_pos [ j ] + G ["init_pos"] [ j ] - G ["leftlimit"] [ j ];
continue;
}
v_step = G ["v_limit"] [ j ];
return_handoff_bypass = ( ( j == 1 ) && ( G ["axis1_fast_return"] || G ["return_cmd"] [ 1 ] ["Busy"] ) ) || ( ( j == 6 ) && ( G ["axis6_fast_retract"] || G ["return_cmd"] [ 6 ] ["Busy"] ) );
if (return_handoff_bypass) {
avg_window = 1;
ramp_counter [ j ] = 0;
} else {
ramp_counter [ j ] = st_min ( ramp_counter [ j ] + 1 , avg_window_size );
avg_window = st_max ( 1 , ramp_counter [ j ] );
}
for (k = 2; double(k) <= 20; ++k) {
act_hh [ j ] [ 22 - k ] = act_hh [ j ] [ 21 - k ];
}
if (slimit_enable [ j ]) {
if (( G ["axis"] [ j ] ["NcToPlc"] ["ActPos"] >= right_slimit [ j ] ) && ( G ["refer"] [ j ] > G ["ref_slow"] [ j ] )) {
G ["refer"] [ j ] = G ["ref_slow"] [ j ];
}
}
if (return_handoff_bypass) {
G ["ref_slow"] [ j ] = G ["refer"] [ j ];
} else if (( G ["refer"] [ j ] - G ["ref_slow"] [ j ] ) > v_step) {
G ["ref_slow"] [ j ] = G ["ref_slow"] [ j ] + v_step;
} else if (( G ["refer"] [ j ] - G ["ref_slow"] [ j ] ) < - v_step) {
G ["ref_slow"] [ j ] = G ["ref_slow"] [ j ] - v_step;
} else {
G ["ref_slow"] [ j ] = G ["refer"] [ j ];
}
if (return_handoff_bypass) {
for (i = 1; double(i) <= 20; ++i) {
act_hh [ j ] [ i ] = G ["ref_slow"] [ j ];
}
G ["act_h"] [ j ] = G ["ref_slow"] [ j ];
} else {
act_hh [ j ] [ 1 ] = G ["ref_slow"] [ j ];
G ["act_h"] [ j ] = 0;
for (i = 1; double(i) <= avg_window; ++i) {
G ["act_h"] [ j ] = G ["act_h"] [ j ] + act_hh [ j ] [ i ] / double ( avg_window );
}
}
if (traj_enable [ j ] && ! return_handoff_bypass && ! G ["startup_smoothing_bypass"]) {
acc_raw = traj_wn * traj_wn * ( G ["refer"] [ j ] - traj_pos [ j ] ) - 2.0 * traj_wn * traj_vel [ j ];
if (acc_raw > traj_amax [ j ]) {
acc_raw = traj_amax [ j ];
} else if (acc_raw < - traj_amax [ j ]) {
acc_raw = - traj_amax [ j ];
}
traj_acc [ j ] = acc_raw;
traj_vel [ j ] = traj_vel [ j ] + 0.001 * traj_acc [ j ];
if (traj_vel [ j ] > traj_vmax [ j ]) {
traj_vel [ j ] = traj_vmax [ j ];
} else if (traj_vel [ j ] < - traj_vmax [ j ]) {
traj_vel [ j ] = - traj_vmax [ j ];
}
traj_pos [ j ] = traj_pos [ j ] + 0.001 * traj_vel [ j ];
if (std::abs ( G ["refer"] [ j ] - traj_pos [ j ] ) < 0.001 && std::abs ( traj_vel [ j ] ) < 0.01) {
traj_pos [ j ] = G ["refer"] [ j ];
traj_vel [ j ] = 0.0;
traj_acc [ j ] = 0.0;
}
} else {
traj_pos [ j ] = G ["act_h"] [ j ];
traj_vel [ j ] = 0.0;
traj_acc [ j ] = 0.0;
}
}
for (i = 1; double(i) <= 7; ++i) {
if (! return_enable [ i ]) {
continue;
}
if (return_state [ i ] == 0) {
G ["return_cmd"] [ i ] ["Busy"] = false;
if (! G ["return_cmd"] [ i ] ["Req"]) {
G ["return_cmd"] [ i ] ["Done"] = false;
G ["return_cmd"] [ i ] ["Error"] = false;
G ["return_cmd"] [ i ] ["ErrorId"] = 0;
return_finish_error [ i ] = false;
return_error_id_pending [ i ] = 0;
} else if (( ! G ["return_cmd"] [ i ] ["Done"] ) && ( ! G ["return_cmd"] [ i ] ["Error"] )) {
return_target_abs [ i ] = G ["return_cmd"] [ i ] ["TargetAbs"];
return_velocity [ i ] = G ["return_cmd"] [ i ] ["Velocity"];
return_acc [ i ] = G ["return_cmd"] [ i ] ["Acc"];
return_dec [ i ] = G ["return_cmd"] [ i ] ["Dec"];
return_jerk [ i ] = G ["return_cmd"] [ i ] ["Jerk"];
return_state [ i ] = 10;
}
}
switch (int(return_state [ i ])) {
case 10:
{
G ["return_cmd"] [ i ] ["Busy"] = true;
spg_disable_exec [ i ] = true;
if (G ["SetPointGenDisable"] [ i ] ["Done"] || ! G ["SetPointGenDisable"] [ i ] ["Enabled"]) {
ext_setpoint_enabled [ i ] = false;
return_state [ i ] = 20;
}
break;
}
case 20:
{
G ["return_cmd"] [ i ] ["Busy"] = true;
return_exec [ i ] = true;
G ["refer_from_left"] [ i ] = return_target_abs [ i ] - G ["leftlimit"] [ i ];
if (fb_return_move_abs [ i ] ["Error"] || fb_return_move_abs [ i ] ["CommandAborted"]) {
return_finish_error [ i ] = true;
return_error_id_pending [ i ] = fb_return_move_abs [ i ] ["ErrorID"];
return_state [ i ] = 30;
} else if (fb_return_move_abs [ i ] ["Done"]) {
return_finish_error [ i ] = false;
return_error_id_pending [ i ] = 0;
return_state [ i ] = 30;
}
break;
}
case 30:
{
G ["return_cmd"] [ i ] ["Busy"] = true;
if (std::abs ( G ["axis"] [ i ] ["NcToPlc"] ["ActVelo"] ) <= 0.5) {
G ["Act_pos"] [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"] - G ["init_pos"] [ i ];
G ["act_pos_from_left"] [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"] - G ["leftlimit"] [ i ];
if (return_state [ i ] != 30) {
G ["refer"] [ i ] = G ["Act_pos"] [ i ];
G ["ref_slow"] [ i ] = G ["Act_pos"] [ i ];
G ["act_h"] [ i ] = G ["Act_pos"] [ i ];
G ["act_hf"] [ i ] = G ["Act_pos"] [ i ];
G ["refer_from_left"] [ i ] = G ["act_pos_from_left"] [ i ];
for (k = 1; double(k) <= 30; ++k) {
act_hh [ i ] [ k ] = G ["Act_pos"] [ i ];
}
act_h_prev [ i ] = G ["Act_pos"] [ i ];
traj_pos [ i ] = G ["Act_pos"] [ i ];
}
traj_vel [ i ] = 0.0;
traj_acc [ i ] = 0.0;
return_state [ i ] = 31;
}
break;
}
case 31:
{
G ["return_cmd"] [ i ] ["Busy"] = true;
spg_enable_exec [ i ] = true;
if (G ["SetPointGenEnable"] [ i ] ["Done"]) {
ext_setpoint_enabled [ i ] = true;
G ["return_cmd"] [ i ] ["Busy"] = false;
if (return_finish_error [ i ]) {
G ["return_cmd"] [ i ] ["Done"] = false;
G ["return_cmd"] [ i ] ["Error"] = true;
G ["return_cmd"] [ i ] ["ErrorId"] = return_error_id_pending [ i ];
} else {
G ["return_cmd"] [ i ] ["Done"] = true;
G ["return_cmd"] [ i ] ["Error"] = false;
G ["return_cmd"] [ i ] ["ErrorId"] = 0;
}
return_finish_error [ i ] = false;
return_error_id_pending [ i ] = 0;
return_state [ i ] = 0;
}
break;
}
}
}
axis4_request_dir_now = 0;
if (G ["axis4_fwd_req"] && ! G ["axis4_rev_req"]) {
axis4_request_dir_now = 1;
} else if (G ["axis4_rev_req"] && ! G ["axis4_fwd_req"]) {
axis4_request_dir_now = - 1;
}
if (axis4_manual_state == 0) {
G ["axis4_manual_busy"] = false;
if (axis4_request_dir_now == 0) {
G ["axis4_manual_done"] = false;
G ["axis4_manual_error"] = false;
G ["axis4_manual_error_id"] = 0;
} else if (! G ["axis4_manual_error"]) {
G ["axis4_manual_done"] = false;
G ["axis4_manual_error"] = false;
G ["axis4_manual_error_id"] = 0;
axis4_requested_dir = axis4_request_dir_now;
axis4_pending_dir = 0;
axis4_finish_error = false;
axis4_error_id_pending = 0;
axis4_manual_state = 10;
}
}
switch (int(axis4_manual_state)) {
case 10:
{
G ["axis4_manual_busy"] = true;
spg_disable_exec [ 4 ] = true;
if (G ["SetPointGenDisable"] [ 4 ] ["Done"] || ! G ["SetPointGenDisable"] [ 4 ] ["Enabled"]) {
ext_setpoint_enabled [ 4 ] = false;
axis4_manual_state = 20;
}
break;
}
case 20:
{
G ["axis4_manual_busy"] = true;
axis4_pending_dir = axis4_request_dir_now;
if (axis4_requested_dir >= 0) {
axis4_manual_direction = 1;
} else {
axis4_manual_direction = -1;
}
if (axis4_pending_dir == 0) {
axis4_manual_state = 30;
} else if (axis4_pending_dir != axis4_requested_dir) {
axis4_manual_state = 30;
} else {
axis4_move_exec = true;
}
if (fb_axis4_move_velocity ["Error"]) {
axis4_finish_error = true;
axis4_error_id_pending = fb_axis4_move_velocity ["ErrorID"];
G ["axis4_manual_error_id"] = fb_axis4_move_velocity ["ErrorID"];
axis4_pending_dir = 0;
axis4_manual_state = 40;
} else if (fb_axis4_move_velocity ["CommandAborted"]) {
axis4_pending_dir = axis4_request_dir_now;
axis4_manual_state = 30;
}
break;
}
case 30:
{
G ["axis4_manual_busy"] = true;
axis4_pending_dir = axis4_request_dir_now;
axis4_stop_exec = true;
if (fb_axis4_stop ["Error"]) {
axis4_finish_error = true;
axis4_error_id_pending = fb_axis4_stop ["ErrorID"];
axis4_pending_dir = 0;
axis4_manual_state = 39;
} else if (fb_axis4_stop ["Done"] || ( std::abs ( G ["axis"] [ 4 ] ["NcToPlc"] ["ActVelo"] ) <= 0.05 )) {
axis4_manual_state = 39;
}
break;
}
case 39:
{
G ["axis4_manual_busy"] = true;
axis4_pending_dir = axis4_request_dir_now;
if (axis4_pending_dir != 0) {
axis4_requested_dir = axis4_pending_dir;
axis4_manual_state = 20;
} else {
axis4_requested_dir = 0;
axis4_manual_state = 40;
}
break;
}
case 40:
{
G ["axis4_manual_busy"] = true;
G ["Act_pos"] [ 4 ] = G ["axis"] [ 4 ] ["NcToPlc"] ["ActPos"] - G ["init_pos"] [ 4 ];
G ["act_pos_from_left"] [ 4 ] = G ["axis"] [ 4 ] ["NcToPlc"] ["ActPos"] - G ["leftlimit"] [ 4 ];
G ["refer"] [ 4 ] = G ["Act_pos"] [ 4 ];
G ["ref_slow"] [ 4 ] = G ["Act_pos"] [ 4 ];
G ["act_h"] [ 4 ] = G ["Act_pos"] [ 4 ];
G ["act_hf"] [ 4 ] = G ["Act_pos"] [ 4 ];
G ["refer_from_left"] [ 4 ] = G ["act_pos_from_left"] [ 4 ];
for (k = 1; double(k) <= 30; ++k) {
act_hh [ 4 ] [ k ] = G ["Act_pos"] [ 4 ];
}
act_h_prev [ 4 ] = G ["Act_pos"] [ 4 ];
traj_pos [ 4 ] = G ["Act_pos"] [ 4 ];
traj_vel [ 4 ] = 0.0;
traj_acc [ 4 ] = 0.0;
spg_enable_exec [ 4 ] = true;
if (G ["SetPointGenEnable"] [ 4 ] ["Done"]) {
ext_setpoint_enabled [ 4 ] = true;
G ["axis4_manual_busy"] = false;
if (axis4_finish_error) {
G ["axis4_manual_done"] = false;
G ["axis4_manual_error"] = true;
G ["axis4_manual_error_id"] = axis4_error_id_pending;
} else {
G ["axis4_manual_done"] = true;
G ["axis4_manual_error"] = false;
G ["axis4_manual_error_id"] = 0;
}
axis4_finish_error = false;
axis4_error_id_pending = 0;
axis4_manual_state = 0;
}
break;
}
}
for (i = 1; double(i) <= 7; ++i) {
if (G ["power_output"] [ i ] ["Error"]) {
G ["axis_Reset"] [ i ] ["Execute"] = true;
}
}
for (i = 1; double(i) <= 7; ++i) {
if (ext_setpoint_enabled [ i ] || ( return_state [ i ] == 31 ) || spg_enable_exec [ i ]) {
if (traj_enable [ i ] && ! G ["startup_smoothing_bypass"]) {
pos_abs = traj_pos [ i ] + G ["init_pos"] [ i ];
feed(G ["axis"] [ i ], {{"Position", pos_abs}, {"Velocity", traj_vel [ i ]}, {"Acceleration", traj_acc [ i ]}, {"Direction", 1}});
} else {
pos_abs = G ["act_h"] [ i ] + G ["init_pos"] [ i ];
legacy_vel = ( G ["act_h"] [ i ] - act_h_prev [ i ] ) / 0.001;
feed(G ["axis"] [ i ], {{"Position", pos_abs}, {"Velocity", legacy_vel}, {"Acceleration", 0}, {"Direction", 1}});
}
G ["act_hf"] [ i ] = G ["act_h"] [ i ];
} else {
G ["act_hf"] [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"] - G ["init_pos"] [ i ];
}
act_h_prev [ i ] = G ["act_h"] [ i ];
}
for (i = 1; double(i) <= 7; ++i) {
G ["SetPointGenEnable"] [ i ] ["Execute"] = spg_enable_exec [ i ];
G ["SetPointGenDisable"] [ i ] ["Execute"] = spg_disable_exec [ i ];
call(fb_return_move_abs [ i ], {{"Position", return_target_abs [ i ]}, {"Velocity", return_velocity [ i ]}, {"Acceleration", return_acc [ i ]}, {"Deceleration", return_dec [ i ]}, {"Jerk", return_jerk [ i ]}, {"Execute", return_exec [ i ]}});
}
call(fb_axis4_move_velocity, {{"Execute", axis4_move_exec}, {"Velocity", axis4_manual_velocity}, {"Acceleration", axis4_manual_acc}, {"Deceleration", axis4_manual_dec}, {"Jerk", axis4_manual_jerk}, {"Direction", axis4_manual_direction}});
call(fb_axis4_stop, {{"Execute", axis4_stop_exec}, {"Deceleration", axis4_manual_dec}, {"Jerk", axis4_manual_jerk}});
}
};
struct Olderr {
Node& G;
Node i;
Node step;
Node pulse_timer;
Node retry_timer;
Node has_err;
Node has_power_not_ready;
Node has_other_error;
Node all_powered;
Node power_not_ready_errid;
explicit Olderr(Node& global) : G(global) {
step = 0;
pulse_timer.kind = "TON";
retry_timer.kind = "TON";
power_not_ready_errid = 18000;
}
void cycle() {
has_err = false;
has_power_not_ready = false;
has_other_error = false;
all_powered = true;
for (i = 1; double(i) <= 7; ++i) {
if (! G ["power_output"] [ i ] ["Done"]) {
all_powered = false;
}
if (G ["axis"] [ i ] ["Status"] ["Error"] || G ["axis"] [ i ] ["Status"] ["DriveDeviceError"] || G ["power_output"] [ i ] ["Error"] || G ["reset_output"] [ i ] ["Error"]) {
has_err = true;
if (( G ["power_output"] [ i ] ["Error"] && ( G ["power_output"] [ i ] ["ErrorID"] == power_not_ready_errid ) ) || ( G ["axis"] [ i ] ["Status"] ["Error"] && ( G ["axis"] [ i ] ["Status"] ["ErrorID"] == power_not_ready_errid ) )) {
has_power_not_ready = true;
}
if (( G ["axis"] [ i ] ["Status"] ["Error"] && ( G ["axis"] [ i ] ["Status"] ["ErrorID"] != power_not_ready_errid ) ) || G ["axis"] [ i ] ["Status"] ["DriveDeviceError"] || G ["reset_output"] [ i ] ["Error"] || ( G ["power_output"] [ i ] ["Error"] && ( G ["power_output"] [ i ] ["ErrorID"] != power_not_ready_errid ) )) {
has_other_error = true;
}
}
}
if (G ["axis2_sync_axis"] ["Status"] ["Error"] || G ["axis2_sync_axis"] ["Status"] ["DriveDeviceError"]) {
has_err = true;
has_other_error = true;
}
G ["estop_hold_req"] = true;
switch (int(step)) {
case 0:
{
for (i = 1; double(i) <= 7; ++i) {
G ["SetPointGenEnable"] [ i ] ["Execute"] = false;
G ["SetPointGenDisable"] [ i ] ["Execute"] = true;
G ["axis_Reset"] [ i ] ["Execute"] = false;
}
call(pulse_timer, {{"IN", false}});
call(retry_timer, {{"IN", false}});
step = 1;
break;
}
case 1:
{
for (i = 1; double(i) <= 7; ++i) {
G ["SetPointGenDisable"] [ i ] ["Execute"] = false;
}
if (( ! has_err ) && all_powered) {
step = 0;
G ["gen_state"] = _clear_err;
return;
}
if (! has_err) {
for (i = 1; double(i) <= 7; ++i) {
G ["axis_Reset"] [ i ] ["Execute"] = false;
G ["power_"] [ i ] ["Enable"] = false;
}
call(pulse_timer, {{"IN", true}, {"PT", 200}});
step = 10;
return;
}
if (has_power_not_ready && ! has_other_error) {
call(pulse_timer, {{"IN", true}, {"PT", 200}});
call(retry_timer, {{"IN", false}});
step = 10;
return;
}
for (i = 1; double(i) <= 7; ++i) {
if (G ["axis"] [ i ] ["Status"] ["Error"] || G ["axis"] [ i ] ["Status"] ["DriveDeviceError"] || G ["power_output"] [ i ] ["Error"] || G ["reset_output"] [ i ] ["Error"]) {
G ["axis_Reset"] [ i ] ["Execute"] = true;
}
}
call(pulse_timer, {{"IN", true}, {"PT", 200}});
step = 2;
break;
}
case 2:
{
call(pulse_timer, {{"IN", true}});
if (pulse_timer ["Q"]) {
for (i = 1; double(i) <= 7; ++i) {
G ["axis_Reset"] [ i ] ["Execute"] = false;
}
call(pulse_timer, {{"IN", false}});
call(retry_timer, {{"IN", true}, {"PT", 2000}});
step = 3;
}
break;
}
case 3:
{
call(retry_timer, {{"IN", true}});
if (( ! has_err ) && all_powered) {
call(retry_timer, {{"IN", false}});
step = 0;
G ["gen_state"] = _clear_err;
return;
}
if (retry_timer ["Q"]) {
call(retry_timer, {{"IN", false}});
step = 1;
}
break;
}
case 10:
{
for (i = 1; double(i) <= 7; ++i) {
if (( G ["power_output"] [ i ] ["Error"] && ( G ["power_output"] [ i ] ["ErrorID"] == power_not_ready_errid ) ) || ( G ["axis"] [ i ] ["Status"] ["Error"] && ( G ["axis"] [ i ] ["Status"] ["ErrorID"] == power_not_ready_errid ) ) || G ["axis"] [ i ] ["Status"] ["DriveDeviceError"]) {
G ["power_"] [ i ] ["Enable"] = false;
G ["axis_Reset"] [ i ] ["Execute"] = true;
} else {
G ["power_"] [ i ] ["Enable"] = true;
G ["axis_Reset"] [ i ] ["Execute"] = false;
}
}
call(pulse_timer, {{"IN", true}, {"PT", 200}});
if (pulse_timer ["Q"]) {
call(pulse_timer, {{"IN", false}});
for (i = 1; double(i) <= 7; ++i) {
G ["axis_Reset"] [ i ] ["Execute"] = false;
G ["power_"] [ i ] ["Enable"] = true;
}
call(retry_timer, {{"IN", true}, {"PT", 3000}});
step = 11;
}
break;
}
case 11:
{
for (i = 1; double(i) <= 7; ++i) {
G ["power_"] [ i ] ["Enable"] = true;
}
call(retry_timer, {{"IN", true}});
if (( ! has_err ) && all_powered) {
call(retry_timer, {{"IN", false}});
step = 0;
G ["gen_state"] = _clear_err;
return;
}
if (has_err) {
call(retry_timer, {{"IN", false}});
step = 1;
return;
}
if (retry_timer ["Q"]) {
call(retry_timer, {{"IN", false}});
step = 1;
}
break;
}
}
}
};
struct Oldclear_err {
Node& G;
Node i;
Node step;
Node settle_timer;
Node all_powered;
Node has_err;
explicit Oldclear_err(Node& global) : G(global) {
step = 0;
settle_timer.kind = "TON";
}
void cycle() {
has_err = false;
for (i = 1; double(i) <= 7; ++i) {
if (G ["axis"] [ i ] ["Status"] ["Error"] || G ["power_output"] [ i ] ["Error"] || G ["reset_output"] [ i ] ["Error"]) {
has_err = true;
break;
}
}
all_powered = true;
for (i = 1; double(i) <= 7; ++i) {
if (! G ["power_output"] [ i ] ["Done"]) {
all_powered = false;
break;
}
}
G ["estop_hold_req"] = true;
switch (int(step)) {
case 0:
{
for (i = 1; double(i) <= 7; ++i) {
G ["SetPointGenEnable"] [ i ] ["Execute"] = false;
G ["SetPointGenDisable"] [ i ] ["Execute"] = false;
G ["axis_Reset"] [ i ] ["Execute"] = false;
}
call(settle_timer, {{"IN", false}});
step = 1;
break;
}
case 1:
{
if (has_err) {
call(settle_timer, {{"IN", false}});
step = 0;
G ["gen_state"] = _err;
return;
}
if (! G ["self_check_done"]) {
call(settle_timer, {{"IN", false}});
step = 0;
G ["gen_state"] = _init;
return;
}
if (! all_powered) {
for (i = 1; double(i) <= 7; ++i) {
G ["power_"] [ i ] ["Enable"] = true;
}
call(settle_timer, {{"IN", false}});
return;
}
call(settle_timer, {{"IN", true}, {"PT", 200}});
if (settle_timer ["Q"]) {
call(settle_timer, {{"IN", false}});
step = 0;
G ["handle_reinit_req"] = true;
G ["gen_state"] = _handle;
}
break;
}
}
}
};
struct OldAxis2Sync {
Node& G;
Node fb_power;
Node fb_gear;
Node fb_reset;
Node gear_execute;
Node gear_active;
Node gear_done;
Node gear_failed;
explicit OldAxis2Sync(Node& global) : G(global) {
fb_power.kind = "MC_Power";
fb_gear.kind = "MC_GearIn";
fb_reset.kind = "MC_Reset";
gear_execute = false;
gear_active = false;
gear_done = false;
gear_failed = false;
}
void cycle() {
call(G ["axis2_sync_axis"], {});
call(fb_reset, {{"Execute", G ["axis_Reset"] [ 2 ] ["Execute"]}});
call(fb_power, {{"Enable", G ["power_"] [ 2 ] ["Enable"]}, {"Enable_Positive", true}, {"Enable_Negative", true}});
if (! ( G ["power_"] [ 2 ] ["Enable"] && G ["power_output"] [ 2 ] ["Done"] && fb_power ["Status"] )) {
gear_execute = false;
gear_active = false;
gear_done = false;
gear_failed = false;
} else if (fb_power ["Status"] && G ["power_output"] [ 2 ] ["Done"] && ! gear_done && ! gear_failed && ! gear_active && ( std::abs ( G ["axis"] [ 2 ] ["NcToPlc"] ["ActVelo"] ) <= 0.01 ) && ( std::abs ( G ["axis2_sync_axis"] ["NcToPlc"] ["ActVelo"] ) <= 0.01 )) {
gear_execute = true;
gear_active = true;
}
call(fb_gear, {{"Execute", gear_execute}, {"RatioNumerator", 1.0}, {"RatioDenominator", 1}, {"Acceleration", 100.0}, {"Deceleration", 100.0}, {"Jerk", 500.0}, {"BufferMode", 0}});
if (fb_gear ["InGear"]) {
gear_execute = false;
gear_active = false;
gear_done = true;
} else if (fb_gear ["Error"] || fb_gear ["CommandAborted"]) {
gear_execute = false;
gear_active = false;
gear_failed = true;
}
}
};
struct OldMAIN {
Node& G;
Node init_;
Node handle1_;
Node clear_err_;
Node err_;
Node reset_;
Node self_check_;
Node arm_manual_;
Node arm_cartesian_;
Node arm_cart_jog_;
Node axis2_sync_;
Node injector_manual_;
Node axis6_enable_negative;
Node selfcheck_started;
Node selfcheck_target_latched;
Node host_watchdog_timer;
Node host_last_heartbeat_sequence;
Node host_last_session_id;
Node host_session_changed;
Node host_heartbeat_changed;
Node host_heartbeat_seen;
Node i;
Node POWERED;
Node RESETED;
Node SETPOSITIONGEN;
explicit OldMAIN(Node& global) : G(global) {
axis6_enable_negative = true;
selfcheck_started = false;
host_watchdog_timer.kind = "TON";
}
void cycle() {
for (i = 1; double(i) <= 7; ++i) {
call(G ["axis"] [ i ], {});
}
for (i = 1; double(i) <= 2; ++i) {
call(G ["inject_axis"] [ i ], {});
}
if (TwinCAT_SystemInfoVarList ["_TaskInfo"] [ 1 ] ["FirstCycle"]) {
G ["gen_state"] = _init;
G ["self_check_done"] = false;
G ["selfcheck_start_req"] = false;
G ["selfcheck_status"] = 0;
G ["handle_reinit_req"] = false;
G ["handle_reinit_done"] = false;
G ["estop_hold_req"] = false;
G ["selfcheck_reset_req"] = true;
for (i = 1; double(i) <= 7; ++i) {
G ["SetPointGenEnable"] [ i ] ["Execute"] = false;
G ["SetPointGenDisable"] [ i ] ["Execute"] = false;
G ["axis_Reset"] [ i ] ["Execute"] = false;
}
G ["arm_manual_enable"] = false;
for (i = 1; double(i) <= 5; ++i) {
G ["arm_enable_req"] [ i ] = false;
G ["arm_reset_req"] [ i ] = false;
G ["arm_jog_pos_req"] [ i ] = false;
G ["arm_jog_neg_req"] [ i ] = false;
}
}
host_session_changed = G ["host_session_id"] != host_last_session_id;
host_heartbeat_changed = host_session_changed || ( G ["host_heartbeat_sequence"] != host_last_heartbeat_sequence );
if (host_session_changed) {
if (host_last_session_id != 0) {
G ["host_comm_timeout"] = true;
}
host_last_session_id = G ["host_session_id"];
}
if (host_heartbeat_changed) {
host_last_heartbeat_sequence = G ["host_heartbeat_sequence"];
}
host_heartbeat_seen = G ["host_session_id"] != 0;
call(host_watchdog_timer, {{"IN", host_heartbeat_seen && ! host_heartbeat_changed}, {"PT", 2000}});
if (( ! host_heartbeat_seen ) || host_watchdog_timer ["Q"]) {
G ["host_comm_timeout"] = true;
} else if (G ["gen_state"] != _handle) {
if (G ["host_recover_req"]) {
G ["host_comm_timeout"] = false;
G ["host_recover_req"] = false;
}
}
call(arm_manual_, {});
call(arm_cartesian_, {});
call(arm_cart_jog_, {});
call(axis2_sync_, {});
call(injector_manual_, {});
axis6_enable_negative = ! ( ( G ["gen_state"] == _self_check ) && G ["axis"] [ 6 ] ["Status"] ["DriveLimitActive"] );
call(POWERED, {});
if (G ["gen_state"] != _handle) {
call(SETPOSITIONGEN, {});
}
call(RESETED, {});
if (G ["gen_state"] != _self_check) {
selfcheck_started = false;
G ["selfcheck_start_req"] = false;
G ["selfcheck_status"] = 0;
}
switch (int(G ["gen_state"])) {
case _init:
{
call(init_, {});
break;
}
case _self_check:
{
if (! selfcheck_started) {
if (G ["selfcheck_status"] != 3) {
G ["selfcheck_status"] = 1;
}
if (G ["selfcheck_start_req"]) {
G ["selfcheck_start_req"] = false;
if (G ["selfcheck_target_from_left"] [ 1 ] >= 5.0 && G ["selfcheck_target_from_left"] [ 1 ] <= 96.0 && G ["selfcheck_target_from_left"] [ 3 ] >= 10.0 && G ["selfcheck_target_from_left"] [ 3 ] <= 650.0 && G ["selfcheck_target_from_left"] [ 5 ] >= 10.0 && G ["selfcheck_target_from_left"] [ 5 ] <= 670.0 && G ["selfcheck_target_from_left"] [ 6 ] >= 10.0 && G ["selfcheck_target_from_left"] [ 6 ] <= 670.0 && G ["selfcheck_target_from_left"] [ 1 ] <= G ["selfcheck_target_from_left"] [ 3 ] && G ["selfcheck_target_from_left"] [ 3 ] <= G ["selfcheck_target_from_left"] [ 5 ] && G ["selfcheck_target_from_left"] [ 5 ] <= G ["selfcheck_target_from_left"] [ 6 ]) {
selfcheck_target_latched = G ["selfcheck_target_from_left"];
selfcheck_started = true;
G ["selfcheck_status"] = 2;
} else {
G ["selfcheck_status"] = 3;
}
}
}
if (selfcheck_started) {
call(self_check_, {{"init_target_from_left", selfcheck_target_latched}});
}
break;
}
case _handle:
{
call(handle1_, {});
call(SETPOSITIONGEN, {});
break;
}
case _clear_err:
{
call(clear_err_, {});
break;
}
case _err:
{
call(err_, {});
break;
}
case _reset:
{
call(reset_, {});
break;
}
}
}
};
struct OldSelfCheck {
Node& G;
Node init_target_from_left;
Node i;
Node stu;
Node fb_move_abs;
Node fb_move_vel;
Node fb_stop;
Node fb_set_pos;
Node power_not_ready_errid;
Node vel_scan;
Node vel_back;
Node target_axes;
Node stagger_delay;
Node scan_stagger_delay;
Node scan_stagger_timer;
Node stagger_started;
Node all_done;
Node all_reach_limit;
Node Execute;
Node debug_limit_pos;
Node debug_init_target_pos;
Node scan_start_pos;
Node mv_vel_exec;
Node mv_vel_vel;
Node mv_abs_exec;
Node mv_abs_pos;
Node mv_abs_vel;
Node drive_limit_stop;
Node set_pos_exec;
Node wait_timer;
Node power_pulse_timer;
Node power_wait_timer;
Node stagger_timer;
explicit OldSelfCheck(Node& global) : G(global) {
fb_move_abs[1].kind = "MC_MoveAbsolute";
fb_move_abs[2].kind = "MC_MoveAbsolute";
fb_move_abs[3].kind = "MC_MoveAbsolute";
fb_move_abs[4].kind = "MC_MoveAbsolute";
fb_move_abs[5].kind = "MC_MoveAbsolute";
fb_move_abs[6].kind = "MC_MoveAbsolute";
fb_move_abs[7].kind = "MC_MoveAbsolute";
fb_move_vel[1].kind = "MC_MoveVelocity";
fb_move_vel[2].kind = "MC_MoveVelocity";
fb_move_vel[3].kind = "MC_MoveVelocity";
fb_move_vel[4].kind = "MC_MoveVelocity";
fb_move_vel[5].kind = "MC_MoveVelocity";
fb_move_vel[6].kind = "MC_MoveVelocity";
fb_move_vel[7].kind = "MC_MoveVelocity";
fb_stop[1].kind = "MC_Stop";
fb_stop[2].kind = "MC_Stop";
fb_stop[3].kind = "MC_Stop";
fb_stop[4].kind = "MC_Stop";
fb_stop[5].kind = "MC_Stop";
fb_stop[6].kind = "MC_Stop";
fb_stop[7].kind = "MC_Stop";
fb_set_pos[1].kind = "MC_SetPosition";
fb_set_pos[2].kind = "MC_SetPosition";
fb_set_pos[3].kind = "MC_SetPosition";
fb_set_pos[4].kind = "MC_SetPosition";
fb_set_pos[5].kind = "MC_SetPosition";
fb_set_pos[6].kind = "MC_SetPosition";
fb_set_pos[7].kind = "MC_SetPosition";
power_not_ready_errid = 18000;
vel_scan = 40.0;
vel_back = 40.0;
target_axes[1] = true;
target_axes[2] = false;
target_axes[3] = true;
target_axes[4] = false;
target_axes[5] = true;
target_axes[6] = true;
target_axes[7] = false;
stagger_delay[1] = 3000;
stagger_delay[2] = 0;
stagger_delay[3] = 2000;
stagger_delay[4] = 0;
stagger_delay[5] = 1000;
stagger_delay[6] = 0;
stagger_delay[7] = 0;
scan_stagger_delay[1] = 0;
scan_stagger_delay[2] = 0;
scan_stagger_delay[3] = 300;
scan_stagger_delay[4] = 0;
scan_stagger_delay[5] = 600;
scan_stagger_delay[6] = 900;
scan_stagger_delay[7] = 0;
scan_stagger_timer[1].kind = "TON";
scan_stagger_timer[2].kind = "TON";
scan_stagger_timer[3].kind = "TON";
scan_stagger_timer[4].kind = "TON";
scan_stagger_timer[5].kind = "TON";
scan_stagger_timer[6].kind = "TON";
scan_stagger_timer[7].kind = "TON";
stagger_started = false;
wait_timer[1].kind = "TON";
wait_timer[2].kind = "TON";
wait_timer[3].kind = "TON";
wait_timer[4].kind = "TON";
wait_timer[5].kind = "TON";
wait_timer[6].kind = "TON";
wait_timer[7].kind = "TON";
power_pulse_timer[1].kind = "TON";
power_pulse_timer[2].kind = "TON";
power_pulse_timer[3].kind = "TON";
power_pulse_timer[4].kind = "TON";
power_pulse_timer[5].kind = "TON";
power_pulse_timer[6].kind = "TON";
power_pulse_timer[7].kind = "TON";
power_wait_timer[1].kind = "TON";
power_wait_timer[2].kind = "TON";
power_wait_timer[3].kind = "TON";
power_wait_timer[4].kind = "TON";
power_wait_timer[5].kind = "TON";
power_wait_timer[6].kind = "TON";
power_wait_timer[7].kind = "TON";
stagger_timer.kind = "TON";
}
void cycle() {
if (G ["selfcheck_reset_req"]) {
G ["startup_loading_ready"] = false;
for (i = 1; double(i) <= 7; ++i) {
stu [ i ] = 0;
mv_vel_exec [ i ] = false;
mv_vel_vel [ i ] = 0.0;
mv_abs_exec [ i ] = false;
mv_abs_pos [ i ] = 0.0;
mv_abs_vel [ i ] = 0.0;
drive_limit_stop [ i ] = false;
set_pos_exec [ i ] = false;
debug_limit_pos [ i ] = 0.0;
debug_init_target_pos [ i ] = 0.0;
scan_start_pos [ i ] = 0.0;
G ["selfcheck_stu"] [ i ] = 0;
call(wait_timer [ i ], {{"IN", false}});
call(scan_stagger_timer [ i ], {{"IN", false}});
call(power_pulse_timer [ i ], {{"IN", false}});
call(power_wait_timer [ i ], {{"IN", false}});
call(G ["SetPointGenDisable"] [ i ], {{"Execute", false}});
call(G ["axis_Reset"] [ i ], {{"Execute", false}});
call(fb_move_vel [ i ], {{"Execute", false}});
call(fb_move_abs [ i ], {{"Execute", false}});
call(fb_stop [ i ], {{"Execute", false}});
call(fb_set_pos [ i ], {{"Execute", false}});
}
stagger_started = false;
call(stagger_timer, {{"IN", false}});
G ["selfcheck_all_reach_limit"] = false;
for (i = 1; double(i) <= 7; ++i) {
G ["return_cmd"] [ i ] ["Req"] = false;
G ["return_cmd"] [ i ] ["Busy"] = false;
G ["return_cmd"] [ i ] ["Done"] = false;
G ["return_cmd"] [ i ] ["Error"] = false;
G ["return_cmd"] [ i ] ["ErrorId"] = 0;
}
G ["axis4_fwd_req"] = false;
G ["axis4_rev_req"] = false;
G ["axis4_manual_busy"] = false;
G ["axis4_manual_done"] = false;
G ["axis4_manual_error"] = false;
G ["axis4_manual_error_id"] = 0;
G ["selfcheck_reset_req"] = false;
}
G ["cylinder5_value"] = 2000;
G ["cylinder5_cmd"] = 2000;
G ["cylinder5_press_req"] = false;
all_reach_limit = true;
for (i = 1; double(i) <= 7; ++i) {
if (target_axes [ i ] && ( ( stu [ i ] < 6 ) || ( ! G ["power_output"] [ i ] ["Done"] ) )) {
all_reach_limit = false;
break;
}
}
G ["selfcheck_all_reach_limit"] = all_reach_limit;
call(stagger_timer, {{"IN", stagger_started}, {"PT", 10000}});
for (i = 1; double(i) <= 7; ++i) {
mv_vel_exec [ i ] = false;
mv_abs_exec [ i ] = false;
call(fb_stop [ i ], {});
if (target_axes [ i ]) {
switch (int(stu [ i ])) {
case 0:
{
call(G ["SetPointGenDisable"] [ i ], {{"Execute", true}});
if (G ["power_output"] [ i ] ["Done"] && ( G ["SetPointGenDisable"] [ i ] ["Done"] || ! G ["SetPointGenDisable"] [ i ] ["Enabled"] )) {
call(G ["SetPointGenDisable"] [ i ], {{"Execute", false}});
stu [ i ] = 1;
}
break;
}
case 1:
{
if (i == 1) {
G ["cylinder1_value"] = 1000;
G ["cylinder2_value"] = 10;
G ["cylinder3_value"] = 1000;
G ["cylinder4_value"] = 10;
}
call(scan_stagger_timer [ i ], {{"IN", true}, {"PT", scan_stagger_delay [ i ]}});
if (scan_stagger_timer [ i ] ["Q"] || scan_stagger_delay [ i ] == 0) {
call(scan_stagger_timer [ i ], {{"IN", false}});
scan_start_pos [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"];
drive_limit_stop [ i ] = false;
mv_vel_vel [ i ] = vel_scan;
mv_vel_exec [ i ] = true;
stu [ i ] = 2;
}
break;
}
case 2:
{
mv_vel_vel [ i ] = vel_scan;
mv_vel_exec [ i ] = true;
call(wait_timer [ i ], {{"IN", ( fb_move_vel [ i ] ["Busy"] && std::abs ( G ["axis"] [ i ] ["NcToPlc"] ["ActVelo"] ) < 0.5 && ( ( std::abs ( G ["axis"] [ i ] ["NcToPlc"] ["SetVelo"] ) > 5.0 ) || ( ( std::abs ( G ["axis"] [ i ] ["NcToPlc"] ["SetVelo"] ) < 0.5 ) && ( std::abs ( G ["axis"] [ i ] ["NcToPlc"] ["ActPos"] - scan_start_pos [ i ] ) > 1.0 ) ) ) )}, {"PT", 100}});
if (G ["axis"] [ i ] ["Status"] ["DriveLimitActive"]) {
G ["leftlimit"] [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"];
mv_vel_exec [ i ] = false;
drive_limit_stop [ i ] = true;
call(wait_timer [ i ], {{"IN", false}});
call(fb_stop [ i ], {{"Deceleration", 500}, {"Jerk", 1000}, {"Execute", true}});
stu [ i ] = 3;
} else if (fb_move_vel [ i ] ["Error"] || wait_timer [ i ] ["Q"]) {
G ["leftlimit"] [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"];
mv_vel_exec [ i ] = false;
drive_limit_stop [ i ] = false;
call(fb_stop [ i ], {{"Deceleration", 500}, {"Jerk", 1000}, {"Execute", true}});
stu [ i ] = 3;
}
break;
}
case 3:
{
if (std::abs ( G ["axis"] [ i ] ["NcToPlc"] ["ActVelo"] ) < 1.0) {
call(fb_stop [ i ], {{"Execute", false}});
if (drive_limit_stop [ i ]) {
call(wait_timer [ i ], {{"IN", true}, {"PT", 300}});
if (wait_timer [ i ] ["Q"]) {
call(wait_timer [ i ], {{"IN", false}});
if (G ["axis"] [ i ] ["Status"] ["Error"] || ( G ["power_output"] [ i ] ["Error"] && ( G ["power_output"] [ i ] ["ErrorID"] != power_not_ready_errid ) )) {
drive_limit_stop [ i ] = false;
stu [ i ] = 4;
} else if (G ["power_output"] [ i ] ["Done"] && ! G ["power_output"] [ i ] ["Error"]) {
drive_limit_stop [ i ] = false;
stu [ i ] = 6;
} else {
call(power_pulse_timer [ i ], {{"IN", false}});
call(power_wait_timer [ i ], {{"IN", false}});
stu [ i ] = 55;
}
}
} else {
call(wait_timer [ i ], {{"IN", false}});
if (G ["axis"] [ i ] ["Status"] ["Error"]) {
stu [ i ] = 4;
} else if (G ["power_output"] [ i ] ["Done"] && ! G ["power_output"] [ i ] ["Error"]) {
stu [ i ] = 6;
} else {
call(power_pulse_timer [ i ], {{"IN", false}});
call(power_wait_timer [ i ], {{"IN", false}});
stu [ i ] = 55;
}
}
} else {
call(wait_timer [ i ], {{"IN", false}});
}
break;
}
case 4:
{
call(G ["axis_Reset"] [ i ], {{"Execute", false}});
call(G ["axis_Reset"] [ i ], {{"Execute", true}});
stu [ i ] = 5;
break;
}
case 5:
{
call(G ["axis_Reset"] [ i ], {});
call(power_pulse_timer [ i ], {{"IN", false}});
call(power_wait_timer [ i ], {{"IN", false}});
if (G ["axis_Reset"] [ i ] ["Done"] || ! G ["axis"] [ i ] ["Status"] ["Error"]) {
call(G ["axis_Reset"] [ i ], {{"Execute", false}});
if (G ["power_output"] [ i ] ["Done"]) {
stu [ i ] = 6;
} else {
call(power_pulse_timer [ i ], {{"IN", false}});
call(power_wait_timer [ i ], {{"IN", false}});
if (i == 6) {
stagger_started = false;
call(stagger_timer, {{"IN", false}});
}
stu [ i ] = 55;
}
} else if (G ["axis_Reset"] [ i ] ["Error"]) {
call(G ["axis_Reset"] [ i ], {{"Execute", false}});
stu [ i ] = 4;
}
break;
}
case 55:
{
G ["power_"] [ i ] ["Enable"] = false;
call(power_pulse_timer [ i ], {{"IN", true}, {"PT", 200}});
if (power_pulse_timer [ i ] ["Q"]) {
call(power_pulse_timer [ i ], {{"IN", false}});
G ["power_"] [ i ] ["Enable"] = true;
stu [ i ] = 56;
}
break;
}
case 56:
{
G ["power_"] [ i ] ["Enable"] = true;
call(power_wait_timer [ i ], {{"IN", true}, {"PT", 3000}});
if (G ["power_output"] [ i ] ["Done"] && ! G ["power_output"] [ i ] ["Error"]) {
call(power_wait_timer [ i ], {{"IN", false}});
drive_limit_stop [ i ] = false;
stu [ i ] = 6;
} else if (power_wait_timer [ i ] ["Q"]) {
call(power_wait_timer [ i ], {{"IN", false}});
call(power_pulse_timer [ i ], {{"IN", false}});
stu [ i ] = 55;
}
break;
}
case 6:
{
if (all_reach_limit && G ["power_output"] [ i ] ["Done"]) {
stu [ i ] = 7;
} else if (! G ["power_output"] [ i ] ["Done"]) {
call(power_pulse_timer [ i ], {{"IN", false}});
call(power_wait_timer [ i ], {{"IN", false}});
if (i == 6) {
stagger_started = false;
call(stagger_timer, {{"IN", false}});
}
stu [ i ] = 55;
}
break;
}
case 7:
{
if (! G ["power_output"] [ i ] ["Done"]) {
set_pos_exec [ i ] = false;
call(fb_set_pos [ i ], {{"Execute", false}});
call(power_pulse_timer [ i ], {{"IN", false}});
call(power_wait_timer [ i ], {{"IN", false}});
if (i == 6) {
stagger_started = false;
call(stagger_timer, {{"IN", false}});
}
stu [ i ] = 55;
} else {
set_pos_exec [ i ] = true;
call(fb_set_pos [ i ], {{"Position", G ["axis"] [ i ] ["NcToPlc"] ["ActPos"]}, {"Mode", false}, {"Execute", set_pos_exec [ i ]}});
stu [ i ] = 71;
}
break;
}
case 71:
{
call(fb_set_pos [ i ], {{"Execute", set_pos_exec [ i ]}});
if (! G ["power_output"] [ i ] ["Done"]) {
set_pos_exec [ i ] = false;
call(fb_set_pos [ i ], {{"Execute", false}});
call(power_pulse_timer [ i ], {{"IN", false}});
call(power_wait_timer [ i ], {{"IN", false}});
if (i == 6) {
stagger_started = false;
call(stagger_timer, {{"IN", false}});
}
stu [ i ] = 55;
} else if (fb_set_pos [ i ] ["Done"]) {
set_pos_exec [ i ] = false;
call(fb_set_pos [ i ], {{"Execute", false}});
debug_limit_pos [ i ] = G ["leftlimit"] [ i ];
debug_init_target_pos [ i ] = G ["leftlimit"] [ i ] + init_target_from_left [ i ];
mv_abs_pos [ i ] = debug_init_target_pos [ i ];
mv_abs_vel [ i ] = vel_back;
mv_abs_exec [ i ] = false;
call(wait_timer [ i ], {{"IN", false}});
stu [ i ] = 72;
} else if (fb_set_pos [ i ] ["Error"]) {
set_pos_exec [ i ] = false;
call(fb_set_pos [ i ], {{"Execute", false}});
if (G ["power_output"] [ i ] ["Error"]) {
call(power_pulse_timer [ i ], {{"IN", false}});
call(power_wait_timer [ i ], {{"IN", false}});
if (i == 6) {
stagger_started = false;
call(stagger_timer, {{"IN", false}});
}
stu [ i ] = 55;
} else {
stu [ i ] = 4;
}
}
break;
}
case 72:
{
mv_abs_pos [ i ] = debug_init_target_pos [ i ];
mv_abs_vel [ i ] = vel_back;
mv_abs_exec [ i ] = false;
call(wait_timer [ i ], {{"IN", false}});
if (i == 6) {
if (! G ["power_output"] [ i ] ["Done"]) {
call(power_pulse_timer [ i ], {{"IN", false}});
call(power_wait_timer [ i ], {{"IN", false}});
stagger_started = false;
call(stagger_timer, {{"IN", false}});
stu [ i ] = 55;
} else {
mv_abs_exec [ i ] = true;
if (fb_move_abs [ i ] ["Busy"] || fb_move_abs [ i ] ["Done"] || ( std::abs ( G ["axis"] [ i ] ["NcToPlc"] ["ActVelo"] ) > 0.5 )) {
if (! stagger_started) {
stagger_started = true;
call(stagger_timer, {{"IN", false}});
}
stu [ i ] = 8;
}
}
} else if (stagger_started && ( stagger_timer ["ET"] >= stagger_delay [ i ] )) {
if (G ["power_output"] [ i ] ["Done"]) {
mv_abs_exec [ i ] = true;
stu [ i ] = 8;
} else {
call(power_pulse_timer [ i ], {{"IN", false}});
call(power_wait_timer [ i ], {{"IN", false}});
stu [ i ] = 55;
}
}
break;
}
case 8:
{
mv_abs_pos [ i ] = debug_init_target_pos [ i ];
mv_abs_vel [ i ] = vel_back;
mv_abs_exec [ i ] = true;
if (fb_move_abs [ i ] ["Done"]) {
mv_abs_exec [ i ] = false;
stu [ i ] = 10;
} else if (fb_move_abs [ i ] ["Error"] || fb_move_abs [ i ] ["CommandAborted"]) {
mv_abs_exec [ i ] = false;
if (G ["power_output"] [ i ] ["Error"]) {
call(power_pulse_timer [ i ], {{"IN", false}});
call(power_wait_timer [ i ], {{"IN", false}});
if (i == 6) {
stagger_started = false;
call(stagger_timer, {{"IN", false}});
}
stu [ i ] = 55;
} else {
stu [ i ] = 4;
}
}
break;
}
case 10:
{
G ["init_pos"] [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"];
break;
}
}
} else {
if (i == 4) {
switch (int(stu [ i ])) {
case 0:
{
call(G ["SetPointGenDisable"] [ i ], {{"Execute", true}});
if (G ["power_output"] [ i ] ["Done"] && ( G ["SetPointGenDisable"] [ i ] ["Done"] || ! G ["SetPointGenDisable"] [ i ] ["Enabled"] )) {
call(G ["SetPointGenDisable"] [ i ], {{"Execute", false}});
stu [ i ] = 10;
}
break;
}
case 10:
{
G ["init_pos"] [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"];
break;
}
}
} else {
switch (int(stu [ i ])) {
case 0:
{
call(G ["SetPointGenDisable"] [ i ], {{"Execute", true}});
if (G ["power_output"] [ i ] ["Done"] && ( G ["SetPointGenDisable"] [ i ] ["Done"] || ! G ["SetPointGenDisable"] [ i ] ["Enabled"] )) {
call(G ["SetPointGenDisable"] [ i ], {{"Execute", false}});
stu [ i ] = 1;
}
break;
}
case 1:
{
mv_abs_pos [ i ] = 0.0;
mv_abs_vel [ i ] = vel_back;
mv_abs_exec [ i ] = true;
stu [ i ] = 2;
break;
}
case 2:
{
mv_abs_pos [ i ] = 0.0;
mv_abs_vel [ i ] = vel_back;
mv_abs_exec [ i ] = true;
if (fb_move_abs [ i ] ["Done"] || ( ! fb_move_abs [ i ] ["Busy"] && std::abs ( G ["axis"] [ i ] ["NcToPlc"] ["ActPos"] - mv_abs_pos [ i ] ) < 0.01 )) {
mv_abs_exec [ i ] = false;
stu [ i ] = 10;
} else if (fb_move_abs [ i ] ["Error"] || fb_move_abs [ i ] ["CommandAborted"]) {
mv_abs_exec [ i ] = false;
stu [ i ] = 4;
}
break;
}
case 4:
{
call(G ["axis_Reset"] [ i ], {{"Execute", false}});
call(G ["axis_Reset"] [ i ], {{"Execute", true}});
stu [ i ] = 5;
break;
}
case 5:
{
call(G ["axis_Reset"] [ i ], {});
if (G ["axis_Reset"] [ i ] ["Done"] || ! G ["axis"] [ i ] ["Status"] ["Error"]) {
call(G ["axis_Reset"] [ i ], {{"Execute", false}});
stu [ i ] = 1;
} else if (G ["axis_Reset"] [ i ] ["Error"]) {
call(G ["axis_Reset"] [ i ], {{"Execute", false}});
stu [ i ] = 4;
}
break;
}
}
}
}
call(fb_move_vel [ i ], {{"Execute", mv_vel_exec [ i ]}, {"Velocity", mv_vel_vel [ i ]}, {"Acceleration", 50}, {"Deceleration", 100}, {"Jerk", 100}, {"Direction", -1}});
call(fb_move_abs [ i ], {{"Position", mv_abs_pos [ i ]}, {"Velocity", mv_abs_vel [ i ]}, {"Acceleration", 100}, {"Deceleration", 100}, {"Jerk", 100}, {"Execute", mv_abs_exec [ i ]}});
G ["selfcheck_stu"] [ i ] = stu [ i ];
}
all_done = true;
for (i = 1; double(i) <= 7; ++i) {
if (stu [ i ] != 10) {
all_done = false;
break;
}
}
if (all_done) {
G ["cylinder1_value"] = 1000;
G ["cylinder2_value"] = 150;
G ["cylinder3_value"] = 1000;
G ["cylinder4_value"] = 150;
G ["self_check_done"] = true;
G ["startup_loading_ready"] = true;
G ["handle_reinit_req"] = true;
for (i = 1; double(i) <= 7; ++i) {
if (! target_axes [ i ] && i != 4) {
G ["init_pos"] [ i ] = 0.0;
} else if (i == 4) {
G ["init_pos"] [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"];
}
G ["Act_pos"] [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"] - G ["init_pos"] [ i ];
G ["act_pos_from_left"] [ i ] = G ["axis"] [ i ] ["NcToPlc"] ["ActPos"] - G ["leftlimit"] [ i ];
G ["refer"] [ i ] = G ["Act_pos"] [ i ];
G ["ref_slow"] [ i ] = G ["Act_pos"] [ i ];
G ["act_h"] [ i ] = G ["Act_pos"] [ i ];
G ["act_hf"] [ i ] = G ["Act_pos"] [ i ];
G ["refer_from_left"] [ i ] = G ["act_pos_from_left"] [ i ];
}
G ["gen_state"] = _handle;
}
}
};
inline void initOld(Node& G) {
G["gen_state"] = _init;
G["power_"][1].kind = "MC_Power";
G["power_"][2].kind = "MC_Power";
G["power_"][3].kind = "MC_Power";
G["power_"][4].kind = "MC_Power";
G["power_"][5].kind = "MC_Power";
G["power_"][6].kind = "MC_Power";
G["power_"][7].kind = "MC_Power";
G["axis_Reset"][1].kind = "MC_Reset";
G["axis_Reset"][2].kind = "MC_Reset";
G["axis_Reset"][3].kind = "MC_Reset";
G["axis_Reset"][4].kind = "MC_Reset";
G["axis_Reset"][5].kind = "MC_Reset";
G["axis_Reset"][6].kind = "MC_Reset";
G["axis_Reset"][7].kind = "MC_Reset";
G["SetPointGenEnable"][1].kind = "MC_ExtSetPointGenEnable";
G["SetPointGenEnable"][2].kind = "MC_ExtSetPointGenEnable";
G["SetPointGenEnable"][3].kind = "MC_ExtSetPointGenEnable";
G["SetPointGenEnable"][4].kind = "MC_ExtSetPointGenEnable";
G["SetPointGenEnable"][5].kind = "MC_ExtSetPointGenEnable";
G["SetPointGenEnable"][6].kind = "MC_ExtSetPointGenEnable";
G["SetPointGenEnable"][7].kind = "MC_ExtSetPointGenEnable";
G["SetPointGenDisable"][1].kind = "MC_ExtSetPointGenDisable";
G["SetPointGenDisable"][2].kind = "MC_ExtSetPointGenDisable";
G["SetPointGenDisable"][3].kind = "MC_ExtSetPointGenDisable";
G["SetPointGenDisable"][4].kind = "MC_ExtSetPointGenDisable";
G["SetPointGenDisable"][5].kind = "MC_ExtSetPointGenDisable";
G["SetPointGenDisable"][6].kind = "MC_ExtSetPointGenDisable";
G["SetPointGenDisable"][7].kind = "MC_ExtSetPointGenDisable";
G["move_jog"][1].kind = "MC_Jog";
G["move_jog"][2].kind = "MC_Jog";
G["move_jog"][3].kind = "MC_Jog";
G["move_jog"][4].kind = "MC_Jog";
G["move_jog"][5].kind = "MC_Jog";
G["move_jog"][6].kind = "MC_Jog";
G["move_jog"][7].kind = "MC_Jog";
G["flag"] = 0;
G["flagpos"] = 0;
G["init_pos"][1] = 30;
G["init_pos"][2] = 9;
G["init_pos"][3] = 0;
G["init_pos"][4] = 0;
G["init_pos"][5] = 0;
G["init_pos"][6] = 30;
G["init_pos"][7] = 13;
G["self_check_done"] = false;
G["selfcheck_start_req"] = false;
G["selfcheck_status"] = 0;
G["selfcheck_target_from_left"][1] = 96;
G["selfcheck_target_from_left"][2] = 0;
G["selfcheck_target_from_left"][3] = 500;
G["selfcheck_target_from_left"][4] = 0;
G["selfcheck_target_from_left"][5] = 530;
G["selfcheck_target_from_left"][6] = 580;
G["selfcheck_target_from_left"][7] = 0;
G["startup_loading_ready"] = false;
G["handle_reinit_req"] = false;
G["handle_reinit_done"] = false;
G["selfcheck_reset_req"] = false;
G["selfcheck_all_reach_limit"] = false;
G["estop_hold_req"] = false;
G["host_session_id"] = 0;
G["host_heartbeat_sequence"] = 0;
G["host_recover_req"] = false;
G["host_comm_timeout"] = true;
G["axis6_fast_retract"] = false;
G["axis1_fast_return"] = false;
G["startup_smoothing_bypass"] = false;
G["axis4_fwd_req"] = false;
G["axis4_rev_req"] = false;
G["axis4_manual_busy"] = false;
G["axis4_manual_done"] = false;
G["axis4_manual_error"] = false;
G["axis4_manual_error_id"] = 0;
G["v_limit"][1] = 7.5;
G["v_limit"][2] = 7.5;
G["v_limit"][3] = 1.5;
G["v_limit"][4] = 1.5;
G["v_limit"][5] = 1.5;
G["v_limit"][6] = 7.5;
G["v_limit"][7] = 1.5;
G["ref_slow"][1] = 0;
G["ref_slow"][2] = 0;
G["ref_slow"][3] = 0;
G["ref_slow"][4] = 0;
G["ref_slow"][5] = 0;
G["ref_slow"][6] = 0;
G["ref_slow"][7] = 0;
G["Kp"][1] = 0.14;
G["Kp"][2] = 0.14;
G["Kp"][3] = 0.14;
G["Kp"][4] = 0.14;
G["Kp"][5] = 0.14;
G["Kp"][6] = 0.14;
G["Kp"][7] = 0.14;
G["Ki"][1] = 0.016;
G["Ki"][2] = 0.016;
G["Ki"][3] = 0.016;
G["Ki"][4] = 0.016;
G["Ki"][5] = 0.016;
G["Ki"][6] = 0.016;
G["Ki"][7] = 0.016;
G["Kd"][1] = 0.1;
G["Kd"][2] = 0.1;
G["Kd"][3] = 0.1;
G["Kd"][4] = 0.1;
G["Kd"][5] = 0.1;
G["Kd"][6] = 0.1;
G["Kd"][7] = 0.1;
G["pid_i"][1] = 0;
G["pid_i"][2] = 0;
G["pid_i"][3] = 0;
G["pid_i"][4] = 0;
G["pid_i"][5] = 0;
G["pid_i"][6] = 0;
G["pid_i"][7] = 0;
G["pid_e_prev"][1] = 0;
G["pid_e_prev"][2] = 0;
G["pid_e_prev"][3] = 0;
G["pid_e_prev"][4] = 0;
G["pid_e_prev"][5] = 0;
G["pid_e_prev"][6] = 0;
G["pid_e_prev"][7] = 0;
G["pid_i_limit"][1] = 100;
G["pid_i_limit"][2] = 100;
G["pid_i_limit"][3] = 100;
G["pid_i_limit"][4] = 100;
G["pid_i_limit"][5] = 100;
G["pid_i_limit"][6] = 100;
G["pid_i_limit"][7] = 100;
G["arm_manual_enable"] = false;
G["arm_enable_req"][1] = false;
G["arm_enable_req"][2] = false;
G["arm_enable_req"][3] = false;
G["arm_enable_req"][4] = false;
G["arm_enable_req"][5] = false;
G["arm_reset_req"][1] = false;
G["arm_reset_req"][2] = false;
G["arm_reset_req"][3] = false;
G["arm_reset_req"][4] = false;
G["arm_reset_req"][5] = false;
G["arm_jog_pos_req"][1] = false;
G["arm_jog_pos_req"][2] = false;
G["arm_jog_pos_req"][3] = false;
G["arm_jog_pos_req"][4] = false;
G["arm_jog_pos_req"][5] = false;
G["arm_jog_neg_req"][1] = false;
G["arm_jog_neg_req"][2] = false;
G["arm_jog_neg_req"][3] = false;
G["arm_jog_neg_req"][4] = false;
G["arm_jog_neg_req"][5] = false;
G["arm_jog_velocity"][1] = 5.0;
G["arm_jog_velocity"][2] = 5.0;
G["arm_jog_velocity"][3] = 5.0;
G["arm_jog_velocity"][4] = 5.0;
G["arm_jog_velocity"][5] = 5.0;
G["arm_jog_acc"][1] = 50.0;
G["arm_jog_acc"][2] = 50.0;
G["arm_jog_acc"][3] = 50.0;
G["arm_jog_acc"][4] = 50.0;
G["arm_jog_acc"][5] = 50.0;
G["arm_jog_dec"][1] = 50.0;
G["arm_jog_dec"][2] = 50.0;
G["arm_jog_dec"][3] = 50.0;
G["arm_jog_dec"][4] = 50.0;
G["arm_jog_dec"][5] = 50.0;
G["arm_jog_jerk"][1] = 500.0;
G["arm_jog_jerk"][2] = 500.0;
G["arm_jog_jerk"][3] = 500.0;
G["arm_jog_jerk"][4] = 500.0;
G["arm_jog_jerk"][5] = 500.0;
G["arm_motion_busy"][1] = false;
G["arm_motion_busy"][2] = false;
G["arm_motion_busy"][3] = false;
G["arm_motion_busy"][4] = false;
G["arm_motion_busy"][5] = false;
G["arm_motion_done"][1] = false;
G["arm_motion_done"][2] = false;
G["arm_motion_done"][3] = false;
G["arm_motion_done"][4] = false;
G["arm_motion_done"][5] = false;
G["arm_motion_error"][1] = false;
G["arm_motion_error"][2] = false;
G["arm_motion_error"][3] = false;
G["arm_motion_error"][4] = false;
G["arm_motion_error"][5] = false;
G["arm_motion_error_id"][1] = 0;
G["arm_motion_error_id"][2] = 0;
G["arm_motion_error_id"][3] = 0;
G["arm_motion_error_id"][4] = 0;
G["arm_motion_error_id"][5] = 0;
G["arm_cmd_dir"][1] = 0;
G["arm_cmd_dir"][2] = 0;
G["arm_cmd_dir"][3] = 0;
G["arm_cmd_dir"][4] = 0;
G["arm_cmd_dir"][5] = 0;
G["arm_cmd_conflict"][1] = false;
G["arm_cmd_conflict"][2] = false;
G["arm_cmd_conflict"][3] = false;
G["arm_cmd_conflict"][4] = false;
G["arm_cmd_conflict"][5] = false;
G["arm_curve_cancel"] = true;
G["arm_cjog_if_version"] = 1;
G["inject_push_req"][1] = false;
G["inject_push_req"][2] = false;
G["inject_pull_req"][1] = false;
G["inject_pull_req"][2] = false;
G["cylinder5_cmd"] = 2000;
G["cylinder5_press_req"] = false;
}