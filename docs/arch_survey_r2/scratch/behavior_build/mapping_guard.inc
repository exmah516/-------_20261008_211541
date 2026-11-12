		if (guidewire_mode != GuidewireMode::None ||
			axis1_reverse_pressed ||
			cfg.axis1_post_return_lead_mm <= 1e-6 ||
			!control_active ||
			connection_hold_active ||
			ads_soft_hold_active ||
			return_ads_fault_hold ||
			estop_hold_active ||
			startup_sequence_active ||
			emergency_retract_active ||
			spacing_recovery.active() ||
			spacing_recovery.requested ||

			(planned_return.active() &&
				(planned_return.mode != PlannedReturnMode::Dual || planned_return.contains_axis(0))) ||
			axis6_soft_limit_hold)
		{
			clear_axis1_delivery_mapping();
		}
