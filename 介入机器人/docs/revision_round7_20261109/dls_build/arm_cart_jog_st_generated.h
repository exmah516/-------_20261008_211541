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
std::array<double,6> qdot = {};
double path_v = {};
double path_v_previous = {};
double v0 = {};
double v1 = {};
double tau = {};
double sigma = {};
double duration = T_ACC;
int latched_mode = {};
int latched_dir = {};
bool reanchor = {};
int i = {};
int j = {};
double u = {};
double delta_sigma_des = {};
double delta_sigma = {};
double signed_path = {};
double q2 = {};
double q3 = {};
double q4 = {};
double psi = {};
double tool_y = {};
double s = {};
double lambda = {};
double ratio = {};
double scale = {};
double j11 = {};
double j12 = {};
double j13 = {};
double j21 = {};
double j22 = {};
double j23 = {};
double m11 = {};
double m12 = {};
double m13 = {};
double m22 = {};
double m23 = {};
double m33 = {};
double c11 = {};
double c12 = {};
double c13 = {};
double c22 = {};
double c23 = {};
double c33 = {};
double det = {};
double task_x = {};
double task_y = {};
double task_psi = {};
double z1 = {};
double z2 = {};
double z3 = {};
static constexpr double PI = 3.14159265358979323846;
static constexpr double RAD = PI / 180.0;
static constexpr double L1 = 200.0;
static constexpr double L2 = 180.0;
static constexpr double TOOL_LATERAL_OFFSET_MM = 456.3;
static constexpr double TOOL_LENGTH_MM = 554.5;
static constexpr double T_ACC = 1.0;
static constexpr double T_DEC = 0.5;
static constexpr std::array<double,6> JOINT_VCAP = {{0,24,30,30,30,30}};
static constexpr double DLS_PSI_WEIGHT = 200.0;
static constexpr double DLS_S0 = 0.5;
static constexpr double DLS_LAMBDA_MAX = 30.0;
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
}
limited = false;
if (start) {
path_speed = 0;
target_speed = 0;
return;
}
if (( latched_mode == 3 ) || ( latched_mode == 4 )) {
delta_sigma_des = 0.5 * ( path_v_previous + path_v ) * cycle_dt;
if (latched_mode == 3) {
j = 1;
} else {
j = 5;
}
delta_sigma = std::min ( delta_sigma_des , JOINT_VCAP [ j ] * cycle_dt );
signed_path = latched_dir * ( sigma + delta_sigma );
p = anchor;
p [ j ] = anchor [ j ] - signed_path;
sigma = sigma + delta_sigma;
path_speed = delta_sigma / cycle_dt;
} else {
q2 = p [ 2 ] * RAD;
q3 = p [ 3 ] * RAD;
q4 = p [ 4 ] * RAD;
psi = q2 + q3 + q4;
tool_y = TOOL_LENGTH_MM * std::cos ( anchor [ 5 ] * RAD );
j13 = - TOOL_LATERAL_OFFSET_MM * std::sin ( psi ) + tool_y * std::cos ( psi );
j23 = TOOL_LATERAL_OFFSET_MM * std::cos ( psi ) + tool_y * std::sin ( psi );
j12 = L2 * std::cos ( q2 + q3 + PI ) + j13;
j22 = L2 * std::sin ( q2 + q3 + PI ) + j23;
j11 = L1 * std::cos ( q2 ) + j12;
j21 = L1 * std::sin ( q2 ) + j22;
s = std::abs ( std::sin ( q3 ) );
lambda = 0;
if (s < DLS_S0) {
ratio = s / DLS_S0;
lambda = DLS_LAMBDA_MAX * std::sqrt ( 1.0 - ratio * ratio );
}
m11 = j11 * j11 + j12 * j12 + j13 * j13 + lambda * lambda;
m12 = j11 * j21 + j12 * j22 + j13 * j23;
m13 = DLS_PSI_WEIGHT * ( j11 + j12 + j13 );
m22 = j21 * j21 + j22 * j22 + j23 * j23 + lambda * lambda;
m23 = DLS_PSI_WEIGHT * ( j21 + j22 + j23 );
m33 = 3.0 * DLS_PSI_WEIGHT * DLS_PSI_WEIGHT + lambda * lambda;
c11 = m22 * m33 - m23 * m23;
c12 = m13 * m23 - m12 * m33;
c13 = m12 * m23 - m13 * m22;
c22 = m11 * m33 - m13 * m13;
c23 = m12 * m13 - m11 * m23;
c33 = m11 * m22 - m12 * m12;
det = m11 * c11 + m12 * c12 + m13 * c13;
task_x = 0;
task_y = 0;
task_psi = 0;
if (latched_mode == 1) {
task_x = latched_dir * path_v;
} else if (latched_mode == 2) {
task_y = latched_dir * path_v;
} else {
task_psi = DLS_PSI_WEIGHT * latched_dir * path_v * RAD;
}
z1 = ( c11 * task_x + c12 * task_y + c13 * task_psi ) / det;
z2 = ( c12 * task_x + c22 * task_y + c23 * task_psi ) / det;
z3 = ( c13 * task_x + c23 * task_y + c33 * task_psi ) / det;
qdot [ 2 ] = j11 * z1 + j21 * z2 + DLS_PSI_WEIGHT * z3;
qdot [ 3 ] = j12 * z1 + j22 * z2 + DLS_PSI_WEIGHT * z3;
qdot [ 4 ] = j13 * z1 + j23 * z2 + DLS_PSI_WEIGHT * z3;
scale = 1.0;
for (i = 2; i <= 4; ++i) {
if (std::abs ( qdot [ i ] ) > JOINT_VCAP [ i ] * RAD) {
scale = std::min ( scale , JOINT_VCAP [ i ] * RAD / std::abs ( qdot [ i ] ) );
}
}
p [ 1 ] = anchor [ 1 ];
p [ 5 ] = anchor [ 5 ];
for (i = 2; i <= 4; ++i) {
qdot [ i ] = scale * qdot [ i ];
p [ i ] = p [ i ] + qdot [ i ] * cycle_dt / RAD;
}
if (latched_mode == 1) {
path_speed = latched_dir * ( j11 * qdot [ 2 ] + j12 * qdot [ 3 ] + j13 * qdot [ 4 ] );
} else if (latched_mode == 2) {
path_speed = latched_dir * ( j21 * qdot [ 2 ] + j22 * qdot [ 3 ] + j23 * qdot [ 4 ] );
} else {
path_speed = ( qdot [ 2 ] + qdot [ 3 ] + qdot [ 4 ] ) / RAD;
}
sigma = sigma + path_speed * cycle_dt;
limited = ( lambda > 0 ) && ( path_v > 0 );
}
for (i = 1; i <= 5; ++i) {
v [ i ] = ( p [ i ] - position_before [ i ] ) / cycle_dt;
a [ i ] = ( v [ i ] - velocity_before [ i ] ) / cycle_dt;
}
}
};
