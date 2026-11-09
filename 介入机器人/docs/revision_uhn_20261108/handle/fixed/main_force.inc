		process_force_feedback(
			ff,
			force_sample,
			handle_axis6,
			handle_axis1,
			control_active && !spacing_recovery.active() &&
				!ads_soft_hold_active && !handle_soft_hold_active && !return_ads_fault_hold,
			estop_hold_active,
			plc_act_pos[0] + plc_init_pos[0],
			plc_act_pos[5] + plc_init_pos[5],
			ads_motion_cycle_valid,
			GetTickCount64(),
			cfg,
			cal_cfg,
			cal_state);
