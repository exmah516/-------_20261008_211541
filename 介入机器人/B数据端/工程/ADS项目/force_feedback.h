#pragma once

#include "control_types.h"
#include "force_calibration.h"

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
	const ForceCalibrationState& cal_state);
