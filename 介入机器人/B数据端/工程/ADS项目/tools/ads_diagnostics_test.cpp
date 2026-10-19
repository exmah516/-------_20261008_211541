// 离线测试真实通信服务，替换 CADSComm 实现，不链接 ADS DLL、不连接设备。
#include <ADSComm1.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <thread>
#include <cassert>
#include <cmath>
#include <limits>
#define private public
#include "../ads_communication.h"
#undef private
#include "../ads_communication.cpp"

void copy_positions(const double* source, double* destination, int count) {
    std::copy(source, source + count, destination);
}

namespace {
    std::unordered_map<unsigned long, std::string> symbols;
    unsigned long next_handle = 1;
    std::uint32_t cycle = 10;
    bool fail_read = false, fail_transport = false, stall = false, nonfinite = false;
    bool fail_local = false, fail_remote = false;
    std::string missing_symbol, nonfinite_velocity;
    const std::unordered_map<std::string,double> velocities = {
        {"G.axis[1].NcToPlc.ActVelo",1.25}, {"G.axis[3].NcToPlc.ActVelo",-3.5},
        {"G.axis[5].NcToPlc.ActVelo",5.75}, {"G.axis[6].NcToPlc.ActVelo",-6.25}
    };
    void fill_velocity(const std::string& symbol, void* out) {
        const auto it=velocities.find(symbol);
        if(it!=velocities.end()) *static_cast<double*>(out)=it->second;
        if(!nonfinite_velocity.empty() && symbol==nonfinite_velocity)
            *static_cast<double*>(out)=std::numeric_limits<double>::quiet_NaN();
    }
    std::string fail_item, error;
    std::vector<double> targets;
    unsigned int committed = 0;
    const std::unordered_map<std::string, unsigned long> plc_sizes = {
#include "ads_symbol_sizes.inc"
    };
    void validate_size(const std::string& name, unsigned long size) {
        auto expected = plc_sizes.find(name);
        if (expected != plc_sizes.end() && expected->second != size) {
            std::cerr << "FAIL: PLC size mismatch " << name << " expected=" << expected->second << " requested=" << size << std::endl;
            std::exit(1);
        }
    }
    void check(bool ok, const char* name) {
        if (!ok) { std::cerr << "FAIL: " << name << std::endl; std::exit(1); }
        std::cout << "PASS: " << name << std::endl;
    }
}

CADSComm::CADSComm() : m_PAmsAddr(nullptr), m_adsPort(0), m_bOpen(true), m_timeoutMs(300) {}
CADSComm::~CADSComm() {}
bool CADSComm::IsCommOpen() const { return m_bOpen; }
bool CADSComm::OpenCommInsideReadOnly() { if (fail_local) { error = "LOCAL_READ_STATE 1861"; return false; } m_bOpen = true; return true; }
bool CADSComm::OpenCommReadOnly() { if (fail_remote) { error = "REMOTE_READ_STATE 29"; return false; } m_bOpen = true; return true; }
bool CADSComm::CloseComm() { m_bOpen = false; error.clear(); return true; }
bool CADSComm::SetTimeout(unsigned long) { return true; }
bool CADSComm::ReadDeviceState(unsigned short& a, unsigned short& d) { a = ADSSTATE_RUN; d = 0; return true; }
std::string CADSComm::GetLastErrorCopy() const { return error; }
std::string CADSComm::ConnectionDescription() const { return "target=offline:851 timeout_ms=300"; }
unsigned long CADSComm::ADSGetAddr(const char* name) {
    if (name == missing_symbol) { error = "HNDBYNAME 1808 symbol=" + missing_symbol; return 0; }
    for (auto& entry : symbols) if (entry.second == name) return entry.first;
    symbols[next_handle] = name;
    return next_handle++;
}
bool CADSComm::ADSRead(const char* name, unsigned long size, void* out) {
    validate_size(name, size);
    std::memset(out, 0, size);
    fill_velocity(name, out);
    if (std::string(name) == "G.self_check_done" || std::string(name) == "G.handle_reinit_done") *static_cast<bool*>(out) = true;
    return true;
}
bool CADSComm::ADSWrite(const char* name, unsigned long size, void*) { validate_size(name, size); return true; }
bool CADSComm::ADSReadSum(const char* const* names, const unsigned long* sizes, void* const* out, unsigned long count) {
    for (unsigned long i = 0; i < count; ++i) ADSRead(names[i], sizes[i], out[i]);
    return true;
}
bool CADSComm::ADSWriteSum(const char* const*, const unsigned long*, const void* const*, unsigned long) { return true; }
bool CADSComm::ADSAddNotification(const char* name, unsigned long size, PAdsNotificationFuncEx, std::uint32_t, unsigned long* h, ADSTRANSMODE, unsigned long, unsigned long) { validate_size(name, size); *h = next_handle++; return true; }
bool CADSComm::ADSDeleteNotification(unsigned long) { return true; }
bool CADSComm::ADSReadSumByHandle(const unsigned long* handles, const unsigned long* sizes, void* const* out, unsigned long count) {
    if (fail_read) { error = "injected read timeout code=1861"; return false; }
    if (!stall) ++cycle;
    for (unsigned long i = 0; i < count; ++i) {
        std::memset(out[i], 0, sizes[i]);
        const auto& s = symbols.at(handles[i]);
        validate_size(s, sizes[i]);
        fill_velocity(s, out[i]);
        if (s.find("CycleCount") != std::string::npos) *static_cast<std::uint32_t*>(out[i]) = cycle;
        if (s.find("DcTaskTime") != std::string::npos) *static_cast<std::int64_t*>(out[i]) = static_cast<std::int64_t>(cycle) * 10000000;
        if (nonfinite && s == "G.axis[1].NcToPlc.ActPos") *static_cast<double*>(out[i]) = std::numeric_limits<double>::quiet_NaN();
    }
    error.clear(); return true;
}
bool CADSComm::ADSWriteSumByHandle(const unsigned long* handles, const unsigned long* sizes, const void* const* in, unsigned long count, unsigned long* results, bool* transport) {
    *transport = !fail_transport;
    if (fail_transport) { error = "injected write timeout code=1861"; return false; }
    bool ok = true;
    for (unsigned long i = 0; i < count; ++i) {
        const auto& s = symbols.at(handles[i]);
        validate_size(s, sizes[i]);
        results[i] = s == fail_item ? 1797 : 0;
        if (results[i]) { ok = false; error = "injected item code=1797 symbol=" + s; }
        if (s.find("TargetAbs") != std::string::npos) { assert(sizes[i] == sizeof(double)); targets.push_back(*static_cast<const double*>(in[i])); }
        if (s.find(".Req") != std::string::npos && *static_cast<const bool*>(in[i])) ++committed;
    }
    if (ok) error.clear();
    return ok;
}

int main() {
    CADSComm ads;
    AdsCommunicationService service(ads);
    check(service.resolve_fast_handles(), "resolve production symbol tables");
    service.running_.store(true);
    AdsOutputCommand output{};
    output.motion_enabled = true; output.cylinder_valid = true;
    output.cylinder[1] = output.cylinder[3] = 1000;
    service.publish_output(output);
    bool processed = false;
    check(service.write_output_cycle(processed), "normal 1000/1000 output succeeds");
    AdsPlannedReturnCommand command{};
    command.operation = AdsPlannedReturnOperation::Prepare; command.leg_count = 2;
    command.legs[0] = {0, 100, 20, 4800, 4800, 70000};
    command.legs[1] = {5, 200, 20, 4800, 4800, 70000};
    check(service.submit_planned_return_command(command) != 0, "enqueue two-leg prepare");
    check(service.write_output_cycle(processed) && processed && targets.size() == 2 && targets[0] == 100 && targets[1] == 200, "production prepare writes both targets");
    command.operation = AdsPlannedReturnOperation::Commit;
    service.submit_planned_return_command(command);
    check(service.write_output_cycle(processed) && committed == 2, "production commit writes both requests");

    command.operation = AdsPlannedReturnOperation::Prepare;
    fail_item = "G.return_cmd[6].TargetAbs";
    auto seq = service.submit_planned_return_command(command);
    check(service.write_output_cycle(processed), "queue-only item failure preserves core communication");
    bool completed = false, success = true; std::uint8_t mask = 0;
    check(service.planned_return_command_result(seq, completed, success, mask) && completed && !success, "failed prepare cannot become successful");
    check(std::string(service.stats().diagnostic).find(fail_item) != std::string::npos, "item error and symbol preserved");
    fail_item = "G.host_heartbeat_sequence";
    check(!service.write_output_cycle(processed), "heartbeat write failure invalidates core cycle");
    check(std::string(service.stats().diagnostic).find("SUM_WRITE") == 0, "heartbeat failure classified as SUM_WRITE");
    fail_item.clear(); fail_transport = true;
    command.operation = AdsPlannedReturnOperation::Commit;
    seq = service.submit_planned_return_command(command);
    check(!service.write_output_cycle(processed), "transport timeout invalidates core cycle");
    check(service.planned_return_command_result(seq, completed, success, mask) && completed && !success && mask == 3, "uncertain commit retains both possibly-started bits");
    check(std::string(service.stats().diagnostic).find("planned_operation=1") != std::string::npos, "commit context retained with timeout");
    fail_transport = false;
    const auto failure = service.stats().diagnostic_sequence;
    const std::string captured = service.stats().diagnostic;
    service.write_output_cycle(processed);
    check(error.empty() && captured == service.stats().diagnostic && service.stats().diagnostic_sequence == failure, "success cannot erase captured failure");

    AdsFastSnapshot snapshot{};
    service.event_state_.self_check_done = true;
    check(service.read_fast_snapshot(snapshot, true) && snapshot.position_valid, "valid snapshot accepted");
    const auto confirmed_snapshot = snapshot;
    double actual[7]={}, from_left[7]={}, init[7]={}, left[7]={};
    AppContext ctx;
    ctx.ads=&ads; ctx.ads_service=&service;
    ctx.plc_act_pos=actual; ctx.plc_act_pos_from_left=from_left;
    ctx.plc_init_pos=init; ctx.plc_leftlimit=left;
    // 用不同的有符号速度检查句柄顺序，防止全零样本掩盖映射错误。
    for(bool direct:{true,false}) {
        service.use_direct_nc_position_=direct;
        snapshot={};
        check(service.read_fast_snapshot(snapshot,false) && snapshot.position_valid,"direct/fallback snapshot reads velocity");
        check(snapshot.axis1_act_velocity_mm_s==1.25 && snapshot.axis3_act_velocity_mm_s==-3.5 &&
              snapshot.axis5_act_velocity_mm_s==5.75 && snapshot.axis6_act_velocity_mm_s==-6.25,
              "axis 1/3/5/6 signed velocities map to their own snapshot fields");
        service.publish_snapshot(snapshot);
        check(plc_io::read_plc_state(ctx) && ctx.plc_act_velocity_mm_s[0]==1.25 &&
              ctx.plc_act_velocity_mm_s[2]==-3.5 && ctx.plc_act_velocity_mm_s[4]==5.75 &&
              ctx.plc_act_velocity_mm_s[5]==-6.25,"control reads position and velocity from same published snapshot");
        for(int axis:{1,3,5,6}) {
            nonfinite_velocity="G.axis["+std::to_string(axis)+"].NcToPlc.ActVelo";
            snapshot={};
            check(!service.read_fast_snapshot(snapshot,false) && !snapshot.position_valid,"nonfinite velocity rejects motion snapshot");
            service.publish_snapshot(snapshot);
            check(!plc_io::read_plc_state(ctx),"invalid velocity cannot reach motion control");
        }
        nonfinite_velocity.clear();
    }
    service.use_direct_nc_position_=true;
    ctx.ads_service=nullptr;
    check(plc_io::read_plc_state(ctx) && ctx.plc_act_velocity_mm_s[0]==1.25 &&
          ctx.plc_act_velocity_mm_s[2]==-3.5 && ctx.plc_act_velocity_mm_s[4]==5.75 &&
          ctx.plc_act_velocity_mm_s[5]==-6.25,"startup compatibility read maps all four velocities");
    fail_read = true;
    snapshot = {};
    check(!service.read_fast_snapshot(snapshot, false) && !snapshot.position_valid, "read timeout invalidates snapshot");
    check(std::string(service.stats().diagnostic).find("SUM_READ") == 0, "read timeout distinguished from write timeout");
    fail_read = false; stall = true;
    for (int i = 0; i < 3; ++i) { snapshot = {}; service.read_fast_snapshot(snapshot, false); }
    check(!snapshot.position_valid && std::string(service.stats().diagnostic).find("PLC_TASK_STALLED") == 0, "three stalled PLC samples identified");
    stall = false; nonfinite = true;
    snapshot = {}; service.read_fast_snapshot(snapshot, false);
    check(!snapshot.position_valid && std::string(service.stats().diagnostic).find("NONFINITE_POSITION") == 0, "nonfinite position identified");
    nonfinite = false; cycle = 1;
    snapshot = {}; service.read_fast_snapshot(snapshot, false);
    check(!snapshot.position_valid && std::string(service.stats().diagnostic).find("PLC_TIME_REGRESSED") == 0, "regressed PLC task identified");
    service.event_state_.self_check_done = true;
    service.clear_runtime_connection_state();
    check(!service.event_state().self_check_done, "disconnect clears self-check cache without a PLC self-check restart");
    check(confirmed_snapshot.self_check_done, "published snapshot retains its own confirmed self-check state after cache reset");
    check(service.stats().diagnostic_sequence > 0, "disconnect cleanup preserves evidence");
    ads.CloseComm();
    fail_local = fail_remote = true;
    check(!service.ensure_connection(true), "both connection failures remain failures");
    auto connection_error = std::string(service.stats().diagnostic);
    check(connection_error.find("LOCAL_READ_STATE 1861") != std::string::npos &&
          connection_error.find("REMOTE_READ_STATE 29") != std::string::npos,
          "fallback failure cannot erase primary connection error");
    fail_local = fail_remote = false;
    missing_symbol = "G.return_cmd[1].TargetAbs";
    check(!service.ensure_connection(true) && !service.fast_handles_valid_,
          "missing return symbol prevents readiness before first return");
    check(std::string(service.stats().diagnostic).find(missing_symbol) != std::string::npos,
          "missing symbol failure preserves variable name");
    missing_symbol.clear();
    check(service.ensure_connection(true) && service.event_state().self_check_done,
          "reconnection reloads actual PLC self-check instead of retaining cleared cache");
    const std::int16_t plc_state = 6;
    service.on_notification(kNotifyGenState, &plc_state, sizeof(plc_state));
    check(service.event_state().gen_state == 6, "two-byte PLC state notification is accepted");
    const std::int32_t oversized_state = 10;
    service.on_notification(kNotifyGenState, &oversized_state, sizeof(oversized_state));
    check(service.event_state().gen_state == 6, "wrong-size state notification is rejected");
    service.running_.store(false);
}
