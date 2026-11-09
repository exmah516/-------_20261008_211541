#pragma once
#include <array>
#include <cmath>
#include <algorithm>
struct StCartJog {
std::array<double,6> p = {};
std::array<double,6> v = {};
std::array<double,6> a = {};
double path_speed = {};
double target_speed = {};
bool limited = {};
std::array<double,6> anchor = {};
std::array<double,6> position_before = {};
std::array<double,6> velocity_before = {};
double path_v = {};
double path_v_previous = {};
double v0 = {};
double v1 = {};
double tau = {};
double sigma = {};
double duration = T_ACC;
double wx = {};
double wy = {};
double psi0 = {};
double d0x = {};
double d0y = {};
double tcp0x = {};
double tcp0y = {};
int latched_mode = {};
int latched_dir = {};
int branch = INITIAL_BRANCH;
bool reanchor = {};
bool trial_ok = {};
int i = {};
int j = {};
double u = {};
double delta_sigma_des = {};
double delta_sigma = {};
double lower_step = {};
double upper_step = {};
double best_step = {};
double trial_step = {};
double axis_sign = {};
double signed_path = {};
double ex = {};
double ey = {};
double candidate_wx = {};
double candidate_wy = {};
double best_wx = {};
double best_wy = {};
double candidate_radius = {};
double candidate_cosine = {};
double candidate_elbow = {};
double candidate_azimuth = {};
double azimuth_denominator = {};
double candidate_q2 = {};
double candidate_q3 = {};
double candidate_q4 = {};
double candidate_psi = {};
double best_q2 = {};
double best_q3 = {};
double best_q4 = {};
double branch_q3 = {};
static constexpr double PI = 3.14159265358979323846;
static constexpr double RAD = PI / 180.0;
static constexpr double L1 = 200.0;
static constexpr double L2 = 180.0;
static constexpr double TOOL_LATERAL_OFFSET_MM = 456.3;
static constexpr double TOOL_LENGTH_MM = 554.5;
static constexpr double T_ACC = 1.0;
static constexpr double T_DEC = 0.5;
static constexpr int INITIAL_BRANCH = 1;
static constexpr int BISECTION_COUNT = 12;
static constexpr std::array<double,6> JOINT_VCAP = {{0,24,30,30,30,30}};
void step(bool start,bool held,int mode,int dir,double speed,const std::array<double,6>& seed,double cycle_dt) {
reanchor = start;
if (start) {
p = seed;
for (i = 1; i <= 5; ++i) {
v [ i ] = 0;
a [ i ] = 0;
}
path_v = 0;
v0 = 0;
v1 = 0;
tau = 0;
duration = T_ACC;
latched_mode = mode;
latched_dir = dir;
}
position_before = p;
velocity_before = v;
path_v_previous = path_v;
target_speed = 0;
if (held && ( mode == latched_mode ) && ( dir == latched_dir )) {
target_speed = speed;
}
if (target_speed != v1) {
v0 = path_v;
v1 = target_speed;
if (v1 > v0) {
duration = T_ACC;
} else {
duration = T_DEC;
}
tau = 0;
}
if (! start) {
tau = tau + cycle_dt;
}
u = std::min ( 1.0 , tau / duration );
path_v = v0 + ( v1 - v0 ) * ( 3.0 * u * u - 2.0 * u * u * u );
if (( path_v == 0 ) && ( v1 == 0 ) && held && ( ( mode != latched_mode ) || ( dir != latched_dir ) )) {
reanchor = true;
}
if (reanchor) {
anchor = p;
latched_mode = mode;
latched_dir = dir;
sigma = 0;
branch_q3 = anchor [ 3 ];
while (branch_q3 > 180.0) {
branch_q3 = branch_q3 - 360.0;
}
while (branch_q3 < - 180.0) {
branch_q3 = branch_q3 + 360.0;
}
if (( branch_q3 != 0.0 ) && ( branch_q3 != 180.0 ) && ( branch_q3 != - 180.0 )) {
if (branch_q3 < 0) {
branch = 1;
} else {
branch = - 1;
}
}
wx = L1 * std::sin ( anchor [ 2 ] * RAD ) + L2 * std::sin ( ( anchor [ 2 ] + anchor [ 3 ] ) * RAD + PI );
wy = - L1 * std::cos ( anchor [ 2 ] * RAD ) - L2 * std::cos ( ( anchor [ 2 ] + anchor [ 3 ] ) * RAD + PI );
psi0 = anchor [ 2 ] + anchor [ 3 ] + anchor [ 4 ];
d0x = TOOL_LATERAL_OFFSET_MM;
d0y = - TOOL_LENGTH_MM * std::cos ( anchor [ 5 ] * RAD );
tcp0x = wx + d0x * std::cos ( psi0 * RAD ) - d0y * std::sin ( psi0 * RAD );
tcp0y = wy + d0x * std::sin ( psi0 * RAD ) + d0y * std::cos ( psi0 * RAD );
}
limited = false;
if (start) {
path_speed = 0;
target_speed = 0;
return;
}
delta_sigma_des = 0.5 * ( path_v_previous + path_v ) * cycle_dt;
if (latched_mode == 5) {
best_step = 0;
lower_step = 0;
upper_step = delta_sigma_des;
for (i = 0; i <= BISECTION_COUNT; ++i) {
if (i == 0) {
trial_step = delta_sigma_des;
} else {
trial_step = ( lower_step + upper_step ) / 2.0;
}
candidate_psi = psi0 + latched_dir * ( sigma + trial_step );
candidate_wx = tcp0x - d0x * std::cos ( candidate_psi * RAD ) + d0y * std::sin ( candidate_psi * RAD );
candidate_wy = tcp0y - d0x * std::sin ( candidate_psi * RAD ) - d0y * std::cos ( candidate_psi * RAD );
candidate_radius = std::sqrt ( candidate_wx * candidate_wx + candidate_wy * candidate_wy );
trial_ok = false;
if (( candidate_radius >= L1 - L2 ) && ( candidate_radius <= L1 + L2 )) {
candidate_cosine = ( candidate_radius * candidate_radius - L1 * L1 - L2 * L2 ) / ( 2.0 * L1 * L2 );
if (candidate_cosine > 1.0) {
candidate_cosine = 1.0;
} else if (candidate_cosine < - 1.0) {
candidate_cosine = - 1.0;
}
candidate_elbow = branch * std::acos ( candidate_cosine );
azimuth_denominator = - candidate_wy;
if (azimuth_denominator > 0) {
candidate_azimuth = std::atan ( candidate_wx / azimuth_denominator );
} else if (azimuth_denominator < 0) {
if (candidate_wx >= 0) {
candidate_azimuth = std::atan ( candidate_wx / azimuth_denominator ) + PI;
} else {
candidate_azimuth = std::atan ( candidate_wx / azimuth_denominator ) - PI;
}
} else {
if (candidate_wx > 0) {
candidate_azimuth = PI / 2.0;
} else {
candidate_azimuth = - PI / 2.0;
}
}
candidate_q2 = ( candidate_azimuth - std::atan ( L2 * std::sin ( candidate_elbow ) / ( L1 + L2 * std::cos ( candidate_elbow ) ) ) ) / RAD;
candidate_q3 = ( candidate_elbow - PI ) / RAD;
while (candidate_q2 - p [ 2 ] > 180.0) {
candidate_q2 = candidate_q2 - 360.0;
}
while (candidate_q2 - p [ 2 ] < - 180.0) {
candidate_q2 = candidate_q2 + 360.0;
}
while (candidate_q3 - p [ 3 ] > 180.0) {
candidate_q3 = candidate_q3 - 360.0;
}
while (candidate_q3 - p [ 3 ] < - 180.0) {
candidate_q3 = candidate_q3 + 360.0;
}
candidate_q4 = anchor [ 4 ] + ( candidate_psi - psi0 ) - ( ( candidate_q2 + candidate_q3 ) - ( anchor [ 2 ] + anchor [ 3 ] ) );
trial_ok = ( std::abs ( candidate_q2 - p [ 2 ] ) <= JOINT_VCAP [ 2 ] * cycle_dt ) && ( std::abs ( candidate_q3 - p [ 3 ] ) <= JOINT_VCAP [ 3 ] * cycle_dt ) && ( std::abs ( candidate_q4 - p [ 4 ] ) <= JOINT_VCAP [ 4 ] * cycle_dt );
}
if (trial_ok) {
lower_step = trial_step;
best_step = trial_step;
best_wx = candidate_wx;
best_wy = candidate_wy;
best_q2 = candidate_q2;
best_q3 = candidate_q3;
best_q4 = candidate_q4;
if (i == 0) {
break;
}
} else {
upper_step = trial_step;
}
}
delta_sigma = best_step;
if (delta_sigma > 0) {
p [ 1 ] = anchor [ 1 ];
p [ 5 ] = anchor [ 5 ];
p [ 2 ] = best_q2;
p [ 3 ] = best_q3;
p [ 4 ] = best_q4;
}
} else if (latched_mode >= 3) {
if (latched_mode == 3) {
j = 1;
} else if (latched_mode == 4) {
j = 5;
} else {
j = 4;
}
delta_sigma = std::min ( delta_sigma_des , JOINT_VCAP [ j ] * cycle_dt );
signed_path = latched_dir * ( sigma + delta_sigma );
p = anchor;
axis_sign = - 1.0;
p [ j ] = anchor [ j ] + axis_sign * signed_path;
} else {
if (latched_mode == 1) {
ex = 1.0;
ey = 0.0;
} else {
ex = 0.0;
ey = 1.0;
}
best_step = 0;
lower_step = 0;
upper_step = delta_sigma_des;
for (i = 0; i <= BISECTION_COUNT; ++i) {
if (i == 0) {
trial_step = delta_sigma_des;
} else {
trial_step = ( lower_step + upper_step ) / 2.0;
}
candidate_wx = wx + latched_dir * trial_step * ex;
candidate_wy = wy + latched_dir * trial_step * ey;
candidate_radius = std::sqrt ( candidate_wx * candidate_wx + candidate_wy * candidate_wy );
trial_ok = false;
if (( candidate_radius >= L1 - L2 ) && ( candidate_radius <= L1 + L2 )) {
candidate_cosine = ( candidate_radius * candidate_radius - L1 * L1 - L2 * L2 ) / ( 2.0 * L1 * L2 );
if (candidate_cosine > 1.0) {
candidate_cosine = 1.0;
} else if (candidate_cosine < - 1.0) {
candidate_cosine = - 1.0;
}
candidate_elbow = branch * std::acos ( candidate_cosine );
azimuth_denominator = - candidate_wy;
if (azimuth_denominator > 0) {
candidate_azimuth = std::atan ( candidate_wx / azimuth_denominator );
} else if (azimuth_denominator < 0) {
if (candidate_wx >= 0) {
candidate_azimuth = std::atan ( candidate_wx / azimuth_denominator ) + PI;
} else {
candidate_azimuth = std::atan ( candidate_wx / azimuth_denominator ) - PI;
}
} else {
if (candidate_wx > 0) {
candidate_azimuth = PI / 2.0;
} else {
candidate_azimuth = - PI / 2.0;
}
}
candidate_q2 = ( candidate_azimuth - std::atan ( L2 * std::sin ( candidate_elbow ) / ( L1 + L2 * std::cos ( candidate_elbow ) ) ) ) / RAD;
candidate_q3 = ( candidate_elbow - PI ) / RAD;
while (candidate_q2 - p [ 2 ] > 180.0) {
candidate_q2 = candidate_q2 - 360.0;
}
while (candidate_q2 - p [ 2 ] < - 180.0) {
candidate_q2 = candidate_q2 + 360.0;
}
while (candidate_q3 - p [ 3 ] > 180.0) {
candidate_q3 = candidate_q3 - 360.0;
}
while (candidate_q3 - p [ 3 ] < - 180.0) {
candidate_q3 = candidate_q3 + 360.0;
}
candidate_q4 = anchor [ 4 ] + ( anchor [ 2 ] + anchor [ 3 ] ) - ( candidate_q2 + candidate_q3 );
trial_ok = ( std::abs ( candidate_q2 - p [ 2 ] ) <= JOINT_VCAP [ 2 ] * cycle_dt ) && ( std::abs ( candidate_q3 - p [ 3 ] ) <= JOINT_VCAP [ 3 ] * cycle_dt ) && ( std::abs ( candidate_q4 - p [ 4 ] ) <= JOINT_VCAP [ 4 ] * cycle_dt );
}
if (trial_ok) {
lower_step = trial_step;
best_step = trial_step;
best_wx = candidate_wx;
best_wy = candidate_wy;
best_q2 = candidate_q2;
best_q3 = candidate_q3;
best_q4 = candidate_q4;
if (i == 0) {
break;
}
} else {
upper_step = trial_step;
}
}
delta_sigma = best_step;
if (delta_sigma > 0) {
wx = best_wx;
wy = best_wy;
p [ 1 ] = anchor [ 1 ];
p [ 5 ] = anchor [ 5 ];
p [ 2 ] = best_q2;
p [ 3 ] = best_q3;
p [ 4 ] = best_q4;
}
}
sigma = sigma + delta_sigma;
path_speed = delta_sigma / cycle_dt;
limited = delta_sigma < delta_sigma_des;
for (i = 1; i <= 5; ++i) {
v [ i ] = ( p [ i ] - position_before [ i ] ) / cycle_dt;
a [ i ] = ( v [ i ] - velocity_before [ i ] ) / cycle_dt;
}
}
};
