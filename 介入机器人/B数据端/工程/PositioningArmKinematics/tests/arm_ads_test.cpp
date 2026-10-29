#include "arm_manual_ads_service.h"
#include "ads_communication.h"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
std::mutex fake_mutex;
std::array<double, 5> actual{{100, 90, 180, 180, 0}};
std::array<double, 5> jog_velocity{{5, 5, 5, 5, 5}}, jog_acc{{50, 50, 50, 50, 50}};
std::array<double, 5> jog_dec{{50, 50, 50, 50, 50}}, jog_jerk{{500, 500, 500, 500, 500}};
std::array<bool, 5> jog_pos{}, jog_neg{};
std::array<double, 5> started_velocity{};
ArmCurve staged_curve;
std::uint32_t acknowledged = 0, plc_state = 0, plc_error = 0;
std::vector<std::string> writes;
std::string fail_symbol;
bool fail_read = false;
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
AdsEventState AdsCommunicationService::event_state() const { return {}; }
bool AdsCommunicationService::write_sum(const char* const* names, const unsigned long* sizes,
    const void* const* values, unsigned long size, DWORD)
{
    std::lock_guard<std::mutex> lock(fake_mutex);
    for (unsigned long i = 0; i < size; ++i) {
        const std::string name(names[i]);
        writes.push_back(name);
        if (name == fail_symbol) return false;
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
            std::memcpy(&acknowledged, values[i], sizeof(acknowledged));
            plc_state = 1;
            plc_error = 0;
            stage_written = false;
        }
        if (name == "G.arm_curve_cancel" && *static_cast<const bool*>(values[i]))
            plc_state = 3;
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
        else if (name == "G.arm_motion_busy") std::memset(values[i], fake_busy, sizes[i]);
        else if (name == "G.arm_curve_ack") std::memcpy(values[i], &acknowledged, sizes[i]);
        else if (name == "G.arm_curve_state") std::memcpy(values[i], &plc_state, sizes[i]);
        else if (name == "G.arm_curve_error") std::memcpy(values[i], &plc_error, sizes[i]);
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

int main()
{
    try {
        const std::array<double, 5> velocity{{5,5,5,5,5}}, acceleration{{50,50,50,50,50}};
        ArmCartesianSettings settings{0, 20, 200, 30};
        auto plan = plan_arm_curve(actual, velocity, acceleration, settings, 0, 0);
        check(plan.already_home && plan.error == 0, "Home recognition in PLC units");
        auto q = actual; q[4] = 2;
        plan = plan_arm_curve(q, velocity, acceleration, settings, 0, 0);
        check(plan.error == 0 && plan.curve.values[9] == 0, "Home target uses degrees");
        check(plan.curve.values[5] == 100, "Home changed lift");
        plan = plan_arm_curve(actual, velocity, acceleration, settings, 3, 1);
        check(plan.error == 0 && std::abs(plan.curve.values[5] - 100.1) < 1e-9,
            "Millimeter jog conversion");
        plan = plan_arm_curve(actual, velocity, acceleration, settings, 3, 4.5);
        check(plan.error == 0 && plan.curve.values[5] > 100.19
            && plan.curve.values[5] <= 100.2,
            "Millimeter jog step limit");
        plan = plan_arm_curve(actual, velocity, acceleration, settings, 4, 1);
        check(plan.error == 0 && std::abs(plan.curve.values[9] + 0.1) < 1e-9,
            "Pitch sign conversion");
        plan = plan_arm_curve(actual, velocity, acceleration, settings, 4, 2.5);
        check(plan.error == 0 && plan.curve.values[9] < -0.19
            && plan.curve.values[9] >= -0.2,
            "Pitch jog step limit");
        const std::array<double, 5> bent{{100, 35, 80, 20, 0}};
        for (int axis : {1, 2}) {
            plan = plan_arm_curve(bent, velocity, acceleration, settings, axis, 3);
            check(plan.error == 0, "Planar jog rejected");
            for (int i = 0; i < 5; ++i)
                check(std::abs(plan.curve.values[i + 5] - bent[i]) <= 0.2 + 1e-9,
                    "Planar jog exceeded joint step limit");
            check(std::abs(plan.curve.values[6] + plan.curve.values[7]
                + plan.curve.values[8] - 135) < 1e-4, "Planar jog changed yaw");
        }
        check(plan_arm_curve(actual, velocity, acceleration, settings, 1, 1).error == 1104,
            "Folded-zero Cartesian jog not rejected");
        check(plan_arm_curve(actual, velocity, acceleration, {}, 3, 1).error == 1001,
            "Missing calibration not rejected");

        CADSComm transport;
        AdsCommunicationService ads(transport);
        ArmManualAdsService service(ads);
        for (int i = 1; i <= 5; ++i) service.set_axis_enable(i, true);
        service.set_cartesian_parameter(0, 0);
        service.set_cartesian_parameter(1, 20);
        service.set_cartesian_parameter(2, 200);
        service.set_cartesian_parameter(3, 30);
        check(service.start(), "Service start");
        until([&] { return service.snapshot().valid; });
        for (double scale : { 1.0, 2.0, 2.0, 0.5, 1.0 }) {
            for (int axis = 1; axis <= 5; ++axis) {
                const int direction = axis % 2 ? 1 : -1;
                service.set_jog_direction(axis, direction, scale);
                until([&] {
                    const auto state = service.snapshot();
                    return state.cmd_dir[axis - 1] == direction
                        && std::all_of(state.jog_velocity.begin(), state.jog_velocity.end(),
                            [=](double value) { return value == 5.0 * scale; });
                });
                {
                    std::lock_guard<std::mutex> lock(fake_mutex);
                    check(started_velocity[axis - 1] == 5.0 * scale, "Jog started before the selected velocity was written");
                    check(jog_acc == acceleration && jog_dec == acceleration && jog_jerk[0] == 500,
                        "Speed selection changed acceleration/deceleration/jerk");
                }
                service.set_jog_direction(axis, 0, std::numeric_limits<double>::quiet_NaN());
                until([&] { return service.snapshot().cmd_dir[axis - 1] == 0; });
            }
        }
        for (double invalid : { 0.0, -1.0, 0.75, 3.0, std::numeric_limits<double>::quiet_NaN() })
            service.set_jog_direction(1, 1, invalid);
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        check(service.snapshot().cmd_dir[0] == 0 && service.snapshot().jog_velocity == velocity,
            "Invalid speed changed jog requests or velocity");
        service.set_jog_direction(1, 1, 0.5);
        until([&] { return service.snapshot().cmd_dir[0] == 1; });
        until([&] { return service.snapshot().cmd_dir[0] == 0; });
        service.set_jog_direction(1, 1);
        until([&] { return service.snapshot().cmd_dir[0] == 1 && service.snapshot().jog_velocity == velocity; });
        service.set_jog_direction(1, 0);
        until([&] { return service.snapshot().cmd_dir[0] == 0; });
        service.request_program_zero();
        until([&] { return service.snapshot().cartesian_status == 3; });
        check(count("G.arm_curve_request") == 0, "Already-home caused movement");

        { std::lock_guard<std::mutex> lock(fake_mutex); fake_busy = true; }
        until([&] { return service.snapshot().motion_busy[0]; });
        service.set_cartesian_jog(3, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        check(count("G.arm_curve_request") == 0 && service.snapshot().cartesian_error != 1008,
            "Transient busy aborted Cartesian jog");
        { std::lock_guard<std::mutex> lock(fake_mutex); fake_busy = false; }
        until([&] { return count("G.arm_curve_request") == 1; });
        service.stop_cartesian();
        until([&] { return service.snapshot().cartesian_status == 4; });
        check(count("G.arm_curve_request") == 1, "Stop resubmitted curve");

        { std::lock_guard<std::mutex> lock(fake_mutex); fail_symbol = "G.arm_curve_data"; }
        service.set_cartesian_jog(3, 1);
        until([&] { return service.snapshot().cartesian_error == 1003; });
        const auto failed_stages = count("G.arm_curve_data");
        service.set_cartesian_jog(3, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        check(count("G.arm_curve_data") == failed_stages
            && count("G.arm_curve_request") == 1, "Failed stage retried or committed");
        service.stop_cartesian();
        until([&] { return service.snapshot().cartesian_status == 4; });
        { std::lock_guard<std::mutex> lock(fake_mutex); fail_symbol = "G.arm_curve_request"; }
        service.set_cartesian_jog(3, 1);
        until([&] { return service.snapshot().cartesian_error == 1003; });
        check(count("G.arm_curve_request") == 2, "Commit attempt missing");
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        check(count("G.arm_curve_request") == 2, "Unknown commit retried");

        service.stop_cartesian();
        until([&] { return service.snapshot().cartesian_status == 4; });
        { std::lock_guard<std::mutex> lock(fake_mutex); fail_symbol.clear(); }
        service.set_cartesian_jog(3, 1);
        until([&] { return count("G.arm_curve_request") == 3; });
        until([&] { return service.snapshot().cartesian_error == 1005; });
        check(count("G.arm_curve_request") == 3, "Lease expiry continued movement");
        service.stop_cartesian();
        until([&] { return service.snapshot().cartesian_status == 4; });
        service.set_cartesian_jog(3, 1);
        until([&] { return count("G.arm_curve_request") == 4; });
        { std::lock_guard<std::mutex> lock(fake_mutex); fail_read = true; }
        until([&] { return service.snapshot().cartesian_error == 1004; });
        check(count("G.arm_curve_request") == 4, "Feedback loss continued movement");
        { std::lock_guard<std::mutex> lock(fake_mutex); fail_read = false; }
        service.stop_cartesian();
        until([&] { return service.snapshot().valid && service.snapshot().cartesian_status == 4; });
        service.set_cartesian_jog(3, 1);
        until([&] { return count("G.arm_curve_request") == 5; });
        { std::lock_guard<std::mutex> lock(fake_mutex); plc_state = 4; plc_error = 2008; }
        until([&] { return service.snapshot().cartesian_error == 2008; });
        check(count("G.arm_curve_request") == 5, "PLC error retried motion");
        service.stop_cartesian();
        until([&] { return service.snapshot().cartesian_status == 4; });
        service.set_cartesian_jog(3, 1);
        until([&] { return count("G.arm_curve_request") == 6; });
        const auto cancels_before_shutdown = count("G.arm_curve_cancel");
        service.stop();
        check(count("G.arm_curve_cancel") == cancels_before_shutdown + 1,
            "Service shutdown did not cancel active curve");
        check(count("G.arm_curve_request") == 6, "Shutdown committed another curve");
        std::cout << "PASS: three jog speeds/all axes/no accumulation/write order/release/lease, PLC units, home window, staged commit, cancellation, no retries, lease/feedback loss, PLC error, shutdown; no hardware.\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
