const fs = require('fs');
const path = require('path');
const os = require('os');
const cp = require('child_process');
// 抽取生产分支，用内存替身验证运动与通信，不加载设备DLL。
const project = path.resolve(__dirname, '..');
const selected = process.argv[2] || 'all';
if (!['all', 'dual', 'recovery', 'cylinder', 'ads', 'handle'].includes(selected)) throw Error('Usage: node run_offline_tests.cjs [all|dual|recovery|cylinder|ads|handle] [report-directory]');
const report = process.argv[3] && path.resolve(process.argv[3]);
const build = fs.mkdtempSync(path.join(os.tmpdir(), 'handoff-offline-'));
if (report) fs.mkdirSync(report, {recursive:true});
const read = name => fs.readFileSync(path.join(project, name), 'utf8').replace(/^\uFEFF/, '').replace(/\r\n/g, '\n');
function between(text, start, end) {
    const lo = text.indexOf(start), hi = text.indexOf(end, lo);
    if (lo < 0 || hi < 0) throw Error('Missing extraction marker: ' + start);
    return text.slice(lo, hi);
}
const vswhere = path.join(process.env['ProgramFiles(x86)'], 'Microsoft Visual Studio', 'Installer', 'vswhere.exe');
const vs = cp.execFileSync(vswhere, ['-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'], {encoding:'utf8'}).trim();
function run(name, sources, flags = '') {
    const script = '@echo off\r\nchcp 65001 >nul\r\ncall "' + path.join(vs,'VC','Auxiliary','Build','vcvars64.bat') + '" >nul\r\nif errorlevel 1 exit /b 1\r\n'
        + 'cl /nologo /std:c++17 /EHsc /utf-8 /I"' + build + '" ' + flags + ' '
        + sources.map(s => '"' + path.join(project,s) + '"').join(' ') + ' /Fe:' + name + '.exe /link /OPT:REF\r\nif errorlevel 1 exit /b 1\r\n'
        + name + '.exe\r\nexit /b %errorlevel%\r\n';
    fs.writeFileSync(path.join(build,'build.cmd'),script,'utf8');
    const r = cp.spawnSync('cmd.exe',['/d','/c','build.cmd'],{cwd:build,encoding:'utf8'});
    const output = (r.stdout || '') + (r.stderr || '');
    process.stdout.write(output);
    fs.writeFileSync(path.join(build,name+'.log'),output,'utf8');
    if (report) fs.writeFileSync(path.join(report,name+'.log'),output,'utf8');
    if (r.error) throw r.error;
    if (r.status !== 0) { console.log('Artifacts: ' + build); process.exit(r.status || 1); }
}
if (selected === 'all' || selected === 'dual') {
    const source = read('main.cpp'), header = read('control_types.h'), sync = read('motion_sync.cpp');
    const parts = {
        axis4_speed: between(source,'\tconstexpr double axis4_coupled_axis6_speed_mm_s =','\n'),
        motion: between(source,'\t\t\t\tconst double tol = cfg.crawl_arrive_tol_mm;','\n\t\t\t}\n\t\t\telse\n\t\t\t{\n\t\t\t\t// 常规导管模式：'),
        cancel: between(source,'\t\tif ((planned_return.active() || dual_handoff.active()) &&','\t\t// 正式控制阶段：'),
        axis4_pause: between(source,'\t\tconst bool axis4_paused_by_axis6 =','\t\tads_output.axis4_forward_req'),
        mapping_helpers: between(source,'\tauto clear_axis1_delivery_mapping =','\tauto apply_axis1_mirror_from_abs ='),
        mapping_begin: between(source,'\t\t\t\t// 双手柄仅轴6换手时','\t\t\t\tplanned_return.phase_t0_ms'),
        single_mapping: between(source,'\t\t\t\t\t// 计划回退完成后，映射只作用于', '\t\t\t\t\tconst double axis1_effective_delta_axis_mm'),
        mapping_guard: between(source,'\t\tif (guidewire_mode != GuidewireMode::None ||','\t\t// 恢复键只登记请求'),
        mapping_complete: between(source,'\t\t\t\t\t\t\tif (planned_return.mode == PlannedReturnMode::CatheterDelivery ||', '\t\t\t\t\t\t\treset_planned_return();'),
        windows: between(sync,'\tdouble from_left_to_abs','\tvoid lock_axis6_window_from_current'),
        target_safety: between(source,'\t\t\tauto planned_return_targets_safe =','\t\t\tauto rebase_planned_return ='),
        recovery_type: between(header,'enum class SpacingRecoveryPhase','// 启动准备是在'),
        handoff_type: between(header,'struct DualHandoffRequest','enum class SpacingRecoveryPhase'),
        mode: between(source,'\tbool vis_reverse_override_active =','\n\tauto request_mode_selection'),
        direction: between(source,'\t\t// 换手/屈曲恢复期间冻结方向','\t\tconst bool startup_sequence_active'),
        save: between(source,'\t\tif (spacing_recovery_button_press_edge)','\t\t// 该映射只属于'),
        exit: between(source,'\t\t\t\tconst ModeSelection restore_selection =','\t\t\t\tstd::cout << "屈曲恢复退出后'),
        switch: between(source,'\t\tif (!single_handle_mode && axis1_reverse_pressed != axis1_reverse_pressed_prev)','\t\t// 单手柄调试保留'),
        filter: between(header,'struct HandleFilterState','struct ControlConfig')
    };
    const fields = new Set([...((parts.motion+parts.windows).matchAll(/cfg(?:\.|->)(\w+)/g))].map(m=>m[1]));
    for (const f of ['linear_increment_noise_deadband_mm','axis3_delivery_stop_from_left_mm','axis3_delivery_release_hysteresis_mm']) fields.add(f);
    parts.config = [...fields].sort().map(f => {
        const m = header.match(new RegExp('double\\s+'+f+'\\s*=\\s*([^;]+)'));
        if (!m) throw Error('Missing config: '+f);
        return 'double '+f+' = '+m[1]+';';
    }).join('\n').replace('-Rad','-1.0');
    for (const [name, content] of Object.entries(parts)) fs.writeFileSync(path.join(build,name+'.inc'),content,'utf8');
    run('dual_control_test',['tools/dual_control_test.cpp']);
    if (report && fs.existsSync(path.join(build,'trajectories.csv'))) fs.copyFileSync(path.join(build,'trajectories.csv'),path.join(report,'trajectories.csv'));
}
if (selected === 'all' || selected === 'recovery') {
    const source = read('main.cpp'), header = read('control_types.h');
    const parts = {
        types: between(header, 'struct ControlConfig', '// 启动准备是在'),
        cancel: between(source, '\tauto reset_planned_return =', '\tauto axis6_from_left_mm ='),
        guard: between(source, '\t\tif ((planned_return.active() || dual_handoff.active()) &&', '\t\t// 正式控制阶段：'),
        save: between(source, '\t\tif (spacing_recovery_button_press_edge)', '\t\t// 该映射只属于'),
        enter: between(source, '\t\t\tauto calculate_spacing_recovery_remaining =', '\t\t\tif (emergency_retract_active)'),
        output: between(source, '\t\t\tauto apply_planned_return_outputs =', '\t\t\tauto enter_planned_return_retry ='),
        refresh: between(source, '\t\t\tauto refresh_planned_return_leg =', '\t\t\tauto planned_return_targets_safe ='),
        failure: between(source, '\t\t\tauto fail_planned_return =', '\t\tauto begin_planned_return ='),
        retry: between(source, '\t\t\tauto enter_planned_return_retry =', '\t\t\tauto step_planned_return ='),
        step: between(source, '\t\t\tauto step_planned_return =', '\t\t\tauto compute_axis7_cmd_rel ='),
        reinit: between(source, '\t\tif (handle_reinit_req && !last_handle_reinit_req', '\t\t// 8) 当启动已完成'),
        reconnect: between(source, '\t\tif (ads_stats.reconnect_count != handled_reconnect_count)', '\t\t// 运动控制只依赖位置快照'),
        connection: between(source, '\t\tconst bool connection_hold_active =', '\t\tif (!initial_sync_done)'),
        exit: between(source, '\t\tconst DWORD spacing_recovery_exit_elapsed_ms =', '\t\tconst bool axis4_manual_error_now =')
    };
    for (const [name, content] of Object.entries(parts)) fs.writeFileSync(path.join(build,'return_recovery_'+name+'.inc'),content,'utf8');
    const activeRecovery = '\t\telse if (ads_soft_hold_active && planned_return.active())';
    const recoveryStart = source.includes(activeRecovery) ? activeRecovery : '\t\telse if ((ads_soft_hold_active';
    fs.writeFileSync(path.join(build,'main_rebase.inc'),between(source,recoveryStart,'\t\tconst bool connection_hold_active').replace('else if','if'),'utf8');
    fs.writeFileSync(path.join(build,'main_observe.inc'),between(source,'\t\tconst bool ads_self_check_valid','\t\tif (ads_events.selfcheck_status'),'utf8');
    fs.writeFileSync(path.join(build,'main_auto_resume.inc'),between(source,'\t\tif (!control_active && !return_ads_fault_hold','\t\tunsigned short cylinder1_cmd'),'utf8');
    fs.writeFileSync(path.join(build,'main_start.inc'),between(source,'\t\t\t\tcase VisCommandType::SelectDirectControl:','\t\t\t\t\tcase VisCommandType::SetGravityCompensation:').split('\n').slice(1).join('\n'),'utf8');
    run('main_recovery_test',['tools/main_recovery_test.cpp']);
    run('return_recovery_test',['tools/return_recovery_test.cpp']);
}
if (selected === 'all' || selected === 'cylinder') {
    const source = read('main.cpp'), ads = read('ads_communication.cpp'), header = read('ads_communication.h');
    const parts = {
        type: between(header, 'struct AdsOutputCommand', 'enum class AdsPlannedReturnOperation'),
        handles: between(ads, '\tenum FastWriteHandleIndex', '\tstruct NotificationTarget'),
        changed: between(ads, '\tbool same_discrete_output', '\tint planned_return_axis_slot'),
        write: between(ads, '\t\tif (output.cylinder_valid)', '\t\tappend(fast_write_handles_[kWriteCylinder5Press]'),
        clear: between(source, '\tauto clear_cylinder_manual_overrides =', '\tauto prepare_cylinder_manual_override ='),
        prepare: between(source, '\tauto prepare_cylinder_manual_override =', '\tbool vis_reverse_override_active ='),
        gates: between(source, '\t\tbool cylinder5_req =', '\t\t// 记录器直接消费'),
        output: between(source, '\t\tads_output.cylinder[0] =', '\t\tconst bool axis4_paused_by_axis6 =')
    };
    for (const [name, content] of Object.entries(parts)) fs.writeFileSync(path.join(build,'cylinder_'+name+'.inc'),content,'utf8');
    run('cylinder_manual_test',['tools/cylinder_manual_test.cpp']);
}
if (selected === 'all' || selected === 'ads') {
    const entries = read('tools/ads_symbol_sizes.tsv').split('\n').filter(l=>l && !l.startsWith('#')).map(l=>{
        const [name,size] = l.split('\t');
        if (!name || !/^\d+$/.test(size)) throw Error('Invalid symbol size: '+l);
        return '{"'+name+'", '+size+'},';
    });
    fs.writeFileSync(path.join(build,'ads_symbol_sizes.inc'),entries.join('\n'),'utf8');
    const includes = ['','ADS/Include','手柄','protocol/cpp'].map(p=>'/I"'+path.join(project,p)+'"').join(' ');
    run('ads_diagnostics_test',['tools/ads_diagnostics_test.cpp','plc_io.cpp'], '/Gy /MD '+includes);
}
if (selected === 'all' || selected === 'handle') {
    run('remote_handle_test', ['tools/remote_handle_test.cpp', '手柄/手柄.cpp']);
}
console.log('Offline test artifacts: '+build);
