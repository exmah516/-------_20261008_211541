#pragma once
#include "arm_manual_ads_service.h"
#include "ads_communication.h"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
std::mutex fake_mutex;
std::array<double, 5> actual{{100, 0, 0, 0, 0}};
std::array<double, 5> actual_velocity{};
std::array<double, 5> jog_velocity{{5, 5, 5, 5, 5}}, jog_acc{{50, 50, 50, 50, 50}};
std::array<double, 5> jog_dec{{50, 50, 50, 50, 50}}, jog_jerk{{500, 500, 500, 500, 500}};
std::array<bool, 5> jog_pos{}, jog_neg{};
std::array<double, 5> started_velocity{};
ArmCurve staged_curve;
ArmCurve executing_curve;
bool plc_owned = false;
std::int16_t cjog_mode = 0, cjog_dir = 0;
double cjog_speed = 0;
std::uint32_t cjog_alive = 0;
bool cjog_owned = false;
std::vector<std::vector<std::string>> batches;
std::uint32_t acknowledged = 0, plc_state = 0, plc_error = 0;
std::vector<std::string> writes;
std::string fail_symbol;
bool fail_read = false;
bool host_unhealthy = false;
bool stage_written = false;
bool fake_busy = false;
void check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
template<typename F> void until(F condition)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!condition()) {
        check(std::chrono::steady_clock::now() < deadline, "Mock ADS wait timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
size_t count(const char* name)
{
    std::lock_guard<std::mutex> lock(fake_mutex);
    return static_cast<size_t>(std::count(writes.begin(), writes.end(), name));
}
} // namespace

// 仅替换测试二进制的传输实现，不链接ADS DLL，不访问网络或真实PLC。
CADSComm::CADSComm() {}
CADSComm::~CADSComm() {}
AdsCommunicationService::AdsCommunicationService(CADSComm& ads) : ads_(ads) {}
AdsCommunicationService::~AdsCommunicationService() {}
AdsCommunicationStats AdsCommunicationService::stats() const
{
    AdsCommunicationStats result;
    result.state = AdsConnectionState::Running;
    return result;
}
AdsEventState AdsCommunicationService::event_state() const
{
    std::lock_guard<std::mutex> lock(fake_mutex);
    AdsEventState result;
    result.host_comm_timeout = host_unhealthy;
    return result;
}
bool AdsCommunicationService::write_sum(const char* const* names, const unsigned long* sizes,
    const void* const* values, unsigned long size, DWORD)
{
    std::lock_guard<std::mutex> lock(fake_mutex);
    batches.emplace_back(names, names+size);
    for (unsigned long i = 0; i < size; ++i) {
        const std::string name(names[i]);
        writes.push_back(name);
        if (name == fail_symbol) return false;
        if (name == "G.arm_cjog_mode") std::memcpy(&cjog_mode, values[i], sizes[i]);
        if (name == "G.arm_cjog_dir") std::memcpy(&cjog_dir, values[i], sizes[i]);
        if (name == "G.arm_cjog_speed") std::memcpy(&cjog_speed, values[i], sizes[i]);
        if (name == "G.arm_cjog_alive") std::memcpy(&cjog_alive, values[i], sizes[i]);
        if (name == "G.arm_jog_velocity") std::memcpy(jog_velocity.data(), values[i], sizes[i]);
        if (name == "G.arm_jog_acc") std::memcpy(jog_acc.data(), values[i], sizes[i]);
        if (name == "G.arm_jog_dec") std::memcpy(jog_dec.data(), values[i], sizes[i]);
        if (name == "G.arm_jog_jerk") std::memcpy(jog_jerk.data(), values[i], sizes[i]);
        if (name == "G.arm_jog_pos_req" || name == "G.arm_jog_neg_req") {
            auto& requests = name == "G.arm_jog_pos_req" ? jog_pos : jog_neg;
            const auto* next = static_cast<const bool*>(values[i]);
            for (int axis = 0; axis < 5; ++axis)
                if (next[axis] && !requests[axis]) started_velocity[axis] = jog_velocity[axis];
            std::memcpy(requests.data(), values[i], sizes[i]);
        }
        if (name == "G.arm_curve_data") {
            check(sizes[i] == sizeof(staged_curve.values), "Wrong curve packet size");
            std::memcpy(staged_curve.values.data(), values[i], sizes[i]);
            stage_written = true;
        }
        if (name == "G.arm_curve_request") {
            check(stage_written, "Commit before curve staging");
            executing_curve = staged_curve;
            plc_owned = true;
            std::memcpy(&acknowledged, values[i], sizeof(acknowledged));
            plc_state = 1;
            plc_error = 0;
            stage_written = false;
        }
        if (name == "G.arm_curve_cancel" && *static_cast<const bool*>(values[i])) {
            plc_state = plc_error == 0 ? 3 : 4;
            plc_owned = false;
            actual_velocity.fill(0);
            fake_busy = false;
        }
    }
    return true;
}
bool AdsCommunicationService::read_sum(const char* const* names, const unsigned long* sizes,
    void* const* values, unsigned long size, DWORD)
{
    std::lock_guard<std::mutex> lock(fake_mutex);
    if (fail_read) return false;
    for (unsigned long i = 0; i < size; ++i) {
        const std::string name(names[i]);
        std::memset(values[i], 0, sizes[i]);
        if (name == "G.arm_act_pos") std::memcpy(values[i], actual.data(), sizes[i]);
        else if (name == "G.arm_act_vel") std::memcpy(values[i], actual_velocity.data(), sizes[i]);
        else if (name == "G.arm_motion_busy") std::memset(values[i], fake_busy, sizes[i]);
        else if (name == "G.arm_curve_ack") std::memcpy(values[i], &acknowledged, sizes[i]);
        else if (name == "G.arm_curve_state") std::memcpy(values[i], &plc_state, sizes[i]);
        else if (name == "G.arm_curve_error") std::memcpy(values[i], &plc_error, sizes[i]);
        else if (name == "G.arm_curve_owned") std::memcpy(values[i], &plc_owned, sizes[i]);
        else if (name == "G.arm_cjog_owned") std::memcpy(values[i], &cjog_owned, sizes[i]);
        else if (name == "G.arm_cjog_state") *static_cast<std::uint32_t*>(values[i]) = cjog_mode ? 2 : 0;
        else if (name == "G.arm_cjog_if_version") *static_cast<std::uint32_t*>(values[i]) = 1;
        else if (name == "G.arm_cjog_limited") *static_cast<bool*>(values[i]) = true;
        else if (name == "G.arm_cjog_path_speed") *static_cast<double*>(values[i]) = cjog_speed;
        else if (name == "G.arm_manual_enable" || name == "G.arm_enable_req"
            || (name.find("G.arm_power_output[") == 0 && name.find(".Done") != std::string::npos))
            std::memset(values[i], 1, sizes[i]);
        else if (name == "G.arm_jog_velocity") std::memcpy(values[i], jog_velocity.data(), sizes[i]);
        else if (name == "G.arm_jog_acc") std::memcpy(values[i], jog_acc.data(), sizes[i]);
        else if (name == "G.arm_jog_dec") std::memcpy(values[i], jog_dec.data(), sizes[i]);
        else if (name == "G.arm_jog_jerk") std::memcpy(values[i], jog_jerk.data(), sizes[i]);
        else if (name == "G.arm_cmd_dir") {
            auto* output = static_cast<std::int8_t*>(values[i]);
            for (int axis = 0; axis < 5; ++axis) output[axis] = jog_pos[axis] ? 1 : (jog_neg[axis] ? -1 : 0);
        }
    }
    return true;
}

