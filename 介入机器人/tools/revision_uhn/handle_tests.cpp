// 只替换时钟和硬件输入；恢复、滤波、基准重建、力反馈及映射表达式来自生产代码。
#include <windows.h>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "Handle.h"
static ULONGLONG clock_ms = 10000;
static ULONGLONG replay_tick() { return clock_ms; }
#define GetTickCount64 replay_tick
#include "remote_handle_bridge.h"
#include "手柄.cpp"
#include "control_types.h"
#include "force_feedback.h"
#include "motion_sync.h"
#include "plc_io.h"

static void check(bool ok, const char* text)
{
    if (!ok) { std::cerr << "FAIL H: " << text << '\n'; std::exit(1); }
}
static bool settling(const Handle& handle)
{
#ifdef BASELINE
    (void)handle; return false;
#else
    return handle.resume_settling();
#endif
}

struct Replay
{
    Handle handle_axis1{587}, handle_axis6{582};
    Handle* axis1_input_handle = &handle_axis1;
    Handle* axis6_input_handle = &handle_axis6;
    HandleFilterState axis1_handle_filter, axis6_handle_filter;
    ControlConfig cfg;
    CrawlState axis1_crawl, axis6_crawl;
    GuidewireMode guidewire_mode = GuidewireMode::None;
    bool single_handle_mode = false, ads_soft_hold_active = false, handle_soft_hold_active = false;
    bool initial_sync_done = true, connection_hold_active_prev = false;
    bool axis1_fast_return = false, axis6_fast_retract = false;
    PlannedReturnCoordinator planned_return;
    bool control_active = true, return_ads_fault_hold = false, estop_hold_active = false;
    struct { bool active() const { return false; } } spacing_recovery;
    bool ads_motion_cycle_valid = true;
    ForceFeedbackState ff;
    ForceSampleFrame force_sample;
    ForceCalibrationConfig cal_cfg;
    ForceCalibrationState cal_state;
    double plc_act_pos[7] = {20, 30, 600, 0, 610, 625, 40}, plc_init_pos[7] = {}, plc_leftlimit[7] = {};
    double pos[7] = {}, axis2_hold_rel = 30, axis7_hold_rel = 40;
    double axis1_prev_linear_filtered = 0, axis6_prev_linear_filtered = 0;
    double axis1_prev_rot_filtered = 0, axis6_prev_rot_filtered = 0;
    double axis1_follow_cmd_abs = 20, axis6_follow_cmd_abs = 625;
    double axis3_base_rel = 600, axis5_base_rel = 610, axis6_mirror_base_rel = 625;
    double axis1_prev_abs_for_trigger = 20, axis6_prev_abs_for_trigger = 625;
    bool axis1_prev_abs_valid = true, axis6_prev_abs_valid = true, axis1_reverse_switch_guard_active = false;
    std::string recovery_event_text;
    AppContext ctx;
    std::array<double, 4> command{20, 30, 625, 40};
    int rebases = 0;

    Replay()
    {
        remote_handle_bridge::clear_samples(); remote_handle_bridge::clear_outputs();
        ctx.cfg = &cfg; ctx.pos = pos; ctx.plc_act_pos = plc_act_pos;
        ctx.plc_init_pos = plc_init_pos; ctx.plc_leftlimit = plc_leftlimit;
        ctx.axis1_input_handle = axis1_input_handle; ctx.axis6_input_handle = axis6_input_handle;
        ctx.axis1_handle_filter = &axis1_handle_filter; ctx.axis6_handle_filter = &axis6_handle_filter;
        ctx.axis1_crawl = &axis1_crawl; ctx.axis6_crawl = &axis6_crawl;
        ctx.axis2_hold_rel = &axis2_hold_rel; ctx.axis7_hold_rel = &axis7_hold_rel;
        ctx.axis1_prev_linear_filtered = &axis1_prev_linear_filtered;
        ctx.axis6_prev_linear_filtered = &axis6_prev_linear_filtered;
        ctx.axis1_prev_rot_filtered = &axis1_prev_rot_filtered; ctx.axis6_prev_rot_filtered = &axis6_prev_rot_filtered;
        ctx.axis1_follow_cmd_abs = &axis1_follow_cmd_abs; ctx.axis6_follow_cmd_abs = &axis6_follow_cmd_abs;
        ctx.axis1_reverse_switch_guard_active = &axis1_reverse_switch_guard_active;
        ctx.axis1_prev_abs_for_trigger = &axis1_prev_abs_for_trigger; ctx.axis6_prev_abs_for_trigger = &axis6_prev_abs_for_trigger;
        ctx.axis1_prev_abs_valid = &axis1_prev_abs_valid; ctx.axis6_prev_abs_valid = &axis6_prev_abs_valid;
        ctx.axis3_base_rel = &axis3_base_rel; ctx.axis5_base_rel = &axis5_base_rel;
        ctx.axis6_mirror_base_rel = &axis6_mirror_base_rel;
        ff.set_enabled(1, true); ff.set_enabled(2, true);
        cal_state.zeroed = true; force_sample.valid = true;
        force_sample.fn_1_value_v = force_sample.fn_2_value_v = 1;
        sample(0, 0, 1); handle_axis1.init(); handle_axis6.init();
        handle_axis1.poll(); handle_axis6.poll();
        axis1_handle_filter.reset(0, 0); axis6_handle_filter.reset(0, 0);
        check(motion_sync::rebase_dual_after_return(ctx), "initial production baseline");
    }
    void sample(float linear, float rotation, std::uint64_t epoch, int only_slot = -1)
    {
        remote_handle_bridge::Sample s;
        s.valid = true; s.joints[0] = linear; s.joints[1] = rotation;
        s.vels[0] = 17; s.vels[1] = -23;
#ifndef BASELINE
        s.device_epoch = epoch;
#else
        (void)epoch;
#endif
        for (int slot = 0; slot < 2; ++slot) {
            auto value = s;
            if (only_slot >= 0 && slot != only_slot) {
                value.joints[0] = value.joints[1] = 0;
#ifndef BASELINE
                value.device_epoch = 1;
#endif
            }
            remote_handle_bridge::put_sample(slot, value);
        }
    }
    void load_pos_from_actual() { plc_io::load_pos_from_actual(ctx); }
    void cycle()
    {
        const bool handle1_poll_ok = handle_axis1.poll(), handle6_poll_ok = handle_axis6.poll();
#include "main_soft_hold.inc"
        const bool handle1_rebase_requested = handle_axis1.take_rebase_request();
        const bool handle6_rebase_requested = handle_axis6.take_rebase_request();
        const bool handle_rebase_requested = handle1_rebase_requested || handle6_rebase_requested;
        const bool rebuild = !handle_soft_hold_active && (ads_soft_hold_active ||
            handle_rebase_requested || connection_hold_active_prev);
#include "main_rebase.inc"
        if (rebuild) ++rebases;
        connection_hold_active_prev = handle_soft_hold_active || ads_soft_hold_active;
#include "main_filter.inc"
        if (!handle_soft_hold_active && !ads_soft_hold_active) {
            double probe[4] = {};
#include "main_mapping.inc"
            // 探针只累计生产表达式算出的线性增量；窗口/回退约束另由原有完整回放覆盖。
            command[0] += probe[0]; command[2] += probe[2];
            command[1] = probe[1]; command[3] = probe[3];
        }
        force_cycle();
        axis1_prev_linear_filtered = axis1_handle_filter.axis0_filtered;
        axis6_prev_linear_filtered = axis6_handle_filter.axis0_filtered;
        axis1_prev_rot_filtered = axis1_handle_filter.axis1_filtered;
        axis6_prev_rot_filtered = axis6_handle_filter.axis1_filtered;
    }
    void force_cycle()
    {
#include "main_force.inc"
    }
};

int main()
{
    std::ofstream csv("four_axes.csv"); csv << "scenario,elapsed_ms,axis1,axis2,axis6,axis7,settling,rebases\n";
    for (int slot : {0, 1})
    for (const std::string scenario : {"reopen_2mm_10deg", "gap1600", "epoch50_zero_shift", "epoch_wrap"}) {
        clock_ms += 10000; Replay replay;
        const auto reference = replay.command;
        if (scenario == "reopen_2mm_10deg") {
            (slot == 0 ? replay.handle_axis6 : replay.handle_axis1).close();
            replay.connection_hold_active_prev = true;
            clock_ms += 50; replay.sample(0, 0, 1, slot);
            replay.handle_axis1.init(); replay.handle_axis6.init();
        } else clock_ms += scenario == "gap1600" ? 1600 : 50;
        const auto began = clock_ms;
        double max_during = 0;
        for (int elapsed = 0; elapsed <= 1000; elapsed += 10) {
            clock_ms = began + elapsed;
            const bool epoch = scenario.find("epoch") == 0;
            const float fraction = static_cast<float>((std::min)(elapsed, 400)) / 400.0f;
            const float linear = epoch ? 0.2f : fraction * static_cast<float>(2.0 / replay.cfg.k_handle_to_mm);
            const float rotation = scenario == "epoch_wrap" ? static_cast<float>(-2 * 3.141592653589793) :
                (epoch ? 1.4f : fraction * static_cast<float>(10.0 / Rad));
            replay.sample(linear, rotation, epoch ? 2 : 1, slot); replay.cycle();
            csv << scenario << "_SN" << (slot == 0 ? 582 : 587) << ',' << elapsed;
            for (double value : replay.command) csv << ',' << value;
            csv << ',' << settling(replay.handle_axis1) << ',' << replay.rebases << '\n';
            if (elapsed < 500) {
                for (int i = 0; i < 4; ++i) max_during = (std::max)(max_during, std::abs(replay.command[i] - reference[i]));
#ifndef BASELINE
                check(replay.command == reference, "axes1/2/6/7 commands exactly unchanged during500ms");
                const auto& recovering = slot == 0 ? replay.handle_axis6 : replay.handle_axis1;
                check(recovering.fVels2[0] == 0 && recovering.fVels2[1] == 0, "recovering handle velocities zero");
                check(replay.ff.force_582_f == 0 && replay.ff.force_587_f == 0 &&
                    replay.ff.force_582_n == 0 && replay.ff.force_587_n == 0 &&
                    remote_handle_bridge::get_output(0).force == 0 && remote_handle_bridge::get_output(1).force == 0,
                    "production force call cannot reapply force while settling");
#endif
            }
#ifndef BASELINE
            if (elapsed >= 500) check(replay.command == reference, "newest baseline has no residual convergence jump");
            if (elapsed == 500) {
                check(replay.rebases == 1 && !settling(replay.handle_axis1), "exactly one rebase at500ms");
                check(replay.axis1_crawl.handle_ref == (slot == 1 ? static_cast<double>(linear) : 0) &&
                    replay.axis6_crawl.rot_ref == (slot == 0 ? static_cast<double>(rotation) : 0),
                    "completion sample supplies linear and rotational baselines");
                check(replay.axis1_crawl.base_rel == reference[0] && replay.axis6_crawl.base_rel == reference[2],
                    "axes1/6 base_rel rebuilt from current actual position");
                const auto& recovering = slot == 0 ? replay.handle_axis6 : replay.handle_axis1;
                check(recovering.fVels2[0] == 0 && recovering.fVels2[1] == 0, "completion poll zeros velocity");
            }
#endif
        }
#ifdef BASELINE
        check(max_during > 0.1, "old production path reproduces premature follow jump");
        std::cout << "PASS H old_jump scenario=" << scenario << " SN=" << (slot == 0 ? 582 : 587)
            << " max_command_delta=" << max_during << '\n';
#else
        check(replay.rebases == 1, "no repeated rebase after stable recovery");
        const auto before_follow = replay.command;
        clock_ms += 10;
        const bool epoch = scenario.find("epoch") == 0;
        const float linear = epoch ? .2f : static_cast<float>(2.0 / replay.cfg.k_handle_to_mm);
        const float rotation = scenario == "epoch_wrap" ? static_cast<float>(-2 * 3.141592653589793) :
            (epoch ? 1.4f : static_cast<float>(10.0 / Rad));
        replay.sample(linear + .0002f, rotation + .001f, epoch ? 2 : 1, slot); replay.cycle();
        const int linear_axis = slot == 0 ? 2 : 0, rotation_axis = slot == 0 ? 3 : 1;
        check(replay.command[linear_axis] < before_follow[linear_axis] &&
            before_follow[linear_axis] - replay.command[linear_axis] < .1 &&
            replay.command[rotation_axis] < before_follow[rotation_axis] &&
            before_follow[rotation_axis] - replay.command[rotation_axis] < .1, "new motion follows continuously after recovery");
        std::cout << "PASS H scenario=" << scenario << " SN=" << (slot == 0 ? 582 : 587)
            << " held_ms=500 max_delta=" << max_during << " rebases=1 follow_resumed=1\n";
#endif
    }
#ifndef BASELINE
    for (bool invalid_frame : {false, true}) {
        clock_ms += 10000; Replay replay;
        clock_ms += 50; const auto began = clock_ms;
        for (int elapsed = 0; elapsed <= 300; elapsed += 10) {
            clock_ms = began + elapsed; replay.sample(.1f, .3f, 2); replay.cycle();
        }
        if (invalid_frame) {
            const auto cached_rx = remote_handle_bridge::detail::state().samples[0].rx_ms;
            clock_ms += 10; remote_handle_bridge::put_sample(0, {}); remote_handle_bridge::put_sample(1, {});
            check(remote_handle_bridge::detail::state().samples[0].rx_ms == cached_rx, "invalid frame preserves cached valid timestamp");
            replay.cycle(); clock_ms += 10;
        } else {
            clock_ms += 251; replay.cycle();
            check(settling(replay.handle_axis1) && replay.rebases == 0, "cached sample cannot finish500ms");
            clock_ms += 10;
        }
        const auto restarted = clock_ms;
        for (int elapsed = 0; elapsed <= 500; elapsed += 10) {
            clock_ms = restarted + elapsed; replay.sample(.1f, .3f, 2); replay.cycle();
            check(replay.rebases == (elapsed == 500 ? 1 : 0), "new interruption restarts full500ms timer");
        }
        std::cout << "PASS H continuity_reset=" << (invalid_frame ? "invalid_frame" : "251ms_gap") << " timer_ms=500\n";
    }
#endif
    clock_ms += 10000; Replay force;
    force.handle_soft_hold_active = true; force.force_cycle();
#ifdef BASELINE
    check(force.ff.force_582_f != 0 && force.ff.force_587_f != 0, "old loop reapplies force after handle hold");
    std::cout << "PASS H old_force_reapplied wire=" << force.ff.force_582_f << " catheter=" << force.ff.force_587_f << '\n';
#else
    check(force.ff.force_582_f == 0 && force.ff.force_587_f == 0 && force.ff.force_582_n == 0 && force.ff.force_587_n == 0,
        "force and torque remain zero for existing connection hold");
    std::cout << "PASS H force_hold_gate all_outputs_zero=1\n";
#endif
}
