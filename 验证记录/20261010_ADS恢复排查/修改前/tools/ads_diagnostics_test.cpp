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
    std::string fail_item, error;
    std::vector<double> targets;
    unsigned int committed = 0;
    void check(bool ok, const char* name) {
        if (!ok) { std::cerr << "FAIL: " << name << std::endl; std::exit(1); }
        std::cout << "PASS: " << name << std::endl;
    }
}

CADSComm::CADSComm() : m_PAmsAddr(nullptr), m_adsPort(0), m_bOpen(true), m_timeoutMs(300) {}
CADSComm::~CADSComm() {}
bool CADSComm::IsCommOpen() const { return m_bOpen; }
bool CADSComm::OpenCommInsideReadOnly() { m_bOpen = true; return true; }
bool CADSComm::OpenCommReadOnly() { m_bOpen = true; return true; }
bool CADSComm::CloseComm() { m_bOpen = false; error.clear(); return true; }
bool CADSComm::SetTimeout(unsigned long) { return true; }
bool CADSComm::ReadDeviceState(unsigned short& a, unsigned short& d) { a = ADSSTATE_RUN; d = 0; return true; }
std::string CADSComm::GetLastErrorCopy() const { return error; }
unsigned long CADSComm::ADSGetAddr(const char* name) {
    for (auto& entry : symbols) if (entry.second == name) return entry.first;
    symbols[next_handle] = name;
    return next_handle++;
}
bool CADSComm::ADSRead(const char* name, unsigned long size, void* out) {
    std::memset(out, 0, size);
    if (std::string(name) == "G.self_check_done" || std::string(name) == "G.handle_reinit_done") *static_cast<bool*>(out) = true;
    return true;
}
bool CADSComm::ADSWrite(const char*, unsigned long, void*) { return true; }
bool CADSComm::ADSReadSum(const char* const* names, const unsigned long* sizes, void* const* out, unsigned long count) {
    for (unsigned long i = 0; i < count; ++i) ADSRead(names[i], sizes[i], out[i]);
    return true;
}
bool CADSComm::ADSWriteSum(const char* const*, const unsigned long*, const void* const*, unsigned long) { return true; }
bool CADSComm::ADSAddNotification(const char*, unsigned long, PAdsNotificationFuncEx, std::uint32_t, unsigned long* h, ADSTRANSMODE, unsigned long, unsigned long) { *h = next_handle++; return true; }
bool CADSComm::ADSDeleteNotification(unsigned long) { return true; }
bool CADSComm::ADSReadSumByHandle(const unsigned long* handles, const unsigned long* sizes, void* const* out, unsigned long count) {
    if (fail_read) { error = "injected read timeout code=1861"; return false; }
    if (!stall) ++cycle;
    for (unsigned long i = 0; i < count; ++i) {
        std::memset(out[i], 0, sizes[i]);
        const auto& s = symbols.at(handles[i]);
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
    check(service.read_fast_snapshot(snapshot, true) && snapshot.position_valid, "valid snapshot accepted");
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
    check(service.stats().diagnostic_sequence > 0, "disconnect cleanup preserves evidence");
    service.running_.store(false);
}
