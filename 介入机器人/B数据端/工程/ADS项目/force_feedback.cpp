#include "force_feedback.h"

void process_force_feedback(
	ForceFeedbackState& ff,
	const ForceSampleFrame& sample,
	Handle& handle_582,
	Handle& handle_587,
	bool control_active,
	bool estop_hold_active,
	bool axis1_fast_return,
	bool axis6_fast_retract,
	bool clamp_hold_582_trigger,
	bool clamp_hold_587_trigger,
	ULONGLONG now_ms,
	const ControlConfig& cfg,
	const ForceCalibrationConfig& cal_cfg,
	const ForceCalibrationState& cal_state)
{
	ForceOutputCmd out_cmd;
	const bool fast_move_active = axis1_fast_return || axis6_fast_retract;
	const bool output_enabled =
		ff.enabled && control_active && !estop_hold_active &&
		cal_state.zeroed && sample.valid;

	if (output_enabled)
	{
		const CalibratedForce pair_1 = calibrate_force(
			sample.fn_1_value_v, sample.ft_1_value_v, sample.axis2_pos_rel,
			cal_cfg, cal_state);
		const CalibratedForce pair_2 = calibrate_guidewire_force(
			sample.fn_2_value_v, sample.ft_2_value_v, sample.axis2_pos_rel,
			cal_cfg, cal_state);
		const double scale = cfg.force_feedback_scale;
		const double theory_582_f = pair_1.f_feedback_n * scale;
		const double theory_582_n = pair_1.t_feedback_nm * scale;
		const double theory_587_f = pair_2.f_feedback_n * scale;
		const double theory_587_n = pair_2.t_feedback_nm * scale;
		ff.force_582_theory_f = theory_582_f;
		ff.force_582_theory_n = theory_582_n;

		if (fast_move_active)
		{
			if (!ff.freeze_582_active)
			{
				ff.freeze_582_f = theory_582_f;
				ff.freeze_582_n = theory_582_n;
				ff.freeze_582_active = true;
			}
			out_cmd.force_582_f = ff.freeze_582_f;
			out_cmd.force_582_n = ff.freeze_582_n;
			if (!ff.freeze_587_active)
			{
				ff.freeze_587_f = theory_587_f;
				ff.freeze_587_n = theory_587_n;
				ff.freeze_587_active = true;
			}
			out_cmd.force_587_f = ff.freeze_587_f;
			out_cmd.force_587_n = ff.freeze_587_n;
		}
		else
		{
			ff.freeze_582_active = false;
			ff.freeze_587_active = false;
			out_cmd.force_582_f = theory_582_f;
			out_cmd.force_582_n = theory_582_n;
			out_cmd.force_587_f = theory_587_f;
			out_cmd.force_587_n = theory_587_n;
		}

		if (ff.clamp_hold_enabled && clamp_hold_582_trigger)
		{
			ff.clamp_hold_582_f = out_cmd.force_582_f;
			ff.clamp_hold_582_n = out_cmd.force_582_n;
			ff.clamp_hold_582_until_ms = now_ms + 200;
			ff.clamp_hold_582_active = true;
		}
		if (ff.clamp_hold_enabled && ff.clamp_hold_582_active)
		{
			if (now_ms < ff.clamp_hold_582_until_ms)
			{
				out_cmd.force_582_f = ff.clamp_hold_582_f;
				out_cmd.force_582_n = ff.clamp_hold_582_n;
			}
			else ff.clamp_hold_582_active = false;
		}
		if (ff.clamp_hold_enabled && clamp_hold_587_trigger)
		{
			ff.clamp_hold_587_f = out_cmd.force_587_f;
			ff.clamp_hold_587_n = out_cmd.force_587_n;
			ff.clamp_hold_587_until_ms = now_ms + 200;
			ff.clamp_hold_587_active = true;
		}
		if (ff.clamp_hold_enabled && ff.clamp_hold_587_active)
		{
			if (now_ms < ff.clamp_hold_587_until_ms)
			{
				out_cmd.force_587_f = ff.clamp_hold_587_f;
				out_cmd.force_587_n = ff.clamp_hold_587_n;
			}
			else ff.clamp_hold_587_active = false;
		}
	}
	else
	{
		ff.clear_clamp_holds();
		ff.freeze_582_active = false;
		ff.freeze_587_active = false;
		ff.force_582_theory_f = 0.0;
		ff.force_582_theory_n = 0.0;
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
