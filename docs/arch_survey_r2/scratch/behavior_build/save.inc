		if (spacing_recovery_button_press_edge)
		{
			// 恢复后优先还原已确认的模式覆盖值，不能用本拍 B6 电平推断；
			// 否则恢复组合键的采样边沿可能把递送误保存成回退。
			const bool restore_guidewire = vis_reverse_override_active
				? vis_reverse_override_target == 1
				: guidewire_mode == GuidewireMode::Independent;
			const bool restore_reverse = vis_reverse_override_active
				? vis_reverse_override_value
				: (restore_guidewire ? axis6_effective_reverse_pressed : axis1_reverse_pressed);
			spacing_recovery.restore_mode_selection = restore_guidewire
				? (restore_reverse ? static_cast<int>(ModeSelection::GuidewireRetraction)
					: static_cast<int>(ModeSelection::GuidewireDelivery))
				: (restore_reverse ? static_cast<int>(ModeSelection::CatheterRetraction)
					: static_cast<int>(ModeSelection::CatheterDelivery));
			spacing_recovery.restore_vis_reverse_override_active = vis_reverse_override_active;
			spacing_recovery.restore_vis_reverse_override_value = vis_reverse_override_value;
			spacing_recovery.restore_vis_reverse_override_target = vis_reverse_override_target;
			spacing_recovery.requested = true;
			std::cout << "587 屈曲恢复按键：按下，准备进入恢复模式。" << std::endl;
		}
		if (spacing_recovery_button_release_edge)
		{
			spacing_recovery.requested = false;
			std::cout << "587 屈曲恢复按键：松开，准备退出恢复模式。" << std::endl;
		}
		spacing_recovery_button_pressed_prev = spacing_recovery_button_pressed;
