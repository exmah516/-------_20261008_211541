			const double axis1_linear_filtered = axis1_handle_filter.axis0_filtered;
			const double axis1_rot_filtered = axis1_handle_filter.axis1_filtered;
			const double axis6_linear_filtered = axis6_handle_filter.axis0_filtered;
			const double axis6_rot_filtered = axis6_handle_filter.axis1_filtered;

			const double axis1_linear_increment_raw_mm =
				(axis1_linear_filtered - axis1_prev_linear_filtered) * cfg.k_handle_to_mm * cfg.axis_push_sign;
			const double axis1_linear_increment_mm =
				(std::abs(axis1_linear_increment_raw_mm) >= cfg.linear_increment_noise_deadband_mm)
				? axis1_linear_increment_raw_mm
				: 0.0;
			const double axis6_linear_increment_raw_mm =
				(axis6_linear_filtered - axis6_prev_linear_filtered) * cfg.k_handle_to_mm * cfg.axis_push_sign;
			const double axis6_linear_increment_mm =
				(std::abs(axis6_linear_increment_raw_mm) >= cfg.linear_increment_noise_deadband_mm)
				? axis6_linear_increment_raw_mm
				: 0.0;
probe[0] = axis1_linear_increment_mm; probe[2] = axis6_linear_increment_mm;
				pos[1] = axis1_crawl.rot_base_rel +
					(axis1_rot_filtered - axis1_crawl.rot_ref) * cfg.axis_rot_scale_deg;
probe[3] = axis6_crawl.rot_base_rel + (axis6_rot_filtered - axis6_crawl.rot_ref) * cfg.axis_rot_scale_deg;
probe[1] = pos[1];
