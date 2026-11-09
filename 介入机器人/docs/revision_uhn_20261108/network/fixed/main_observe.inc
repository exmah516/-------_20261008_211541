		const bool ads_self_check_valid = has_new_ads_snapshot && ads_snapshot.position_valid &&
			ads_stats.state == AdsConnectionState::Running;
		if (ads_self_check_valid) self_check_done = ads_snapshot.self_check_done;
