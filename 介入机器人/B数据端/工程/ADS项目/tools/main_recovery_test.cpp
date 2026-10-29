// 由运行脚本从 main.cpp 提取实际状态门控代码；运动同步替身只计数，不访问设备。
#include <iostream>
#include <cstdlib>
enum class AdsConnectionState { Running = 2, Reconnecting = 4 };
enum class StartupPhase { WaitForEnter, Done };
enum class GuidewireMode { None, Independent };
namespace motion_sync {
    static int axis1_calls=0, axis6_calls=0, dual_calls=0;
    static bool ok=true;
    static bool sync_axis1(int, int, bool preserve) { ++axis1_calls; return ok && !preserve; }
    static bool sync_axis6(int, int, bool, bool, bool preserve) { ++axis6_calls; return ok && !preserve; }
    static bool rebase_dual_after_return(int) { ++dual_calls; return ok; }
}
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
    bool initial_sync_done = true, control_active = true;
    bool connection_hold_active = false, connection_hold_active_prev = true, single_handle_mode = false;
    bool handle_rebase_requested = false;
    bool axis1_fast_return = false, axis6_fast_retract = false;
    int ctx = 0;
    struct Filter {
        double axis0_filtered=0, axis1_filtered=0;
        void reset(double a,double b) { axis0_filtered=a; axis1_filtered=b; }
    } axis1_handle_filter, axis6_handle_filter;
    struct Input { double fJoints2[2] = {3, 4}; } input1, input6;
    Input* axis1_input_handle=&input1;
    Input* axis6_input_handle=&input6;
    double axis1_prev_linear_filtered=0, axis6_prev_linear_filtered=0;
    double axis1_prev_rot_filtered=0, axis6_prev_rot_filtered=0;
    struct { bool active() const { return false; } } planned_return;
    void load_pos_from_actual() {}
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
    void recover() {
#include "main_rebase.inc"
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
    f.ads_events.self_check_done = f.ads_snapshot.self_check_done = false;
    f.observe();
    check(f.self_check_done, "disconnect cache reset does not masquerade as PLC self-check falling edge");
    check(f.control_active, "a transient snapshot loss does not cancel the selected control mode");
    f.ads_soft_hold_active = true;
    f.ads_motion_cycle_valid = f.has_new_ads_snapshot = f.ads_snapshot.position_valid = true;
    f.ads_stats.state = AdsConnectionState::Running;
    f.ads_events.self_check_done = f.ads_snapshot.self_check_done = true;
    f.observe();
    f.recover();
    check(!f.ads_soft_hold_active && motion_sync::dual_calls == 1 && motion_sync::axis1_calls == 0,
          "dual-handle recovery rebases both handles together");
    check(f.control_active && f.guidewire_mode == GuidewireMode::Independent && f.axis6_window_locked,
          "recovered control keeps the selected mode without explicit restart");
    f.ads_soft_hold_active = true;
    motion_sync::ok = false;
    f.recover();
    check(f.ads_soft_hold_active, "failed coordinate rebase keeps input output gated");
    motion_sync::ok = true;
    f.single_handle_mode = true;
    f.recover();
    check(!f.ads_soft_hold_active && motion_sync::axis6_calls == 1,
          "single-handle recovery keeps the current independent mode");
    f.start();
    check(f.control_active && f.guidewire_mode == GuidewireMode::Independent && f.axis6_window_locked,
          "repeated start preserves a previously completed stable mode");

    Fixture waiting;
    waiting.control_active = false;
    waiting.ads_soft_hold_active = true;
    waiting.auto_resume(); waiting.start();
    check(!waiting.control_active && waiting.sync_count == 0, "start cannot bypass ADS hold");
    waiting.ads_soft_hold_active = false;
    waiting.handle_soft_hold_active = true;
    waiting.auto_resume(); waiting.start();
    check(!waiting.control_active && waiting.sync_count == 0, "missing handle samples do not start control");
    waiting.handle_soft_hold_active = false;
    waiting.sync_ok = false;
    waiting.auto_resume();
    check(!waiting.control_active, "failed rebase is retried without activating control");
    waiting.sync_ok = true;
    waiting.auto_resume();
    check(waiting.control_active && waiting.sync_count == 2,
          "fresh ADS and handles recover the completed control without a manual resume latch");
    waiting.control_active = false;
    waiting.ads_events.self_check_done = waiting.ads_snapshot.self_check_done = false;
    waiting.observe();
    waiting.auto_resume(); waiting.start();
    check(!waiting.self_check_done && !waiting.control_active,
          "confirmed incomplete PLC preparation still blocks control");
    waiting.self_check_done = true;
    waiting.return_ads_fault_hold = true;
    waiting.auto_resume(); waiting.start();
    check(!waiting.control_active, "a return fault is not bypassed by communication recovery");
    Fixture fresh;
    fresh.control_active = false;
    fresh.startup.completed = false;
    fresh.auto_resume();
    check(!fresh.control_active && fresh.sync_count == 0, "fresh startup does not auto-select control");
}
