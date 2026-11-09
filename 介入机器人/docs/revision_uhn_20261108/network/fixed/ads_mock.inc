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
#include <future>
#include <chrono>
#define private public
#include "ads_communication.h"
#undef private
#include "ads_communication.cpp"

void copy_positions(const double* source, double* destination, int count) {
    std::copy(source, source + count, destination);
}

namespace {
    std::unordered_map<unsigned long, std::string> symbols;
    std::atomic<unsigned long> next_handle{1};
    std::mutex symbols_mutex;
    std::atomic<unsigned int> closes{0}, heartbeats{0};
    std::atomic<bool> low_started{false};
    std::atomic<std::uint32_t> cycle{10};
    std::atomic<bool> fail_read{false}, fail_transport{false}, stall{false}, nonfinite{false};
    std::atomic<bool> fail_read_once{false};
    std::atomic<bool> network_down{false};
    std::atomic<unsigned int> single_read_failures{0};
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
    std::string fail_item;
    thread_local std::string error;
    std::vector<double> targets;
    std::atomic<unsigned int> committed{0};
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
bool CADSComm::OpenCommInsideReadOnly() { if (fail_local || network_down) { error = "LOCAL_READ_STATE 1861"; return false; } m_bOpen = true; return true; }
bool CADSComm::OpenCommReadOnly() { if (fail_remote || network_down) { error = "REMOTE_READ_STATE 29"; return false; } m_bOpen = true; return true; }
bool CADSComm::CloseComm() { m_bOpen = false; ++closes; error.clear(); return true; }
bool CADSComm::SetTimeout(unsigned long) { return true; }
bool CADSComm::ReadDeviceState(unsigned short& a, unsigned short& d) { if (network_down) { error = "READ_STATE 1861"; return false; } a = ADSSTATE_RUN; d = 0; return true; }
std::string CADSComm::GetLastErrorCopy() const { return error; }
std::string CADSComm::ConnectionDescription() const { return "target=offline:851 timeout_ms=300"; }
unsigned long CADSComm::ADSGetAddr(const char* name) {
    std::lock_guard<std::mutex> lock(symbols_mutex);
    if (name == missing_symbol) { error = "HNDBYNAME 1808 symbol=" + missing_symbol; return 0; }
    for (auto& entry : symbols) if (entry.second == name) return entry.first;
    const auto handle = next_handle++; symbols[handle] = name;
    return handle;
}
bool CADSComm::ADSRead(const char* name, unsigned long size, void* out) {
    if (std::string(name) == "Test.Block") { low_started = true; Sleep(2000); }
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
    if (network_down) { error = "network read timeout code=1861"; return false; }
    if (fail_read_once.exchange(false)) { ++single_read_failures; error = "injected single read failure"; return false; }
    if (fail_read) { error = "injected read timeout code=1861"; return false; }
    if (!stall) ++cycle;
    for (unsigned long i = 0; i < count; ++i) {
        std::memset(out[i], 0, sizes[i]);
        std::string s; { std::lock_guard<std::mutex> lock(symbols_mutex); s = symbols.at(handles[i]); }
        validate_size(s, sizes[i]);
        fill_velocity(s, out[i]);
        if (s == "G.self_check_done" || s == "G.handle_reinit_done") *static_cast<bool*>(out[i]) = true;
        if (s.find("CycleCount") != std::string::npos) *static_cast<std::uint32_t*>(out[i]) = cycle.load();
        if (s.find("DcTaskTime") != std::string::npos) *static_cast<std::int64_t*>(out[i]) = static_cast<std::int64_t>(cycle.load()) * 10000000;
        if (nonfinite && s == "G.axis[1].NcToPlc.ActPos") *static_cast<double*>(out[i]) = std::numeric_limits<double>::quiet_NaN();
    }
    error.clear(); return true;
}
bool CADSComm::ADSWriteSumByHandle(const unsigned long* handles, const unsigned long* sizes, const void* const* in, unsigned long count, unsigned long* results, bool* transport) {
    *transport = !fail_transport && !network_down;
    if (!*transport) { error = "injected write timeout code=1861"; return false; }
    bool ok = true;
    for (unsigned long i = 0; i < count; ++i) {
        std::string s; { std::lock_guard<std::mutex> lock(symbols_mutex); s = symbols.at(handles[i]); }
        validate_size(s, sizes[i]);
        results[i] = s == fail_item ? 1797 : 0;
        if (results[i]) { ok = false; error = "injected item code=1797 symbol=" + s; }
        if (s.find("TargetAbs") != std::string::npos) { assert(sizes[i] == sizeof(double)); targets.push_back(*static_cast<const double*>(in[i])); }
        if (s == "G.host_heartbeat_sequence") ++heartbeats;
        if (s.find(".Req") != std::string::npos && *static_cast<const bool*>(in[i])) ++committed;
    }
    if (ok) error.clear();
    return ok;
}
