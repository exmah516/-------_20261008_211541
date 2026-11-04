// 只链接生产记录器、标定与远程手柄内存桥，不连接 PLC 或物理手柄。
#include "../experiment_recorder.h"
#include "../force_feedback.h"
#include "../手柄/remote_handle_bridge.h"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

static std::vector<std::string> split(const std::string& line)
{
    std::vector<std::string> result;
    std::istringstream input(line);
    for (std::string cell; std::getline(input, cell, ',');) result.push_back(cell);
    return result;
}
static std::vector<std::vector<std::string>> csv(const std::filesystem::path& path)
{
    std::ifstream in(path);
    assert(in);
    std::vector<std::vector<std::string>> rows;
    for (std::string line; std::getline(in, line);) rows.push_back(split(line));
    return rows;
}
static void recording()
{
    namespace fs = std::filesystem;
    const auto original = fs::current_path();
    const auto test_directory = original / "recording-test";
    assert(fs::create_directory(test_directory));
    fs::current_path(test_directory);
    ExperimentRecorder experiment_recorder;
    const unsigned serial_axis1_handle = 587, serial_axis6_handle = 582;
    const bool single_handle_mode = false;
#include "recording_start.inc"
    auto& recorder = experiment_recorder;
    const auto info = recording_info;
    assert(recorder.is_recording());
    assert(!recorder.start("auto", info));
    assert(recorder.enqueue_event(ExperimentEventCode::ModeSwitch, 1));
    assert(recorder.enqueue_event(ExperimentEventCode::SpacingRecoveryStart, 1));
    for (unsigned i = 1; i <= 100; ++i)
    {
        ForceCsvRow force;
        force.ads_snapshot_sequence = i;
        force.source_qpc_ticks = recorder.clock().now_qpc();
        force.sample_valid = i != 20;
        force.calibrated_valid = i >= 30 && force.sample_valid;
        force.fn_1_raw_v = 1.25; force.ft_1_raw_v = 2.5;
        force.fn_2_raw_v = 3.75; force.ft_2_raw_v = 4.5;
        force.clean_force_n = 1.0; force.clean_handle_torque_nm = .002;
        force.clean_force_2_n = 2.0; force.clean_handle_torque_2_nm = .003;
        assert(recorder.enqueue_force(force));
        assert(!recorder.enqueue_force(force));
        if ((i & 1) == 0)
        {
            MotionCsvRow motion;
            motion.ads_snapshot_sequence = i; motion.source_qpc_ticks = force.source_qpc_ticks;
            motion.position_valid = i != 20;
            for (int axis = 0; axis < 7; ++axis) motion.axis_from_left_mm[axis] = 10 + axis;
            motion.axis1_handle_valid = i >= 30; motion.axis6_handle_valid = i >= 50;
            assert(recorder.enqueue_motion(motion));
        }
        Sleep(10);
    }
    recorder.stop_and_wait("test_complete");
    assert(!recorder.is_recording() && recorder.snapshot().error == ExperimentRecordingError::None);
    assert(recorder.snapshot().force_dropped == 0 && recorder.snapshot().motion_dropped == 0);
    fs::path session;
    for (const auto& file : fs::recursive_directory_iterator("records"))
        if (file.path().filename() == "session.json") session = file.path().parent_path();
    assert(!session.empty());
    const auto force = csv(session / "force.csv"), motion = csv(session / "motion.csv"), events = csv(session / "events.csv");
    assert(force.size() == 101 && force[0].size() == 16);
    assert(motion.size() == 51 && motion[0].size() == 12);
    assert(events.size() == 3 && events[0].size() == 4);
    assert(events[0][2] == "event_code" && events[0][3] == "value");
    assert(events[1][2] == "1" && events[1][3] == "1");
    assert(events[2][2] == "2" && events[2][3] == "1");
    assert(motion[0][10] == "axis1_handle_valid" && motion[0][11] == "axis6_handle_valid");
    for (const auto& row : motion) assert(row.size() == 12);
    assert(force[1][2] == "1" && force[1][3] == "0" && std::stod(force[1][4]) == 1.25 && std::isnan(std::stod(force[1][8])));
    assert(force[20][2] == "0" && std::isnan(std::stod(force[20][4])));
    assert(std::stod(force[30][8]) == 1.0 && std::stod(force[30][9]) == .002);
    assert(motion[10][2] == "0" && std::isnan(std::stod(motion[10][3])));
    assert(motion[1][10] == "0" && motion[1][11] == "0");
    assert(motion[15][10] == "1" && motion[15][11] == "0");
    assert(motion[25][10] == "1" && motion[25][11] == "1" && std::stod(motion[25][8]) == 15);
    std::ifstream metadata(session / "session.json");
    std::string json((std::istreambuf_iterator<char>(metadata)), {});
    assert(json.find("\"schema_version\": 7") != std::string::npos && json.find("test_complete") != std::string::npos);
    assert(recorder.start("auto", info)); recorder.stop_and_wait("second");
    int sessions = 0;
    for (const auto& file : fs::recursive_directory_iterator("records"))
        if (file.path().filename() == "session.json") ++sessions;
    assert(sessions == 2);
    assert(fs::create_directory("blocked"));
    fs::current_path("blocked");
    { std::ofstream file("records"); file << "not a directory"; }
    assert(!recorder.start("auto", info) && recorder.snapshot().error == ExperimentRecordingError::CreateDirectoryFailed);
    fs::current_path(original);
    std::cout << "PASS: recorder actual CSV writes, 100/50 samples, events, validity, NaN, drain, duplicate rejection, schema7, separate sessions, I/O failure.\n";
}
static bool close(double a, double b) { return std::abs(a-b) < 1e-9; }
static void handoff_holds()
{
    PlannedReturnCoordinator planned_return;
    struct { bool motion_enabled = true, axis1_fast_return = false, axis6_fast_retract = false; } ads_output;
    const std::uint64_t output_generation = 42;
    planned_return.leg_count = 2;
    planned_return.legs[0].axis_index = 0; planned_return.legs[1].axis_index = 5;
    auto tick = [&] {
#include "recording_handoff.inc"
        return planned_return.phase;
    };
    auto test = [&](bool catheter, bool wire) {
        planned_return.phase = PlannedReturnPhase::PublishHandoff;
        planned_return.legs[0].active = catheter; planned_return.legs[1].active = wire;
        const auto result = tick();
        assert(result == PlannedReturnPhase::AwaitHandoffApplied);
        assert(planned_return.handoff_generation == output_generation);
    };
    test(true, false); test(false, true); test(true, true);
    assert(tick() == PlannedReturnPhase::AwaitHandoffApplied);
    planned_return.phase = PlannedReturnPhase::PublishHandoff;
    ads_output.motion_enabled = false; assert(tick() == PlannedReturnPhase::PublishHandoff);
    ads_output.motion_enabled = true; ads_output.axis1_fast_return = true;
    assert(tick() == PlannedReturnPhase::PublishHandoff);
    ads_output.axis1_fast_return = false; ads_output.axis6_fast_retract = true;
    assert(tick() == PlannedReturnPhase::PublishHandoff);
    std::cout << "PASS: handoff publish remains one-shot and obeys existing motion/fast-return gates.\n";
}
static void calibration()
{
    ForceCalibrationConfig cal; ForceCalibrationState zero; zero.zeroed = true;
    zero.f_zero = 1.5; zero.ft_zero = -.75; zero.fn_2_zero = 2.25; zero.ft_2_zero = .5;
    bool passed = true;
    auto expect = [&](const char* name, double actual, double expected) {
        if (!close(actual, expected)) {
            std::cerr << name << ": actual=" << actual << ", expected=" << expected << '\n';
            passed = false;
        }
    };
    // 固定 ADS 计数差及外部标定结果，不能反推配置构造样本而掩盖错误系数。
    expect("fn1 N/V", cal.axial_direct_n_per_v, .319519429);
    expect("ft1 N/V", cal.tangential_direct_n_per_v, .703683250522);
    expect("fn2 N/V", cal.guidewire_axial_direct_n_per_v, 1.134444473);
    expect("ft2 N/V", cal.guidewire_tangential_direct_n_per_v, .133373639);
    expect("installed axial gain", cal.installed_axial_gain, 1.80);
    expect("installed axial offset", cal.installed_axial_offset_n, .183108);
    expect("installed torque gain", cal.installed_torque_nmm_per_n, 18.440851);
    expect("installed torque offset", cal.installed_torque_offset_nmm, .020906);
    const auto clean1 = calculate_clean_force(2.5, -.5, cal, zero); // +1000 / +250 counts
    const auto clean2 = calculate_clean_guidewire_force(2.05, -.3, cal, zero); // -200 / -800 counts
    expect("pair1 clean force N", clean1.force_n, .5751349722);
    expect("pair1 clean torque Nm", clean1.handle_torque_nm, .703683250522 * .25 * 18.440851 * .001);
    expect("pair2 clean force N", clean2.force_n, -.40840001028);
    expect("pair2 clean torque Nm", clean2.handle_torque_nm, .133373639 * -.8 * 18.440851 * .001);
    const auto feedback1 = calibrate_force(2.5, -.5, 0, cal, zero);
    const auto feedback2 = calibrate_guidewire_force(2.05, -.3, 0, cal, zero);
    expect("pair1 feedback force N", feedback1.f_feedback_n, .5751349722);
    expect("pair1 feedback torque Nm", feedback1.t_feedback_nm, .703683250522 * .25 * 18.440851 * .001);
    expect("pair2 feedback force N", feedback2.f_feedback_n, -.40840001028);
    expect("pair2 feedback torque Nm", feedback2.t_feedback_nm, .133373639 * -.8 * 18.440851 * .001);
    const auto at_zero1 = calculate_clean_force(1.5, -.75, cal, zero);
    const auto at_zero2 = calculate_clean_guidewire_force(2.25, .5, cal, zero);
    expect("zero1 force", at_zero1.force_n, 0); expect("zero1 torque", at_zero1.handle_torque_nm, 0);
    expect("zero2 force", at_zero2.force_n, 0); expect("zero2 torque", at_zero2.handle_torque_nm, 0);
    const double absolute_force = 1.80 * .000319519429 * 2500 + .183108;
    const double absolute_zero_force = 1.80 * .000319519429 * 1500 + .183108;
    expect("affine force zero cancellation", clean1.force_n, absolute_force - absolute_zero_force);
    const double absolute_torque_nmm = 18.440851 * .000703683250522 * -500 + .020906;
    const double absolute_zero_torque_nmm = 18.440851 * .000703683250522 * -750 + .020906;
    expect("affine torque zero cancellation", clean1.handle_torque_nm, (absolute_torque_nmm - absolute_zero_torque_nmm) * .001);
    const auto below_deadband = calibrate_force(1.501, -.74999, 0, cal, zero);
    expect("force deadband", below_deadband.f_feedback_n, 0); expect("torque deadband", below_deadband.t_feedback_nm, 0);
    const auto saturated = calibrate_force(101.5, -100.75, 0, cal, zero);
    expect("force limit", saturated.f_feedback_n, 5); expect("torque limit", saturated.t_feedback_nm, -.02);
    zero.zeroed = false;
    const auto invalid = calculate_clean_force(2.5, -.5, cal, zero);
    expect("unzeroed force", invalid.force_n, 0); expect("unzeroed torque", invalid.handle_torque_nm, 0);
    zero.zeroed = true;
    remote_handle_bridge::Sample bridge; bridge.valid = true;
    remote_handle_bridge::put_sample(0, bridge); remote_handle_bridge::put_sample(1, bridge);
    Handle wire(582), catheter(587); assert(wire.init() && catheter.init());
    ControlConfig cfg;
    cfg.force_feedback_582_force_scale = cfg.force_feedback_582_torque_scale = 1;
    cfg.force_feedback_587_force_scale = cfg.force_feedback_587_torque_scale = 1;
    cfg.force_feedback_force_filter_alpha = 1.0;
    ForceFeedbackState state; state.set_enabled(1, true); state.set_enabled(2, true);
    ForceSampleFrame sample; sample.valid = true;
    sample.fn_1_value_v = 2; sample.ft_1_value_v = -.5;
    sample.fn_2_value_v = zero.fn_2_zero; sample.ft_2_value_v = zero.ft_2_zero;
    auto tick = [&] { process_force_feedback(state, sample, wire, catheter, true, false,
        0, 0, true, 1, cfg, cal, zero); };
    tick();
    expect("catheter fn1 -> 587 force", state.force_587_f, .2875674861);
    expect("catheter ft1 -> 587 torque", state.force_587_n, .0032441294935179684);
    expect("catheter load must not affect 582 force", state.force_582_f, 0);
    expect("catheter load must not affect 582 torque", state.force_582_n, 0);
    sample.fn_1_value_v = zero.f_zero; sample.ft_1_value_v = zero.ft_zero;
    sample.fn_2_value_v = 2.05; sample.ft_2_value_v = -.3; tick();
    expect("guidewire fn2 -> 582 force", state.force_582_f, -.40840001028);
    expect("guidewire ft2 -> 582 torque", state.force_582_n, -.0019676187233014313);
    expect("guidewire load must not affect 587 force", state.force_587_f, 0);
    expect("guidewire load must not affect 587 torque", state.force_587_n, 0);
    expect("wire bridge force", remote_handle_bridge::get_output(0).force, -.40840001028);
    expect("catheter bridge force", remote_handle_bridge::get_output(1).force, 0);
    sample.fn_2_value_v = zero.fn_2_zero; sample.ft_2_value_v = zero.ft_2_zero;
    sample.ft_1_value_v = -.5; tick();
    expect("ft1 must not leak into catheter axial force", state.force_587_f, 0);
    expect("ft1 changes catheter torque", state.force_587_n, .0032441294935179684);
    wire.close(); catheter.close();
    assert(passed);
    std::cout << "PASS: fixed-count two-layer calibration, exact slopes, torque units, mounted zero, limits, catheter587/wire582 routing, no axial/torque cross-talk.\n";
}
static void feedback()
{
    remote_handle_bridge::Sample bridge; bridge.valid = true;
    remote_handle_bridge::put_sample(0, bridge); remote_handle_bridge::put_sample(1, bridge);
    Handle wire(582), catheter(587);
    assert(wire.init() && catheter.init());
    ControlConfig cfg; ForceCalibrationConfig cal; ForceCalibrationState zero; zero.zeroed = true;
    ForceFeedbackState state; ForceSampleFrame sample; sample.valid = true;
    sample.fn_1_value_v = 1.0 / (.319519429 * 1.80);
    sample.ft_1_value_v = .002 / (.703683250522 * 18.440851 * .001);
    sample.fn_2_value_v = 1.0 / (1.134444473 * 1.80);
    sample.ft_2_value_v = .002 / (.133373639 * 18.440851 * .001);
    assert(close(cfg.force_feedback_582_force_scale,.3) && close(cfg.force_feedback_582_torque_scale,.3));
    assert(close(cfg.force_feedback_587_force_scale,.3) && close(cfg.force_feedback_587_torque_scale,.3));
    assert(close(cfg.force_feedback_force_filter_alpha,.25));
    // 其余比例测试关闭低通，低通行为在下面单独验证。
    cfg.force_feedback_force_filter_alpha = 1.0;
    cfg.force_feedback_582_force_scale = .5; cfg.force_feedback_582_torque_scale = .5;
    cfg.force_feedback_587_force_scale = .5; cfg.force_feedback_587_torque_scale = .5;
    double axis1_abs = 100, axis6_abs = 200;
    bool position_valid = true;
    auto tick = [&](ULONGLONG now) {
        process_force_feedback(state, sample, wire, catheter, true, false,
            axis1_abs, axis6_abs, position_valid, now, cfg, cal, zero);
    };
    auto capture_hold = [&](int channel, bool reverse) {
        const bool guidewire = channel == 2;
        state.capture_clamp_hold(channel, reverse,
            guidewire ? state.force_582_f : state.force_587_f,
            guidewire ? state.force_582_n : state.force_587_n);
    };
    tick(1); assert(state.force_582_f == 0 && state.force_587_f == 0);
    state.set_enabled(1, true); tick(10);
    assert(state.force_582_f == 0 && close(state.force_587_f,.5) && close(state.force_587_n,.001));
    state.set_enabled(2, true); tick(20);
    assert(close(state.force_582_f,.5) && close(state.force_582_n,.001));
    assert(close(remote_handle_bridge::get_output(0).force,.5) && close(remote_handle_bridge::get_output(1).torque,.001));
	// 阻尼只作用于对应手柄的轴向力，正速度减力、负速度增力，扭矩和另一侧不变。
	cfg.force_feedback_damping_n_per_velocity = .1;
	wire.fVels2[0] = 2.0;
	catheter.fVels2[0] = -3.0;
	tick(25);
	assert(close(state.force_582_f,.3) && close(state.force_587_f,.8));
	assert(close(state.force_582_n,.001) && close(state.force_587_n,.001));
	wire.fVels2[0] = catheter.fVels2[0] = 0.0;
	cfg.force_feedback_damping_n_per_velocity = 0.0;
	tick(26);
	assert(close(state.force_582_f,.5) && close(state.force_587_f,.5));
	// 低通先于阻尼：5 倍单帧尖峰只让输出从 0.5 到 1.0，恢复后下一拍为 0.875。
	cfg.force_feedback_force_filter_alpha = .25;
	sample.fn_1_value_v *= 5; sample.fn_2_value_v *= 5;
	tick(27);
	assert(close(state.force_582_f,1.0) && close(state.force_587_f,1.0));
	sample.fn_1_value_v /= 5; sample.fn_2_value_v /= 5;
	tick(28);
	assert(close(state.force_582_f,.875) && close(state.force_587_f,.875));
	cfg.force_feedback_force_filter_alpha = 1.0;
	tick(29);
	assert(close(state.force_582_f,.5) && close(state.force_587_f,.5));
    // 四个比例逐项调整，检查另一手柄和另一输出分量不会被连带改变。
    cfg.force_feedback_582_torque_scale = .25; tick(30);
    assert(close(state.force_582_f,.5) && close(state.force_582_n,.0005));
    assert(close(state.force_587_f,.5) && close(state.force_587_n,.001));
    cfg.force_feedback_582_force_scale = .75; tick(40);
    assert(close(state.force_582_f,.75) && close(state.force_582_n,.0005));
    assert(close(state.force_587_f,.5) && close(state.force_587_n,.001));
    cfg.force_feedback_587_force_scale = .25; tick(50);
    assert(close(state.force_582_f,.75) && close(state.force_582_n,.0005));
    assert(close(state.force_587_f,.25) && close(state.force_587_n,.001));
    cfg.force_feedback_587_torque_scale = .75; tick(60);
    assert(close(state.force_582_f,.75) && close(state.force_582_n,.0005));
    assert(close(state.force_587_f,.25) && close(state.force_587_n,.0015));
    assert(close(remote_handle_bridge::get_output(0).force,.75) && close(remote_handle_bridge::get_output(0).torque,.0005));
    assert(close(remote_handle_bridge::get_output(1).force,.25) && close(remote_handle_bridge::get_output(1).torque,.0015));
    cfg.force_feedback_582_force_scale = 0; cfg.force_feedback_587_torque_scale = 0; tick(70);
    assert(state.force_582_f == 0 && close(state.force_582_n,.0005));
    assert(close(state.force_587_f,.25) && state.force_587_n == 0);
    cfg.force_feedback_582_force_scale = cfg.force_feedback_587_force_scale = .5;
    cfg.force_feedback_582_torque_scale = cfg.force_feedback_587_torque_scale = .5;
    tick(80);
    capture_hold(1, false);
    capture_hold(2, true);
    sample.fn_1_value_v *= 2; sample.fn_2_value_v *= 2;
    tick(100);
    assert(close(state.force_582_f,.5) && close(state.force_587_f,.5));
    axis1_abs = 90; axis6_abs = 210; tick(120);
    assert(close(state.force_582_f,.5) && close(state.force_587_f,.5));
    assert(!state.hold_582.handoff_complete && !state.hold_587.handoff_complete);
    axis1_abs = 100; axis6_abs = 200;
    state.complete_clamp_hold(1, axis1_abs);
    state.complete_clamp_hold(2, axis6_abs);
    tick(150); assert(close(state.force_582_f,.5) && close(state.force_587_f,.5));
    tick(5000); assert(close(state.force_582_f,.5) && close(state.force_587_f,.5));
    axis1_abs = 99; axis6_abs = 199; tick(5010);
    assert(close(state.force_582_f,.5) && close(state.force_587_f,.5));
    sample.valid = false; tick(5011);
    assert(close(state.force_582_f,.5) && close(state.force_587_f,.5));
    sample.valid = true;
    axis1_abs = 98; tick(5020);
    assert(close(state.force_587_f,.5) && state.hold_587.blending);
    assert(close(state.force_582_f,.5) && !state.hold_582.blending);
    tick(5070); assert(close(state.force_587_f,.75) && close(state.force_582_f,.5));
    tick(5120); assert(close(state.force_587_f,1) && !state.hold_587.active);
    axis6_abs = 202; tick(5130);
    assert(close(state.force_582_f,.5) && state.hold_582.blending);
    tick(5180); assert(close(state.force_582_f,.75));
    tick(5230); assert(close(state.force_582_f,1) && !state.hold_582.active);
    capture_hold(1, false); tick(6000);
    state.clamp_hold_enabled = false; state.clear_clamp_holds(); tick(6010);
    assert(close(state.force_582_f,1) && close(state.force_587_f,1));
    cfg.force_feedback_force_filter_alpha = .25;
    cfg.force_feedback_damping_n_per_velocity = .1;
    wire.fVels2[0] = 2.0; catheter.fVels2[0] = -3.0;
    sample.fn_1_value_v *= 2; sample.fn_2_value_v *= 2;
    tick(6020);
    assert(close(state.force_582_f,2) && close(state.force_587_f,2));
    assert(!state.force_582_filter_initialized && !state.force_587_filter_initialized);
    sample.fn_1_value_v /= 2; sample.fn_2_value_v /= 2;
    wire.fVels2[0] = catheter.fVels2[0] = 0.0;
    tick(6030);
    assert(close(state.force_582_f,1) && close(state.force_587_f,1));
    tick(7000);
    sample.fn_1_value_v *= 2; sample.fn_2_value_v *= 2;
    tick(7010); assert(close(state.force_582_f,2) && close(state.force_587_f,2));
    capture_hold(2, true); state.set_enabled(2, false); tick(7020);
    assert(state.force_582_f == 0 && !state.hold_582.active && close(state.force_587_f,2));
    state.set_enabled(2, true); tick(7030);
    assert(close(state.force_582_f,2) && close(state.force_587_f,2));
    state.set_enabled(1, false); tick(7040);
    assert(state.force_587_f == 0 && !state.hold_587.active && close(state.force_582_f,2));
    sample.valid = false; tick(7050);
    assert(state.force_582_f == 0 && state.force_587_f == 0 && !state.hold_582.active);
    sample.valid = true; zero.zeroed = false; tick(7060);
    assert(state.force_582_f == 0);
    std::cout << "PASS: independent feedback/scales, per-handle handoff hold, signed 2mm release, 100ms blend, disable/re-enable, invalid samples.\n";
}
int main() { calibration(); handoff_holds(); recording(); feedback(); }
