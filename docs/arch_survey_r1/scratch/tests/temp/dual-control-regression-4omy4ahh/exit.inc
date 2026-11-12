				const ModeSelection restore_selection =
					static_cast<ModeSelection>(spacing_recovery.restore_mode_selection);
				vis_reverse_override_active = spacing_recovery.restore_vis_reverse_override_active;
				vis_reverse_override_value = spacing_recovery.restore_vis_reverse_override_value;
				vis_reverse_override_target = spacing_recovery.restore_vis_reverse_override_target;
				const double recovered_axis3_from_left_mm =
					(plc_act_pos[2] + plc_init_pos[2]) - plc_leftlimit[2];
				if (recovered_axis3_from_left_mm >
					(cfg.axis3_delivery_stop_from_left_mm + cfg.axis3_delivery_release_hysteresis_mm))
				{
					axis1_delivery_stop_latched = false;
					axis1_delivery_stop_prompted = false;
				}
				axis1_push_rearm_after_hold = false;
				spacing_recovery.reset();
				control_active = true;
				if (single_handle_mode) apply_mode_selection(restore_selection);
				else vis_reverse_override_active = false;
