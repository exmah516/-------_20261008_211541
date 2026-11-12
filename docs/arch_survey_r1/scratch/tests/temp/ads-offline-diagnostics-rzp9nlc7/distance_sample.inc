		auto handle_distance_follow_allowed = [&]() -> bool
		{
			return control_active && initial_sync_done && !connection_hold_active &&
				!ads_soft_hold_active && !handle_soft_hold_active && !return_ads_fault_hold &&
				!estop_hold_active && !startup.is_active() && !emergency_retract_active &&
				!spacing_recovery.active() && !spacing_recovery.requested &&
				!planned_return.active() && !axis6_soft_limit_hold &&
				!tracking_manual_cylinder_override;
		};
		const bool distance_follow_before = handle_distance_follow_allowed();
		if (has_new_ads_snapshot)
		{
			axis1_net_distance.sample(ads_snapshot.act_pos_rel[0], ads_snapshot.init_pos[0],
				ads_snapshot.attempt_sequence, ads_motion_cycle_valid,
				distance_follow_before && guidewire_mode == GuidewireMode::None);
			axis6_net_distance.sample(ads_snapshot.act_pos_rel[5], ads_snapshot.init_pos[5],
				ads_snapshot.attempt_sequence, ads_motion_cycle_valid,
				distance_follow_before && (!single_handle_mode || guidewire_mode == GuidewireMode::Independent) &&
				!axis4_forward_request && !axis4_reverse_request);
		}

