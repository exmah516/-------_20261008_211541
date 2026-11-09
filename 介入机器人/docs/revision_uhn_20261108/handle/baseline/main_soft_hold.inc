		handle_soft_hold_active = single_handle_mode
			? (!handle1_poll_ok && !handle6_poll_ok)
			: (!handle1_poll_ok || !handle6_poll_ok);

