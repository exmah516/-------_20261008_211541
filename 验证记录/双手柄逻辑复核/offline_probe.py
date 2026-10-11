"""提取实际 C++ 分支离线验证；不加载 ADS 或手柄 SDK，不修改生产文件。"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import tempfile

here = Path(__file__).resolve().parent
package = here.parents[1]
project = package / 'B_从端电脑/工程/ADS项目'
source = (project / 'main.cpp').read_text(encoding='utf-8-sig')
header = (project / 'control_types.h').read_text(encoding='utf-8-sig')

def between(text, start, end):
    begin = text.index(start)
    return text[begin:text.index(end, begin)]

build = Path(tempfile.mkdtemp(prefix='dual-control-review-'))
parts = {
    'recovery_type': between(header, 'enum class SpacingRecoveryPhase', '// 启动准备是在'),
    'mode': between(source, '\tbool vis_reverse_override_active =', '\n\tauto request_mode_selection'),
    'direction': between(source, '\t\tconst PhysicalModeSource effective_physical_mode_source =', '\t\tconst bool startup_sequence_active'),
    'save': between(source, '\t\tif (spacing_recovery_button_press_edge)', '\t\tif (spacing_recovery_button_release_edge)'),
    'exit': between(source, '\t\t\t\tconst ModeSelection restore_selection =', '\t\t\t\tstd::cout << "屈曲恢复退出后'),
    'triggers': between(source, '\t\t\t\tconst bool g_toward_hi =', '\t\t\t\tif ((want_axis1_leg'),
    'increments': between(source, '\t\t\t\tconst double cmd1_start = axis1_follow_cmd_abs;', '\n\t\t\t\taxis1_follow_cmd_abs = cmd1;'),
}
for name, text in parts.items():
    (build / (name + '.inc')).write_text(text, encoding='utf-8')

cpp = r'''
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <array>
using DWORD = unsigned long;
enum class GuidewireMode { None, Independent };
#include "recovery_type.inc"
static void check(bool ok, const char* name) {
    std::cout << (ok ? "PASS: " : "FAIL: ") << name << std::endl;
    if (!ok) std::exit(1);
}
static bool probe_recovery(unsigned char entry_buttons, bool prior_ui_override) {
    bool single_handle_mode = false;
    GuidewireMode guidewire_mode = GuidewireMode::None;
    GuidewireMode single_handle_requested_mode = GuidewireMode::None;
    SpacingRecoveryState spacing_recovery;
    auto clear_axis1_delivery_mapping = []() {};
    auto arm_axis1_delivery_mapping = []() {};
    auto clear_cylinder_manual_overrides = []() {};
    auto cancel_active_return_motion = [](bool) { return true; };
#include "mode.inc"
    if (prior_ui_override) apply_mode_selection(ModeSelection::CatheterDelivery);
    auto sample = [&](unsigned char cat, unsigned char wire, bool press_edge) {
        bool catheter_b6_pressed = (cat & 0x40) != 0;
        bool guidewire_b6_pressed = (wire & 0x40) != 0;
        bool spacing_recovery_button_press_edge = press_edge;
#include "direction.inc"
#include "save.inc"
        return axis1_reverse_pressed || guidewire_b6_pressed;
    };
    check(!sample(0, 0, false), "before recovery: delivery");
    sample(entry_buttons, 0, true);
    spacing_recovery.phase = SpacingRecoveryPhase::Active;
    sample(entry_buttons, 0, false);
    spacing_recovery.phase = SpacingRecoveryPhase::ExitSync;
    spacing_recovery.requested = false;
    // 假设退出重同步成功，仅执行其后的生产状态恢复代码。
    double plc_act_pos[7] = {}, plc_init_pos[7] = {}, plc_leftlimit[7] = {};
    struct { double axis3_delivery_stop_from_left_mm = 20, axis3_delivery_release_hysteresis_mm = 2; } cfg;
    bool axis1_delivery_stop_latched = false, axis1_delivery_stop_prompted = false;
    bool axis1_push_rearm_after_hold = false, control_active = false;
#include "exit.inc"
    check(!spacing_recovery.active() && control_active, "exit completed; not a recovery-phase gate");
    bool all_released_still_reverse = sample(0, 0, false);
    bool catheter_button_works = sample(0x40, 0, false) != sample(0, 0, false);
    for (int i = 0; i < 10; ++i) {
        sample(0x40, 0, false);
        check(sample(0, 0, false) == all_released_still_reverse, "B6 release result remains latched");
    }
    std::cout << "RESULT entry=" << int(entry_buttons) << " prior_ui=" << prior_ui_override
              << " override_active=" << vis_reverse_override_active
              << " reverse_with_both_released=" << all_released_still_reverse
              << " catheter_button_works=" << catheter_button_works << std::endl;
    check(!catheter_button_works, "BUG REPRODUCED: recovery exit disables catheter B6 level control");
    return all_released_still_reverse;
}
static void probe_trigger(double g_now, double inc1, double inc6, bool expected_axis6, const char* label) {
    const double gmin=0, g_wire_hi=29, tol=0.2, merge_eps=1.5;
    const double cmd1=18, w1_left=8, w1_right=28;
    const bool reverse=false, axis6_reverse=false, axis1_window_ok=true;
#include "triggers.inc"
    check(want_axis6_leg == expected_axis6, label);
}
static std::array<double, 3> probe_increments(double gap, double inc1, double inc6) {
    const double axis1_follow_cmd_abs=18, axis6_follow_cmd_abs=100+gap, axis5_base_rel=100;
    const double plc_init_pos[7]={}, plc_leftlimit[7]={};
    const double gmin=0, gmax=26, inset=3, lo1=8, hi1=31;
    const bool axis6_reverse=false, axis1_window_ok=true;
    struct { double axis6_soft_limit_from_left_mm=670; } cfg;
    struct { double base_rel=18; } axis1_crawl;
#include "increments.inc"
    return {d1, cmd6-cmd6_start, g_now};
}
int main() {
    check(probe_recovery(0x47, false), "BUG REPRODUCED: recovery+B6 pattern stays reverse with both buttons released");
    check(!probe_recovery(0x07, false), "plain recovery restores delivery but disables catheter direction input");
    check(!probe_recovery(0x47, true), "previous patch only protects an existing delivery override");
    probe_trigger(0, 1, 0, false, "passive near-end with still wire does not trigger axis6 in delivery");
    probe_trigger(0, 1, -0.03, true, "small accepted wire input turns passive contact into active wire return");
    probe_trigger(29, -1, 0, true, "passive far-end while pushing catheter triggers axis6 relocation");
    auto blocked = probe_increments(0, 1, 0);
    check(blocked[0] == 0 && blocked[1] == 0, "passive near-end blocks catheter pull and preserves still wire");
    auto forward = probe_increments(0, -1, 0);
    check(forward[0] == -1 && forward[1] == 0, "passive near-end permits catheter push without moving wire");
    auto both = probe_increments(0, -1, -1);
    check(both[0] == -1 && both[1] == 0 && both[2] == 1,
          "BUG REPRODUCED: sequential clipping loses legal equal forward increments at near boundary");
}
'''
(build / 'probe.cpp').write_text(cpp, encoding='utf-8')
vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
script = build / 'build.cmd'
script.write_text('@echo off\nchcp 65001 >nul\n'
    f'call "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
    'if errorlevel 1 exit /b 1\n'
    'cl /nologo /std:c++17 /EHsc /utf-8 probe.cpp /Fe:probe.exe\n'
    'if errorlevel 1 exit /b 1\nprobe.exe\nexit /b %errorlevel%\n', encoding='utf-8')
result = subprocess.run(['cmd.exe', '/d', '/c', str(script)], cwd=build, capture_output=True)
output = result.stdout.decode('utf-8', errors='replace') + result.stderr.decode('utf-8', errors='replace')
print(output)
(here / 'offline_probe.log').write_text(output, encoding='utf-8')
(here / 'source_hashes.json').write_text(json.dumps({
    str(p.relative_to(package)): hashlib.sha256(p.read_bytes()).hexdigest()
    for p in (project / 'main.cpp', project / 'control_types.h')
}, indent=2, ensure_ascii=False), encoding='utf-8')
print('Generated C++ and executable:', build)
raise SystemExit(result.returncode)
