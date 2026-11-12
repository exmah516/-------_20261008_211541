		if (ads_soft_hold_active && planned_return.active())
		{
			// 有效快照恢复后先完成原回退；不能重建 Follow 基准或重新提交运动。
			ads_soft_hold_active = false;
		}
		else if ((ads_soft_hold_active || handle_rebase_requested || (connection_hold_active_prev && initial_sync_done)) &&
			!handle_soft_hold_active && !planned_return.active())
		{
			if (!handle_soft_hold_active)
			{
				axis1_handle_filter.reset(axis1_input_handle->fJoints2[0], axis1_input_handle->fJoints2[1]);
				axis6_handle_filter.reset(axis6_input_handle->fJoints2[0], axis6_input_handle->fJoints2[1]);
				axis1_prev_linear_filtered = axis1_handle_filter.axis0_filtered;
				axis6_prev_linear_filtered = axis6_handle_filter.axis0_filtered;
				axis1_prev_rot_filtered = axis1_handle_filter.axis1_filtered;
				axis6_prev_rot_filtered = axis6_handle_filter.axis1_filtered;
			}
			load_pos_from_actual();
			bool baseline_rebuilt = true;
			// 首次连接只走下方 sync_all，禁止先用尚未初始化的旋转保持值写 refer。
			// ADS 或手柄恢复时重建当前反馈基准，不追赶中断前的旋转目标。
			if (!handle_soft_hold_active && initial_sync_done)
			{
				if (!single_handle_mode)
				{
					baseline_rebuilt = motion_sync::rebase_dual_after_return(ctx);
				}
				else if (guidewire_mode == GuidewireMode::Independent)
				{
					baseline_rebuilt = motion_sync::sync_axis6(ctx, 1, false, false, false);
				}
				else
				{
					baseline_rebuilt = motion_sync::sync_axis1(ctx, 1, false);
				}
			}
            const bool recovered_ads_snapshot = ads_soft_hold_active;
			ads_soft_hold_active = !baseline_rebuilt;
			if (baseline_rebuilt)
			{
				axis1_fast_return = false;
				axis6_fast_retract = false;
                if (recovered_ads_snapshot) recovery_event_text = "ADS 快照恢复";
                else if (handle_rebase_requested)
                {
                    std::ostringstream reason;
                    if (handle1_rebase_requested) reason << "手柄587采样间隔 " << handle_axis1.sample_gap_ms() << " ms 后重建；";
                    if (handle6_rebase_requested) reason << "手柄582采样间隔 " << handle_axis6.sample_gap_ms() << " ms 后重建；";
                    recovery_event_text = reason.str();
                }
                else recovery_event_text = "连接保持结束";
                std::cout << recovery_event_text << "：实际位置与手柄基准已重建。" << std::endl;
			}
		}

