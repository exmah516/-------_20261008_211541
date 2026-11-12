	auto clear_axis1_delivery_mapping = [&]()
	{
		axis1_delivery_mapping_active = false;
		axis1_delivery_mapping_progress_mm = 0.0;
		axis1_delivery_mapping_applied_extra_mm = 0.0;
	};
	auto arm_axis1_delivery_mapping = [&]()
	{
		axis1_delivery_mapping_active = cfg.axis1_post_return_lead_mm > 1e-6;
		axis1_delivery_mapping_progress_mm = 0.0;
		axis1_delivery_mapping_applied_extra_mm = 0.0;
	};
