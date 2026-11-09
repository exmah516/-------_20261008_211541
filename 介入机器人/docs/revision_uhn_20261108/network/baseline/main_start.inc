					{
						if (control_active || planned_return.active() || spacing_recovery.active()) break;
						// 自检完成后点击“开始控制”：不再走到准备位置，直接在当前位置开始手柄控制。
						pending_selfcheck_axes = 0;
						if (!startup.is_active() &&
							!return_ads_fault_hold && !handle_soft_hold_active && initial_sync_done &&
							!estop_hold_active &&
							!ads_soft_hold_active &&
							has_self_check_flag && self_check_done)
						{
							clear_cylinder_manual_overrides();
							spacing_recovery.reset();
							if (!restore_startup_v_limit())
							{
								std::cout << "开始控制失败：无法恢复启动期速度限制参数。" << std::endl;
							}
							else if (!consume_startup_loading_ready())
							{
								std::cout << "开始控制失败：无法清除 PLC 装卸位就绪标志。" << std::endl;
							}
							else if (sync_all(20))
							{
								if (!startup.completed)
								{
									guidewire_mode = GuidewireMode::None;
									axis6_crawl.enabled = false;
									axis6_window_locked = false;
								}
								startup.recovery_mode = false;
								startup.direct_positioning = false;
								startup.phase = StartupPhase::Done;
								startup.completed = true;
								startup.prompted = false;
								control_active = true;
								plc_restart_recovery_latched = false;
								std::cout << "已在当前位置开始控制。" << std::endl;
							}
							else
							{
								std::cout << "开始控制失败：ADS 重同步失败。" << std::endl;
							}
						}
						else
						{
							std::cout << "开始控制被拒绝：当前不在已自检待机状态或 ADS 未就绪。" << std::endl;
						}
						break;
					}
