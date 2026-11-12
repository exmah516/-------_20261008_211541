	double from_left_to_abs(const AppContext& ctx, int axis_index, double from_left_mm)
	{
		// 左限位参考坐标 -> PLC 绝对坐标。
		return ctx.plc_leftlimit[axis_index] + from_left_mm;
	}

	double from_left_to_rel(const AppContext& ctx, int axis_index, double from_left_mm)
	{
		// 左限位参考坐标 -> 上位机相对坐标（refer 使用的坐标系）。
		return from_left_to_abs(ctx, axis_index, from_left_mm) - ctx.plc_init_pos[axis_index];
	}

	double axis1_window_left_abs(const AppContext& ctx)
	{
		return from_left_to_abs(ctx, 0, ctx.cfg->axis1_window_left_from_left_mm);
	}

	double axis1_window_right_abs(const AppContext& ctx)
	{
		return from_left_to_abs(ctx, 0, ctx.cfg->axis1_window_right_from_left_mm);
	}

	double axis6_window_size_mm(const AppContext& ctx)
	{
		return ctx.guidewire_mode && *ctx.guidewire_mode == GuidewireMode::Independent
			? ctx.cfg->guidewire_axis6_window_size_mm : ctx.cfg->catheter_axis6_window_size_mm;
	}

	void calculate_axis6_window_from_axis5(
		const AppContext& ctx,
		double& window_start_abs,
		double& window_end_abs)
	{
		const double axis5_abs = ctx.plc_act_pos[4] + ctx.plc_init_pos[4];
		calculate_axis6_window_from_axis5_abs(ctx, axis5_abs, window_start_abs, window_end_abs);
	}

	void calculate_axis6_window_from_axis5_abs(
		const AppContext& ctx,
		double axis5_abs,
		double& window_start_abs,
		double& window_end_abs)
	{
		const double axis5_from_left_mm = axis5_abs - ctx.plc_leftlimit[4];
		const bool guidewire_mode = ctx.guidewire_mode &&
			*ctx.guidewire_mode == GuidewireMode::Independent;
		const double gap_mm = guidewire_mode
			? ctx.cfg->guidewire_axis6_window_min_gap_from_axis5_mm
			: ctx.cfg->catheter_axis6_window_min_gap_from_axis5_mm;
		const double size_mm = axis6_window_size_mm(ctx);
		window_start_abs = ctx.plc_leftlimit[5] + axis5_from_left_mm + gap_mm;
		window_end_abs = window_start_abs + size_mm;
	}

