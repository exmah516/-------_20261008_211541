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
	std::int64_t now_ms,
	const ControlConfig& cfg,
	const ForceCalibrationConfig& cal_cfg,
	const ForceCalibrationState& cal_state)
{
	ForceOutputCmd out_cmd;
	constexpr std::int64_t blend_duration_ms = 300;
	const bool output_enabled =
		ff.enabled() && control_active && !estop_hold_active &&
		cal_state.zeroed && sample.valid;

	if (output_enabled)
	{
		const CalibratedForce pair_1 = calibrate_force(
			sample.fn_1_value_v, sample.ft_1_value_v, sample.axis2_pos_rel,
			cal_cfg, cal_state);
		const CalibratedForce pair_2 = calibrate_guidewire_force(
			sample.fn_2_value_v, sample.ft_2_value_v, sample.axis2_pos_rel,
			cal_cfg, cal_state);
		// 反馈与运动控制对象一致：导管fn1/ft1到587，导丝fn2/ft2到582。
		const double theory_582_n = pair_2.t_feedback_nm * cfg.force_feedback_582_torque_scale;
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
		const double filtered_582_f = apply_force_filter(
			pair_2.f_feedback_n, ff.force_582_filtered_f, ff.force_582_filter_initialized)
			* cfg.force_feedback_582_force_scale;
		const double filtered_587_f = apply_force_filter(
			pair_1.f_feedback_n, ff.force_587_filtered_f, ff.force_587_filter_initialized)
			* cfg.force_feedback_587_force_scale;
		// 慢速平滑仅用于换手锁存，不增加实时力和扭矩的输出延迟。
		const auto smooth_hold_sample = [&](ForceFeedbackState::HoldSample& smooth, double force, double torque)
		{
			const double alpha = smooth.initialized
				? 1.0 - std::exp(-static_cast<double>(now_ms - smooth.tick_ms) / 100.0) : 1.0;
			smooth.force += alpha * (force - smooth.force);
			smooth.torque += alpha * (torque - smooth.torque);
			smooth.tick_ms = now_ms;
			smooth.initialized = true;
		};
		smooth_hold_sample(ff.smooth_582, filtered_582_f, theory_582_n);
		smooth_hold_sample(ff.smooth_587, filtered_587_f, theory_587_n);
		ff.force_582_theory_f = filtered_582_f;
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
		apply_handoff_hold(ff.hold_582, filtered_582_f, theory_582_n,
			out_cmd.force_582_f, out_cmd.force_582_n);
		apply_handoff_hold(ff.hold_587, filtered_587_f, theory_587_n,
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
