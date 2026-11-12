		if (!single_handle_mode && axis1_reverse_pressed != axis1_reverse_pressed_prev)
		{
			record_experiment_event(
				ExperimentEventCode::ModeSwitch,
				axis1_reverse_pressed
					? static_cast<int>(ModeSelection::CatheterRetraction)
					: static_cast<int>(ModeSelection::CatheterDelivery));
			// 丢弃切换前的滤波尾量，避免换向本身触发换手。
			axis1_handle_filter.axis0_filtered = axis1_input_handle->fJoints2[0];
			axis6_handle_filter.axis0_filtered = axis6_input_handle->fJoints2[0];
			axis1_prev_linear_filtered = axis1_handle_filter.axis0_filtered;
			axis6_prev_linear_filtered = axis6_handle_filter.axis0_filtered;
			std::cout << "全局方向：" << (axis1_reverse_pressed ? "撤出。" : "递送。") << std::endl;
		}
