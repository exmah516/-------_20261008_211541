// 生产分支由run_offline_tests.cjs抽取，设备读写仅用内存替身。
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
using DWORD = unsigned long;
using ULONGLONG = unsigned long long;
enum class GuidewireMode { None, Independent };
enum class PlannedReturnMode { Dual, CatheterDelivery, CatheterRetraction };
enum class PlannedReturnRebaseScope { Both };
struct Config {
#include "config.inc"
};
struct AppContext {
    Config* cfg;
    bool* dual_reverse;
    GuidewireMode* guidewire_mode;
    double* plc_leftlimit;
    double* plc_init_pos;
    double* plc_act_pos;
    double plc_act_velocity_mm_s[7] = {};
};
namespace motion_sync {
    void calculate_axis6_window_from_axis5_abs(const AppContext&, double, double&, double&);
#include "windows.inc"
}
#include "recovery_type.inc"
#include "filter.inc"
struct Crawl {
    double base_rel, start_abs, end_abs, rot_base_rel=0, rot_ref=0;
    bool window_active=true;
    double min_abs() const { return start_abs; }
    double max_abs() const { return end_abs; }
};
struct Transaction {
    bool running=false, axis1=false;
    PlannedReturnMode mode=PlannedReturnMode::Dual;
    bool active() const { return running; }
    bool contains_axis(int axis) const { return axis==0 && axis1; }
};
struct Plant { double a1=28, a5=615, a6=628, a3=0; };
#include "handoff_type.inc"
struct FollowSession {
    Plant command;
    double base1, base3, base5;
    DualHandoffRequest pending;
    bool mapping_active=false;
    double mapping_progress=0, mapping_extra=0;
    std::array<double,7> velocity{};
    explicit FollowSession(Plant p):command(p),base1(p.a1),base3(p.a3),base5(p.a5) {}
};
struct Outcome {
    Plant after, follow;
    bool transaction=false, leg1=false, leg6=false;
    double target1=0, target6=0;
};
static int cases=0;
static void require(bool ok, const char* name) {
    ++cases;
    if (!ok) { std::cerr << "FAIL " << name << std::endl; std::exit(1); }
}
static bool close(double a, double b) { return std::abs(a-b)<1e-7; }
static double clamp_double(double x,double lo,double hi) { return std::clamp(x,lo,hi); }
static Outcome tick(Plant p, double raw1, double raw6, bool reverse_input=false,
                    int axis4=0, bool window_active=true, FollowSession* session=nullptr) {
    Config cfg;
    auto gate=[&](double d) { return std::abs(d)>=cfg.linear_increment_noise_deadband_mm ? d : 0.0; };
    const double axis1_linear_increment_mm=gate(raw1), axis6_linear_increment_mm=gate(raw6);
    const bool axis1_reverse_pressed=reverse_input;
    bool axis1_delivery_mapping_active=session && session->mapping_active;
    double axis1_delivery_mapping_progress_mm=session?session->mapping_progress:0;
    double axis1_delivery_mapping_applied_extra_mm=session?session->mapping_extra:0;
#include "mapping_helpers.inc"
    // 与生产循环相同：撤出清除映射，镜像基准保留已执行的差额。
    if(reverse_input) clear_axis1_delivery_mapping();
    GuidewireMode guidewire_mode=GuidewireMode::None;
    const bool axis4_forward_request=axis4<0, axis4_reverse_request=axis4>0;
    bool axis4_axis6_coupling_active_prev=true;
    ULONGLONG axis4_axis6_coupling_last_ms=1000;
#include "axis4_speed.inc"
    auto GetTickCount64=[]() { return ULONGLONG(1010); };
    double plc_init_pos[7]={}, plc_leftlimit[7]={}, actual[7]={};
    actual[0]=p.a1; actual[2]=p.a3; actual[4]=p.a5; actual[5]=p.a6;
    AppContext ctx{&cfg,&reverse_input,&guidewire_mode,plc_leftlimit,plc_init_pos,actual};
    if (session) std::copy(session->velocity.begin(),session->velocity.end(),ctx.plc_act_velocity_mm_s);
    DualHandoffRequest local_handoff;
    auto& dual_handoff=session?session->pending:local_handoff;
    Crawl axis1_crawl{session?session->base1:p.a1,motion_sync::axis1_window_left_abs(ctx),motion_sync::axis1_window_right_abs(ctx)};
    axis1_crawl.window_active=window_active;
    double axis1_follow_cmd_abs=session?session->command.a1:p.a1, axis6_follow_cmd_abs=session?session->command.a6:p.a6;
    const double axis1_abs=p.a1, axis5_abs=p.a5, axis6_abs=p.a6;
    double axis3_base_rel=session?session->base3:p.a3,axis5_base_rel=session?session->base5:p.a5;
    double pos[7]={p.a1,0,p.a3,0,p.a5,p.a6,0}, axis2_hold_rel=0;
    const double axis1_rot_filtered=0;
    const bool axis3_delivery_stop_active=false;
    auto is_within_range=[](double x,double lo,double hi,double tol) { return x>=lo-tol && x<=hi+tol; };
    auto capture_axis1_follow_baseline=[]() {};
    auto apply_axis1_mirror_from_abs=[&](double x,bool) {
        pos[4]=axis5_base_rel+(x-axis1_crawl.base_rel);
        pos[2]=axis3_base_rel+(x-axis1_crawl.base_rel);
    };
    auto compute_axis7_cmd_rel=[]() { return 0.0; };
    struct { int cyl1_open=0, cyl2_clamp=1800, cyl3_open=0, cyl4_clamp=1800; } cyl;
    int cylinder1_cmd=0,cylinder2_cmd=0,cylinder3_cmd=0,cylinder4_cmd=0;
    Transaction planned_return;
    Outcome out{p,p};
    auto begin_planned_return=[&](PlannedReturnMode mode,PlannedReturnRebaseScope,
                                bool a1,double t1,bool a6,double t6) {
        const bool axis1_active=a1;
#include "mapping_begin.inc"
        out.transaction=true; out.leg1=a1; out.leg6=a6; out.target1=t1; out.target6=t6;
        return true;
    };
    auto apply_planned_return_outputs=[]() {};
#include "motion.inc"
    out.follow={pos[0],pos[4],pos[5],pos[2]};
    if (session) {
        session->command=out.follow;
        session->base3=axis3_base_rel; session->base5=axis5_base_rel;
        session->mapping_active=axis1_delivery_mapping_active;
        session->mapping_progress=axis1_delivery_mapping_progress_mm;
        session->mapping_extra=axis1_delivery_mapping_applied_extra_mm;
    }
    // 事务触发拍保持实际位置，理想PLC只完成重定位，不能把候选位移计入递送量。
    if (out.transaction) {
        if (out.leg1) out.after.a1=out.target1;
        if (out.leg6) out.after.a6=out.target6;
    } else out.after=out.follow;
    return out;
}

static void test_windows() {
    Config cfg;
    bool reverse=false;
    GuidewireMode mode=GuidewireMode::None;
    double left[7]={100,0,0,0,300,500,0}, init[7]={}, actual[7]={};
    AppContext ctx{&cfg,&reverse,&mode,left,init,actual};
    require(close(motion_sync::axis1_window_left_abs(ctx),105),"axis1 left reference 5");
    require(close(motion_sync::axis1_window_right_abs(ctx),130),"axis1 delivery right 30");
    double lo=0,hi=0;
    motion_sync::calculate_axis6_window_from_axis5_abs(ctx,915,lo,hi);
    require(close(lo,1116) && close(hi,1148),"wire delivery gap 1 to 33 with distinct left references");
    reverse=true;
    require(close(motion_sync::axis1_window_left_abs(ctx),105) &&
            close(motion_sync::axis1_window_right_abs(ctx),130),"axis1 retraction uses same 5 to 30 window");
    motion_sync::calculate_axis6_window_from_axis5_abs(ctx,915,lo,hi);
    require(close(lo,1116) && close(hi,1148),"wire retraction uses same 1 to 33 gap");
    ctx.dual_reverse=nullptr;
    require(close(motion_sync::axis6_window_size_mm(ctx),32),"single-handle helper compatibility");
    mode=GuidewireMode::Independent;
    motion_sync::calculate_axis6_window_from_axis5_abs(ctx,915,lo,hi);
    require(close(lo,1116) && close(hi,1148),"legacy wire helper uses same gap bounds");
}

static void test_passive_targets() {
    // 显式列出不同剩余行程的预期目标，覆盖3 mm余量和27 mm封顶。
    for (auto example : {std::array<double,2>{6,29}, {10,25}, {18,17},
                         {28,7}, {29,6}, {30,6}, {40,6}}) {
        auto r=tick({example[0],615,648},-0.1,0);
        require(r.leg6 && !r.leg1 && close(r.target6-615,example[1]),
                "delivery passive target follows axis1 remaining travel with 27mm cap");
    }
    for (auto example : {std::array<double,2>{5,28}, {6,28}, {7,27},
                         {18,16}, {25,9}, {29,5}}) {
        auto r=tick({example[0],615,616},0.1,0,true);
        require(r.leg6 && !r.leg1 && close(r.target6-615,example[1]),
                "retraction passive target follows axis1 remaining travel with 27mm cap");
    }
    auto r=tick({18,615,616},0,-0.1);
    require(r.leg6 && !r.leg1 && close(r.target6-615,28),"active delivery returns to g28");
    r=tick({18,615,648},0,0.1,true);
    require(r.leg6 && !r.leg1 && close(r.target6-615,6),"active retraction returns to g6");
    for (bool reverse : {false,true}) {
        const double end1=reverse?30:5, active_end=reverse?33:1;
        const double sign=reverse?1:-1;
        r=tick({end1,615,615+active_end},0,sign*0.1,reverse);
        require(r.leg6 && !r.leg1 && close(r.target6-615,reverse?6:28),
                "wire active handoff does not pull in axis1 at its endpoint");
        r=tick(r.after,sign*0.1,0,reverse);
        require(r.leg1 && !r.leg6,"axis1 handoff after wire active return cannot immediately re-merge wire");
        r=tick({end1,615,615+(reverse?1.0:33.0)},sign*0.1,0,reverse);
        require(r.leg1 && r.leg6 && close(r.target6-615,reverse?28:6),
                "passive merge uses 25mm of axis1 post-return travel and caps clearance at 27");
    }
}

static void test_one_way_merge() {
    auto r=tick({5,615,620},-0.1,0);
    require(r.leg1 && !r.leg6,"delivery at g5 must not merge wire near its active end");
    r=tick({5.1,615,643.1},-3,-0.2);
    require(r.leg1 && !r.leg6,"moving delivery inputs reaching exactly g28 do not merge");
    r=tick({29.9,615,620.9},3,0.2,true);
    require(r.leg1 && !r.leg6,"moving retraction inputs reaching exactly g6 do not merge");
    // 使用实际配置构造旧版也能触发的场景，防止只验证参数改动。
    Config cfg;
    const double lo=cfg.axis1_window_left_from_left_mm;
    const double gmin=cfg.catheter_axis6_window_min_gap_from_axis5_mm;
    r=tick({lo+1,615,615+gmin+0.2},0,-0.1);
    require(r.leg6 && !r.leg1,"wire-only handoff never pulls in nearby stationary axis1");

    for (bool reverse : {false,true}) {
        const double end1=reverse?30:5;
        const double passive_end=reverse?1:33, active_end=reverse?33:1;
        const double sign=reverse?1:-1;
        for (double distance : {0.,0.1,3.,4.99,5.,5.01}) {
            const double gap=passive_end+sign*distance;
            r=tick({end1,615,615+gap},sign*0.1,0,reverse);
            require(r.leg1 && r.leg6==(distance<5),"axis1 merges wire only within strict 5mm of passive end");
            if (r.leg6) require(close(r.target6-615,reverse?28:6),"merged wire uses passive target 6 or 28");
        }
        // 静止、趋近和远离的导丝输入均不改变已授权的距离判据。
        for (double d6 : {-0.1,0.,0.1}) {
            r=tick({end1,615,615+passive_end+sign*2},sign*0.1,d6,reverse);
            require(r.leg1 && r.leg6 && close(r.target6-615,reverse?28:6),
                    "axis1 merges nearby passive wire regardless of wire input");
        }
        for (double distance : {0.,0.1,2.,4.99,5.,10.,20.}) {
            r=tick({end1,615,615+active_end-sign*distance},sign*0.1,0,reverse);
            require(r.leg1 && !r.leg6,"axis1 cannot preempt wire active handoff at any non-passive location");
        }
        for (double axis1 : {4.,5.,5.2,6.,18.,29.,29.8,30.,31.}) {
            r=tick({axis1,615,615+active_end},0,sign*0.1,reverse);
            require(r.leg6 && !r.leg1,"wire handoff leaves stationary axis1 at any location");
        }
        r=tick({end1,615,615+active_end},sign*0.1,sign*0.2,reverse);
        require(r.leg1 && r.leg6 && close(r.target6-615,reverse?6:28),
                "independent active wire trigger keeps active target during axis1 handoff");
        r=tick({18,615,615+passive_end+sign*2},sign*0.1,0,reverse);
        require(!r.transaction,"near-passive-end alone cannot trigger an early handoff");
    }
}

static void test_target_safety() {
    Config cfg;
    bool reverse=false;
    GuidewireMode mode=GuidewireMode::None;
    double left[7]={100,0,0,0,300,500,0}, init[7]={}, actual[7]={};
    AppContext ctx{&cfg,&reverse,&mode,left,init,actual};
    const double axis5_abs=400;
    Crawl axis1_crawl{0,105,130}, axis6_crawl{0,601,633};
    struct PlannedReturnLeg { int axis_index; double target_abs; };
    struct { int leg_count=2; PlannedReturnLeg legs[2]={{0,105},{5,606}};
             PlannedReturnMode mode=PlannedReturnMode::Dual; } planned_return;
    auto is_within_range=[](double x,double lo,double hi,double tol) { return x>=lo-tol && x<=hi+tol; };
    auto axis6_target_exceeds_soft_limit=[&](double x) { return x>left[5]+cfg.axis6_soft_limit_from_left_mm; };
#include "target_safety.inc"
    for (bool rev : {false,true}) {
        reverse=rev;
        for (double a1 : {105.,130.})
        for (double a6 : {601.,604.,606.,616.,617.,628.,630.,633.}) {
            planned_return.legs[0].target_abs=a1;
            planned_return.legs[1].target_abs=a6;
            require(planned_return_targets_safe(),"handoff endpoints and passive target accepted in both modes");
        }
        planned_return.legs[1].target_abs=606;
        for (double a1 : {102.,133.}) {
            planned_return.legs[0].target_abs=a1;
            require(!planned_return_targets_safe(),"axis1 target cannot use former 3mm extension");
        }
        planned_return.legs[0].target_abs=105;
        for (double a6 : {600.,636.,1200.}) {
            planned_return.legs[1].target_abs=a6;
            require(!planned_return_targets_safe(),"wire target cannot use extension or exceed absolute soft limit");
        }
    }
}

static void test_examples() {
    auto r=tick({18,615,616},-1,-1);
    require(!r.transaction && close(r.after.a1,17) && close(r.after.a6,615),"equal push at near boundary");
    r=tick({18,615,648},-0.1,-0.1);
    require(!r.transaction && close(r.after.a6-r.after.a5,33),"equal push at far boundary does not yield");
    r=tick({18,615,648},-0.05,-0.1);
    require(!r.transaction && close(r.after.a6-r.after.a5,32.95),"wire faster after return must not reverse handoff");
    r=tick({18,615,648},-0.1,0);
    require(r.leg6 && !r.leg1 && close(r.after.a6-r.after.a5,17),"delivery passive target at axis1=18 is g17");
    r=tick({18,615,616},1,0);
    require(!r.transaction && close(r.after.a1,18) && close(r.after.a6,616),"passive near blocks only catheter pull");
    r=tick({18,615,616},-1,0);
    require(!r.transaction && close(r.after.a1,17) && close(r.after.a6,616),"passive near permits renewed catheter push");
    r=tick({5.2,615,628},-0.1,0);
    require(r.leg1 && !r.leg6 && close(r.target1,30),"delivery axis1 handoff uses endpoint without extension");
    r=tick({18,615,616.2},0,-0.1);
    require(!r.leg1 && r.leg6 && close(r.target6-615,28),"active wire delivery handoff");
    r=tick(r.after,-0.05,-0.1);
    require(!r.transaction,"active wire handoff cannot immediately become opposite passive handoff");
    r=tick({5,615,648},-0.1,0.1);
    require(r.leg1 && r.leg6 && close(r.target6-615,6),"axis1 at delivery endpoint merges far passive wire");
    r=tick({30,615,616},0.1,-0.1,true);
    require(r.leg1 && r.leg6 && close(r.target6-615,28),"axis1 at retraction endpoint merges near passive wire");
    r=tick({18,615,648},0,0,false,1);
    require(!r.transaction && close(r.after.a6,648),"axis4 jog cannot override global direction");
    r=tick({18,615,648},1,1);
    require(!r.transaction && close(r.after.a6-r.after.a5,33),"equal pull at delivery far boundary");
    r=tick({30,615,640},1,0,true);
    require(r.leg1 && close(r.target1,5),"retraction axis1 target 5, trigger 30");
    r=tick({18,615,648},0,1,true);
    require(r.leg6 && close(r.target6-615,6),"retraction wire trigger 33 and target 6");
    r=tick({18,615,616},1,0,true);
    require(r.leg6 && close(r.target6-615,16),"retraction passive target at axis1=18 is g16");
    r=tick({18,615,648},0.1,0.05,true);
    require(!r.transaction && close(r.after.a6-r.after.a5,32.95),"retraction gap decreasing must not hit far");
    r=tick({18,615,616},0.1,0.1,true);
    require(!r.transaction && close(r.after.a6-r.after.a5,1),"equal pull at retraction near end");
    r=tick({40,615,650},0,0);
    require(!r.transaction && close(r.after.a1,40) && close(r.after.a6,650),"shrinking windows cannot cause movement");
    r=tick({40,615,650},-1,-2);
    require(!r.transaction && close(r.after.a1,39) && close(r.after.a6-r.after.a5,34),"out-of-window input can return toward bounds");
    r=tick({40,615,650},1,0,false,0,false);
    require(!r.transaction && close(r.after.a1,40),"reconnect outside smaller window cannot move further outside");
    for (bool reverse : {false,true}) {
        r=tick({18,615,648},0,0,reverse);
        require(!r.transaction,"stationary far boundary never hands off");
        r=tick({18,615,616},0,0,reverse);
        require(!r.transaction,"stationary near boundary never hands off");
    }
}

static void test_direction_and_recovery() {
    bool single_handle_mode=false, dual_reverse_active=false;
    GuidewireMode guidewire_mode=GuidewireMode::None, single_handle_requested_mode=GuidewireMode::None;
    SpacingRecoveryState spacing_recovery;
    Transaction planned_return;
    Config cfg;
    Crawl axis1_crawl{};
    double zeros[7]={};
    AppContext ctx{&cfg,&dual_reverse_active,&guidewire_mode,zeros,zeros,zeros};
    auto axis1_window_left_abs=[&]() { return motion_sync::axis1_window_left_abs(ctx); };
    auto axis1_window_right_abs=[&]() { return motion_sync::axis1_window_right_abs(ctx); };
    auto clear_axis1_delivery_mapping=[]() {};
    auto arm_axis1_delivery_mapping=[]() {};
    auto clear_cylinder_manual_overrides=[]() {};
    auto cancel_active_return_motion=[](bool) { return true; };
#include "mode.inc"
    auto sample=[&](unsigned char cat,unsigned char wire) {
        bool catheter_b6_pressed=(cat&0x40)!=0, guidewire_b6_pressed=(wire&0x40)!=0;
        bool spacing_recovery_button_pressed=(cat&7)==7;
        bool spacing_recovery_button_press_edge=spacing_recovery_button_pressed && !spacing_recovery_button_pressed_prev;
        bool spacing_recovery_button_release_edge=!spacing_recovery_button_pressed && spacing_recovery_button_pressed_prev;
#include "direction.inc"
#include "save.inc"
        require(axis1_reverse_pressed==axis6_effective_reverse_pressed,"both chains use same B6 direction");
        require(requested_guidewire_mode==GuidewireMode::None,"dual handles cannot select control object");
        return axis1_reverse_pressed;
    };
    auto exit_recovery=[&]() {
        double plc_act_pos[7]={},plc_init_pos[7]={},plc_leftlimit[7]={};
        bool axis1_delivery_stop_latched=false,axis1_delivery_stop_prompted=false;
        bool axis1_push_rearm_after_hold=true,control_active=false;
#include "exit.inc"
        require(!spacing_recovery.active() && control_active,"recovery exits without restarting control");
        require(!vis_reverse_override_active,"recovery cannot latch dual-handle direction");
    };
    for (int repeat=0;repeat<3;++repeat) {
        for (int bits : {0,1,3,2,0,2,3,1,0})
            require(sample(bits&1?0x40:0,bits&2?0x40:0)==(bits!=0),"B6 press/hold/release truth table");
        require(!sample(0x80,0x80),"B7 does not change direction or control object");
    }
    sample(0,0);
    planned_return.running=true;
    require(!sample(0x40,0) && close(axis1_crawl.end_abs-axis1_crawl.start_abs,25),"handoff freezes delivery direction and window");
    planned_return.running=false;
    require(sample(0x40,0) && close(axis1_crawl.end_abs-axis1_crawl.start_abs,25),"pending B6 applies after handoff without changing window");
    planned_return.running=true;
    require(sample(0,0),"release during retraction handoff is deferred");
    planned_return.running=false;
    require(!sample(0,0),"release returns to delivery after handoff");
    for (unsigned char entry : {0x07,0x47}) {
        for (bool prior_reverse : {false,true}) {
            sample(prior_reverse?0x40:0,0);
            require(sample(entry,0)==prior_reverse,"recovery combination cannot change direction");
            spacing_recovery.phase=SpacingRecoveryPhase::Active;
            sample(entry,0);
            sample(0,0);
            spacing_recovery.phase=SpacingRecoveryPhase::ExitSync;
            exit_recovery();
            require(!sample(0,0),"recovery release returns to delivery");
            require(sample(0x40,0) && !sample(0,0),"catheter B6 still works after recovery");
            require(sample(0,0x40) && !sample(0,0),"wire B6 still works after recovery");
        }
    }
    sample(7,0);
    spacing_recovery.phase=SpacingRecoveryPhase::ExitSync;
    spacing_recovery.requested=false;
    exit_recovery();
    require(sample(0,0x40),"B6 held after recovery remains retraction");
    require(!sample(0,0),"release after recovery is never latched");
}

static void test_constraints() {
    int n=0;
    for (bool rev : {false,true})
    for (double a1 : {4.,5.,5.1,7.9,18.,29.9,30.,31.,40.})
    for (double a5 : {615.,650.})
    for (double gap : {0.,0.9,1.,1.1,5.,5.9,6.,6.1,18.,27.9,28.,28.1,32.9,33.,36.})
    for (double d1 : {-3.,-0.2,0.,0.2,3.})
    for (double d6 : {-3.,-0.2,0.,0.2,3.}) {
        Plant p{a1,a5,a5+gap};
        auto r=tick(p,d1,d6,rev);
        double x=r.follow.a1-p.a1,y=r.follow.a6-p.a6,g=r.follow.a6-r.follow.a5;
        require(std::isfinite(x) && std::isfinite(y),"finite outputs");
        require(x>=std::min(0.,d1)-1e-7 && x<=std::max(0.,d1)+1e-7,"catheter input cannot be amplified or reversed");
        require(y>=std::min(0.,d6)-1e-7 && y<=std::max(0.,d6)+1e-7,"wire input cannot be amplified or reversed");
        require(close(r.follow.a5-p.a5,x),"axis5 mirrors accepted catheter displacement");
        require(g>=std::min(1.,gap)-1e-7 && g<=std::max(33.,gap)+1e-7,"joint gap bound and outside recovery");
        require(r.follow.a1>=std::min(5.,a1)-1e-7 && r.follow.a1<=std::max(30.,a1)+1e-7,"axis1 bounds without extensions");
        require(r.follow.a6<=std::max(p.a6,670.)+1e-7,"absolute axis6 soft limit");
        if (r.leg1) require(rev?r.target1<p.a1:r.target1>p.a1,"axis1 handoff cannot oppose global mode");
        if (r.leg6 && !rev && r.target6<p.a6-1e-7)
            require((d1<0 && g>28.) || close(r.target6,670.),"delivery far-to-near is passive relief or absolute soft limit");
        if (r.leg6 && rev && r.target6>p.a6+1e-7)
            require(d1>0 && g<6.,"retraction near-to-far is only passive catheter relief");
        if (d1==0) require(!r.leg1,"stationary catheter cannot be pulled into wire handoff");
        if (r.leg6) require(r.target6<=670.0+1e-7,"handoff target soft limit");
        ++n;
    }
    std::cout << "PASS " << n << " boundary/input combinations\n";
}

static void test_direction_filter() {
    bool single_handle_mode=false;
    bool axis1_reverse_pressed_prev=false,axis1_reverse_pressed=true;
    HandleFilterState axis1_handle_filter,axis6_handle_filter;
    struct Handle { double fJoints2[2]; } cat{{1.0,2.0}},wire{{3.0,4.0}};
    Handle* axis1_input_handle=&cat;
    Handle* axis6_input_handle=&wire;
    axis1_handle_filter.reset(0.9,2.0);
    axis6_handle_filter.reset(2.9,4.0);
    double axis1_prev_linear_filtered=0.8,axis6_prev_linear_filtered=2.8;
#include "switch.inc"
    for (int i=0;i<10;++i) {
        axis1_handle_filter.update(1.0,2.0,0.2,0.2);
        axis6_handle_filter.update(3.0,4.0,0.2,0.2);
        require(close(axis1_handle_filter.axis0_filtered,axis1_prev_linear_filtered) &&
                close(axis6_handle_filter.axis0_filtered,axis6_prev_linear_filtered),
                "mode switch clears residual motion while handles are stationary");
    }
}

static void test_trajectories() {
    std::ofstream csv("trajectories.csv");
    csv<<"mode,width,cat_input,wire_input,handoffs,cat_actual,wire_actual\n";
    for (bool rev : {false,true})
    for (auto inputs : {std::array<double,2>{0.1,0}, {0,0.1}, {0.1,0.1}, {0.1,0.2}, {0.2,0.1}}) {
        Plant p{28,100,113};
        int count=0;
        double total1=0,total6=0;
        double sign=rev?1.:-1.;
        for (int i=0;i<1000;++i) {
            auto r=tick(p,sign*inputs[0],sign*inputs[1],rev);
            if (r.transaction) ++count;
            else { total1+=r.after.a1-p.a1; total6+=r.after.a6-p.a6; }
            p=r.after;
        }
        require(count<100,"trajectory cannot oscillate into repeated handoffs");
        if (inputs[1]==0) require(count<15,"catheter-only travel cannot repeatedly hand off after short passive resets");
        require((inputs[0]==0 || sign*total1>70) && (inputs[1]==0 || sign*total6>70),"requested trajectories make progress");
        csv<<(rev?"retraction":"delivery")<<",25,"<<inputs[0]<<','<<inputs[1]<<','<<count<<','<<total1<<','<<total6<<'\n';
    }
}
static double approach(double a,double b,double step) { return a+std::clamp(b-a,-step,step); }
static void advance(Plant& p,FollowSession& s,double mirror_step=0.25) {
    const Plant before=p;
    p.a1=approach(p.a1,s.command.a1,0.25);
    p.a3=approach(p.a3,s.command.a3,mirror_step);
    p.a5=approach(p.a5,s.command.a5,mirror_step);
    p.a6=approach(p.a6,s.command.a6,0.25);
    s.velocity[0]=(p.a1-before.a1)/0.01; s.velocity[2]=(p.a3-before.a3)/0.01;
    s.velocity[4]=(p.a5-before.a5)/0.01; s.velocity[5]=(p.a6-before.a6)/0.01;
}
static void test_handoff_lag() {
    for(int axis:{1,6}) for(bool rev:{false,true}) for(double input:{0.1,1.,4.}) {
        Plant p=axis==1?(rev?Plant{5,100,132}:Plant{30,100,102})
                       :(rev?Plant{18,100,106}:Plant{18,100,128});
        FollowSession session(p);
        const Plant start=p;
        bool completed=false, stopped_while_pending=false;
        for(int n=0;n<1500;++n) {
            const double d=n<300?(rev?input:-input):0;
            // 请求记住后立即停手，验证无需后续手柄增量。
            const double move=session.pending.active()?0:d;
            const auto r=tick(p,axis==1?move:0,axis==6?move:0,rev,0,true,&session);
            if(session.pending.active() && move==0) stopped_while_pending=true;
            if(r.transaction) {
                const double edge=axis==1?(rev?30:5):(rev?33:1);
                require(std::abs((axis==1?p.a1:p.a6-p.a5)-edge)<=0.2+1e-9,"actual boundary reached before clamping");
                require(std::abs(axis==1?p.a1-start.a1:p.a6-start.a6)>=(axis==1?24.8:26.8)-1e-9,"fast handle preserves full effective travel");
                for(int a:{0,2,4,5}) require(std::abs(session.velocity[a])<=0.5,"actual speed below handoff threshold");
                require(!session.pending.active(),"request cleared after handoff");
                completed=true; break;
            }
            // 镜像轴比轴1慢，防止只等轴1而截断轴3/5行程。
            advance(p,session,axis==1?0.12:0.25);
        }
        require(completed && stopped_while_pending,"handoff completes after fast input stops");
    }
    for(bool rev:{false,true}) for(int moving_axis:{0,2,4,5}) for(double v:{-0.51,-0.5,0.,0.5,0.51}) {
        Plant p{18,100,rev?133.:101.}; FollowSession session(p);
        session.velocity[moving_axis]=v;
        auto r=tick(p,0,rev?0.1:-0.1,rev,0,true,&session);
        require(r.transaction==(std::abs(v)<=0.5),"inclusive 0.5mm/s gate checks every frozen linear axis");
    }
    Plant p{18,100,128}; FollowSession session(p);
    auto r=tick(p,0,-100,false,0,true,&session);
    require(session.pending.active() && !r.transaction,"command endpoint only arms handoff");
    const Plant endpoint=session.command;
    r=tick(p,-4,-4,false,0,true,&session);
    require(close(r.follow.a1,endpoint.a1)&&close(r.follow.a6,endpoint.a6),"pending handoff preserves both endpoint targets");
    r=tick(p,0,0,true,0,true,&session);
    require(!r.transaction&&!session.pending.active(),"direction change discards pending handoff");
    for(bool rev:{false,true}) {
        // 导管推动g到被动端，轴1尚未到自身端点；停手后也必须完成被动换手。
        Plant passive=rev?Plant{18,100,102}:Plant{18,100,132};
        FollowSession s(passive); bool done=false;
        for(int n=0;n<500;++n) {
            auto out=tick(passive,s.pending.active()?0:(rev?4.:-4.),0,rev,0,true,&s);
            if(out.transaction) {require(out.leg6&&!out.leg1,"passive gap handoff retains its original cause");done=true;break;}
            advance(passive,s,0.12);
        }
        require(done,"passive handoff completes with lag and stopped handle");
    }
    std::cout<<"PASS lag, stop-input, mirror lag, passive handoff, reversal and 0.5mm/s cases\n";
}
static void test_pending_guards() {
    bool control_active=true, connection_hold_active=false, estop_hold_active=false;
    bool return_ads_fault_hold=false, startup_sequence_active=false, emergency_retract_active=false;
    bool axis6_soft_limit_hold=false;
    SpacingRecoveryState spacing_recovery;
    Transaction planned_return;
    DualHandoffRequest dual_handoff;
    int cancel_calls=0;
    auto cancel_active_return_motion=[&](bool) { ++cancel_calls; dual_handoff.reset(); return true; };
    auto cancel_if_needed=[&]() {
#include "cancel.inc"
    };
    auto axis4_paused=[&]() {
#include "axis4_pause.inc"
        return axis4_paused_by_axis6;
    };
    require(!axis4_paused(),"axis4 jog allowed during ordinary follow");
    dual_handoff.axis6=true;
    require(axis4_paused(),"pending handoff pauses axis4 together with axis6 target");
    cancel_if_needed();
    require(cancel_calls==0 && dual_handoff.active(),"ordinary follow retains pending handoff");
    for(bool* flag:{&connection_hold_active,&estop_hold_active,&return_ads_fault_hold,
                   &startup_sequence_active,&emergency_retract_active,&axis6_soft_limit_hold,
                   &spacing_recovery.requested}) {
        dual_handoff.axis6=true; *flag=true;
        const int before=cancel_calls;
        cancel_if_needed();
        require(cancel_calls==before+1 && !dual_handoff.active(),"takeover clears pending handoff");
        *flag=false;
    }
    dual_handoff.axis1=true; control_active=false;
    cancel_if_needed();
    require(!dual_handoff.active(),"pause clears pending handoff");
    require(!axis4_paused(),"axis4 pause clears after pending request cancellation");
}
static void test_post_return_lead() {
    Config cfg;
    require(close(cfg.axis1_post_return_lead_mm,2),"默认先行量为2 mm");
    Plant p{30,100,102,90}; FollowSession s(p); s.mapping_active=true;
    for(int i=0;i<100;++i) {
        const auto r=tick(p,-0.1,0,false,0,true,&s);
        require(!r.transaction,"前10 mm不额外创建回退任务");
        p=r.follow;
    }
    require(close(p.a1,18)&&close(p.a5,90)&&close(p.a3,80)&&close(p.a6,102),
            "名义10 mm使轴1前进12 mm，轴3/5前进10 mm，轴6保持");
    require(!s.mapping_active&&close(s.mapping_extra,2),"累计先行2 mm后关闭本段映射");
    auto r=tick(p,0,0,false,0,true,&s);
    require(close(r.follow.a1,p.a1)&&close(r.follow.a5,p.a5),"停手不自动补走");
    r=tick(p,-1,0,false,0,true,&s); p=r.follow;
    require(close(p.a1,17)&&close(p.a5,89),"10 mm之后恢复一比一");
    r=tick(p,1,0,true,0,true,&s);
    require(close(r.follow.a1,18)&&close(r.follow.a5,90),"切换撤出时镜像轴没有2 mm跳变");

    p={30,100,102,90}; s=FollowSession(p); s.mapping_active=true;
    r=tick(p,-12,0,false,0,true,&s);
    require(close(r.follow.a1,16)&&close(r.follow.a5,88)&&close(s.mapping_extra,2),
            "单拍跨过10 mm只增加2 mm");
    p={6,100,102,90}; s=FollowSession(p); s.mapping_active=true;
    r=tick(p,-4,0,false,0,true,&s);
    require(close(r.follow.a1,5)&&close(r.follow.a5,100-1.0/1.2),"先行与名义位移共同受轴1窗口约束");
    require(s.pending.axis1&&!r.transaction,"先行触边仍等待实际到位");
    const Plant end=s.command;
    r=tick(p,-4,0,false,0,true,&s);
    require(close(r.follow.a1,end.a1)&&close(r.follow.a5,end.a5),"等待到位期间冻结先行及镜像目标");
    p=end; s.velocity[0]=0.51;
    require(!tick(p,0,0,false,0,true,&s).transaction,"先行后速度超过0.5不换手");
    s.velocity[0]=0.5;
    require(tick(p,0,0,false,0,true,&s).transaction,"先行后实际到位且速度0.5允许换手");

    p={30,100,102,90}; s=FollowSession(p); s.mapping_active=true;
    r=tick(p,-5,0,false,0,true,&s); p=r.follow;
    r=tick(p,0,-0.1,false,0,true,&s);
    require(close(s.mapping_progress,5)&&close(s.mapping_extra,1),"只动导丝不会消耗先行进度");
    p=r.follow;
    r=tick(p,0,0,true,0,true,&s);
    require(close(r.follow.a1,p.a1)&&close(r.follow.a5,p.a5)&&!s.mapping_active,
            "中途换向停手不会让镜像轴补走");

    // 扫过输入、窗口和间距边界，检验名义位移与额外位移分离后的约束。
    for(double a1:{5.,5.1,6.,18.,30.})
    for(double gap:{0.,1.,1.1,15.,32.9,33.,36.})
    for(double d1:{-30.,-3.,-0.1,0.,0.1,3.})
    for(double d6:{-3.,0.,3.}) {
        Plant start{a1,100,100+gap,90}; FollowSession f(start); f.mapping_active=true;
        auto out=tick(start,d1,d6,false,0,true,&f);
        const double nominal=out.follow.a5-start.a5,actual=out.follow.a1-start.a1;
        const double g=out.follow.a6-out.follow.a5;
        require(close(actual,nominal-std::min(std::max(-nominal,0.0),10.0)*0.2),"只有轴1获得先行差额");
        require(out.follow.a1>=5-1e-7&&out.follow.a1<=30+1e-7,"先行不越过轴1窗口");
        require(g>=std::min(1.,gap)-1e-7&&g<=std::max(33.,gap)+1e-7,"导丝间距按轴5名义目标约束");
        require(nominal-actual>=-1e-7&&nominal-actual<=2+1e-7,"先行量不超过2 mm");
        if(d1==0) require(close(actual,0),"导管手柄静止不产生先行");
    }
    std::cout<<"PASS dual-handle 2mm lead, mirror continuity, gap limits and handoff speed gate\n";
}

static void test_lead_lifecycle() {
    Config cfg;
    bool axis1_delivery_mapping_active=false;
    double axis1_delivery_mapping_progress_mm=0,axis1_delivery_mapping_applied_extra_mm=0;
#include "mapping_helpers.inc"
    GuidewireMode guidewire_mode=GuidewireMode::None;
    bool axis1_reverse_pressed=false,control_active=true,connection_hold_active=false;
    bool ads_soft_hold_active=false,return_ads_fault_hold=false,estop_hold_active=false;
    bool startup_sequence_active=false,emergency_retract_active=false,axis6_soft_limit_hold=false;
    SpacingRecoveryState spacing_recovery;
    Transaction planned_return;
    auto guard=[&]() {
#include "mapping_guard.inc"
    };
    auto complete=[&]() {
#include "mapping_complete.inc"
    };
    planned_return.axis1=true;
    complete();
    require(axis1_delivery_mapping_active,"双手柄轴1正向回退完成启用先行");
    axis1_delivery_mapping_progress_mm=5; axis1_delivery_mapping_applied_extra_mm=1;
    planned_return.axis1=false; planned_return.running=true;
    guard(); complete();
    require(axis1_delivery_mapping_active&&close(axis1_delivery_mapping_progress_mm,5),
            "仅轴6换手不清除或重新启用轴1先行");
    planned_return.axis1=true;
    guard();
    require(!axis1_delivery_mapping_active,"轴1开始新回退清除旧先行");
    axis1_reverse_pressed=true; complete();
    require(!axis1_delivery_mapping_active,"双手柄撤出换手不启用先行");
    axis1_reverse_pressed=false; planned_return.running=false; complete();
    for(bool* flag:{&connection_hold_active,&ads_soft_hold_active,&return_ads_fault_hold,
        &estop_hold_active,&startup_sequence_active,&emergency_retract_active,
        &axis6_soft_limit_hold,&spacing_recovery.requested}) {
        arm_axis1_delivery_mapping(); *flag=true; guard();
        require(!axis1_delivery_mapping_active,"保持与恢复接管清除先行");
        *flag=false;
    }
    arm_axis1_delivery_mapping(); control_active=false; guard();
    require(!axis1_delivery_mapping_active,"暂停清除先行");
}

static void test_lead_across_handoffs() {
    for(bool reverse:{false,true})
    for(auto input:{std::array<double,2>{0.1,0}, {0.1,0.1}, {0,0.1}, {0.1,0.2}}) {
        Plant p{30,100,106,90}; FollowSession s(p);
        s.mapping_active=!reverse;
        int count=0; double travel=0;
        for(int i=0;i<2000;++i) {
            const Plant before=p;
            const auto r=tick(p,(reverse?1:-1)*input[0],(reverse?1:-1)*input[1],reverse,0,true,&s);
            require(s.mapping_extra<=2+1e-7,"跨多次换手的每段先行不超过2 mm");
            if(r.transaction) {
                ++count;
                Config cfg;
                Transaction planned_return; planned_return.axis1=r.leg1;
                bool axis1_reverse_pressed=reverse;
                auto& axis1_delivery_mapping_active=s.mapping_active;
                auto& axis1_delivery_mapping_progress_mm=s.mapping_progress;
                auto& axis1_delivery_mapping_applied_extra_mm=s.mapping_extra;
#include "mapping_helpers.inc"
                const double prior_progress=s.mapping_progress;
                p=r.after;
                s.command=p; s.base1=p.a1; s.base3=p.a3; s.base5=p.a5;
                s.pending.reset();
#include "mapping_complete.inc"
                if(!r.leg1) require(close(s.mapping_progress,prior_progress),"轴6独立换手后保留先行进度");
                const auto stopped=tick(p,0,0,reverse,0,true,&s);
                require(!stopped.transaction&&close(stopped.follow.a1,p.a1)&&close(stopped.follow.a5,p.a5),
                        "回退重建基准后停手不会补走或重复换手");
            } else {
                p=r.follow;
                travel+=std::abs(p.a1-before.a1)+std::abs(p.a6-before.a6);
            }
        }
        require(count>0&&count<70&&travel>150,"先行下多次换手仍持续递送且不振荡");
    }
}

static void test_single_mapping_mirror() {
    Config cfg;
    bool axis1_delivery_mapping_active=false;
    double axis1_delivery_mapping_progress_mm=0,axis1_delivery_mapping_applied_extra_mm=0;
#include "mapping_helpers.inc"
    arm_axis1_delivery_mapping();
    bool axis1_reverse_pressed=false;
    double axis1_nominal_forward_mm=5,axis1_cmd_abs=25;
    double axis3_base_rel=90,axis5_base_rel=100;
    auto constrain_axis1_follow_cmd_abs=[](double x){return std::clamp(x,5.0,30.0);};
#include "single_mapping.inc"
    require(close(axis1_cmd_abs,24)&&close(axis5_base_rel+axis1_cmd_abs-30,95),
            "保留的单手柄分支也仅让轴1先行");
    clear_axis1_delivery_mapping();
    require(close(axis5_base_rel+axis1_cmd_abs-30,95),"单手柄清除先行进度不会让镜像目标跳变");
}

int main() {
    test_lead_across_handoffs();
    test_single_mapping_mirror();
    test_post_return_lead();
    test_lead_lifecycle();
    test_pending_guards();
    test_handoff_lag();
    test_passive_targets();
    test_one_way_merge();
    test_windows();
    test_target_safety();
    test_examples();
    test_direction_and_recovery();
    test_direction_filter();
    test_constraints();
    test_trajectories();
    std::cout<<"PASS "<<cases<<" assertions; production motion/windows/B6/recovery branches\n";
}
