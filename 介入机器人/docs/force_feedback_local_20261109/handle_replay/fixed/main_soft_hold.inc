		handle_soft_hold_active = single_handle_mode
			? ((!handle1_poll_ok && !handle6_poll_ok) || axis1_input_handle->resume_settling())
			: (!handle1_poll_ok || !handle6_poll_ok || handle_axis1.resume_settling() || handle_axis6.resume_settling());
        if (handle_axis1.resume_settling() || handle_axis6.resume_settling())
        {
            recovery_event_text = handle_axis1.resume_settling() ? "导管手柄恢复中" : "";
            if (handle_axis6.resume_settling())
                recovery_event_text += recovery_event_text.empty() ? "导丝手柄恢复中" : "；导丝手柄恢复中";
        }

