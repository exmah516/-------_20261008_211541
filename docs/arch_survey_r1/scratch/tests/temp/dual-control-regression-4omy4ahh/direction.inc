		// 换手/屈曲恢复期间冻结方向；结束后按两只B6的最新电平统一决定。
		if (!single_handle_mode && !planned_return.active() &&
			!spacing_recovery.active() && !spacing_recovery.requested && !spacing_recovery_button_pressed)
		{
			dual_reverse_active = catheter_b6_pressed || guidewire_b6_pressed;
		}
		if (!single_handle_mode)
		{
			axis1_crawl.start_abs = axis1_window_left_abs();
			axis1_crawl.end_abs = axis1_window_right_abs();
		}
		bool axis1_reverse_pressed = !single_handle_mode ? dual_reverse_active
			: (vis_reverse_override_active ? vis_reverse_override_value : catheter_b6_pressed);
		GuidewireMode requested_guidewire_mode = !single_handle_mode ? GuidewireMode::None
			: (vis_reverse_override_active
				? (vis_reverse_override_target == 0 ? GuidewireMode::None : GuidewireMode::Independent)
				: single_handle_requested_mode);
		if (spacing_recovery.active())
		{
			requested_guidewire_mode = GuidewireMode::None;
			(void)cancel_active_return_motion(true);
		}
		bool axis6_effective_reverse_pressed =
			(single_handle_mode && guidewire_mode == GuidewireMode::Independent && !vis_reverse_override_active)
			? guidewire_b6_pressed : axis1_reverse_pressed;
