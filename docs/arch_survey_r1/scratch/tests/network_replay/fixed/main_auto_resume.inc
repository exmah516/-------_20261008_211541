		if (!control_active && !return_ads_fault_hold && !motion_startup_active &&
			!planned_return.active() && !spacing_recovery.active() &&
			ads_motion_cycle_valid && initial_sync_done && !connection_hold_active &&
			!handle_soft_hold_active && self_check_done &&
			!estop_hold_active && !ads_soft_hold_active && startup.completed)
		{
			if (sync_all(20))
			{
				control_active = true;
			}
		}
