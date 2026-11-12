	bool vis_reverse_override_active = false;
	bool vis_reverse_override_value = false;
	int vis_reverse_override_target = 0;
	enum class ModeSelection : int
	{
		None = 0,
		CatheterDelivery = 1,
		CatheterRetraction = 2,
		GuidewireDelivery = 3,
		GuidewireRetraction = 4,
	};
	ModeSelection pending_mode_selection = ModeSelection::None;
	bool spacing_recovery_button_pressed_prev = false;

	auto apply_mode_selection = [&](ModeSelection selection)
	{
		clear_axis1_delivery_mapping();
		if (single_handle_mode && selection == ModeSelection::CatheterDelivery)
		{
			// 每次回到普通导管正向递送段都重新启用前10 mm映射。
			arm_axis1_delivery_mapping();
		}
		const bool guidewire =
			selection == ModeSelection::GuidewireDelivery ||
			selection == ModeSelection::GuidewireRetraction;
		const bool reverse =
			selection == ModeSelection::CatheterRetraction ||
			selection == ModeSelection::GuidewireRetraction;
		if (single_handle_mode)
		{
			single_handle_requested_mode = guidewire ? GuidewireMode::Independent : GuidewireMode::None;
		}
		clear_cylinder_manual_overrides();
		vis_reverse_override_active = single_handle_mode;
		vis_reverse_override_target = guidewire ? 1 : 0;
		vis_reverse_override_value = reverse;
	};
