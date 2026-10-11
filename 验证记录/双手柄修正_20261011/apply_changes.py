from pathlib import Path
import codecs
import re

here = Path(__file__).resolve().parent
package = here.parents[1]
b = package / 'B_从端电脑/工程/ADS项目'
a = package / 'A_主端电脑_地址修正版/工程/master'

def edit(path, transform):
    raw = path.read_bytes()
    backup = here / 'before' / path.relative_to(package)
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists():
        print('Already applied:', path.relative_to(package))
        return
    original = raw.decode('utf-8-sig').replace('\r\n', '\n')
    changed = transform(original)
    assert changed != original, path
    backup.write_bytes(raw)
    if b'\r\n' in raw:
        changed = changed.replace('\n', '\r\n')
    path.write_bytes((codecs.BOM_UTF8 if raw.startswith(codecs.BOM_UTF8) else b'') + changed.encode('utf-8'))
    print(path.relative_to(package))

def replace(s, old, new):
    assert s.count(old) == 1, (old[:100], s.count(old))
    return s.replace(old, new)

def span(s, start, end, new):
    lo = s.index(start)
    hi = s.index(end, lo)
    return s[:lo] + new + s[hi:]

def config(s):
    s = replace(s, 'axis1_window_right_from_left_mm = 28.0;', 'axis1_window_right_from_left_mm = 34.0;')
    s = replace(s, '\t// 双手柄同时控制：非触发端（限位端）相对窗口端点内缩的距离；\n\t// 该内缩后的限位端同时是换手后的重定位位置（axis1 窗口与 g 窗口共用）。',
        '\t// 撤出时两条链路共用36 mm基础窗口，左侧参考保持不变。\n\tdouble dual_retraction_window_size_mm = 36.0;\n\t// 非触发端向外扩展的余量；g下端为接触位置，不外扩。')
    s = s.replace('\tint restore_physical_mode_source = 0;\n', '')
    s = replace(s, '\tGuidewireMode* guidewire_mode = nullptr;',
        '\tGuidewireMode* guidewire_mode = nullptr;\n\tbool* dual_reverse = nullptr;')
    return s

edit(b/'control_types.h', config)

def sync(s):
    s = replace(s, '\t\treturn from_left_to_abs(ctx, 0, ctx.cfg->axis1_window_right_from_left_mm);',
        '\t\tconst double right_mm = ctx.dual_reverse && *ctx.dual_reverse\n'
        '\t\t\t? ctx.cfg->axis1_window_left_from_left_mm + ctx.cfg->dual_retraction_window_size_mm\n'
        '\t\t\t: ctx.cfg->axis1_window_right_from_left_mm;\n'
        '\t\treturn from_left_to_abs(ctx, 0, right_mm);')
    marker = '\tvoid calculate_axis6_window_from_axis5(\n'
    s = replace(s, marker,
        '\tdouble axis6_window_size_mm(const AppContext& ctx)\n\t{\n'
        '\t\tif (ctx.dual_reverse && *ctx.dual_reverse) return ctx.cfg->dual_retraction_window_size_mm;\n'
        '\t\treturn ctx.guidewire_mode && *ctx.guidewire_mode == GuidewireMode::Independent\n'
        '\t\t\t? ctx.cfg->guidewire_axis6_window_size_mm : ctx.cfg->catheter_axis6_window_size_mm;\n\t}\n\n' + marker)
    s = replace(s, '\t\tconst double size_mm = guidewire_mode\n\t\t\t? ctx.cfg->guidewire_axis6_window_size_mm\n\t\t\t: ctx.cfg->catheter_axis6_window_size_mm;',
        '\t\tconst double size_mm = axis6_window_size_mm(ctx);')
    return s

edit(b/'motion_sync.cpp', sync)
edit(b/'motion_sync.h', lambda s: replace(s, '\tdouble axis1_window_right_abs(const AppContext& ctx);',
    '\tdouble axis1_window_right_abs(const AppContext& ctx);\n\tdouble axis6_window_size_mm(const AppContext& ctx);'))

def main(s):
    s = replace(s, '\tGuidewireMode guidewire_mode = GuidewireMode::None;',
        '\tGuidewireMode guidewire_mode = GuidewireMode::None;\n\tbool dual_reverse_active = false;')
    s = replace(s, '\tctx.guidewire_mode = &guidewire_mode;',
        '\tctx.guidewire_mode = &guidewire_mode;\n\tctx.dual_reverse = single_handle_mode ? nullptr : &dual_reverse_active;')
    s = span(s, '\tbool catheter_mode_button_pressed_prev =', '\tbool axis4_manual_error_prev', '')
    s = re.sub(r'^\s*(?:catheter|guidewire)_mode_button_pressed_prev = .*?;\n', '', s, flags=re.M)
    s = span(s, '\tenum class PhysicalModeSource', '\tenum class ModeSelection', '')
    s = s.replace('\tstd::uint32_t physical_button_event_counter = 0;\n\tint physical_button_event_code = 0;\n', '')
    s = replace(s, 'auto apply_mode_selection = [&](ModeSelection selection,\n\t\tPhysicalModeSource mode_source = PhysicalModeSource::None)',
        'auto apply_mode_selection = [&](ModeSelection selection)')
    s = replace(s, '\t\t// UI/键盘选择清除物理模式源；B7选择保留对应手柄作为模式源。\n\t\tphysical_mode_source = mode_source;\n', '')
    s = replace(s, '\t\tvis_reverse_override_active = true;', '\t\tvis_reverse_override_active = single_handle_mode;')
    s = replace(s, 'auto request_mode_selection = [&](ModeSelection selection,\n\t\tconst char* source,\n\t\tPhysicalModeSource mode_source = PhysicalModeSource::None)',
        'auto request_mode_selection = [&](ModeSelection selection, const char* source)')
    s = span(s, '\t\tif (mode_source == PhysicalModeSource::None)', '\t\tif (spacing_recovery.active() || spacing_recovery.requested)', '')
    s = s.replace('\t\t\tpending_physical_mode_source = mode_source;\n', '')
    s = s.replace('apply_mode_selection(selection, mode_source);', 'apply_mode_selection(selection);')
    s = re.sub(r'^\s*(?:pending_physical_mode_source|physical_mode_source) = PhysicalModeSource::None;\n', '', s, flags=re.M)
    s = span(s, '\t\tconst bool catheter_b7_pressed =', '\t\tif (pending_mode_selection', '')
    s = s.replace('\t\t\tconst PhysicalModeSource mode_source = pending_physical_mode_source;\n', '')
    s = span(s, '\t\tconst bool physical_mode_switch_allowed =', '\t\tconst bool startup_sequence_active', '''		// 换手/屈曲恢复期间冻结方向；结束后按两只B6的最新电平统一决定。
		if (!single_handle_mode && !planned_return.active() &&
			!spacing_recovery.active() && !spacing_recovery.requested && !spacing_recovery_button_pressed)
		{
			dual_reverse_active = catheter_b6_pressed || guidewire_b6_pressed;
		}
		if (!single_handle_mode)
		{
			axis1_crawl.start_abs = axis1_window_left_abs();
			axis1_crawl.end_abs = axis1_window_right_abs();
		}
		bool axis1_reverse_pressed = !single_handle_mode ? dual_reverse_active
			: (vis_reverse_override_active ? vis_reverse_override_value : catheter_b6_pressed);
		GuidewireMode requested_guidewire_mode = !single_handle_mode ? GuidewireMode::None
			: (vis_reverse_override_active
				? (vis_reverse_override_target == 0 ? GuidewireMode::None : GuidewireMode::Independent)
				: single_handle_requested_mode);
		if (spacing_recovery.active())
		{
			requested_guidewire_mode = GuidewireMode::None;
			(void)cancel_active_return_motion(true);
		}
		bool axis6_effective_reverse_pressed = axis1_reverse_pressed;
''')
    s = replace(s, ': (physical_mode_source == PhysicalModeSource::Guidewire ||\n\t\t\t\t\tguidewire_mode == GuidewireMode::Independent);',
        ': guidewire_mode == GuidewireMode::Independent;')
    s = s.replace('\t\t\tspacing_recovery.restore_physical_mode_source = static_cast<int>(physical_mode_source);\n', '')
    s = s.replace('B6 不再承担暂停/电缸5语义，而只作为所选手柄的方向电平。', '任一B6按住为全局撤出，两只均松开为递送。')
    s = replace(s, '\t\t// 2) 正反键切换：启用一次性触发保护，不再执行线性重同步。', '''		if (!single_handle_mode && axis1_reverse_pressed != axis1_reverse_pressed_prev)
		{
			// 丢弃切换前的滤波尾量，避免换向本身触发换手。
			axis1_prev_linear_filtered = axis1_handle_filter.axis0_filtered;
			axis6_prev_linear_filtered = axis6_handle_filter.axis0_filtered;
			std::cout << "全局方向：" << (axis1_reverse_pressed ? "撤出。" : "递送。") << std::endl;
		}
		// 单手柄调试保留原有触发保护。''')
    s = replace(s, '\t\tif (!estop_hold_active && !startup_sequence_active && control_active &&\n\t\t\t!spacing_recovery.active())',
        '\t\tif (single_handle_mode && !estop_hold_active && !startup_sequence_active && control_active &&\n\t\t\t!spacing_recovery.active())')
    s = s.replace('// 5) 导丝模式切换：由对应手柄 B7 或 UI 请求进入。', '// 5) 单手柄调试模式切换。')
    s = s.replace('\t\tbool physical_mode_transition_rejected = false;\n', '')
    s = replace(s, '\t\tif (requested_guidewire_mode != requested_guidewire_mode_prev)',
        '\t\tif (single_handle_mode && requested_guidewire_mode != requested_guidewire_mode_prev)')
    s = replace(s, '''							if (!physical_mode_request_pending)
							{
								guidewire_mode = GuidewireMode::None;
								axis6_crawl.enabled = false;
								axis6_window_locked = false;
							}
							// B7 请求在退出导丝时的 ADS 重同步失败，必须进入统一失败回滚；
							// 否则物理模式源已切到导管，但请求会永久挂起。
							physical_mode_transition_rejected = physical_mode_request_pending;
''', '''							guidewire_mode = GuidewireMode::None;
							axis6_crawl.enabled = false;
							axis6_window_locked = false;
''')
    s = re.sub(r'^\s*physical_mode_transition_rejected = physical_mode_request_pending;\n', '', s, flags=re.M)
    s = re.sub(r'\n\s*if \(\(!mode_attempted && mode_rejected\) \|\| \(mode_attempted && !mode_ok\)\)\n\s*\{\n\s*\}', '', s)
    s = span(s, '\t\tif (physical_mode_request_pending)', '\n\t\t// 6) 自检',
        '\t\trequested_guidewire_mode_prev = requested_guidewire_mode;\n')
    s = replace(s, '\t\t\t\t\tcfg.catheter_axis6_window_size_mm;',
        '\t\t\t\t\tmotion_sync::axis6_window_size_mm(ctx) +\n'
        '\t\t\t\t\t(!single_handle_mode && !dual_reverse_active ? cfg.relocation_inset_mm : 0.0);')
    s = span(s, '\t\t\t\t// 双手柄同时控制（不区分递送/撤出）：', '\t\t\t\tconst double tol',
        '\t\t\t\t// 两轴共同受g约束；只裁掉越界增量，不让静止手柄对应的轴被动跟随。\n')
    s = replace(s, '\t\t\t\tconst double gmax = gmin + cfg.catheter_axis6_window_size_mm;',
        '\t\t\t\tconst double gmax = gmin + motion_sync::axis6_window_size_mm(ctx);')
    s = span(s, '\t\t\t\t// 方向按钮（B6 按住 / 界面按钮）', '\t\t\t\t// 限位端相对窗口端点',
        '\t\t\t\tconst bool reverse = axis1_reverse_pressed;\n\t\t\t\tconst bool axis6_reverse = reverse;\n')
    s = span(s, '\t\t\t\tconst double abs6_lo =', '\n\t\t\t\taxis1_follow_cmd_abs = cmd1;', '''				const double abs6_soft = plc_leftlimit[5] + cfg.axis6_soft_limit_from_left_mm;
				const double g_start = (cmd6_start - plc_leftlimit[5]) - a5_left_cmd;
				double d1 = inc1;
				if (axis1_window_ok)
				{
					if (d1 > 0.0) d1 = (std::min)(d1, (std::max)(hi1 - cmd1_start, 0.0));
					else d1 = (std::max)(d1, (std::min)(lo1 - cmd1_start, 0.0));
				}
				double d6 = inc6 > 0.0
					? (std::min)(inc6, (std::max)(abs6_soft - cmd6_start, 0.0)) : inc6;
				const double requested_gap_delta = d6 - d1;
				// 已在窗外时只允许保持或回到窗内，不因缩窗自行跳变。
				const double gap_delta_hi = (std::max)(g_wire_hi - g_start, 0.0);
				const double gap_delta_lo = (std::min)(g_wire_lo - g_start, 0.0);
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
				const double cmd1 = cmd1_start + d1;
				const double cmd6 = cmd6_start + d6;
				const double g_now = g_start + d6 - d1;
''')
    s = span(s, '\t\t\t\t// 触发判定：', '\t\t\t\tconst bool hit_g_hi', '''				// 按截断前的净间距变化识别触边；已经离开端点的输入不能再次换手。
				const bool g_toward_hi = requested_gap_delta > 1e-9;
				const bool g_toward_lo = requested_gap_delta < -1e-9;
				const bool wire_hit_hi = (g_now >= g_wire_hi - tol) && axis6_reverse && inc6 > 0.0 && g_toward_hi;
				const bool wire_hit_lo = (g_now <= gmin + tol) && !axis6_reverse && inc6 < 0.0 && g_toward_lo;
				const bool cath_hit_hi = (g_now >= g_wire_hi - tol) && !reverse && inc1 < 0.0 && g_toward_hi;
				const bool cath_hit_lo = (g_now <= gmin + tol) && reverse && inc1 > 0.0 && g_toward_lo;
''')
    s = s.replace('// 重定位目标：对侧端点内缩 inset。axis3/5 在事务中保持，g 以当前 axis5 实际位置计算。\n\t\t\t\t\t// 重定位到对侧限位端（已内缩 inset）。',
        '// 换手时axis3/5保持，g目标以axis5当前实际位置计算。')
    s = replace(s, '\t\t\t\tconst PhysicalModeSource restore_source =\n\t\t\t\t\tstatic_cast<PhysicalModeSource>(spacing_recovery.restore_physical_mode_source);\n', '')
    s = replace(s, '\t\t\t\tapply_mode_selection(restore_selection, restore_source);\n\t\t\t\tstd::cout << "屈曲恢复退出后已恢复原模式。" << std::endl;',
        '\t\t\t\tif (single_handle_mode) apply_mode_selection(restore_selection);\n'
        '\t\t\t\telse vis_reverse_override_active = false;\n'
        '\t\t\t\tstd::cout << "屈曲恢复退出后，双手柄方向按当前B6状态生效。" << std::endl;')
    s = replace(s, '\t\t\tvs.physical_button_event_counter = physical_button_event_counter;', '\t\t\tvs.physical_button_event_counter = 0;')
    s = replace(s, '\t\t\tvs.physical_button_event_code = physical_button_event_code;', '\t\t\tvs.physical_button_event_code = 0;')
    s = replace(s, '\t\t\t\tcase VisCommandType::SetReverseMode:\n\t\t\t\t{',
        '\t\t\t\tcase VisCommandType::SetReverseMode:\n\t\t\t\t{\n'
        '\t\t\t\t\tif (!single_handle_mode)\n\t\t\t\t\t{\n'
        '\t\t\t\t\t\tstd::cout << "双手柄方向由两只B6统一控制。" << std::endl;\n'
        '\t\t\t\t\t\tbreak;\n\t\t\t\t\t}')
    assert 'PhysicalModeSource' not in s and 'physical_mode_' not in s
    return s

edit(b/'main.cpp', main)

edit(b/'remote_gateway.cpp', lambda s: replace(s, '\t\t\tif (ff_zeroing_.load()) flags |= rp::kFlagFfZeroing;',
    '\t\t\tif (ff_zeroing_.load()) flags |= rp::kFlagFfZeroing;\n\t\t\tif (v.axis1_reverse) flags |= rp::kFlagGlobalReverse;'))
for base in [b/'protocol', package/'B_从端电脑/工程/protocol', package/'A_主端电脑_地址修正版/工程/protocol']:
    edit(base/'cpp/remote_protocol.h', lambda s: replace(s, '\tkFlagFfZeroing = 1u << 10,',
        '\tkFlagFfZeroing = 1u << 10,\n\tkFlagGlobalReverse = 1u << 11,'))
edit(a/'MasterConsole.Protocol/ProtocolConstants.cs', lambda s: replace(s, '        FfZeroing = 1u << 10,',
    '        FfZeroing = 1u << 10,\n        GlobalReverse = 1u << 11,'))

def master_vm(s):
    s = re.sub(r'^.*public bool Is(?:Catheter|Guidewire)Mode =>.*\n', '', s, flags=re.M)
    return replace(s, '_stats.CatheterReversePressed ? "撤出" : "递送"', 'Has(StatusFlags.GlobalReverse) ? "撤出" : "递送"')
edit(a/'MasterConsole/ViewModels/MainViewModel.cs', master_vm)
edit(a/'MasterConsole/Services/IRobotLink.cs', lambda s: replace(s, '        public bool CatheterReversePressed { get; set; }\n', ''))
edit(a/'MasterConsole/Services/RemoteRobotLink.cs', lambda s: replace(s,
    '                    // 导管/轴1固定使用物理 587（ControlFrame.HandleB）。\n                    CatheterReversePressed = _handles != null && ((_handles.GetSample(1).Buttons & 0x40) != 0),\n', ''))

def local_vm(s):
    s = re.sub(r'^.*_physicalButtonNoticeText =.*\n', '', s, flags=re.M)
    s = s.replace('\t\t\t\tif (state.physical_button_event_counter != 0)\n', '')
    s = re.sub(r'^.*OnPropertyChanged\(nameof\((?:Mode(?:Cath|Guide)(?:Fwd|Rev)Selected|PhysicalButtonNoticeText)\)\);\n', '', s, flags=re.M)
    s = re.sub(r'\n\s*if \(prev.physical_button_event_counter != state.physical_button_event_counter\)\n\s*\{\n\s*\}', '', s)
    s = replace(s, '                string mode = _state.guidewire_mode == 0 ? "导管" : "导丝";\n                bool rev = _state.guidewire_mode == 0 ? _state.axis1_reverse : _state.axis6_reverse;\n                return mode + (rev ? "撤出" : "递送");',
        '                return _state.axis1_reverse ? "撤出" : "递送";')
    s = span(s, '        public bool ModeCathFwdSelected', 'public bool Axis6SoftLimitHold', '        ')
    s = replace(s, '        public void SetMode(int guidewireMode, int reverse) =>\n            _client.SendCommand(VisCommandType.SetReverseMode, guidewireMode, reverse);\n\n', '')
    return s
edit(b/'AdsControlUI/AdsControlViewModel.cs', local_vm)
edit(b/'AdsControlUI/MainWindow.xaml', lambda s: span(s, '                <GroupBox Header="模式选择">', '                         <!-- 屈曲恢复',
    '                <GroupBox Header="全局方向">\n                    <StackPanel>\n'
    '                        <TextBlock Text="任一B6按住：撤出；两只均松开：递送。"\n'
    '                                   Margin="6" TextWrapping="Wrap"/>\n'
    '                        <TextBlock Text="换手中切换方向，完成后生效。"\n'
    '                                   Margin="6,0,6,4" TextWrapping="Wrap"/>\n\n'))
edit(b/'AdsControlUI/MainWindow.xaml.cs', lambda s: re.sub(r'^.*private void Mode(?:Cath|Guide)(?:Fwd|Rev)_Click.*\n', '', s, flags=re.M))
