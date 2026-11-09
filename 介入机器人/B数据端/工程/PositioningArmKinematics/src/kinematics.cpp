#include "positioning_arm/kinematics.h"

#include <Eigen/SVD>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace positioning_arm {
namespace {
constexpr double Pi = 3.14159265358979323846;

void require(bool condition, const char* message)
{
    if (!condition) throw std::invalid_argument(message);
}

bool positive(double value)
{
    return std::isfinite(value) && value > 0;
}

Eigen::Quaterniond normalized(Eigen::Quaterniond value)
{
    require(value.coeffs().allFinite() && positive(value.norm()), "四元数无效");
    value.normalize();
    return value;
}

Eigen::Quaterniond rotationZ(double angle)
{
    return Eigen::Quaterniond(Eigen::AngleAxisd(angle, Eigen::Vector3d::UnitZ()));
}

ArmKinematicConfig validate(ArmKinematicConfig config, const SolverOptions& options)
{
    require(config.link_lengths.allFinite() && (config.link_lengths.array() > 0).all(),
        "两根连杆长度必须为正");
    require(positive(config.lateral_offset) && positive(config.tool_length),
        "L形工具尺寸必须为正");
    require(config.base_offset_mm.allFinite(), "基座平移偏移必须为有限毫米值");
    for (int i = 0; i < AxisCount; ++i) {
        require(!std::isnan(config.lower[i]) && !std::isnan(config.upper[i])
            && config.lower[i] < config.upper[i],
            "轴限位必须有序；使用正负无穷表示不设软件限位");
    }
    require(config.max_velocity.allFinite()
        && (config.max_velocity.array() > 0).all(),
        "五轴速度限位必须为正");
    for (const auto& calibration : config.calibration) {
        require(std::isfinite(calibration.zero) && positive(calibration.scale)
            && (calibration.direction == -1 || calibration.direction == 1),
            "编码器标定参数无效");
    }
    for (double value : {options.position_tolerance,
        options.singularity_stop, options.singularity_slow,
        options.max_translation, options.max_rotation}) {
        require(positive(value), "求解器参数必须为正");
    }
    require(options.singularity_stop < options.singularity_slow
        && options.max_rotation < Pi,
        "奇异阈值或角度步长无效");
    return config;
}
} // namespace

PositioningArmKinematics::PositioningArmKinematics(
    const ArmKinematicConfig& config, const SolverOptions& options)
    : config_(validate(config, options)), options_(options)
{
    characteristic_length_ = config_.link_lengths.sum()
        + std::hypot(config_.lateral_offset, config_.tool_length);
    require(positive(characteristic_length_), "特征长度无效");
}

PositioningArmKinematics::~PositioningArmKinematics() = default;

ArmPose PositioningArmKinematics::forward(const JointVector& joints) const
{
    require(checkLimits(joints), "轴位置无效或超出限位");

    ArmPose result;
    // 基座平移只改变原点；轴1的Z符号受归零逐位回归约束，见交付报告。
    result.position << 0, 0, joints[0];

    const double first_link_yaw = joints[1];
    result.position += config_.link_lengths[0]
        * Eigen::Vector3d(std::sin(first_link_yaw), -std::cos(first_link_yaw), 0);

    // 轴3机械零位使第二根连杆反向；工具偏置仍按轴2+轴3+轴4定义。
    const double second_link_yaw = first_link_yaw + joints[2] + Pi;
    result.position += config_.link_lengths[1]
        * Eigen::Vector3d(std::sin(second_link_yaw), -std::cos(second_link_yaw), 0);

    const double tool_yaw = joints[1] + joints[2] + joints[3];
    const Eigen::Quaterniond wrist_rotation = rotationZ(tool_yaw)
        * Eigen::Quaterniond(Eigen::AngleAxisd(joints[4], Eigen::Vector3d::UnitX()));
    result.position += wrist_rotation
        * Eigen::Vector3d(config_.lateral_offset, -config_.tool_length, 0);
    result.position += config_.base_offset_mm * 0.001;
    result.quaternion = normalized(wrist_rotation);
    if (result.quaternion.w() < 0) result.quaternion.coeffs() *= -1;
    return result;
}

FrameJacobian PositioningArmKinematics::computeFrameJacobian(
    const JointVector& joints) const
{
    require(checkLimits(joints), "轴位置无效或超出限位");

    const ArmPose pose = forward(joints);
    FrameJacobian result = FrameJacobian::Zero();
    result.block<3, 1>(0, 0) = Eigen::Vector3d::UnitZ();

    const double z = joints[0];
    Eigen::Vector3d joint_origin = config_.base_offset_mm * 0.001;
    joint_origin.z() += z;
    double yaw = 0;
    for (int i = 0; i < 3; ++i) {
        const Eigen::Vector3d axis = Eigen::Vector3d::UnitZ();
        result.block<3, 1>(0, i + 1) = axis.cross(pose.position - joint_origin);
        result.block<3, 1>(3, i + 1) = axis;
        yaw += joints[i + 1];
        if (i == 0) {
            joint_origin += config_.link_lengths[0]
                * Eigen::Vector3d(std::sin(yaw), -std::cos(yaw), 0);
        } else if (i == 1) {
            joint_origin += config_.link_lengths[1]
                * Eigen::Vector3d(std::sin(yaw + Pi), -std::cos(yaw + Pi), 0);
        }
    }

    const Eigen::Vector3d pitch_axis_world =
        rotationZ(yaw) * Eigen::Vector3d::UnitX();
    result.block<3, 1>(0, 4) =
        pitch_axis_world.cross(pose.position - joint_origin);
    result.block<3, 1>(3, 4) = pitch_axis_world;
    return result;
}

bool PositioningArmKinematics::checkLimits(const JointVector& joints) const
{
    return joints.allFinite()
        && (joints.array() >= config_.lower.array()).all()
        && (joints.array() <= config_.upper.array()).all();
}

bool PositioningArmKinematics::isNearSingularity(const JointVector& joints) const
{
    require(checkLimits(joints), "轴位置无效或超出限位");

    // 平面机构在轴3伸直或折叠时，优先按机械结构直接判定。
    if (std::abs(std::sin(joints[2])) < options_.singularity_slow) return true;

    const auto full = computeFrameJacobian(joints);
    Eigen::Matrix3d jacobian;
    jacobian.topRows<2>() = full.block<2, 3>(0, 1) / characteristic_length_;
    jacobian.bottomRows<1>().setOnes();
    return Eigen::JacobiSVD<Eigen::Matrix3d>(jacobian)
        .singularValues().minCoeff() < options_.singularity_slow;
}

JointVector PositioningArmKinematics::fromEncoder(const JointVector& encoder) const
{
    require(encoder.allFinite(), "编码器位置无效");
    JointVector joints;
    for (int i = 0; i < AxisCount; ++i) {
        const auto& calibration = config_.calibration[i];
        joints[i] = (encoder[i] - calibration.zero)
            * calibration.scale * calibration.direction;
    }
    require(checkLimits(joints), "编码器换算后超出轴限位");
    return joints;
}

JointVector PositioningArmKinematics::toEncoder(const JointVector& joints) const
{
    require(checkLimits(joints), "轴位置无效或超出限位");
    JointVector encoder;
    for (int i = 0; i < AxisCount; ++i) {
        const auto& calibration = config_.calibration[i];
        encoder[i] = joints[i]
            / (calibration.scale * calibration.direction) + calibration.zero;
    }
    require(encoder.allFinite(), "编码器换算结果无效");
    return encoder;
}

Eigen::Quaterniond PositioningArmKinematics::continuousQuaternion(
    Eigen::Quaterniond value, const Eigen::Quaterniond& reference)
{
    value = normalized(value);
    if (value.dot(normalized(reference)) < 0) value.coeffs() *= -1;
    return value;
}

IkResult PositioningArmKinematics::solveCartesianJog(
    const JointVector& current, const CartesianJog& command) const
{
    IkResult result;
    result.target = current;
    if (!current.allFinite() || !command.translation.allFinite()
        || !std::isfinite(command.yaw) || !std::isfinite(command.pitch)
        || !positive(command.duration) || command.duration > 1) {
        return result;
    }
    if (!checkLimits(current)) {
        result.status = IkStatus::JointLimit;
        return result;
    }
    const ArmPose current_pose = forward(current);
    result.pose = current_pose;
    const auto active = (command.translation.array() != 0).count()
        + (command.yaw != 0) + (command.pitch != 0);
    if (active > 1) return result;
    if (command.translation.norm() > options_.max_translation
        || std::abs(command.yaw) + std::abs(command.pitch) > options_.max_rotation) {
        result.status = IkStatus::StepLimit;
        return result;
    }

    // 升降、抬头和静止命令不经过平面逆解，也不受平面机械奇异影响。
    if (active == 0 || command.translation.z() != 0 || command.pitch != 0) {
        JointVector next = current;
        next[0] += command.translation.z();
        next[4] -= command.pitch;
        if (!checkLimits(next)) {
            result.status = IkStatus::JointLimit;
            return result;
        }
        if (((next - current).cwiseAbs().array()
            > (config_.max_velocity * command.duration).array()).any()) {
            result.status = IkStatus::StepLimit;
            return result;
        }
        result.status = IkStatus::Success;
        result.target = next;
        result.pose = forward(next);
        result.pose.quaternion = continuousQuaternion(result.pose.quaternion, current_pose.quaternion);
        result.position_error = result.orientation_error = 0;
        return result;
    }

    ArmPose target = current_pose;
    target.position += command.translation;
    target.quaternion = normalized(
        rotationZ(command.yaw) * current_pose.quaternion);
    target.quaternion = continuousQuaternion(target.quaternion, current_pose.quaternion);

    const JointVector travel = config_.max_velocity * command.duration;
    if (!travel.allFinite()) return result;

    const auto planar_sigma = [&](const JointVector& joints) {
        const auto full = computeFrameJacobian(joints);
        Eigen::Matrix3d planar;
        planar.topRows<2>() = full.block<2, 3>(0, 1) / characteristic_length_;
        planar.bottomRows<1>().setOnes();
        return (std::min)(std::abs(std::sin(joints[2])),
            Eigen::JacobiSVD<Eigen::Matrix3d>(planar).singularValues().minCoeff());
    };
    if (planar_sigma(current) < options_.singularity_stop) {
        result.status = IkStatus::NearSingularity;
        return result;
    }

    // 解析解只处理平面三轴：先扣除工具偏置，再解轴2、3两连杆，最后由总偏航角解轴4。
    const double target_yaw = current[1] + current[2] + current[3] + command.yaw;
    const double pitch = current[4];
    const double tool_x = config_.lateral_offset * std::cos(target_yaw)
        + config_.tool_length * std::cos(pitch) * std::sin(target_yaw);
    const double tool_y = config_.lateral_offset * std::sin(target_yaw)
        - config_.tool_length * std::cos(pitch) * std::cos(target_yaw);
    const double wrist_x = target.position.x() - config_.base_offset_mm.x() * 0.001 - tool_x;
    const double wrist_y = target.position.y() - config_.base_offset_mm.y() * 0.001 - tool_y;
    const double radius = std::hypot(wrist_x, wrist_y);
    const double minimum_radius = std::abs(config_.link_lengths[0]
        - config_.link_lengths[1]);
    const double maximum_radius = config_.link_lengths.sum();
    const double tolerance = (std::max)(options_.position_tolerance, 1e-12);
    if (!std::isfinite(radius)
        || radius < minimum_radius - tolerance
        || radius > maximum_radius + tolerance) {
        result.status = IkStatus::NoConvergence;
        return result;
    }

    const double cosine_q3 = (std::max)(-1.0, (std::min)(1.0,
        (radius * radius - config_.link_lengths[0] * config_.link_lengths[0]
            - config_.link_lengths[1] * config_.link_lengths[1])
        / (2 * config_.link_lengths[0] * config_.link_lengths[1])));
    const double elbow = std::acos(cosine_q3);
    const double current_branch = std::sin(current[2]);
    if (std::abs(current_branch) < options_.singularity_stop) {
        result.status = IkStatus::NearSingularity;
        return result;
    }

    const auto nearest_equivalent = [](double principal, double reference) {
        return reference + std::remainder(
            principal - std::remainder(reference, 2 * Pi), 2 * Pi);
    };
    JointVector best = current;
    double best_cost = std::numeric_limits<double>::infinity();
    IkStatus blocked = IkStatus::NoConvergence;
    for (double branch : {1.0, -1.0}) {
        const double effective_q3 = branch * elbow;
        const double q3 = effective_q3 - Pi;
        const double q2 = std::atan2(wrist_x, -wrist_y)
            - std::atan2(config_.link_lengths[1] * std::sin(effective_q3),
                config_.link_lengths[0] + config_.link_lengths[1] * std::cos(effective_q3));
        JointVector candidate = current;
        candidate[1] = nearest_equivalent(q2, current[1]);
        candidate[2] = nearest_equivalent(q3, current[2]);
        candidate[3] = nearest_equivalent(
            target_yaw - candidate[1] - candidate[2], current[3]);

        if (current_branch * std::sin(candidate[2]) <= 0) {
            blocked = IkStatus::NearSingularity;
            continue;
        }
        if (!checkLimits(candidate)) {
            blocked = IkStatus::JointLimit;
            continue;
        }
        if (((candidate - current).cwiseAbs().array() > travel.array()).any()) {
            blocked = IkStatus::StepLimit;
            continue;
        }
        if (planar_sigma(candidate) < options_.singularity_stop) {
            blocked = IkStatus::NearSingularity;
            continue;
        }

        const double cost = (candidate.tail<4>() - current.tail<4>()).squaredNorm();
        if (cost < best_cost) {
            best = candidate;
            best_cost = cost;
        }
    }
    if (!std::isfinite(best_cost)) {
        result.status = blocked;
        return result;
    }

    // 解析求解成功后直接返回请求姿态；不再用一次正运动学迭代复核。
    result.status = IkStatus::Success;
    result.target = best;
    result.pose = target;
    result.position_error = result.orientation_error = 0;
    return result;
}

IkResult PositioningArmKinematics::solveJog(
    const JointVector& current, const JogCommand& command) const
{
    CartesianJog jog;
    jog.duration = command.duration;
    double scale = 1;
    if (checkLimits(current) && (command.axis == JogAxis::X
        || command.axis == JogAxis::Y || command.axis == JogAxis::Yaw)) {
        const auto full = computeFrameJacobian(current);
        Eigen::Matrix3d planar;
        planar.topRows<2>() = full.block<2, 3>(0, 1) / characteristic_length_;
        planar.bottomRows<1>().setOnes();
        const double sigma = (std::min)(std::abs(std::sin(current[2])),
            Eigen::JacobiSVD<Eigen::Matrix3d>(planar).singularValues().minCoeff());
        // 硬停止区保留非零命令，让逆解明确报告奇异，而非伪装成静止成功。
        if (sigma >= options_.singularity_stop)
            scale = std::min(1.0, sigma / options_.singularity_slow);
    }
    const double displacement = command.speed * command.duration * scale;
    switch (command.axis) {
    case JogAxis::X: jog.translation.x() = displacement; break;
    case JogAxis::Y: jog.translation.y() = displacement; break;
    case JogAxis::Z: jog.translation.z() = displacement; break;
    case JogAxis::Pitch: jog.pitch = displacement; break;
    case JogAxis::Yaw: jog.yaw = displacement; break;
    default: {
        IkResult result;
        result.target = current;
        return result;
    }
    }
    auto result = solveCartesianJog(current, jog);
    result.speed_scale = scale;
    return result;
}

const char* statusName(IkStatus status)
{
    switch (status) {
    case IkStatus::Success: return "Success";
    case IkStatus::InvalidInput: return "InvalidInput";
    case IkStatus::JointLimit: return "JointLimit";
    case IkStatus::StepLimit: return "StepLimit";
    case IkStatus::NearSingularity: return "NearSingularity";
    case IkStatus::NoConvergence: return "NoConvergence";
    }
    return "Unknown";
}
} // namespace positioning_arm
