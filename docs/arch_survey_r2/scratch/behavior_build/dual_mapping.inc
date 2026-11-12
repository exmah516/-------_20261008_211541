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
