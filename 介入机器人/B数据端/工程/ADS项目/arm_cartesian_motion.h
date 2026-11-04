#pragma once
#include "positioning_arm/kinematics.h"
#include <array>
#include <cmath>
#include <limits>

// 归零曲线契约：PLC毫米、度、秒，序号单独提交。
// 0..4起点，5..9终点，10..14弯曲系数，15时间，16必须为0。
struct ArmCurve {
    std::array<double, 17> values{};
};

struct ArmCurvePlan {
    int error = 0;
    bool already_home = false;
    ArmCurve curve;
};

inline ArmCurvePlan plan_arm_curve(const std::array<double, 5>& actual,
    const std::array<double, 5>& velocity, const std::array<double, 5>& acceleration,
    int mode, double speed)
{
    using namespace positioning_arm;
    constexpr double rad = 3.14159265358979323846 / 180;
    ArmCurvePlan out;
    if (mode != 0 || !std::isfinite(speed)) {
        out.error = 1001; return out;
    }
    ArmKinematicConfig c;
    // 轴1至4暂不在运动学层做人为范围裁剪；轴5保留设备要求的±60°姿态范围。
    c.lower.setConstant(-std::numeric_limits<double>::infinity());
    c.upper.setConstant(std::numeric_limits<double>::infinity());
    c.lower[4] = -60 * rad;
    c.upper[4] = 60 * rad;
    JointVector current;
    JointVector amax;
    for (int i = 0; i < 5; ++i) {
        const double scale = i == 0 ? 0.001 : rad;
        if (!std::isfinite(actual[i]) || std::abs(actual[i]) > 100000
            || !std::isfinite(velocity[i]) || velocity[i] <= 0
            || !std::isfinite(acceleration[i]) || acceleration[i] <= 0) {
            out.error = 1001; return out;
        }
        c.calibration[i] = {0, scale, 1};
        c.max_velocity[i] = velocity[i] * scale;
        current[i] = actual[i] * scale;
        amax[i] = acceleration[i] * scale;
    }
    PositioningArmKinematics arm(c);
    if (!arm.checkLimits(current)) { out.error = 1002; return out; }
    JointVector target = current;
    JointVector bend = JointVector::Zero();
    double duration = 0.1;
    if (mode == 0) {
        HomeOptions options;
        options.max_acceleration = amax.tail<4>();
        const auto r = arm.planProgramZeroReturn(current, options);
        if (!r.success()) { out.error = 1200 + static_cast<int>(r.status); return out; }
        if (r.status == HomeStatus::AlreadyAtZero) { out.already_home = true; return out; }
        target = r.target;
        bend.tail<4>() = r.bend;
        duration = r.duration;
    }

    for (int i = 0; i < 5; ++i) {
        const double scale = i == 0 ? 0.001 : rad;
        out.curve.values[i] = actual[i];
        out.curve.values[5 + i] = target[i] / scale;
        out.curve.values[10 + i] = bend[i] / scale;
    }
    out.curve.values[15] = duration;
    return out;
}
