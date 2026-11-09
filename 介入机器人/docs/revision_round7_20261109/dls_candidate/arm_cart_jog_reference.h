#pragma once
#include <array>
#include <algorithm>
#include <cmath>

// 与ArmCartJogGen保持同一算术顺序；位置为毫米/度，内部关节速度为弧度/秒。
struct ArmCartJogReference {
    static constexpr double PI = 3.14159265358979323846;
    static constexpr double RAD = PI / 180.0;
    static constexpr double L1 = 200.0, L2 = 180.0;
    static constexpr double TOOL_LATERAL_OFFSET_MM = 456.3;
    static constexpr double TOOL_LENGTH_MM = 554.5;
    static constexpr double T_ACC = 1.0, T_DEC = 0.5;
    static constexpr std::array<double, 5> JOINT_VCAP{{24,30,30,30,30}};
    static constexpr double DLS_PSI_WEIGHT = 200.0;
    static constexpr double DLS_S0 = 0.5;
    static constexpr double DLS_LAMBDA_MAX = 30.0;
    std::array<double, 5> p{}, v{}, a{}, anchor{}, qdot{};
    double path_speed = 0, target_speed = 0, path_v = 0, path_v_previous = 0;
    double v0 = 0, v1 = 0, duration = T_ACC, tau = 0, sigma = 0;
    double q2 = 0, q3 = 0, q4 = 0, psi = 0, tool_y = 0, s = 0, lambda = 0, ratio = 0, scale = 0;
    double j11 = 0, j12 = 0, j13 = 0, j21 = 0, j22 = 0, j23 = 0;
    double m11 = 0, m12 = 0, m13 = 0, m22 = 0, m23 = 0, m33 = 0;
    double c11 = 0, c12 = 0, c13 = 0, c22 = 0, c23 = 0, c33 = 0, det = 0;
    double task_x = 0, task_y = 0, task_psi = 0, z1 = 0, z2 = 0, z3 = 0;
    int latched_mode = 0, latched_dir = 0;
    bool limited = false;

    void step(bool start, bool held, int mode, int dir, double speed,
        const std::array<double, 5>& seed, double dt)
    {
        bool reanchor = start;
        if (start) {
            p = seed;
            for (int i = 0; i < 5; ++i) { v[i] = 0; a[i] = 0; }
            path_v = 0; v0 = 0; v1 = 0; tau = 0; duration = T_ACC;
            latched_mode = mode; latched_dir = dir;
        }
        const auto position_before = p;
        const auto velocity_before = v;
        path_v_previous = path_v;
        target_speed = 0;
        if (held && mode == latched_mode && dir == latched_dir) target_speed = speed;
        if (target_speed != v1) {
            v0 = path_v; v1 = target_speed;
            duration = v1 > v0 ? T_ACC : T_DEC;
            tau = 0;
        }
        if (!start) tau = tau + dt;
        const double u = std::min(1.0, tau / duration);
        path_v = v0 + (v1-v0)*(3.0*u*u-2.0*u*u*u);
        if (path_v == 0 && v1 == 0 && held
            && (mode != latched_mode || dir != latched_dir)) reanchor = true;
        if (reanchor) {
            anchor = p; latched_mode = mode; latched_dir = dir; sigma = 0;
        }
        limited = false;
        if (start) { path_speed = 0; target_speed = 0; return; }
        if (latched_mode == 3 || latched_mode == 4) {
            const double delta_sigma_des = 0.5*(path_v_previous+path_v)*dt;
            const int j = latched_mode == 3 ? 0 : 4;
            const double delta_sigma = std::min(delta_sigma_des, JOINT_VCAP[j]*dt);
            const double signed_path = latched_dir*(sigma+delta_sigma);
            p = anchor;
            p[j] = anchor[j]-signed_path;
            sigma = sigma+delta_sigma;
            path_speed = delta_sigma/dt;
        } else {
            q2 = p[1]*RAD; q3 = p[2]*RAD; q4 = p[3]*RAD;
            psi = q2+q3+q4;
            tool_y = TOOL_LENGTH_MM*std::cos(anchor[4]*RAD);
            j13 = -TOOL_LATERAL_OFFSET_MM*std::sin(psi)+tool_y*std::cos(psi);
            j23 = TOOL_LATERAL_OFFSET_MM*std::cos(psi)+tool_y*std::sin(psi);
            j12 = L2*std::cos(q2+q3+PI)+j13;
            j22 = L2*std::sin(q2+q3+PI)+j23;
            j11 = L1*std::cos(q2)+j12;
            j21 = L1*std::sin(q2)+j22;
            s = std::abs(std::sin(q3));
            lambda = 0;
            if (s < DLS_S0) {
                ratio = s/DLS_S0;
                lambda = DLS_LAMBDA_MAX*std::sqrt(1.0-ratio*ratio);
            }
            m11 = j11*j11+j12*j12+j13*j13+lambda*lambda;
            m12 = j11*j21+j12*j22+j13*j23;
            m13 = DLS_PSI_WEIGHT*(j11+j12+j13);
            m22 = j21*j21+j22*j22+j23*j23+lambda*lambda;
            m23 = DLS_PSI_WEIGHT*(j21+j22+j23);
            m33 = 3.0*DLS_PSI_WEIGHT*DLS_PSI_WEIGHT+lambda*lambda;
            c11 = m22*m33-m23*m23;
            c12 = m13*m23-m12*m33;
            c13 = m12*m23-m13*m22;
            c22 = m11*m33-m13*m13;
            c23 = m12*m13-m11*m23;
            c33 = m11*m22-m12*m12;
            det = m11*c11+m12*c12+m13*c13;
            task_x = 0; task_y = 0; task_psi = 0;
            if (latched_mode == 1) task_x = latched_dir*path_v;
            else if (latched_mode == 2) task_y = latched_dir*path_v;
            else task_psi = DLS_PSI_WEIGHT*latched_dir*path_v*RAD;
            z1 = (c11*task_x+c12*task_y+c13*task_psi)/det;
            z2 = (c12*task_x+c22*task_y+c23*task_psi)/det;
            z3 = (c13*task_x+c23*task_y+c33*task_psi)/det;
            qdot[1] = j11*z1+j21*z2+DLS_PSI_WEIGHT*z3;
            qdot[2] = j12*z1+j22*z2+DLS_PSI_WEIGHT*z3;
            qdot[3] = j13*z1+j23*z2+DLS_PSI_WEIGHT*z3;
            scale = 1.0;
            for (int i = 1; i <= 3; ++i)
                if (std::abs(qdot[i]) > JOINT_VCAP[i]*RAD)
                    scale = std::min(scale, JOINT_VCAP[i]*RAD/std::abs(qdot[i]));
            p[0] = anchor[0]; p[4] = anchor[4];
            for (int i = 1; i <= 3; ++i) {
                qdot[i] = scale*qdot[i];
                p[i] = p[i]+qdot[i]*dt/RAD;
            }
            if (latched_mode == 1)
                path_speed = latched_dir*(j11*qdot[1]+j12*qdot[2]+j13*qdot[3]);
            else if (latched_mode == 2)
                path_speed = latched_dir*(j21*qdot[1]+j22*qdot[2]+j23*qdot[3]);
            else
                path_speed = (qdot[1]+qdot[2]+qdot[3])/RAD;
            sigma = sigma+path_speed*dt;
            limited = lambda > 0 && path_v > 0;
        }
        for (int i = 0; i < 5; ++i) {
            v[i] = (p[i]-position_before[i])/dt;
            a[i] = (v[i]-velocity_before[i])/dt;
        }
    }
};
