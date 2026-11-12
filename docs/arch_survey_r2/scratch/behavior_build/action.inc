			else if (spacing_recovery.active())
			{
				// 固定导管侧，释放随轴3/5/6移动的夹爪；电缸4保留轻限位，防止导丝脱出。
				cylinder1_cmd = cyl.cyl1_open;
				cylinder2_cmd = cyl.cyl2_clamp;
				cylinder3_cmd = cyl.cyl3_open;
				cylinder4_cmd = cfg.spacing_recovery_cyl4_release;
				pos[0] = spacing_recovery.axis1_hold_rel;
				pos[1] = spacing_recovery.axis2_hold_rel;
				pos[6] = spacing_recovery.axis7_hold_rel;

				if (spacing_recovery.phase == SpacingRecoveryPhase::ClampWait)
				{
					spacing_recovery.pending_delta_mm = 0.0;
					if ((now_ms - spacing_recovery.phase_t0) >= cfg.spacing_recovery_clamp_settle_ms)
					{
						spacing_recovery.phase = SpacingRecoveryPhase::Active;
						spacing_recovery.last_motion_tick = now_ms;
						std::cout << "屈曲恢复：已激活，582 手柄仅控制轴3/5/6向远离左限位方向移动。" << std::endl;
					}
				}
				else if (spacing_recovery.phase == SpacingRecoveryPhase::Active)
				{
					DWORD elapsed_ms = now_ms - spacing_recovery.last_motion_tick;
					spacing_recovery.last_motion_tick = now_ms;
					if (elapsed_ms > 50) elapsed_ms = 50;

					if (axis1_linear_increment_mm > 0.0)
					{
						spacing_recovery.pending_delta_mm += axis1_linear_increment_mm;
					}
					else
					{
						// 不累计反方向或已经停止的手柄输入，避免停止推拉后继续运动。
						spacing_recovery.pending_delta_mm = 0.0;
					}

					spacing_recovery.remaining_mm = calculate_spacing_recovery_remaining();
					const double max_step_mm = cfg.spacing_recovery_speed_limit_mm_s *
						(static_cast<double>(elapsed_ms) / 1000.0);
					double step_mm = spacing_recovery.pending_delta_mm;
					if (step_mm > max_step_mm) step_mm = max_step_mm;
					if (step_mm > spacing_recovery.remaining_mm) step_mm = spacing_recovery.remaining_mm;

					if (step_mm > 0.0)
					{
						spacing_recovery.axis3_cmd_rel += step_mm;
						spacing_recovery.axis5_cmd_rel += step_mm;
						spacing_recovery.axis6_cmd_rel += step_mm;
						spacing_recovery.pending_delta_mm -= step_mm;
						spacing_recovery.moved_mm += step_mm;
						spacing_recovery.remaining_mm = calculate_spacing_recovery_remaining();
					}
					if (spacing_recovery.remaining_mm <= cfg.crawl_arrive_tol_mm)
					{
						spacing_recovery.pending_delta_mm = 0.0;
						if (!spacing_recovery.limit_logged)
						{
							std::cout << "屈曲恢复：已到达轴3/5/6共同安全行程上限。" << std::endl;
							spacing_recovery.limit_logged = true;
						}
					}
				}

				pos[2] = spacing_recovery.axis3_cmd_rel;
				pos[4] = spacing_recovery.axis5_cmd_rel;
				pos[5] = spacing_recovery.axis6_cmd_rel;
			}
