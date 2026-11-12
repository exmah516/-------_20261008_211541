		if (!handle_soft_hold_active)
		{
			axis1_handle_filter.update(
				axis1_input_handle->fJoints2[0],
				axis1_input_handle->fJoints2[1],
				cfg.linear_handle_alpha,
				cfg.rotational_handle_alpha);
			axis6_handle_filter.update(
				axis6_input_handle->fJoints2[0],
				axis6_input_handle->fJoints2[1],
				cfg.linear_handle_alpha,
				cfg.rotational_handle_alpha);
		}
