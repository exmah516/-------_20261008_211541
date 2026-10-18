// 由运行脚本从 main.cpp 提取实际状态门控代码；运动同步替身只计数，不访问设备。
#include <iostream>
#include <cstdlib>
enum class AdsConnectionState { Running = 2, Reconnecting = 4 };
enum class StartupPhase { WaitForEnter, Done };
enum class GuidewireMode { None, Independent };
static void check(bool ok, const char* label) {
    std::cout << (ok ? "PASS: " : "FAIL: ") << label << std::endl;
    if (!ok) std::exit(1);
}
struct Fixture {
    bool self_check_done = true, last_self_check_done = true;
    bool has_new_ads_snapshot = true, ads_motion_cycle_valid = true;
    struct { bool position_valid = true, self_check_done = true; } ads_snapshot;
    struct { AdsConnectionState state = AdsConnectionState::Running; } ads_stats;
    struct { bool self_check_done = true; } ads_events;
    bool initial_sync_done = true, ads_resume_required = false, control_active = true;
    bool return_ads_fault_hold = false, motion_startup_active = false;
    bool estop_hold_active = false, ads_soft_hold_active = false, handle_soft_hold_active = false;
    bool has_self_check_flag = true, axis6_window_locked = true;
    bool plc_restart_recovery_latched = false;
    unsigned int pending_selfcheck_axes = 0;
    struct { bool enabled = false; } axis6_crawl;
    GuidewireMode guidewire_mode = GuidewireMode::Independent;
    struct { void reset() {} } spacing_recovery;
    struct {
        StartupPhase phase = StartupPhase::Done;
        bool completed = true, recovery_mode = false, direct_positioning = false, prompted = false;
        bool is_active() const { return false; }
    } startup;
    int sync_count = 0;
    bool sync_ok = true;
    bool sync_all(int) { ++sync_count; return sync_ok; }
    bool restore_startup_v_limit() { return true; }
    bool consume_startup_loading_ready() { return true; }
    void clear_cylinder_manual_overrides() {}
    bool cancel_active_return_motion(bool) { return true; }
    void observe() {
#include "main_observe.inc"
    }
    void auto_resume() {
#include "main_auto_resume.inc"
    }
    void start() {
        do {
#include "main_start.inc"
        } while (false);
    }
};
int main() {
    Fixture f;
    f.ads_motion_cycle_valid = false;
    f.has_new_ads_snapshot = false;
    f.ads_snapshot.position_valid = false;
    f.ads_stats.state = AdsConnectionState::Reconnecting;
    f.ads_events.self_check_done = false;
    f.ads_snapshot.self_check_done = false;
    f.observe();
    check(f.self_check_done, "disconnect cache reset does not masquerade as PLC self-check falling edge");
    check(!f.control_active && f.ads_resume_required, "snapshot loss pauses control and requires explicit resume");
    f.ads_soft_hold_active = true;
    f.start();
    check(!f.control_active && f.sync_count == 0, "start cannot bypass ADS hold");
    f.ads_soft_hold_active = false;
    f.ads_motion_cycle_valid = f.has_new_ads_snapshot = f.ads_snapshot.position_valid = true;
    f.ads_stats.state = AdsConnectionState::Running;
    f.ads_events.self_check_done = true;
    f.ads_snapshot.self_check_done = true;
    f.observe();
    f.auto_resume();
    check(!f.control_active && f.sync_count == 0, "reconnection alone cannot automatically restart motion");
    f.sync_ok = false;
    f.start();
    check(!f.control_active && f.ads_resume_required, "failed rebase leaves resume requirement latched");
    f.sync_ok = true;
    f.start();
    check(f.control_active && !f.ads_resume_required, "explicit start at recovered position restores control");
    check(f.guidewire_mode == GuidewireMode::Independent && f.axis6_window_locked,
          "communication recovery preserves stable mode");
    f.ads_events.self_check_done = false;
    f.ads_snapshot.self_check_done = false;
    f.observe();
    check(!f.self_check_done, "fresh PLC false remains authoritative");
    f.control_active = false;
    f.start();
    check(!f.control_active, "confirmed incomplete self-check blocks start");
    f.self_check_done = true;
    f.return_ads_fault_hold = true;
    f.start();
    check(!f.control_active, "explicit start cannot bypass a latched return fault");
}
