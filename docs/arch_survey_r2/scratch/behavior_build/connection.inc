		const bool connection_hold_active = !initial_sync_done || ads_soft_hold_active || handle_soft_hold_active;
		const bool connection_hold_enter_edge = connection_hold_active && !connection_hold_active_prev;
		const bool connection_hold_exit_edge = !connection_hold_active && connection_hold_active_prev;
		connection_hold_active_prev = connection_hold_active;

		if (connection_hold_enter_edge)
		{
			clear_cylinder_manual_overrides();
			tracking_controller.disable_compensation();
			if (spacing_recovery.phase != SpacingRecoveryPhase::ExitSync) spacing_recovery.reset();
			clear_force_output();
			std::cout << "连接保持激活：source="
				<< (!initial_sync_done ? "初始化;" : "")
				<< (ads_soft_hold_active ? "ADS;" : "")
				<< (handle_soft_hold_active ? "手柄;" : "")
				<< " 保留回退任务，等待有效快照与手柄采样，清零力反馈。" << std::endl;
		}
		if (connection_hold_exit_edge && !planned_return.active())
		{
			load_pos_from_actual();
			axis1_fast_return = false;
			axis6_fast_retract = false;
			axis2_hold_rel = plc_act_pos[1];
			axis7_hold_rel = plc_act_pos[6];
			independent_axis2_hold_rel = plc_act_pos[1];
			std::cout << "连接保持退出：source=" << recovery_event_text
				<< " 保持 PLC 当前反馈位置，不恢复历史目标。axis2="
				<< plc_act_pos[1] << "，axis7=" << plc_act_pos[6] << "（相对角度）" << std::endl;
		}

