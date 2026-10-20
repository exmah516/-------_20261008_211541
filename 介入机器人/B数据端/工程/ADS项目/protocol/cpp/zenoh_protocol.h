#pragma once

#include <msgpack.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace vi_protocol
{
    constexpr std::uint8_t kVersion = 1;
    constexpr const char* kHandleTopic = "vi/ads/handle/v1";
    constexpr const char* kCommandTopic = "vi/ads/command/v1";
    constexpr const char* kStateTopic = "vi/ads/state/v1";
    constexpr const char* kForceTopic = "vi/ads/force/v1";
    constexpr std::size_t kHandleFields = 24;
    constexpr std::size_t kCommandFields = 15;
    constexpr std::size_t kCommandResultFields = 3;
    constexpr std::size_t kStateFields = 49;
    constexpr std::size_t kForceFields = 13;
    constexpr std::size_t kMaxResults = 32;

    enum class Command : std::uint8_t
    {
        Hello = 0, Authenticate = 1, Acquire = 2, Release = 3,
        PreparePosition = 4, StartControl = 5, ForceFeedback = 6,
        Cylinder = 7, ArmManualEnable = 8, ArmAxisEnable = 9,
        ArmAxisReset = 10, ArmAxisJog = 11, ArmCartesianJog = 12,
        ArmStop = 13, ArmProgramZero = 14, ArmCartesianParameter = 15,
        ArmCartesianAlive = 16, YValve = 17
    };

    enum StateFlag : std::uint32_t
    {
        ControlActive = 1u << 0, EstopHold = 1u << 1,
        SelfCheckDone = 1u << 2, ForceFeedbackEnabled = 1u << 3,
        CalZeroed = 1u << 4, HostCommTimeout = 1u << 5,
        YValveClosed = 1u << 6, LeaseHeld = 1u << 7,
        StartupCompleted = 1u << 8, AdsHealthy = 1u << 9,
        ForceZeroing = 1u << 10, GlobalReverse = 1u << 11,
        CleanForceValid = 1u << 12
    };

    struct HandleFrame
    {
        std::uint8_t version{};
        std::uint64_t session_id{}, sequence{}, monotonic_ms{};
        std::uint8_t buttons582{}; bool valid582{};
        std::int32_t encoder582_0{}, encoder582_1{};
        float joint582_0{}, joint582_1{}, velocity582_0{}, velocity582_1{};
        std::uint8_t buttons587{}; bool valid587{};
        std::int32_t encoder587_0{}, encoder587_1{};
        float joint587_0{}, joint587_1{}, velocity587_0{}, velocity587_1{};
        std::int8_t injector1_direction{}, injector2_direction{}, axis4_direction{};
        std::vector<std::uint8_t> auth_tag;
        MSGPACK_DEFINE(version, session_id, sequence, monotonic_ms, buttons582, valid582,
            encoder582_0, encoder582_1, joint582_0, joint582_1, velocity582_0, velocity582_1,
            buttons587, valid587, encoder587_0, encoder587_1, joint587_0, joint587_1,
            velocity587_0, velocity587_1, injector1_direction, injector2_direction,
            axis4_direction, auth_tag);
    };

    struct CommandFrame
    {
        std::uint8_t version{};
        std::uint64_t session_id{}, sequence{}, monotonic_ms{};
        std::int32_t command_id{}; std::uint8_t command{};
        std::int32_t param1{}, param2{};
        double value1{}, value2{};
        std::string payload;
        std::vector<std::uint8_t> nonce_client, nonce_server, proof, auth_tag;
        MSGPACK_DEFINE(version, session_id, sequence, monotonic_ms, command_id, command,
            param1, param2, value1, value2, payload, nonce_client, nonce_server, proof, auth_tag);
    };

    struct CommandResult
    {
        std::int32_t command_id{}; std::uint8_t result{}; std::string reason;
        MSGPACK_DEFINE(command_id, result, reason);
    };

    struct StateFrame
    {
        std::uint8_t version{}; std::uint64_t session_id{}, sequence{};
        std::uint8_t auth_state{};
        std::vector<std::uint8_t> nonce_server;
        std::uint64_t echo_main_monotonic_ms{}; std::uint32_t flags{};
        std::int32_t mode{}, phase{}, self_check_status{}, ads_state{};
        std::uint16_t cylinder1{}, cylinder2{}, cylinder3{}, cylinder4{};
        std::uint8_t cylinder_manual_mask{};
        std::int8_t injector1_direction{}, injector2_direction{}, axis4_direction{};
        float force582_f{}, force582_n{}, force587_f{}, force587_n{}, clean_force_n{}, ads_actual_hz{};
        float axis_pos0{}, axis_pos1{}, axis_pos2{}, axis_pos3{}, axis_pos4{}, axis_pos5{}, axis_pos6{};
        float axis_from_left0{}, axis_from_left1{}, axis_from_left2{}, axis_from_left3{}, axis_from_left4{}, axis_from_left5{}, axis_from_left6{};
        bool has_net_distance{}; double axis1_net_distance_mm{}, axis6_net_distance_mm{};
        std::uint8_t prepare_task_state{}; std::int32_t prepare_task_command_id{};
        std::uint8_t start_task_state{}; std::int32_t start_task_command_id{};
        std::vector<CommandResult> results; std::string event_text;
        std::vector<std::uint8_t> auth_tag;
        MSGPACK_DEFINE(version, session_id, sequence, auth_state, nonce_server, echo_main_monotonic_ms,
            flags, mode, phase, self_check_status, ads_state, cylinder1, cylinder2, cylinder3,
            cylinder4, cylinder_manual_mask, injector1_direction, injector2_direction,
            axis4_direction, force582_f, force582_n, force587_f, force587_n, clean_force_n,
            ads_actual_hz, axis_pos0, axis_pos1, axis_pos2, axis_pos3, axis_pos4, axis_pos5,
            axis_pos6, axis_from_left0, axis_from_left1, axis_from_left2, axis_from_left3,
            axis_from_left4, axis_from_left5, axis_from_left6, has_net_distance,
            axis1_net_distance_mm, axis6_net_distance_mm, prepare_task_state,
            prepare_task_command_id, start_task_state, start_task_command_id, results,
            event_text, auth_tag);
    };

    struct ForceFrame
    {
        std::uint8_t version{}; std::uint64_t session_id{}, sequence{}, echo_main_monotonic_ms{};
        bool enable582{}; std::int8_t axis582{}; float force582_n{}, torque582_nm{};
        bool enable587{}; std::int8_t axis587{}; float force587_n{}, torque587_nm{};
        std::vector<std::uint8_t> auth_tag;
        MSGPACK_DEFINE(version, session_id, sequence, echo_main_monotonic_ms, enable582,
            axis582, force582_n, torque582_nm, enable587, axis587, force587_n,
            torque587_nm, auth_tag);
    };
}
