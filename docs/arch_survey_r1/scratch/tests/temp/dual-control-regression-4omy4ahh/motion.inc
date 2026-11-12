				const double tol = cfg.crawl_arrive_tol_mm;
				const double gmin = cfg.catheter_axis6_window_min_gap_from_axis5_mm;
				const double gmax = gmin + motion_sync::axis6_window_size_mm(ctx);
				const double merge_eps = cfg.transaction_merge_eps_mm;
				const double lo1 = axis1_crawl.min_abs();
				const double hi1 = axis1_crawl.max_abs();
				const bool reverse = axis1_reverse_pressed;
				// 轴4点动独立选择轴6主动换手方向，轴1及其被动换手仍使用全局方向。
				const bool axis4_direction_active = axis4_forward_request || axis4_reverse_request;
				const bool requested_wire_reverse = axis4_direction_active
					? axis4_reverse_request : reverse;
				// 轴4已触发的换手在松开点动按钮后仍需完成；只有重新按下相反点动方向才取消。
				const bool wire_reverse = dual_handoff.active() && dual_handoff.axis4_caused &&
					!axis4_direction_active
					? dual_handoff.wire_reverse : requested_wire_reverse;
				if (dual_handoff.active() &&
					(((dual_handoff.axis1 || !dual_handoff.wire_caused) && dual_handoff.reverse != reverse) ||
						(dual_handoff.wire_caused && dual_handoff.wire_reverse != wire_reverse)))
				{
					dual_handoff.reset();
				}
				if (!axis1_crawl.window_active &&
					is_within_range(axis1_abs, lo1, hi1, tol))
				{
					capture_axis1_follow_baseline();
					axis1_crawl.window_active = true;
				}
				const bool axis1_window_ok = axis1_crawl.window_active;

				double inc1 = dual_handoff.active() ? 0.0 : axis1_linear_increment_mm;
				// 投送停止位只拦截继续递送的增量，撤出方向始终放行。
				if (axis3_delivery_stop_active && inc1 < 0.0) inc1 = 0.0;
				double inc6 = dual_handoff.active() ? 0.0 : axis6_linear_increment_mm;
				if (!dual_handoff.active() && guidewire_mode == GuidewireMode::None &&
					(axis4_forward_request || axis4_reverse_request))
				{
					// 轴4点动：前进对应 axis6 位置减少，作为导丝链路的附加增量来源，同样受 g 约束。
					const ULONGLONG coupling_now_ms = GetTickCount64();
					if (axis4_axis6_coupling_active_prev && axis4_axis6_coupling_last_ms != 0)
					{
						const double elapsed_s = static_cast<double>(
							coupling_now_ms - axis4_axis6_coupling_last_ms) / 1000.0;
						inc6 += (axis4_forward_request ? -1.0 : 1.0) *
							axis4_coupled_axis6_speed_mm_s * elapsed_s;
					}
					axis4_axis6_coupling_last_ms = coupling_now_ms;
					axis4_axis6_coupling_active_prev = true;
				}
				else
				{
					axis4_axis6_coupling_last_ms = 0;
					axis4_axis6_coupling_active_prev = false;
				}

				const double cmd1_start = axis1_follow_cmd_abs;
				const double cmd6_start = axis6_follow_cmd_abs;
				const double cmd5_start = plc_init_pos[4] + axis5_base_rel +
					(cmd1_start - plc_init_pos[0] - axis1_crawl.base_rel);
				const double a5_left_cmd = cmd5_start - plc_leftlimit[4];
				const double abs6_soft = plc_leftlimit[5] + cfg.axis6_soft_limit_from_left_mm;
				const double g_start = (cmd6_start - plc_leftlimit[5]) - a5_left_cmd;
				// g约束始终使用轴3/5的名义增量；轴1先行另占自身窗口余量。
				const double mapping_gain = axis1_delivery_mapping_active && !reverse &&
					cfg.axis1_post_return_mapping_span_mm > 1e-6
					? clamp_double(cfg.axis1_post_return_lead_mm, 0.0, cfg.axis1_post_return_lead_limit_mm) /
						cfg.axis1_post_return_mapping_span_mm : 0.0;
				const double mapping_remaining_mm = mapping_gain > 0.0
					? (std::max)(0.0, cfg.axis1_post_return_mapping_span_mm - axis1_delivery_mapping_progress_mm) : 0.0;
				double d1 = inc1;
				if (d1 > 0.0) d1 = (std::min)(d1, (std::max)(hi1 - cmd1_start, 0.0));
				else
				{
					const double forward_room_mm = (std::max)(cmd1_start - lo1, 0.0);
					const double extra_room_mm = (std::min)(mapping_remaining_mm,
						forward_room_mm / (1.0 + mapping_gain)) * mapping_gain;
					d1 = (std::max)(d1, -(forward_room_mm - extra_room_mm));
				}
				double d6 = inc6 > 0.0
					? (std::min)(inc6, (std::max)(abs6_soft - cmd6_start, 0.0)) : inc6;
				const double d1_before_gap = d1;
				const double d6_before_gap = d6;
				const double requested_gap_delta = d6 - d1;
				// 已在窗外时只允许保持或回到窗内，不因缩窗自行跳变。
				const double gap_delta_hi = (std::max)(gmax - g_start, 0.0);
				const double gap_delta_lo = (std::min)(gmin - g_start, 0.0);
				if (requested_gap_delta > gap_delta_hi)
				{
					const double closing = (std::max)(d6, 0.0) + (std::max)(-d1, 0.0);
					const double scale = (std::max)(0.0, 1.0 - (requested_gap_delta - gap_delta_hi) / closing);
					if (d6 > 0.0) d6 *= scale;
					if (d1 < 0.0) d1 *= scale;
				}
				else if (requested_gap_delta < gap_delta_lo)
				{
					const double closing = (std::max)(-d6, 0.0) + (std::max)(d1, 0.0);
					const double scale = (std::max)(0.0, 1.0 - (gap_delta_lo - requested_gap_delta) / closing);
					if (d6 < 0.0) d6 *= scale;
					if (d1 > 0.0) d1 *= scale;
				}
				const double mapping_progress_mm = (std::min)((std::max)(-d1, 0.0), mapping_remaining_mm);
				const double mapping_extra_mm = mapping_progress_mm * mapping_gain;
				axis1_delivery_mapping_progress_mm += mapping_progress_mm;
				axis1_delivery_mapping_applied_extra_mm += mapping_extra_mm;
				if (axis1_delivery_mapping_progress_mm >= cfg.axis1_post_return_mapping_span_mm - 1e-6)
					axis1_delivery_mapping_active = false;
				// 把先行差额吸收到镜像基准，清除映射或切换撤出时轴3/5不会补走。
				axis3_base_rel += mapping_extra_mm;
				axis5_base_rel += mapping_extra_mm;
				const double cmd1 = cmd1_start + d1 - mapping_extra_mm;
				const double cmd6 = cmd6_start + d6;
				const double g_now = g_start + d6 - d1;

				axis1_follow_cmd_abs = cmd1;
				axis6_follow_cmd_abs = cmd6;
				pos[0] = cmd1 - plc_init_pos[0];
				pos[1] = axis1_crawl.rot_base_rel +
					(axis1_rot_filtered - axis1_crawl.rot_ref) * cfg.axis_rot_scale_deg;
				axis2_hold_rel = pos[1];
				apply_axis1_mirror_from_abs(cmd1, false);
				pos[5] = cmd6 - plc_init_pos[5];
				pos[6] = compute_axis7_cmd_rel();
				cylinder1_cmd = cyl.cyl1_open;
				cylinder2_cmd = cyl.cyl2_clamp;
				cylinder3_cmd = cyl.cyl3_open;
				cylinder4_cmd = cyl.cyl4_clamp;

				// 按截断前的净间距变化识别触边；已经离开端点的输入不能再次换手。
				const bool g_toward_hi = requested_gap_delta > 1e-9;
				const bool g_toward_lo = requested_gap_delta < -1e-9;
				const bool wire_hit_hi = (g_now >= gmax - tol) && wire_reverse && d6_before_gap > 0.0 && g_toward_hi;
				const bool wire_hit_lo = (g_now <= gmin + tol) && !wire_reverse && d6_before_gap < 0.0 && g_toward_lo;
				const bool cath_hit_hi = (g_now >= gmax - tol) && !reverse && d1_before_gap < 0.0 && g_toward_hi;
				const bool cath_hit_lo = (g_now <= gmin + tol) && reverse && d1_before_gap > 0.0 && g_toward_lo;
				const bool trig_1_left = axis1_window_ok && !reverse &&
					(cmd1 <= lo1 + tol) && inc1 < 0.0;
				const bool trig_1_right = axis1_window_ok && reverse &&
					(cmd1 >= hi1 - tol) && inc1 > 0.0;
				if (!dual_handoff.active())
				{
					dual_handoff.axis1 = trig_1_left || trig_1_right;
					// 仅轴1可合并临近被动端的轴6；主动换手必须由导丝自身触发。
					const bool merge_axis6 = dual_handoff.axis1 &&
						(reverse ? g_now < gmin + merge_eps - 1e-9 : g_now > gmax - merge_eps + 1e-9);
					dual_handoff.wire_caused = wire_hit_hi || wire_hit_lo;
					dual_handoff.gap_boundary = dual_handoff.wire_caused || cath_hit_hi || cath_hit_lo;
					dual_handoff.axis6 = dual_handoff.gap_boundary || merge_axis6;
					dual_handoff.reverse = reverse;
					dual_handoff.wire_reverse = wire_reverse;
					dual_handoff.axis4_caused = dual_handoff.wire_caused && axis4_direction_active;
				}
				const bool want_axis1_leg = dual_handoff.axis1;
				const bool want_axis6_leg = dual_handoff.axis6;
				const bool wire_caused_g = dual_handoff.wire_caused;
				const double g_actual = (axis6_abs - plc_leftlimit[5]) - (axis5_abs - plc_leftlimit[4]);
				const double gap_edge = (wire_caused_g ? dual_handoff.wire_reverse : !reverse) ? gmax : gmin;
				bool handoff_ready = dual_handoff.active() &&
					(!want_axis1_leg || std::abs(axis1_abs - (reverse ? hi1 : lo1)) <= tol + 1e-9) &&
					(!dual_handoff.gap_boundary || std::abs(g_actual - gap_edge) <= tol + 1e-9);
				// 换手会冻结两条链路，轴3/5及另一轴的剩余递送也必须先完成。
				for (int axis : { 0, 2, 4, 5 })
				{
					handoff_ready = handoff_ready &&
						std::abs(ctx.plc_act_pos[axis] - pos[axis]) <= tol + 1e-9 &&
						std::abs(ctx.plc_act_velocity_mm_s[axis]) <= cfg.handoff_stop_velocity_mm_s;
				}
				if (handoff_ready && !planned_return.active())
				{
					// 换手时axis3/5保持，g目标以axis5当前实际位置计算。
					const double axis1_target_abs = reverse ? lo1 : hi1;
					const double axis5_left_now = axis5_abs - plc_leftlimit[4];
					// 同时换手按轴1换手后的位置预留行程，否则使用本次快照的实际位置。
					const double axis1_after_return_abs = want_axis1_leg ? axis1_target_abs : axis1_abs;
					const double axis1_remaining_mm = (std::max)(0.0,
						reverse ? hi1 - axis1_after_return_abs : axis1_after_return_abs - lo1);
					const double passive_distance_mm = (std::min)(cfg.axis6_passive_distance_limit_mm,
						cfg.axis6_passive_clearance_mm + axis1_remaining_mm);
					const double g_target = wire_caused_g
						? (dual_handoff.wire_reverse ? cfg.axis6_active_retraction_gap_mm : cfg.axis6_active_delivery_gap_mm)
						: (reverse ? gmin + passive_distance_mm : gmax - passive_distance_mm);
					double axis6_target_abs = plc_leftlimit[5] + axis5_left_now + g_target;
					axis6_target_abs = (std::min)(axis6_target_abs, abs6_soft); // 越限只夹取，不停机
					const bool use_axis6_leg = want_axis6_leg &&
						std::abs(axis6_target_abs - axis6_abs) > tol;
					if (want_axis1_leg || use_axis6_leg)
					{
						if (begin_planned_return(
							PlannedReturnMode::Dual,
							PlannedReturnRebaseScope::Both,
							want_axis1_leg,
							axis1_target_abs,
							use_axis6_leg,
							axis6_target_abs))
						{
							// 实际到位并低速后才夹紧；电机请求仍等待原有两级夹爪时序。
							dual_handoff.reset();
							apply_planned_return_outputs();
						}
					}
					else dual_handoff.reset();
				}