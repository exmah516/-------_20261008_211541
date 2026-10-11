"""抽取生产双手柄分支做离线测试；理想 PLC 只执行 follow 或完成一次换手，不连接设备。"""
from pathlib import Path
import hashlib
import json
import os
import re
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

block = between(source, '\t\t\t\tconst double tol = cfg.crawl_arrive_tol_mm;',
                '\n\t\t\t}\n\t\t\telse\n\t\t\t{\n\t\t\t\t// 常规导管模式：')
# 仅测试副本的候选策略：让位到近端；或让位时合并轴1并按两窗口行程差对齐。
near_block = block.replace('(0.5 * (gmin + gmax))', '(gmin + inset)')
aligned_block = block.replace('const bool want_axis1_leg = do_1_left || do_1_right;',
    'const bool want_axis1_leg = do_1_left || do_1_right || cath_hit_hi;')
aligned_block = aligned_block.replace('do_1_left ? hi1 : lo1', '(do_1_left || cath_hit_hi) ? hi1 : lo1')
aligned_block = aligned_block.replace('(0.5 * (gmin + gmax))',
    '(std::max(gmin, g_wire_hi - (hi1 - w1_left)))')
assert near_block != block and aligned_block != block

build = Path(tempfile.mkdtemp(prefix='dual-window-probe-'))
for name, text in [('production', block), ('near', near_block), ('aligned', aligned_block)]:
    (build / (name + '.inc')).write_text(text, encoding='utf-8')
fields = sorted(set(re.findall(r'cfg\.(\w+)', block)) | {'linear_increment_noise_deadband_mm'})
cfg_text = '\n'.join('double ' + name + ' = ' + re.search(
    rf'double\s+{name}\s*=\s*([^;]+)', header)[1] + ';' for name in fields)
# 本测试旋转输入为零，旋转比例不影响线性分支。
cfg_text = cfg_text.replace('-Rad', '-1.0')
(build / 'config.inc').write_text(cfg_text, encoding='utf-8')

cpp = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
using ULONGLONG = unsigned long long;
enum class GuidewireMode { None };
enum class PlannedReturnMode { Dual };
enum class PlannedReturnRebaseScope { Both };
struct Config {
#include "config.inc"
};
struct Plant { double a1=28, a5=615, a6=628; };
struct Outcome {
    Plant after;
    bool transaction=false, leg1=false, leg6=false;
    double target1=0, target6=0;
};
static void require(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL " << message << std::endl; std::exit(1); }
}
static bool close(double a, double b) { return std::abs(a-b)<1e-7; }
static Outcome tick(Plant p, double width1, double width6, double raw1, double raw6,
                    bool reverse=false, int axis4=0, int candidate=0) {
    Config cfg;
    cfg.catheter_axis6_window_size_mm = width6;
    const auto gate = [&](double d) { return std::abs(d)>=cfg.linear_increment_noise_deadband_mm ? d : 0.0; };
    const double axis1_linear_increment_mm=gate(raw1), axis6_linear_increment_mm=gate(raw6);
    const bool axis1_reverse_pressed=reverse, guidewire_b6_pressed=false;
    const auto guidewire_mode=GuidewireMode::None;
    const bool axis4_forward_request=axis4<0, axis4_reverse_request=axis4>0;
    bool axis4_axis6_coupling_active_prev=true;
    ULONGLONG axis4_axis6_coupling_last_ms=1000;
    const double axis4_coupled_axis6_speed_mm_s=10;
    auto GetTickCount64=[]() { return ULONGLONG(1010); };
    struct Crawl {
        double base_rel, start_abs, end_abs, rot_base_rel=0, rot_ref=0;
        bool window_active=true;
        double min_abs() const { return start_abs; }
        double max_abs() const { return end_abs; }
    } axis1_crawl{p.a1,8,8+width1};
    double axis1_follow_cmd_abs=p.a1, axis6_follow_cmd_abs=p.a6;
    const double axis1_abs=p.a1, axis5_abs=p.a5, axis6_abs=p.a6, axis5_base_rel=p.a5;
    const double plc_init_pos[7]={}, plc_leftlimit[7]={};
    double pos[7]={p.a1,0,0,0,p.a5,p.a6,0}, axis2_hold_rel=0;
    const double axis1_rot_filtered=0;
    const bool axis3_delivery_stop_active=false;
    auto is_within_range=[](double x,double lo,double hi,double tol) { return x>=lo-tol && x<=hi+tol; };
    auto capture_axis1_follow_baseline=[]() {};
    auto apply_axis1_mirror_from_abs=[&](double x,bool) { pos[4]=p.a5+(x-p.a1); };
    auto compute_axis7_cmd_rel=[]() { return 0.0; };
    struct { int cyl1_open=0, cyl2_clamp=1800, cyl3_open=0, cyl4_clamp=1800; } cyl;
    int cylinder1_cmd=0,cylinder2_cmd=0,cylinder3_cmd=0,cylinder4_cmd=0;
    struct { bool active() const { return false; } } planned_return;
    Outcome out{p};
    auto begin_planned_return=[&](PlannedReturnMode,PlannedReturnRebaseScope,
                                bool a1,double t1,bool a6,double t6) {
        out.transaction=true; out.leg1=a1; out.leg6=a6; out.target1=t1; out.target6=t6;
        return true;
    };
    auto apply_planned_return_outputs=[]() {};
    if (candidate==1) {
#include "near.inc"
    } else if (candidate==2) {
#include "aligned.inc"
    } else {
#include "production.inc"
    }
    // 触发拍实际发出的是保持/夹持指令，不能把本拍候选 follow 位移计作已递送。
    // 换手期间轴5保持；结束后所有基线重建，下一拍从实际位置继续。
    if (out.transaction) {
        if (out.leg1) out.after.a1=out.target1;
        if (out.leg6) out.after.a6=out.target6;
    } else out.after={pos[0],pos[4],pos[5]};
    return out;
}
static void emit_case(const char* name,Plant p,double d1,double d6,
                      bool reverse=false,int axis4=0) {
    auto r=tick(p,20,26,d1,d6,reverse,axis4);
    std::cout << "CASE " << name << " input=" << d1 << '/' << d6
        << " start=" << p.a1 << '/' << p.a6-p.a5
        << " legs=" << r.leg1 << '/' << r.leg6
        << " end=" << r.after.a1 << '/' << r.after.a6-r.after.a5
        << " follow=" << (r.transaction?0:r.after.a1-p.a1) << '/'
        << (r.transaction?0:r.after.a6-p.a6) << '\n';
}
int main() {
    std::cout << std::fixed << std::setprecision(3);
    auto parallel=tick({18,615,615},20,26,-1,-1);
    require(!parallel.transaction && close(parallel.after.a1,17) && close(parallel.after.a6,615),
            "sequential clipping reproduction");
    auto near=tick({18,615,615},20,26,1,0);
    require(!near.transaction && close(near.after.a1,18),"passive near block");
    auto far=tick({18,615,644},20,26,-0.1,0);
    require(far.leg6 && !far.leg1 && close(far.after.a6-far.after.a5,13),"passive far midpoint");
    auto axis1=tick({8.2,615,628},20,26,-0.1,0);
    require(axis1.leg1 && !axis1.leg6 && close(axis1.after.a1,31),"axis1 near to far");
    auto wire=tick({18,615,615.2},20,26,0,-0.1);
    require(wire.leg6 && !wire.leg1 && close(wire.after.a6-wire.after.a5,29),"wire near to far");
    emit_case("catheter_push_mid",{18,615,628},-1,0);
    emit_case("catheter_pull_near",{18,615,615},1,0);
    emit_case("catheter_push_from_near",{18,615,615},-1,0);
    emit_case("catheter_push_passive_far",{18,615,644},-0.1,0);
    emit_case("catheter_push_axis1_near",{8.2,615,628},-0.1,0);
    emit_case("catheter_pull_axis1_far",{31,615,628},1,0);
    emit_case("wire_push_near",{18,615,615.2},0,-0.1);
    emit_case("wire_pull_far",{18,615,644},0,1);
    emit_case("both_push_equal_mid",{18,615,628},-1,-1);
    emit_case("both_push_equal_near",{18,615,615},-1,-1);
    emit_case("both_pull_equal_far",{18,615,644},1,1);
    emit_case("both_push_equal_at_far",{18,615,644},-0.1,-0.1);
    emit_case("wire_faster_immediately_after_return",{18,615,644},-0.05,-0.1);
    auto false_yield=tick(wire.after,20,26,-0.05,-0.1);
    require(false_yield.leg6 && close(false_yield.after.a6-false_yield.after.a5,13),
            "wire return immediately reversed by catheter far-edge misclassification");
    emit_case("cat_faster_far",{18,615,643.9},-0.2,-0.1);
    emit_case("wire_faster_near",{18,615,615.1},-0.1,-0.2);
    emit_case("cat_push_wire_pull_far",{18,615,643.9},-0.1,0.1);
    emit_case("cat_pull_wire_push_near",{18,615,615.1},0.1,-0.1);
    emit_case("idle_at_both_edges",{8,615,615},0,0);
    emit_case("below_noise_deadband",{18,615,615},1,-0.01);
    emit_case("small_wire_input",{18,615,615},1,-0.03);
    emit_case("wire_return_merges_still_cat",{9,615,615.2},0,-0.1);
    emit_case("both_direct_near",{8.2,615,615.1},-0.1,-0.2);
    emit_case("net_toward_near_but_merge_blocked",{8.2,615,616.4},-0.1,-0.2);
    emit_case("both_direct_opposite_edges",{8.2,615,643.9},-0.1,0);
    emit_case("axis4_reverse_overrides_delivery",{18,615,641},0,0,false,1);
    emit_case("actual_reverse_axis1_far",{28,615,628},0.1,0,true);
    emit_case("actual_reverse_wire_far",{18,615,641},0,0.1,true);
    emit_case("actual_reverse_passive_near",{18,615,615},0.1,0,true);
    int grid=0,back1=0,back6=0; std::map<std::string,int> types;
    for(double a1:{8.0,8.2,9.4,18.0,28.0,31.0})
    for(double gap:{0.0,0.1,0.2,1.4,13.0,27.6,28.8,29.0})
    for(double d1:{-1.0,-0.1,-0.03,-0.01,0.0,0.01,0.03,0.1,1.0})
    for(double d6:{-1.0,-0.1,-0.03,-0.01,0.0,0.01,0.03,0.1,1.0}) {
        Plant p{a1,615,615+gap}; auto r=tick(p,20,26,d1,d6); ++grid;
        if(r.leg1 && r.target1<p.a1-0.2) ++back1;
        if(r.leg6 && r.target6<p.a6-0.2) {
            ++back6;
            require(d1<=-0.02,"normal delivery far-to-near requires catheter push");
        }
        if(r.transaction) types[std::string(r.leg1?"axis1":"")+(r.leg6?"+axis6":"")+
            (r.leg6?(r.target6<p.a6?"_near":"_far"):"")]++;
    }
    require(back1==0,"no axis1 reverse reposition in true delivery");
    std::cout << "GRID " << grid << " axis1_far_to_near=" << back1 << " axis6_far_to_near=" << back6 << '\n';
    for(auto kv:types) std::cout << "TYPE " << kv.first << ' ' << kv.second << '\n';
    std::ofstream csv("trajectories.csv");
    csv << "scenario,width1,width6,candidate,step,initial_g,distance,transactions,axis1_legs,axis6_legs,delivered_cat,delivered_wire,blocked\n";
    const std::array<std::array<double,2>,5> patterns={{{-1,0},{0,-1},{-1,-1},{-1,-0.5},{-0.5,-1}}};
    const std::array<const char*,5> names={"catheter_only","wire_only","both_equal","catheter_faster","wire_faster"};
    for(int config=0;config<5;++config) for(int scenario=0;scenario<5;++scenario)
    for(double step:{0.05,0.1,0.2}) for(double initial_g:{3.0,13.0,20.0}) for(double distance:{100.0,230.0}) {
        const double width1=config==2?26:20, width6=config==1?20:26;
        const int candidate=config==3?1:config==4?2:0;
        Plant p{28,615,615+initial_g}; int transactions=0,legs1=0,legs6=0,stalled=0; double cat=0,wire=0;
        for(int k=0;k<100000 && (scenario==1?wire:cat)<distance-1e-7;++k) {
            const double remain=distance-(scenario==1?wire:cat);
            const double step_now=std::min(step,remain/(scenario==1?1.0:-patterns[scenario][0]));
            auto r=tick(p,width1,width6,step_now*patterns[scenario][0],step_now*patterns[scenario][1],false,0,candidate);
            if(r.transaction) { ++transactions; legs1+=r.leg1; legs6+=r.leg6; }
            else { cat+=p.a5-r.after.a5; wire+=p.a6-r.after.a6; }
            if(close(p.a1,r.after.a1)&&close(p.a5,r.after.a5)&&close(p.a6,r.after.a6)) ++stalled; else stalled=0;
            p=r.after;
            if(stalled>=10) break;
        }
        csv << names[scenario] << ',' << width1 << ',' << width6 << ',' << candidate << ',' << step << ',' << initial_g
            << ',' << distance << ',' << transactions << ',' << legs1 << ',' << legs6 << ',' << cat << ',' << wire << ',' << (stalled>=10) << '\n';
        if(step==0.1 && initial_g==13 && distance==230)
            std::cout << "TRAJECTORY " << names[scenario] << " width=" << width1 << '/' << width6 << " candidate=" << candidate
                << " stops=" << transactions << " legs=" << legs1 << '/' << legs6 << " delivered=" << cat << '/' << wire << " blocked=" << (stalled>=10) << '\n';
    }
    std::cout << "PASS: branch assertions, 3888 grid cases, 450 trajectories; reported defects are reproduced, not repaired.\n";
}
'''
(build / 'window_probe.cpp').write_text(cpp, encoding='utf-8')
vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
script = build / 'build.cmd'
script.write_text('@echo off\nchcp 65001 >nul\n'
    f'call "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
    'if errorlevel 1 exit /b 1\ncl /nologo /O2 /std:c++17 /EHsc /utf-8 window_probe.cpp /Fe:window_probe.exe\n'
    'if errorlevel 1 exit /b 1\nwindow_probe.exe\nexit /b %errorlevel%\n', encoding='utf-8')
result = subprocess.run(['cmd.exe','/d','/c',str(script)],cwd=build,capture_output=True)
output = (result.stdout+result.stderr).decode('utf-8',errors='replace').replace('\r\n','\n')
print(output)
(here / 'window_probe.log').write_text(output,encoding='utf-8')
if result.returncode==0:
    (here / 'window_trajectories.csv').write_bytes((build / 'trajectories.csv').read_bytes())
(here / 'window_source_hashes.json').write_text(json.dumps({
    str(p.relative_to(package)):hashlib.sha256(p.read_bytes()).hexdigest()
    for p in (project / 'main.cpp',project / 'control_types.h',project / 'motion_sync.cpp')
},ensure_ascii=False,indent=2),encoding='utf-8')
print('Generated isolated C++:',build)
raise SystemExit(result.returncode)
