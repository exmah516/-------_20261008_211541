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

int main()
{
    try {
        const std::array<double, 5> velocity{{5,5,5,5,5}}, acceleration{{50,50,50,50,50}};
        auto plan = plan_arm_curve(actual, velocity, acceleration, 0, 0);
        check(plan.already_home && plan.error == 0, "Home recognition in PLC units");
        auto q = actual; q[4] = 2;
        plan = plan_arm_curve(q, velocity, acceleration, 0, 0);
        check(plan.error == 0 && plan.curve.values[9] == 0, "Home target uses degrees");
        check(plan.curve.values[5] == 100, "Home changed lift");
        const std::array<double, 5> home_start{{350, -30, 45, 25, -10}};
        plan = plan_arm_curve(home_start, velocity, acceleration, 0, 0);
        check(plan.error == 0 && !plan.already_home && plan.curve.values[5] == 350,
            "Home without legacy parameters rejected or moved lift");
        for (int i = 6; i <= 9; ++i)
            check(plan.curve.values[i] == 0, "Home did not return to Beckhoff zero");
        check(plan.curve.values[16] == 0, "Home must keep the quintic curve format");
        auto invalid_acceleration = acceleration; invalid_acceleration[0] = 0;
        check(plan_arm_curve(actual, velocity, invalid_acceleration, 0, 0).error == 1001,
            "Invalid home acceleration not rejected");
        CADSComm transport;
        AdsCommunicationService ads(transport);
        ArmManualAdsService service(ads);
        for (int i = 1; i <= 5; ++i) service.set_axis_enable(i, true);
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

        const auto cancel0 = count("G.arm_curve_cancel");
        const auto params0 = count("G.arm_jog_velocity");
        service.set_cartesian_jog(3, 3, std::numeric_limits<double>::quiet_NaN());
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex); return cjog_mode==3 && cjog_dir==1 && cjog_speed==3 && cjog_alive>0; });
        const auto first_alive = [&] { std::lock_guard<std::mutex> lock(fake_mutex); return cjog_alive; }();
        for(int n=0;n<8;++n) {
            service.set_cartesian_jog(3,3,2);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        {
            std::lock_guard<std::mutex> lock(fake_mutex);
            check(cjog_alive>=first_alive+8 && cjog_mode==3,"Continuous alive forwarding failed");
            check(jog_velocity==velocity && jog_acc==acceleration,"Cartesian jog changed joint parameters");
            for(const auto& batch:batches) if(std::find(batch.begin(),batch.end(),"G.arm_cjog_mode")!=batch.end()) {
                for(const char* name:{"G.arm_cjog_dir","G.arm_cjog_speed","G.arm_cjog_alive"})
                    check(std::find(batch.begin(),batch.end(),name)!=batch.end(),"Jog request split across sum writes");
            }
        }
        { std::lock_guard<std::mutex> lock(fake_mutex); fail_read=true; }
        for (int n=0; n<4; ++n) {
            service.set_cartesian_jog(3,3);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        {
            std::lock_guard<std::mutex> lock(fake_mutex);
            check(cjog_mode==3 && cjog_alive>=first_alive+12,"Snapshot read failure stopped jog alive forwarding");
            fail_read=false;
        }
        until([&] { return service.snapshot().valid; });
        check(count("G.arm_curve_request")==0 && count("G.arm_curve_cancel")==cancel0
            && count("G.arm_jog_velocity")==params0,"Cartesian jog touched the home curve or joint parameters");
        service.set_cartesian_jog(1,-15,0.75);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==1 && cjog_dir==-1 && cjog_speed==15; });
        service.set_cartesian_jog(2,30,3);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==2 && cjog_dir==1 && cjog_speed==30; });
        service.set_jog_direction(1,1);
        service.request_program_zero();
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        check(count("G.arm_curve_request")==0 && service.snapshot().cmd_dir[0]==0,"Active jog allowed home or single-axis jog");
        check(service.snapshot().cjog_state==2 && service.snapshot().cjog_limited
            && service.snapshot().cjog_path_speed==30 && service.snapshot().cjog_if_version==1,"Jog status snapshot mapping");
        service.stop_cartesian();
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==0 && cjog_dir==0 && cjog_speed==0; });
        const auto stopped_writes=count("G.arm_cjog_mode");
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        check(count("G.arm_cjog_mode")==stopped_writes && count("G.arm_curve_cancel")==cancel0,"Release continued writes or canceled home interface");
        const auto lease_start=std::chrono::steady_clock::now();
        service.set_cartesian_jog(3,2.5);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==3; });
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==0; });
        const auto lease_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-lease_start).count();
        check(lease_ms>=500,"Jog lease expired before 500 ms");
        std::cout << "ADS lease observed_ms=" << lease_ms << '\n';
        const auto expired_writes=count("G.arm_cjog_mode");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        check(count("G.arm_cjog_mode")==expired_writes,"Lease expiry continued alive writes");
        const auto before_invalid=count("G.arm_cjog_mode");
        for(int mode:{0,6})service.set_cartesian_jog(mode,1);
        for(double speed:{0.,101.,std::numeric_limits<double>::quiet_NaN()})service.set_cartesian_jog(3,speed);
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        check(count("G.arm_cjog_mode")==before_invalid,"Invalid request reached PLC");
        { std::lock_guard<std::mutex> lock(fake_mutex);cjog_owned=true; }
        until([&]{return service.snapshot().cjog_owned;});
        service.request_program_zero();
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        check(count("G.arm_curve_request")==0,"Home ignored PLC jog ownership");
        { std::lock_guard<std::mutex> lock(fake_mutex);cjog_owned=false;actual[4]=2; }
        until([&]{return !service.snapshot().cjog_owned && service.snapshot().act_pos[4]==2;});
        service.request_program_zero();
        until([&]{service.keep_cartesian_alive();return count("G.arm_curve_request")==1;});
        service.set_cartesian_jog(3,3);
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        check(count("G.arm_cjog_mode")==before_invalid,"Jog interrupted home");
        {
            std::lock_guard<std::mutex> lock(fake_mutex);
            check(executing_curve.values[16]==0,"Home changed curve format");
            for(int i=0;i<5;++i)actual[i]=executing_curve.values[5+i];
            plc_state=2;plc_owned=false;
        }
        until([&]{service.keep_cartesian_alive();return service.snapshot().cartesian_status==2;});
        check(count("G.arm_curve_request")==1,"Home repeated");
        service.set_cartesian_jog(3,3);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==3; });
        service.set_axis_enable(1,false);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==0; });
        service.set_axis_enable(1,true);
        service.set_cartesian_jog(3,3);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==3; });
        service.request_reset(1);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==0; });
        service.set_cartesian_jog(3,3);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==3; });
        service.set_manual_enable(false);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==0; });
        service.set_manual_enable(true);
        service.set_cartesian_jog(3,3);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==3; });
        { std::lock_guard<std::mutex> lock(fake_mutex);host_unhealthy=true; }
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==0; });
        { std::lock_guard<std::mutex> lock(fake_mutex);host_unhealthy=false; }
        service.set_cartesian_jog(3,3);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==3; });
        service.stop();
        {std::lock_guard<std::mutex> lock(fake_mutex);check(cjog_mode==0 && cjog_dir==0 && cjog_speed==0,"Shutdown did not clear jog");}
        std::cout<<"PASS ADS: single-axis tiers/deadman, atomic jog forwarding/alive despite snapshot read failure, speed and mode changes, release/lease, unhealthy gate, home mutual exclusion/regression, reset/disable/shutdown; no hardware.\n";
    } catch (const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';
        return 1;
    }
}
