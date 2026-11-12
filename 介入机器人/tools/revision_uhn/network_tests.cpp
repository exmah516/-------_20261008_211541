// 生产 ADS 线程、Zenoh 服务认证入口和抽取的主循环保持分支；不链接 ADS DLL。
#include <winsock2.h>
#include <bcrypt.h>
#include <fstream>
#include <sstream>
#include "ads_mock.inc"
#include "zenoh_service.h"
#include "手柄/Handle.h"
#include "experiment_events.h"

namespace main_fixture {
#include "main_fixture.inc"
}

struct ZenohServiceTestAccess
{
    static void initialize(ZenohService& service)
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        assert(BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
            BCRYPT_ALG_HANDLE_HMAC_FLAG)));
        service.bcrypt_algorithm_ = algorithm;
        service.token_.assign(32, 't');
        service.session_key_.assign(16, 0x42);
        service.session_id_ = 41;
        service.authenticated_ = service.lease_held_ = true;
    }
    static void sample(ZenohService& service, std::uint64_t sequence)
    {
        vi_protocol::HandleFrame frame;
        frame.version = 1; frame.session_id = 41; frame.sequence = sequence;
        frame.valid582 = frame.valid587 = true;
        frame.device_epoch582 = frame.device_epoch587 = 1;
        frame.auth_tag = service.sign(frame);
        service.on_handle(frame);
    }
};

static void network_assert(bool ok, const char* label)
{
    if (!ok) { std::cerr << "FAIL N: " << label << std::endl; std::exit(1); }
}

int main(int argc, char** argv)
{
    network_assert(argc == 3, "scenario and duration arguments");
    const std::string scenario = argv[1];
    const int duration = std::stoi(argv[2]);
    const bool break_ads = scenario != "zenoh", break_zenoh = scenario != "ads";
    network_assert(scenario == "zenoh" || scenario == "ads" || scenario == "both", "known scenario");
    network_assert(duration == 10 || duration == 30, "10s or30s duration");
    CADSComm ads;
    AdsCommunicationService communication(ads);
    network_assert(communication.start(nullptr, nullptr), "production ADS thread started");
    ZenohService zenoh; ZenohServiceTestAccess::initialize(zenoh);
    Handle wire(582), catheter(587);
    main_fixture::Fixture control;
    control.connection_hold_active_prev = false;
    std::uint64_t sequence = 0, snapshot_sequence = 0;
    int entries = 0, exits = 0, premature_rebases = 0;
    bool verify_interruption = false;
    auto cycle_once = [&](bool send_handle) {
        if (send_handle) ZenohServiceTestAccess::sample(zenoh, ++sequence);
        if (!wire.is_open()) wire.init();
        if (!catheter.is_open()) catheter.init();
        const bool wire_ok = wire.poll(), catheter_ok = catheter.poll();
        control.handle1_poll_ok = catheter_ok;
        control.handle6_poll_ok = wire_ok;
        if (!wire_ok && wire.is_open()) wire.close();
        if (!catheter_ok && catheter.is_open()) catheter.close();
        control.handle_soft_hold_active = !wire_ok || !catheter_ok ||
            wire.resume_settling() || catheter.resume_settling();
        control.handle6_rebase_requested = wire.take_rebase_request();
        control.handle1_rebase_requested = catheter.take_rebase_request();
        control.handle_rebase_requested = control.handle6_rebase_requested || control.handle1_rebase_requested;
        AdsFastSnapshot snapshot;
        const bool fresh = communication.wait_for_snapshot(snapshot_sequence, 50, snapshot);
        if (fresh) snapshot_sequence = snapshot.attempt_sequence;
        const auto state = communication.stats().state;
        control.ads_motion_cycle_valid = fresh && snapshot.position_valid && state == AdsConnectionState::Running;
        if (!control.ads_motion_cycle_valid) control.ads_soft_hold_active = true;
        else {
            if (verify_interruption && control.handle_soft_hold_active)
                control.recovery_event_text = "等待新鲜手柄采样";
            const auto reason_before = control.recovery_event_text;
            control.recover();
            if (verify_interruption && control.handle_soft_hold_active && control.recovery_event_text != reason_before)
                ++premature_rebases;
#ifndef BASELINE
            network_assert(!verify_interruption || !control.handle_soft_hold_active || control.recovery_event_text == reason_before,
                "missing handles cannot repeatedly report connection-hold end");
#endif
        }
        const bool was_holding = control.connection_hold_active_prev;
        control.edge();
        if (!was_holding && control.connection_hold_active_prev) ++entries;
        if (was_holding && !control.connection_hold_active_prev) ++exits;
        control.auto_resume();
    };
    auto until = [&](auto predicate, int timeout_ms, bool handles) {
        const auto began = GetTickCount64();
        do { cycle_once(handles); if (predicate()) return true; } while (GetTickCount64() - began < static_cast<ULONGLONG>(timeout_ms));
        return predicate();
    };
    network_assert(until([&] { return communication.stats().state == AdsConnectionState::Running &&
        !control.connection_hold_active_prev; }, 3000, true), "initial control ready");
    entries = exits = 0;
    verify_interruption = true;
    const auto began = GetTickCount64();
    network_down = break_ads;
    while (GetTickCount64() - began < static_cast<ULONGLONG>(duration) * 1000) {
        cycle_once(!break_zenoh);
        if (!break_ads) network_assert(communication.stats().state == AdsConnectionState::Running,
            "Zenoh interruption cannot change healthy ADS state");
    }
    network_assert(control.connection_hold_active_prev && entries == 1 && exits == 0,
        "interruption enters hold once and never reports exit while still interrupted");
    const auto restored = GetTickCount64();
    network_down = false;
    network_assert(until([&] { return communication.stats().state == AdsConnectionState::Running &&
        !control.connection_hold_active_prev; }, 1500, true), "recovers within1s reconnect plus initialization");
    const auto recovery_ms = GetTickCount64() - restored;
    const auto stable_since = GetTickCount64();
    while (GetTickCount64() - stable_since < 600) cycle_once(true);
    network_assert(entries == 1 && exits == 1 && control.control_active,
        "control resumes automatically with exactly one hold exit");
    communication.stop();
#ifdef BASELINE
    network_assert(premature_rebases > 1, "old branch repeatedly reports successful rebase with missing handles");
    std::cout << "REPRODUCED N old_premature_rebases=" << premature_rebases << std::endl;
#else
    network_assert(premature_rebases == 0, "no premature rebase during handle hold");
#endif
    std::cout << "PASS N scenario=" << scenario << " duration_s=" << duration
        << " outage_ms=" << restored - began << " recovery_ms=" << recovery_ms
        << " entries=" << entries << " exits=" << exits << " control_active=" << control.control_active << std::endl;
}
