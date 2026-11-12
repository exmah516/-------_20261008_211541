			auto planned_return_targets_safe = [&]() -> bool
			{
				for (int leg_index = 0; leg_index < planned_return.leg_count; ++leg_index)
				{
					const PlannedReturnLeg& leg = planned_return.legs[leg_index];
					if (leg.axis_index == 0)
					{
						if (!is_within_range(
							leg.target_abs,
							axis1_crawl.min_abs(),
							axis1_crawl.max_abs(),
							cfg.crawl_arrive_tol_mm))
						{
							return false;
						}
					}
					else if (leg.axis_index == 5)
					{
						if (axis6_target_exceeds_soft_limit(leg.target_abs)) return false;
						double window_left_abs = axis6_crawl.min_abs();
						double window_right_abs = axis6_crawl.max_abs();
						if (planned_return.mode == PlannedReturnMode::CatheterDelivery ||
							planned_return.mode == PlannedReturnMode::CatheterRetraction ||
							planned_return.mode == PlannedReturnMode::Dual)
						{
							motion_sync::calculate_axis6_window_from_axis5_abs(
								ctx,
								axis5_abs,
								window_left_abs,
								window_right_abs);
						}
						if (!is_within_range(
							leg.target_abs,
							window_left_abs,
							window_right_abs,
							cfg.crawl_arrive_tol_mm))
						{
							return false;
						}
					}
				}
				return true;
			};

