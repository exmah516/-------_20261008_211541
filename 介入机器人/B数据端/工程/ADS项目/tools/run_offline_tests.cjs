const fs = require('fs');
const path = require('path');
const os = require('os');
const cp = require('child_process');
// 抽取生产分支，用内存替身验证运动与通信，不加载设备DLL。
const project = path.resolve(__dirname, '..');
const selected = process.argv[2] || 'all';
if (!['all', 'dual', 'ads'].includes(selected)) throw Error('Usage: node run_offline_tests.cjs [all|dual|ads] [report-directory]');
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
if (selected !== 'ads') {
    const source = read('main.cpp'), header = read('control_types.h'), sync = read('motion_sync.cpp');
    const parts = {
        motion: between(source,'\t\t\t\tconst double tol = cfg.crawl_arrive_tol_mm;','\n\t\t\t}\n\t\t\telse\n\t\t\t{\n\t\t\t\t// 常规导管模式：'),
        cancel: between(source,'\t\tif ((planned_return.active() || dual_handoff.active()) &&','\t\t// 正式控制阶段：'),
        axis4_pause: between(source,'\t\tconst bool axis4_paused_by_axis6 =','\t\tads_output.axis4_forward_req'),
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
if (selected !== 'dual') {
    const entries = read('tools/ads_symbol_sizes.tsv').split('\n').filter(l=>l && !l.startsWith('#')).map(l=>{
        const [name,size] = l.split('\t');
        if (!name || !/^\d+$/.test(size)) throw Error('Invalid symbol size: '+l);
        return '{"'+name+'", '+size+'},';
    });
    fs.writeFileSync(path.join(build,'ads_symbol_sizes.inc'),entries.join('\n'),'utf8');
    const includes = ['','ADS/Include','手柄','手柄/include','protocol/cpp'].map(p=>'/I"'+path.join(project,p)+'"').join(' ');
    run('ads_diagnostics_test',['tools/ads_diagnostics_test.cpp','plc_io.cpp'], '/Gy /MD '+includes);
}
console.log('Offline test artifacts: '+build);
