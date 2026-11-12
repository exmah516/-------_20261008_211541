		const bool distance_follow_output = ads_output.motion_enabled && ads_output.cylinder_valid &&
			handle_distance_follow_allowed();
		axis1_net_distance.set_following(distance_follow_output &&
			guidewire_mode == GuidewireMode::None &&
			cylinder1_cmd == cyl.cyl1_open && cylinder2_cmd == cyl.cyl2_clamp);
		axis6_net_distance.set_following(distance_follow_output &&
			(!single_handle_mode || guidewire_mode == GuidewireMode::Independent) &&
			!axis4_forward_request && !axis4_reverse_request &&
			cylinder3_cmd == cyl.cyl3_open && cylinder4_cmd == cyl.cyl4_clamp);
