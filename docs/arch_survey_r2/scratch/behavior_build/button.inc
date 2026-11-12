		const bool spacing_recovery_button_pressed =
			(physical_587_buttons & cfg.spacing_recovery_button_mask) == cfg.spacing_recovery_button_mask;
		const bool spacing_recovery_button_press_edge =
			spacing_recovery_button_pressed && !spacing_recovery_button_pressed_prev;
		const bool spacing_recovery_button_release_edge =
			!spacing_recovery_button_pressed && spacing_recovery_button_pressed_prev;
