#pragma once
#include <array>
#include <cmath>
#include <algorithm>
#include "arm_cart_jog_stage_signals.h"
struct StCartJogStage : JogStageSignals<StCartJog> {
int stage = {};
int i = {};
double cycle_seconds = {};
double alive_age = {};
double enable_age = {};
std::uint32_t last_alive = {};
bool request_valid = {};
bool held = {};
bool drive_ready = {};
bool fault_stop = {};
bool all_enabled = {};
bool all_disabled = {};
bool all_stopped = {};
bool all_released = {};
int zero_cycles = {};
std::array<int,6> dir_feed = {};
static constexpr double ALIVE_TIMEOUT = 0.5;
static constexpr double ENABLE_TIMEOUT = 2.0;
void step() {
cycle_seconds = double ( cycle_time_100ns ) * 0.0000001;
if (first_cycle) {
stage = 0;
G . arm_cjog_owned = false;
G . arm_cjog_state = 0;
G . arm_cjog_error = 0;
G . arm_cjog_limited = false;
G . arm_cjog_path_speed = 0;
}
if (G . arm_cjog_alive != last_alive) {
last_alive = G . arm_cjog_alive;
alive_age = 0;
} else {
alive_age = alive_age + cycle_seconds;
}
request_valid = ( G . arm_cjog_mode >= 1 ) && ( G . arm_cjog_mode <= 5 ) && ( ( G . arm_cjog_dir == - 1 ) || ( G . arm_cjog_dir == 1 ) ) && ( G . arm_cjog_speed > 0 ) && ( G . arm_cjog_speed <= 100 );
held = request_valid && ( alive_age < ALIVE_TIMEOUT );
if (( G . arm_cjog_mode != 0 ) && ! request_valid && ( G . arm_cjog_error == 0 )) {
G . arm_cjog_error = 3001;
}
drive_ready = G . arm_manual_enable && ! G . host_comm_timeout && ! G . estop_hold_req && ( std::abs ( cycle_seconds - 0.001 ) < 0.00000001 );
for (i = 1; i <= 5; ++i) {
drive_ready = drive_ready && G . arm_power_output [ i ] . Done && G . arm_enable_req [ i ] && ! G . arm_axis [ i ] . Status . Error && ! G . arm_reset_req [ i ] && ! G . arm_jog_pos_req [ i ] && ! G . arm_jog_neg_req [ i ];
}
if (( stage == 0 ) && held && ! G . arm_curve_owned && ( G . arm_curve_request == G . arm_curve_ack )) {
if (! drive_ready) {
if (G . arm_cjog_error == 0) {
G . arm_cjog_error = 3002;
}
} else {
gen.step(true,false,G . arm_cjog_mode,G . arm_cjog_dir,G . arm_cjog_speed,G . arm_act_pos,cycle_seconds);
for (i = 1; i <= 5; ++i) {
dir_feed [ i ] = 1;
}
G . arm_cjog_error = 0;
G . arm_cjog_owned = true;
fault_stop = false;
enable_age = 0;
zero_cycles = 0;
stage = 10;
}
}
if (( stage == 10 || stage == 25 ) && ! drive_ready) {
if (G . arm_cjog_error == 0) {
G . arm_cjog_error = 3002;
}
fault_stop = true;
stage = 40;
}
if (stage == 10) {
enable_age = enable_age + cycle_seconds;
if (enable_age > ENABLE_TIMEOUT) {
if (G . arm_cjog_error == 0) {
G . arm_cjog_error = 3003;
}
fault_stop = true;
stage = 40;
}
}
if (stage == 25) {
gen.step(false,held,G . arm_cjog_mode,G . arm_cjog_dir,G . arm_cjog_speed,gen . p,cycle_seconds);
if (( gen . path_speed == 0 ) && ! held) {
zero_cycles = zero_cycles + 1;
} else {
zero_cycles = 0;
}
if (zero_cycles >= 2) {
stage = 40;
}
}
all_enabled = true;
all_disabled = true;
all_stopped = true;
all_released = true;
for (i = 1; i <= 5; ++i) {
if (gen . v [ i ] > 0) {
dir_feed [ i ] = 1;
}
if (gen . v [ i ] < 0) {
dir_feed [ i ] = - 1;
}
if (G . arm_cjog_owned && ( stage == 10 || stage == 25 )) {
feed(G . arm_axis [ i ],gen . p [ i ],gen . v [ i ],gen . a [ i ],dir_feed [ i ]);
}
fb_enable [ i ].call(G . arm_axis [ i ],( stage == 10 ),gen . p [ i ],0);
fb_disable [ i ].call(G . arm_axis [ i ],( stage == 40 ));
fb_stop [ i ].call(G . arm_axis [ i ],( stage == 50 ) && ! G . arm_axis [ i ] . Status . Error,G . arm_jog_dec [ i ],G . arm_jog_jerk [ i ]);
all_enabled = all_enabled && fb_enable [ i ] . Enabled;
all_disabled = all_disabled && ( fb_disable [ i ] . Done || fb_disable [ i ] . Error || G . arm_axis [ i ] . Status . Error );
all_stopped = all_stopped && ( fb_stop [ i ] . Done || fb_stop [ i ] . Error || G . arm_axis [ i ] . Status . Error );
all_released = all_released && ! fb_stop [ i ] . Busy;
if (G . arm_cjog_owned && ( fb_enable [ i ] . Error || fb_disable [ i ] . Error || fb_stop [ i ] . Error )) {
if (G . arm_cjog_error == 0) {
if (fb_enable [ i ] . Error) {
G . arm_cjog_error = fb_enable [ i ] . ErrorID;
} else if (fb_disable [ i ] . Error) {
G . arm_cjog_error = fb_disable [ i ] . ErrorID;
} else if (fb_stop [ i ] . Error) {
G . arm_cjog_error = fb_stop [ i ] . ErrorID;
}
}
fault_stop = true;
if (stage < 40) {
stage = 40;
}
}
}
switch (stage) {
case 10:
{
if (all_enabled) {
stage = 25;
}
break;
}
case 40:
{
if (all_disabled) {
if (fault_stop) {
stage = 50;
} else {
stage = 60;
}
}
break;
}
case 50:
{
if (all_stopped) {
stage = 60;
}
break;
}
case 60:
{
if (all_released) {
G . arm_cjog_owned = false;
stage = 0;
}
break;
}
}
switch (stage) {
case 0:
{
G . arm_cjog_state = 0;
break;
}
case 10:
{
G . arm_cjog_state = 1;
break;
}
case 25:
{
if (gen . target_speed > 0) {
G . arm_cjog_state = 2;
} else {
G . arm_cjog_state = 3;
}
break;
}
case 40:
case 50:
case 60:
{
G . arm_cjog_state = 4;
break;
}
}
G . arm_cjog_limited = gen . limited;
G . arm_cjog_path_speed = gen . path_speed;
}
};
