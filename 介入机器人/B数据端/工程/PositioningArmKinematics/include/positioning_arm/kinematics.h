#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <array>
#include <limits>
#include <vector>

namespace positioning_arm {

constexpr int AxisCount = 5;
using JointVector = Eigen::Matrix<double, AxisCount, 1>;
using FrameJacobian = Eigen::Matrix<double, 6, AxisCount>;

struct AxisCalibration {
    double zero = std::numeric_limits<double>::quiet_NaN();
    double scale = std::numeric_limits<double>::quiet_NaN();
    int direction = 0;
};

struct ArmKinematicConfig {
    Eigen::Vector2d link_lengths = Eigen::Vector2d(0.200, 0.180);
    double lateral_offset = 0.4563;
    double tool_length = 0.5545;
    Eigen::Vector3d base_offset_mm = Eigen::Vector3d::Zero(); // 基座平移，毫米；其余几何参数仍为米。
    // 限位可用正负无穷表示暂不设软件范围；NaN表示配置缺失。
    JointVector lower = JointVector::Constant(std::numeric_limits<double>::quiet_NaN());
    JointVector upper = JointVector::Constant(std::numeric_limits<double>::quiet_NaN());
    JointVector max_velocity = JointVector::Constant(std::numeric_limits<double>::quiet_NaN());
    std::array<AxisCalibration, AxisCount> calibration;
};

struct SolverOptions {
    double position_tolerance = 1e-7;
    double singularity_stop = 1e-6;
    double singularity_slow = 0.03;
    double max_translation = 0.02;
    double max_rotation = 0.1;
};

struct ArmPose {
    Eigen::Vector3d position = Eigen::Vector3d::Zero();
    Eigen::Quaterniond quaternion = Eigen::Quaterniond::Identity();
};

struct CartesianJog {
    // 只允许一个非零分量；俯仰抬头为正，只移动轴5，不保持前端位置。
    Eigen::Vector3d translation = Eigen::Vector3d::Zero();
    double yaw = 0;
    double pitch = 0;
    double duration = 0.05;
};

enum class JogAxis { X, Y, Z, Pitch, Yaw };

struct JogCommand {
    JogAxis axis = JogAxis::X;
    double speed = 0; // 平移为米/秒，转动为弧度/秒。
    double duration = 0.05;
};

enum class HomeStatus {
    AlreadyAtZero,
    Planned,
    InvalidInput,
    JointLimit,
    // 4、5原为旧归零行程/末端预算错误，保留后续错误编号。
    SamplingLimit = 6,
    NumericalFailure
};

struct HomeOptions {
    // 加速度由调用方读取设备配置，不使用合成测试值作为默认值。
    Eigen::Vector4d max_acceleration = Eigen::Vector4d::Constant(
        std::numeric_limits<double>::quiet_NaN());
    double sample_period = 0.02;
    double minimum_duration = 1.0;
    int max_samples = 20000;
    int optimization_iterations = 30;
};

struct HomeSample {
    double time = 0;
    JointVector position = JointVector::Zero();
    JointVector velocity = JointVector::Zero();
    JointVector acceleration = JointVector::Zero();
};

struct HomeResult {
    HomeStatus status = HomeStatus::InvalidInput;
    JointVector target = JointVector::Zero();
    std::vector<HomeSample> samples;
    double duration = 0;
    double sampled_tip_displacement = 0;
    double sampled_tool_rotation = 0;
    double tip_displacement_bound = 0;
    double tool_rotation_bound = 0;
    double baseline_cost = 0;
    double optimized_cost = 0;
    Eigen::Vector4d bend = Eigen::Vector4d::Zero();
    bool success() const {
        return status == HomeStatus::AlreadyAtZero || status == HomeStatus::Planned;
    }
};

enum class IkStatus {
    Success,
    InvalidInput,
    JointLimit,
    StepLimit,
    NearSingularity,
    NoConvergence
};

struct IkResult {
    IkStatus status = IkStatus::InvalidInput;
    JointVector target = JointVector::Zero();
    ArmPose pose;
    int iterations = 0;
    double position_error = std::numeric_limits<double>::infinity();
    double orientation_error = std::numeric_limits<double>::infinity();
    double speed_scale = 1; // solveJog接近平面奇异位时施加的命令速度比例。
    bool success() const { return status == IkStatus::Success; }
};

// 纯数学模块，由ADS后台调用；不直接通信，也不代替实机端点和速度标定。
class PositioningArmKinematics {
public:
    explicit PositioningArmKinematics(
        const ArmKinematicConfig& config, const SolverOptions& options = {});
    ~PositioningArmKinematics();
    PositioningArmKinematics(const PositioningArmKinematics&) = delete;
    PositioningArmKinematics& operator=(const PositioningArmKinematics&) = delete;

    ArmPose forward(const JointVector& joints) const;
    ArmPose computePose(const JointVector& joints) const { return forward(joints); }
    FrameJacobian computeFrameJacobian(const JointVector& joints) const;
    bool checkLimits(const JointVector& joints) const;
    bool isNearSingularity(const JointVector& joints) const;
    IkResult solveCartesianJog(const JointVector& current, const CartesianJog& command) const;
    IkResult solveJog(const JointVector& current, const JogCommand& command) const;

    // 输入、输出始终是机械坐标；轴1不参与程序回位，角度不做取模。
    bool isAtProgramZero(const JointVector& mechanical) const;
    JointVector toProgramCoordinates(const JointVector& mechanical) const;
    JointVector fromProgramCoordinates(const JointVector& program) const;
    HomeResult planProgramZeroReturn(
        const JointVector& mechanical, const HomeOptions& options) const;

    JointVector fromEncoder(const JointVector& encoder) const;
    JointVector toEncoder(const JointVector& joints) const;
    int degreesOfFreedom() const { return AxisCount; }

    // 无隐藏的上一次姿态状态；调用方需要连续显示时显式传入参考四元数。
    static Eigen::Quaterniond continuousQuaternion(
        Eigen::Quaterniond value, const Eigen::Quaterniond& reference);

private:
    ArmKinematicConfig config_;
    SolverOptions options_;
    double characteristic_length_ = 1;
};

const char* statusName(IkStatus status);
const char* statusName(HomeStatus status);

} // namespace positioning_arm
