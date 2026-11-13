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
#include "../ads_communication.h"
#undef private
#include "../ads_communication.cpp"

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
    std::atomic<std::uint32_t> handoff_version{kHandoffInterfaceVersion};
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
    std::vector<std::string> writes;
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
    if (std::string(name)=="G.handoff_if_version") *static_cast<std::uint32_t*>(out)=handoff_version.load();
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
        if (s=="G.handoff_if_version") *static_cast<std::uint32_t*>(out[i])=handoff_version.load();
        if (s == "G.self_check_done" || s == "G.handle_reinit_done") *static_cast<bool*>(out[i]) = true;
        if (s.find("CycleCount") != std::string::npos) *static_cast<std::uint32_t*>(out[i]) = cycle.load();
        if (s.find("DcTaskTime") != std::string::npos) *static_cast<std::int64_t*>(out[i]) = static_cast<std::int64_t>(cycle.load()) * 10000000;
        if (nonfinite && s == "G.axis[1].NcToPlc.ActPos") *static_cast<double*>(out[i]) = std::numeric_limits<double>::quiet_NaN();
    }
    error.clear(); return true;
}
bool CADSComm::ADSWriteSumByHandle(const unsigned long* handles, const unsigned long* sizes, const void* const* in, unsigned long count, unsigned long* results, bool* transport) {
    const bool transport_ok = !fail_transport && !network_down;
    if (transport) *transport = transport_ok;
    if (!transport_ok) { error = "injected write timeout code=1861"; return false; }
    bool ok = true;
    for (unsigned long i = 0; i < count; ++i) {
        std::string s; { std::lock_guard<std::mutex> lock(symbols_mutex); s = symbols.at(handles[i]); }
        validate_size(s, sizes[i]);
        const unsigned long result = s == fail_item ? 1797 : 0;
        if (results) results[i] = result;
        writes.push_back(s);
        if (result) { ok = false; error = "injected item code=1797 symbol=" + s; }
        if (s == "G.handoff_cmd.target_abs") { assert(sizes[i] == 2*sizeof(double)); const auto v=static_cast<const double*>(in[i]); targets.assign(v,v+2); }
        if (s == "G.host_heartbeat_sequence") ++heartbeats;
        if (s == "G.handoff_cmd.request_id" && !result && *static_cast<const std::uint32_t*>(in[i])) ++committed;
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
    check(service.write_output_cycle(true), "normal output succeeds");
    output.handoff.leg_mask=3; output.handoff.request_id=1; output.handoff_send_token=1;
    output.handoff.target_abs[0]=100; output.handoff.target_abs[1]=200;
    service.publish_output(output); writes.clear();
    check(service.write_output_cycle(true) && targets.size()==2 && targets[0]==100 && targets[1]==200, "latched two-leg parameters written");
    check(writes.back()=="G.handoff_cmd.request_id" && committed==1, "request id is last after all nine parameters");
    service.write_output_cycle(true); check(committed==1, "same send token is attempted only once");
    output.handoff_send_token=2; service.publish_output(output);
    fail_item="G.handoff_cmd.target_abs"; writes.clear();
    check(!service.write_output_cycle(true) && committed==1, "partial parameter failure cannot publish request id");
    check(writes.back()!="G.handoff_cmd.request_id", "failed parameter batch stops id commit");
    fail_item.clear(); output.handoff_send_token=3; service.publish_output(output);
    check(service.write_output_cycle(true) && committed==2, "explicit resend writes same request again");
    output.handoff_send_token=0; output.handoff_takeover_id=1;
    const auto takeover_generation=service.publish_output(output);
    fail_item="G.cyl_cmd[2]"; writes.clear();
    check(!service.write_output_cycle(true) && std::find(writes.begin(),writes.end(),"G.handoff_takeover_id")==writes.end(), "partial cylinder failure cannot release PLC freeze");
    check(service.applied_motion_output_generation()<takeover_generation, "failed takeover does not confirm motion generation");
    fail_item="G.handoff_takeover_id";
    check(!service.write_output_cycle(true) && service.applied_motion_output_generation()<takeover_generation, "failed takeover id cannot confirm output");
    fail_item.clear(); writes.clear();
    check(service.write_output_cycle(true) && writes.back()=="G.handoff_takeover_id" && service.applied_motion_output_generation()==takeover_generation, "takeover id last and confirmed only after full output");
    fail_item="G.host_heartbeat_sequence";
    check(!service.write_output_cycle(true), "heartbeat failure invalidates core cycle");
    check(std::string(service.stats().diagnostic).find("SUM_WRITE")==0, "heartbeat failure classified as SUM_WRITE");
    fail_item.clear(); fail_transport=true;
    check(!service.write_output_cycle(true), "transport timeout invalidates core cycle");
    fail_transport=false;
    const auto failure=service.stats().diagnostic_sequence; const std::string captured=service.stats().diagnostic;
    service.write_output_cycle(true);
    check(error.empty() && captured==service.stats().diagnostic && service.stats().diagnostic_sequence==failure, "success cannot erase captured failure");

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
    handoff_version = 2;
    check(!service.ensure_connection(true) && service.stats().handoff_if_version == 2 && !service.fast_handles_valid_,
          "mismatched PLC interface version prevents initialization and remains visible");
    handoff_version = kHandoffInterfaceVersion;
    missing_symbol = "G.handoff_cmd.target_abs";
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
    {
        CADSComm fast;
        AdsCommunicationService threaded(fast);
        auto wait = [&](auto predicate, int timeout) {
            const auto start=GetTickCount64();
            while(GetTickCount64()-start < static_cast<ULONGLONG>(timeout)) { if(predicate()) return true; Sleep(1); }
            return predicate();
        };
        check(threaded.start(nullptr,nullptr), "V3 start production fast and slow threads");
        check(wait([&]{return threaded.stats().state==AdsConnectionState::Running;},3000), "V3 first full cycle Running");
        AdsOutputCommand queued{}; queued.handoff.leg_mask=1; queued.handoff.request_id=20;
        std::uint64_t token=20; queued.handoff_send_token=token;
        const unsigned int single_close_before=closes.load();
        fail_read_once=true;
        check(wait([&]{return threaded.stats().state==AdsConnectionState::SoftHold;},500), "V3 exactly one failed snapshot enters SoftHold");
        const auto single_committed=committed.load(); threaded.publish_output(queued);
        auto single_low=std::async(std::launch::async,[&]{int value=0;return threaded.read("Test.SinglePending",sizeof(value),&value,50);});
        check(wait([&]{return committed>single_committed;},1000), "V3 failed cycle preserves latest handoff request");
        check(single_low.get() && single_read_failures==1 && closes==single_close_before, "V3 one failed cycle preserves low frequency request without CloseComm");
        for(bool write_failure:{false,true}) for(int duration:{20,500,1200}) {
            const auto close_before=closes.load();
            if(write_failure) fail_transport=true; else fail_read=true;
            check(wait([&]{return threaded.stats().state==AdsConnectionState::SoftHold;},500), "V3 failure enters SoftHold");
            const auto before_committed=committed.load();
            queued.handoff_send_token=++token; ++queued.handoff.request_id; threaded.publish_output(queued);
            auto low=std::async(std::launch::async,[&]{int value=0;return threaded.read("Test.Pending",sizeof(value),&value,50);});
            Sleep(duration);
            check(committed==before_committed, "V3 unready communication cannot submit handoff");
            if(duration<1000) {
                check(closes==close_before, "V3 short failure preserves connection");
                check(low.wait_for(std::chrono::milliseconds(0))==std::future_status::timeout, "V3 SoftHold preserves low frequency queue beyond API timeout");
            } else check(closes>close_before, "V3 1.2s failure closes communication");
            fail_read=false; fail_transport=false;
            check(wait([&]{return threaded.stats().state==AdsConnectionState::Running;},3000), "V3 complete successful cycle restores Running");
            if(duration>=1000) { queued.handoff_send_token=++token; threaded.publish_output(queued); }
            check(wait([&]{return committed>before_committed;},1000), "V3 latest handoff submitted after recovery");
            if(duration<1000) check(low.get(), "V3 low frequency request succeeds after short failure");
            else check(!low.get(), "V3 disconnected low frequency request fails");
        }
        low_started=false;
        auto blocking=std::async(std::launch::async,[&]{int value=0;return threaded.read("Test.Block",sizeof(value),&value);});
        check(wait([&]{return low_started.load();},2000), "V3 low-frequency mock blocks for2s");
        const auto before=threaded.stats().latest_rtt_us;
        AdsFastSnapshot first,last; threaded.latest_snapshot(first);
        const auto heartbeat_before=heartbeats.load();
        check(blocking.get(), "V3 blocked low-frequency operation completes");
        threaded.latest_snapshot(last);
        const auto heartbeat_delta=heartbeats.load()-heartbeat_before;
        std::cout << "V3 2s block: fast_snapshot_delta=" << last.attempt_sequence-first.attempt_sequence << " heartbeat_delta=" << heartbeat_delta << " rtt_us=" << before << std::endl;
        check(last.attempt_sequence-first.attempt_sequence>=170 && heartbeat_delta>=170, "V3 low-frequency blocking cannot block fast snapshot or heartbeat");
        threaded.stop();
    }
}
