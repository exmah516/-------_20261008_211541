#include "force_feedback.h"

#include <cmath>

void process_force_feedback(
	ForceFeedbackState& ff,
	const ForceSampleFrame& sample,
	Handle& handle_582,
	Handle& handle_587,
	bool control_active,
	bool estop_hold_active,
	double axis1_abs,
	double axis6_abs,
	bool position_valid,
	ULONGLONG now_ms,
	const ControlConfig& cfg,
	const ForceCalibrationConfig& cal_cfg,
	const ForceCalibrationState& cal_state)
{
	ForceOutputCmd out_cmd;
	constexpr ULONGLONG blend_duration_ms = 100;
	const bool output_enabled =
		ff.enabled() && control_active && !estop_hold_active &&
		cal_state.zeroed && sample.valid;

	if (output_enabled)
	{
		const bool filtered_feedback_enabled = ff.clamp_hold_enabled;
		if (!filtered_feedback_enabled) ff.reset_force_filters();
		const CalibratedForce pair_1 = calibrate_force(
			sample.fn_1_value_v, sample.ft_1_value_v, sample.axis2_pos_rel,
			cal_cfg, cal_state);
		const CalibratedForce pair_2 = calibrate_guidewire_force(
			sample.fn_2_value_v, sample.ft_2_value_v, sample.axis2_pos_rel,
			cal_cfg, cal_state);
		// 反馈与运动控制对象一致：导管fn1/ft1到587，导丝fn2/ft2到582。
		const double theory_582_f = pair_2.f_feedback_n * cfg.force_feedback_582_force_scale;
		const double theory_582_n = pair_2.t_feedback_nm * cfg.force_feedback_582_torque_scale;
		const double theory_587_f = pair_1.f_feedback_n * cfg.force_feedback_587_force_scale;
		const double theory_587_n = pair_1.t_feedback_nm * cfg.force_feedback_587_torque_scale;
		const double filter_alpha =
			std::isfinite(cfg.force_feedback_force_filter_alpha) &&
			cfg.force_feedback_force_filter_alpha > 0.0 &&
			cfg.force_feedback_force_filter_alpha <= 1.0
			? cfg.force_feedback_force_filter_alpha
			: 1.0;
		const auto apply_force_filter = [&](double force, double& filtered, bool& initialized)
		{
			if (!initialized)
			{
				filtered = force;
				initialized = true;
			}
			else
			{
				filtered += filter_alpha * (force - filtered);
			}
			return filtered;
		};
		const double filtered_582_f = filtered_feedback_enabled
			? apply_force_filter(theory_582_f, ff.force_582_filtered_f, ff.force_582_filter_initialized)
			: theory_582_f;
		const double filtered_587_f = filtered_feedback_enabled
			? apply_force_filter(theory_587_f, ff.force_587_filtered_f, ff.force_587_filter_initialized)
			: theory_587_f;
		const auto apply_damping = [&](double force, double velocity)
		{
			const double safe_velocity = std::isfinite(velocity) ? velocity : 0.0;
			return force - cfg.force_feedback_damping_n_per_velocity * safe_velocity;
		};
		const double display_582_f = filtered_feedback_enabled
			? apply_damping(filtered_582_f, handle_582.fVels2[0])
			: theory_582_f;
		const double display_587_f = filtered_feedback_enabled
			? apply_damping(filtered_587_f, handle_587.fVels2[0])
			: theory_587_f;
		ff.force_582_theory_f = display_582_f;
		ff.force_582_theory_n = theory_582_n;
		ff.observe_clamp_hold_motion(2, axis6_abs, position_valid, now_ms);
		ff.observe_clamp_hold_motion(1, axis1_abs, position_valid, now_ms);

		auto apply_handoff_hold = [&](ForceFeedbackState::HandoffHold& hold,
			double force, double torque, double& output_force, double& output_torque)
		{
			output_force = force;
			output_torque = torque;
			if (!ff.clamp_hold_enabled || !hold.active) return;

			double blend = 0.0;
			if (hold.blending)
			{
				blend = (std::min)(1.0, static_cast<double>(now_ms - hold.blend_start_ms) / blend_duration_ms);
			}
			output_force = hold.force + (force - hold.force) * blend;
			output_torque = hold.torque + (torque - hold.torque) * blend;
			if (blend >= 1.0) hold = ForceFeedbackState::HandoffHold{};
		};
		apply_handoff_hold(ff.hold_582, display_582_f, theory_582_n,
			out_cmd.force_582_f, out_cmd.force_582_n);
		apply_handoff_hold(ff.hold_587, display_587_f, theory_587_n,
			out_cmd.force_587_f, out_cmd.force_587_n);
	}
	else if (!estop_hold_active && control_active && cal_state.zeroed)
	{
		ff.reset_force_filters();
		auto retain_handoff_hold = [&](ForceFeedbackState::HandoffHold& hold,
			double& output_force, double& output_torque)
		{
			if (!ff.clamp_hold_enabled || !hold.active) return;
			if (hold.blending) hold.blend_start_ms = now_ms;
			output_force = hold.force;
			output_torque = hold.torque;
		};
		retain_handoff_hold(ff.hold_582, out_cmd.force_582_f, out_cmd.force_582_n);
		retain_handoff_hold(ff.hold_587, out_cmd.force_587_f, out_cmd.force_587_n);
	}
	else
	{
		ff.clear_clamp_holds();
		ff.reset_force_filters();
		ff.force_582_theory_f = 0.0;
		ff.force_582_theory_n = 0.0;
	}

	// 关闭一侧时立即清零并撤销该侧锁存，不影响另一侧。
	if (!ff.enabled_582)
	{
		out_cmd.force_582_f = out_cmd.force_582_n = 0.0;
		ff.set_enabled(2, false);
	}
	if (!ff.enabled_587)
	{
		out_cmd.force_587_f = out_cmd.force_587_n = 0.0;
		ff.set_enabled(1, false);
	}

	// 582/587 是物理序列号，始终各发各的输出，不再按模式或手柄数量切换。
	handle_582.setforce_axis(out_cmd.force_582_f, cfg.axial_force_axis, out_cmd.force_582_n);
	handle_587.setforce_axis(out_cmd.force_587_f, cfg.axial_force_axis, out_cmd.force_587_n);

	ff.force_582_f = out_cmd.force_582_f;
	ff.force_582_n = out_cmd.force_582_n;
	ff.force_587_f = out_cmd.force_587_f;
	ff.force_587_n = out_cmd.force_587_n;

	if (sample.valid)
	{
		ff.last_fn_1_raw = sample.fn_1_value;
		ff.last_ft_1_raw = sample.ft_1_value;
		ff.last_fn_2_raw = sample.fn_2_value;
		ff.last_ft_2_raw = sample.ft_2_value;
	}
}
