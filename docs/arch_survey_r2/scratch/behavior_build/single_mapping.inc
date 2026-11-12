					double axis1_mapping_extra_requested_mm = 0.0;
					if (axis1_delivery_mapping_active &&
						!axis1_reverse_pressed &&
						axis1_nominal_forward_mm > 0.0 &&
						cfg.axis1_post_return_mapping_span_mm > 1e-6)
					{
						const double mapping_span_mm = cfg.axis1_post_return_mapping_span_mm;
						const double progress_before_mm = clamp_double(
							axis1_delivery_mapping_progress_mm,
							0.0,
							mapping_span_mm);
						const double progress_after_mm = (std::min)(
							mapping_span_mm,
							progress_before_mm + axis1_nominal_forward_mm);
						const double target_extra_mm = clamp_double(
							cfg.axis1_post_return_lead_mm * progress_after_mm / mapping_span_mm,
							0.0,
							cfg.axis1_post_return_lead_limit_mm);
						axis1_mapping_extra_requested_mm = (std::max)(
							0.0,
							target_extra_mm - axis1_delivery_mapping_applied_extra_mm);
						const double mapped_target_abs = axis1_cmd_abs - axis1_mapping_extra_requested_mm;
						const double mapped_cmd_abs = constrain_axis1_follow_cmd_abs(mapped_target_abs);
						const double mapping_extra_applied_mm = (std::max)(
							0.0,
							axis1_cmd_abs - mapped_cmd_abs);
						axis1_cmd_abs = mapped_cmd_abs;
						axis1_delivery_mapping_progress_mm = progress_after_mm;
						axis1_delivery_mapping_applied_extra_mm += mapping_extra_applied_mm;
						axis3_base_rel += mapping_extra_applied_mm;
						axis5_base_rel += mapping_extra_applied_mm;
						if (progress_after_mm >= mapping_span_mm - 1e-6)
						{
							axis1_delivery_mapping_active = false;
						}
					}
