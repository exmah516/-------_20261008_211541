enum class SpacingRecoveryPhase
{
	Idle,
	ClampWait,
	Active,
	ExitSync
};

struct SpacingRecoveryState
{
	bool requested = false;
	SpacingRecoveryPhase phase = SpacingRecoveryPhase::Idle;
	DWORD phase_t0 = 0;
	DWORD last_motion_tick = 0;
	double axis1_hold_rel = 0.0;
	double axis2_hold_rel = 0.0;
	double axis3_cmd_rel = 0.0;
	double axis5_cmd_rel = 0.0;
	double axis6_cmd_rel = 0.0;
	double axis7_hold_rel = 0.0;
	double pending_delta_mm = 0.0;
	double moved_mm = 0.0;
	double remaining_mm = 0.0;
	bool limit_logged = false;
	// 单手柄调试退出时使用；双手柄方向始终以当前B6电平为准。
	int restore_mode_selection = 1;
	bool restore_vis_reverse_override_active = false;
	bool restore_vis_reverse_override_value = false;
	int restore_vis_reverse_override_target = 0;

	bool active() const
	{
		return phase != SpacingRecoveryPhase::Idle;
	}

	void reset()
	{
		requested = false;
		phase = SpacingRecoveryPhase::Idle;
		phase_t0 = 0;
		last_motion_tick = 0;
		pending_delta_mm = 0.0;
		moved_mm = 0.0;
		remaining_mm = 0.0;
		limit_logged = false;
		restore_mode_selection = 1;
		restore_vis_reverse_override_active = false;
		restore_vis_reverse_override_value = false;
		restore_vis_reverse_override_target = 0;
	}
};

