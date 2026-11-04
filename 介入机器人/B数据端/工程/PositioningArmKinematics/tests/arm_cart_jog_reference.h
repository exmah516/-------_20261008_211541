#pragma once
#include <array>
#include <algorithm>
#include <cmath>

// 与ArmCartJogGen同名变量、同一语句顺序；位置为毫米/度，时间为秒。
struct ArmCartJogReference {
    static constexpr double PI = 3.14159265358979323846;
    static constexpr double RAD = PI / 180.0;
    static constexpr double L1 = 200.0, L2 = 180.0;
    static constexpr double T_ACC = 1.0, T_DEC = 0.5;
    static constexpr int INITIAL_BRANCH = 1, BISECTION_COUNT = 12;
    static constexpr std::array<double, 5> JOINT_VCAP{{24,10,10,10,10}};
    std::array<double, 5> p{}, v{}, a{}, anchor{};
    double path_speed = 0, target_speed = 0, path_v = 0;
    double v0 = 0, v1 = 0, duration = T_ACC, tau = 0, sigma = 0;
    double wx = 0, wy = 0;
    int latched_mode = 0, latched_dir = 0, branch = INITIAL_BRANCH;
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
        const auto position_before = p, velocity_before = v;
        const double path_v_previous = path_v;
        target_speed = 0;
        if (held && mode == latched_mode && dir == latched_dir) target_speed = speed;
        if (target_speed != v1) {
            v0 = path_v; v1 = target_speed;
            if (v1 > v0) duration = T_ACC; else duration = T_DEC;
            tau = 0;
        }
        if (!start) tau += dt;
        const double u = std::min(1.0, tau / duration);
        path_v = v0 + (v1-v0)*(3.0*u*u-2.0*u*u*u);
        if (path_v == 0 && v1 == 0 && held
            && (mode != latched_mode || dir != latched_dir)) reanchor = true;
        if (reanchor) {
            anchor = p; latched_mode = mode; latched_dir = dir; sigma = 0;
            double branch_q3 = anchor[2];
            while (branch_q3 > 180.0) branch_q3 -= 360.0;
            while (branch_q3 < -180.0) branch_q3 += 360.0;
            if (branch_q3 != 0.0 && branch_q3 != 180.0 && branch_q3 != -180.0) {
                if (branch_q3 < 0) branch = 1; else branch = -1;
            }
            wx = L1*std::sin(anchor[1]*RAD)+L2*std::sin((anchor[1]+anchor[2])*RAD+PI);
            wy = -L1*std::cos(anchor[1]*RAD)-L2*std::cos((anchor[1]+anchor[2])*RAD+PI);
        }
        limited = false;
        if (start) { path_speed = 0; target_speed = 0; return; }
        const double delta_sigma_des = 0.5*(path_v_previous+path_v)*dt;
        double delta_sigma;
        if (latched_mode >= 3) {
            const int j = latched_mode == 3 ? 0 : latched_mode == 4 ? 4 : 3;
            delta_sigma = std::min(delta_sigma_des, JOINT_VCAP[j]*dt);
            const double s = latched_dir*(sigma+delta_sigma);
            p = anchor;
            const double axis_sign = latched_mode == 5 ? 1.0 : -1.0;
            p[j] = anchor[j]+axis_sign*s;
        } else {
            const double ex = latched_mode == 1 ? 1.0 : 0.0;
            const double ey = latched_mode == 1 ? 0.0 : 1.0;
            double best_step = 0, lower_step = 0, upper_step = delta_sigma_des;
            double best_wx = 0, best_wy = 0, best_q2 = 0, best_q3 = 0, best_q4 = 0;
            for (int i = 0; i <= BISECTION_COUNT; ++i) {
                const double trial_step = i == 0 ? delta_sigma_des : (lower_step+upper_step)/2.0;
                const double candidate_wx = wx+latched_dir*trial_step*ex;
                const double candidate_wy = wy+latched_dir*trial_step*ey;
                const double candidate_radius = std::sqrt(candidate_wx*candidate_wx+candidate_wy*candidate_wy);
                bool trial_ok = false;
                double candidate_q2 = 0, candidate_q3 = 0, candidate_q4 = 0;
                if (candidate_radius >= L1-L2 && candidate_radius <= L1+L2) {
                    double candidate_cosine = (candidate_radius*candidate_radius-L1*L1-L2*L2)/(2.0*L1*L2);
                    if (candidate_cosine > 1.0) candidate_cosine = 1.0;
                    else if (candidate_cosine < -1.0) candidate_cosine = -1.0;
                    const double candidate_elbow = branch*std::acos(candidate_cosine);
                    const double azimuth_denominator = -candidate_wy;
                    double candidate_azimuth;
                    if (azimuth_denominator > 0) candidate_azimuth = std::atan(candidate_wx/azimuth_denominator);
                    else if (azimuth_denominator < 0) {
                        if (candidate_wx >= 0) candidate_azimuth = std::atan(candidate_wx/azimuth_denominator)+PI;
                        else candidate_azimuth = std::atan(candidate_wx/azimuth_denominator)-PI;
                    } else {
                        if (candidate_wx > 0) candidate_azimuth = PI/2.0;
                        else candidate_azimuth = -PI/2.0;
                    }
                    candidate_q2 = (candidate_azimuth-std::atan(L2*std::sin(candidate_elbow)/(L1+L2*std::cos(candidate_elbow))))/RAD;
                    candidate_q3 = (candidate_elbow-PI)/RAD;
                    while (candidate_q2-p[1] > 180.0) candidate_q2 -= 360.0;
                    while (candidate_q2-p[1] < -180.0) candidate_q2 += 360.0;
                    while (candidate_q3-p[2] > 180.0) candidate_q3 -= 360.0;
                    while (candidate_q3-p[2] < -180.0) candidate_q3 += 360.0;
                    candidate_q4 = anchor[3]+(anchor[1]+anchor[2])-(candidate_q2+candidate_q3);
                    trial_ok = std::abs(candidate_q2-p[1]) <= JOINT_VCAP[1]*dt
                        && std::abs(candidate_q3-p[2]) <= JOINT_VCAP[2]*dt
                        && std::abs(candidate_q4-p[3]) <= JOINT_VCAP[3]*dt;
                }
                if (trial_ok) {
                    lower_step = trial_step; best_step = trial_step;
                    best_wx = candidate_wx; best_wy = candidate_wy;
                    best_q2 = candidate_q2; best_q3 = candidate_q3; best_q4 = candidate_q4;
                    if (i == 0) break;
                } else {
                    upper_step = trial_step;
                }
            }
            delta_sigma = best_step;
            if (delta_sigma > 0) {
                wx = best_wx; wy = best_wy;
                p[0] = anchor[0]; p[4] = anchor[4];
                p[1] = best_q2; p[2] = best_q3; p[3] = best_q4;
            }
        }
        sigma += delta_sigma;
        path_speed = delta_sigma/dt;
        limited = delta_sigma < delta_sigma_des;
        for (int i = 0; i < 5; ++i) {
            v[i] = (p[i]-position_before[i])/dt;
            a[i] = (v[i]-velocity_before[i])/dt;
        }
    }
};
