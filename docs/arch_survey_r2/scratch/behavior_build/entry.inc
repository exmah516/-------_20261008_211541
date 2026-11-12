			auto calculate_spacing_recovery_remaining = [&]() -> double
			{
				double remaining = cfg.spacing_recovery_axis3_max_from_left_mm -
					((spacing_recovery.axis3_cmd_rel + plc_init_pos[2]) - plc_leftlimit[2]);
				const double axis5_remaining = cfg.spacing_recovery_axis5_max_from_left_mm -
					((spacing_recovery.axis5_cmd_rel + plc_init_pos[4]) - plc_leftlimit[4]);
				const double axis6_remaining = (std::min)(cfg.spacing_recovery_axis6_max_from_left_mm,
					cfg.axis6_soft_limit_from_left_mm) -
					((spacing_recovery.axis6_cmd_rel + plc_init_pos[5]) - plc_leftlimit[5]);
				if (axis5_remaining < remaining) remaining = axis5_remaining;
				if (axis6_remaining < remaining) remaining = axis6_remaining;
				return remaining > 0.0 ? remaining : 0.0;
			};

			if (spacing_recovery.active() && !spacing_recovery.requested &&
				spacing_recovery.phase != SpacingRecoveryPhase::ExitSync)
			{
				spacing_recovery.phase = SpacingRecoveryPhase::ExitSync;
				spacing_recovery.phase_t0 = now_ms;
				spacing_recovery.pending_delta_mm = 0.0;
				std::cout << "屈曲恢复：正在停止并重建跟随基准。" << std::endl;
			}

			if (spacing_recovery.requested && !spacing_recovery.active() && !planned_return.active())
			{
				const double axis5_from_left_mm = axis5_abs - plc_leftlimit[4];
				const double axis6_from_left_mm = axis6_abs - plc_leftlimit[5];
				const double axis56_gap_mm = axis6_from_left_mm - axis5_from_left_mm;
				const double axis56_max_gap_mm =
					cfg.catheter_axis6_window_min_gap_from_axis5_mm +
					motion_sync::axis6_window_size_mm(ctx);
				const bool prerequisites_ok =
					startup.completed && startup.phase == StartupPhase::Done &&
					control_active && !axis6_soft_limit_hold &&
					!planned_return.active();

				spacing_recovery.axis1_hold_rel = plc_act_pos[0];
				spacing_recovery.axis2_hold_rel = plc_act_pos[1];
				spacing_recovery.axis3_cmd_rel = plc_act_pos[2];
				spacing_recovery.axis5_cmd_rel = plc_act_pos[4];
				spacing_recovery.axis6_cmd_rel = plc_act_pos[5];
				spacing_recovery.axis7_hold_rel = plc_act_pos[6];
				spacing_recovery.remaining_mm = calculate_spacing_recovery_remaining();

				if (!prerequisites_ok)
				{
					std::cout << "屈曲恢复进入被拒绝：需处于已启动的导管递送 Follow 状态，且无回退。" << std::endl;
					spacing_recovery.reset();
				}
				else if (axis5_from_left_mm + cfg.crawl_arrive_tol_mm < axis3_from_left_mm ||
					axis56_gap_mm < (cfg.catheter_axis6_window_min_gap_from_axis5_mm - cfg.crawl_arrive_tol_mm) ||
					axis56_gap_mm > (axis56_max_gap_mm + cfg.crawl_arrive_tol_mm))
				{
					std::cout << "屈曲恢复进入被拒绝：轴3/5/6当前相对位置不满足安全窗口。" << std::endl;
					spacing_recovery.reset();
				}
				else if (spacing_recovery.remaining_mm <= cfg.crawl_arrive_tol_mm)
				{
					std::cout << "屈曲恢复进入被拒绝：轴3/5/6已无可用恢复行程。" << std::endl;
					spacing_recovery.reset();
				}
				else
				{
					clear_axis1_group_return_requests();
					clear_axis_return_request(AdsSymbol::axis6_return);
					for (int cylinder_index = 0; cylinder_index < 4; ++cylinder_index)
					{
						cylinder_manual_mode[cylinder_index] = CylinderManualMode::Automatic;
					}
					spacing_recovery.phase = SpacingRecoveryPhase::ClampWait;
					record_experiment_event(ExperimentEventCode::SpacingRecoveryStart, 1);
					spacing_recovery.phase_t0 = now_ms;
					spacing_recovery.last_motion_tick = now_ms;
					spacing_recovery.pending_delta_mm = 0.0;
					spacing_recovery.moved_mm = 0.0;
					spacing_recovery.limit_logged = false;
					clear_force_output();
					std::cout << "屈曲恢复：夹爪准备中，轴1保持；恢复速度上限 "
						<< cfg.spacing_recovery_speed_limit_mm_s << " mm/s。" << std::endl;
				}
			}

