#include "../handle_net_distance.h"
#include <cassert>
#include <iostream>
#include <limits>

// 编译 main.cpp 中的真实统计门控，测试替身仅提供状态与位置。
static void test_main_gates()
{
    bool control_active = true, initial_sync_done = true, connection_hold_active = false;
    bool ads_soft_hold_active = false, handle_soft_hold_active = false, return_ads_fault_hold = false;
    bool estop_hold_active = false, emergency_retract_active = false, axis6_soft_limit_hold = false;
    bool tracking_manual_cylinder_override = false, single_handle_mode = false;
    bool axis4_forward_request = false, axis4_reverse_request = false;
    bool has_new_ads_snapshot = true, ads_motion_cycle_valid = true;
    struct State {
        bool running = false, requested = false;
        bool active() const { return running; }
        bool is_active() const { return running; }
    } startup, spacing_recovery, planned_return;
    enum class GuidewireMode { None, Independent };
    auto guidewire_mode = GuidewireMode::None;
    struct { double act_pos_rel[7] = {}, init_pos[7] = {}; std::uint64_t attempt_sequence = 0; } ads_snapshot;
    struct { bool motion_enabled = true, cylinder_valid = true; } ads_output;
    struct { int cyl1_open = 1, cyl2_clamp = 2, cyl3_open = 3, cyl4_clamp = 4; } cyl;
    int cylinder1_cmd = 1, cylinder2_cmd = 2, cylinder3_cmd = 3, cylinder4_cmd = 4;
    HandleNetDistance axis1_net_distance, axis6_net_distance;
    auto cycle = [&](double p1, double p6, bool trigger_return = false, bool finish_return = false) {
        ads_snapshot.act_pos_rel[0] = p1; ads_snapshot.act_pos_rel[5] = p6;
        ++ads_snapshot.attempt_sequence;
#include "distance_sample.inc"
        if (trigger_return) planned_return.running = true;
        if (finish_return) planned_return.running = false;
#include "distance_output.inc"
    };
    cycle(30, 50); cycle(25, 47);
    assert(axis1_net_distance.net_mm() == 5 && axis6_net_distance.net_mm() == 3);
    cycle(20, 45, true);
    cycle(35, 70); cycle(40, 80, false, true); cycle(39, 79);
    assert(axis1_net_distance.net_mm() == 11 && axis6_net_distance.net_mm() == 6);
    axis4_forward_request = true; cycle(38, 60);
    axis4_forward_request = false; cycle(37, 59); cycle(36, 58);
    assert(axis1_net_distance.net_mm() == 14 && axis6_net_distance.net_mm() == 7);
    for (bool* hold : { &connection_hold_active, &ads_soft_hold_active, &handle_soft_hold_active,
        &return_ads_fault_hold, &estop_hold_active, &emergency_retract_active, &axis6_soft_limit_hold,
        &tracking_manual_cylinder_override, &startup.running, &spacing_recovery.running,
        &spacing_recovery.requested })
    {
        const double before1 = axis1_net_distance.net_mm(), before6 = axis6_net_distance.net_mm();
        *hold = true; cycle(100, 100); *hold = false; cycle(36, 58);
        assert(axis1_net_distance.net_mm() == before1 && axis6_net_distance.net_mm() == before6);
    }
    single_handle_mode = true; cycle(35, 57); cycle(34, 50);
    assert(axis1_net_distance.net_mm() == 16 && axis6_net_distance.net_mm() == 7);
    guidewire_mode = GuidewireMode::Independent; cycle(10, 49); cycle(0, 48);
    assert(axis1_net_distance.net_mm() == 16 && axis6_net_distance.net_mm() == 8);
    std::cout << "PASS: production gates exclude full handoff, automatic modes, faults, axis4 jog and inactive axes\n";
}

int main()
{
    test_main_gates();
    HandleNetDistance axis1, axis6;
    axis1.sample(30, 100, 1, true, false);
    axis1.set_following(true);
    axis1.sample(25, 100, 2, true, true);
    assert(axis1.net_mm() == 5);
    axis1.sample(28, 100, 3, true, true);
    assert(axis1.net_mm() == 2);
    // 触发换手这拍仍是手柄的实际位移；其后的回位和交接全部排除。
    axis1.sample(20, 100, 4, true, true);
    axis1.set_following(false);
    axis1.sample(35, 100, 5, true, false);
    axis1.sample(40, 100, 6, true, false);
    axis1.set_following(true);
    axis1.sample(38, 100, 7, true, true);
    assert(axis1.net_mm() == 12);
    axis1.sample(38, 100, 7, true, true);
    assert(axis1.net_mm() == 12);
    axis1.sample(0, 0, 8, false, false);
    axis1.sample(5, 200, 9, true, true);
    assert(axis1.net_mm() == 12);
    axis1.sample(4, 200, 10, true, true);
    assert(axis1.net_mm() == 13);
    // 坐标重建不能形成虚假行程，NaN 不能污染累计值。
    axis1.sample(104, 100, 11, true, true);
    axis1.sample(std::numeric_limits<double>::quiet_NaN(), 100, 12, true, true);
    axis1.set_following(true);
    axis1.sample(103, 100, 13, true, true);
    assert(axis1.net_mm() == 13);
    // 轴6独立累计；撤出可以超过当前累计前进量。
    axis6.sample(30, 0, 1, true, true);
    axis6.sample(35, 0, 2, true, true);
    assert(axis6.net_mm() == -5 && axis1.net_mm() == 13);
    axis6.set_following(false);
    axis6.sample(20, 0, 3, true, false);
    axis6.set_following(true);
    axis6.sample(19, 0, 4, true, true);
    assert(axis6.net_mm() == -4);
    std::cout << "PASS: signed actual travel, handoff boundaries, invalid snapshots, rebasing, independent axes\n";
}
