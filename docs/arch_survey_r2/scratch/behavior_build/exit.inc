		const DWORD spacing_recovery_exit_elapsed_ms =
			GetTickCount() - spacing_recovery.phase_t0;
		const bool spacing_recovery_targets_settled =
			std::abs(plc_act_pos[2] - spacing_recovery.axis3_cmd_rel) <= cfg.crawl_arrive_tol_mm &&
			std::abs(plc_act_pos[4] - spacing_recovery.axis5_cmd_rel) <= cfg.crawl_arrive_tol_mm &&
			std::abs(plc_act_pos[5] - spacing_recovery.axis6_cmd_rel) <= cfg.crawl_arrive_tol_mm;
		const bool spacing_recovery_exit_ready =
			spacing_recovery_exit_elapsed_ms >= cfg.spacing_recovery_exit_settle_ms &&
			(spacing_recovery_targets_settled ||
				spacing_recovery_exit_elapsed_ms >= cfg.spacing_recovery_exit_timeout_ms);
		if (spacing_recovery.phase == SpacingRecoveryPhase::ExitSync && spacing_recovery_exit_ready &&
			ads_motion_cycle_valid && !ads_soft_hold_active && !handle_soft_hold_active &&
			!estop_hold_active && !return_ads_fault_hold)
		{
			const double recovered_mm = spacing_recovery.moved_mm;
			if (sync_all(20))
			{
				if (!spacing_recovery_targets_settled)
				{
					std::cout << "屈曲恢复退出等待超时，已按轴3/5/6当前实际位置重同步。" << std::endl;
				}
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
				std::cout << "屈曲恢复退出后，双手柄方向按当前B6状态生效。" << std::endl;
				std::cout << "屈曲恢复已退出并完成重同步，本次共同移动 "
					<< recovered_mm << " mm。" << std::endl;
			}
			// 单次失败保留 ExitSync，下一份有效快照重试，不设置永久故障标志。
		}

