// 由运行脚本从 main.cpp 提取实际状态门控代码；运动同步替身只计数，不访问设备。
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
    bool handle_rebase_requested = false, handle1_rebase_requested = false, handle6_rebase_requested = false;
    std::string recovery_event_text;
    struct GapHandle { int sample_gap_ms() const { return 1600; } } handle_axis1, handle_axis6;
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
    struct { bool running=false; bool active() const { return running; } } planned_return;
    void load_pos_from_actual() {}
    bool return_ads_fault_hold = false, motion_startup_active = false;
    bool estop_hold_active = false, ads_soft_hold_active = false, handle_soft_hold_active = false;
    bool has_self_check_flag = true, axis6_window_locked = true;
    bool plc_restart_recovery_latched = false;
    unsigned int pending_selfcheck_axes = 0;
    struct { bool enabled = false; } axis6_crawl;
    GuidewireMode guidewire_mode = GuidewireMode::Independent;
    struct { bool active() const { return false; } SpacingRecoveryPhase phase = SpacingRecoveryPhase::Idle; void reset() {} } spacing_recovery;
    struct {
        StartupPhase phase = StartupPhase::Done;
        bool completed = true, recovery_mode = false, direct_positioning = false, prompted = false;
        bool is_active() const { return false; }
    } startup;
    int sync_count = 0, cancel_count = 0;
    bool sync_ok = true;
    bool sync_all(int) { ++sync_count; return sync_ok; }
    bool restore_startup_v_limit() { return true; }
    bool consume_startup_loading_ready() { return true; }
    void clear_cylinder_manual_overrides() {}
    bool cancel_active_return_motion(bool) { ++cancel_count; planned_return.running=false; return true; }
    void observe() {
#include "main_observe.inc"
    }

    double plc_act_pos[7] = {}, axis2_hold_rel = 0, axis7_hold_rel = 0, independent_axis2_hold_rel = 0;
    struct { void disable_compensation() {} } tracking_controller;
    void clear_force_output() {}
    void edge() {
#include "main_connection.inc"
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