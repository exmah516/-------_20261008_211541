// 生产分支由run_dual_control_test.py抽取，设备读写仅用内存替身。
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
enum class PlannedReturnMode { Dual };
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
struct Transaction { bool running=false; bool active() const { return running; } };
struct Plant { double a1=28, a5=615, a6=628; };
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
static Outcome tick(Plant p, double raw1, double raw6, bool reverse_input=false,
                    int axis4=0, double width1=26, double width6=26, bool window_active=true) {
    Config cfg;
    cfg.axis1_window_right_from_left_mm=cfg.axis1_window_left_from_left_mm+width1;
    cfg.catheter_axis6_window_size_mm=width6;
    auto gate=[&](double d) { return std::abs(d)>=cfg.linear_increment_noise_deadband_mm ? d : 0.0; };
    const double axis1_linear_increment_mm=gate(raw1), axis6_linear_increment_mm=gate(raw6);
    const bool axis1_reverse_pressed=reverse_input;
    GuidewireMode guidewire_mode=GuidewireMode::None;
    const bool axis4_forward_request=axis4<0, axis4_reverse_request=axis4>0;
    bool axis4_axis6_coupling_active_prev=true;
    ULONGLONG axis4_axis6_coupling_last_ms=1000;
    const double axis4_coupled_axis6_speed_mm_s=10;
    auto GetTickCount64=[]() { return ULONGLONG(1010); };
    double plc_init_pos[7]={}, plc_leftlimit[7]={}, actual[7]={};
    AppContext ctx{&cfg,&reverse_input,&guidewire_mode,plc_leftlimit,plc_init_pos,actual};
    Crawl axis1_crawl{p.a1,motion_sync::axis1_window_left_abs(ctx),motion_sync::axis1_window_right_abs(ctx)};
    axis1_crawl.window_active=window_active;
    double axis1_follow_cmd_abs=p.a1, axis6_follow_cmd_abs=p.a6;
    const double axis1_abs=p.a1, axis5_abs=p.a5, axis6_abs=p.a6, axis5_base_rel=p.a5;
    double pos[7]={p.a1,0,0,0,p.a5,p.a6,0}, axis2_hold_rel=0;
    const double axis1_rot_filtered=0;
    const bool axis3_delivery_stop_active=false;
    auto is_within_range=[](double x,double lo,double hi,double tol) { return x>=lo-tol && x<=hi+tol; };
    auto capture_axis1_follow_baseline=[]() {};
    auto apply_axis1_mirror_from_abs=[&](double x,bool) { pos[4]=p.a5+(x-p.a1); };
    auto compute_axis7_cmd_rel=[]() { return 0.0; };
    struct { int cyl1_open=0, cyl2_clamp=1800, cyl3_open=0, cyl4_clamp=1800; } cyl;
    int cylinder1_cmd=0,cylinder2_cmd=0,cylinder3_cmd=0,cylinder4_cmd=0;
    Transaction planned_return;
    Outcome out{p,p};
    auto begin_planned_return=[&](PlannedReturnMode,PlannedReturnRebaseScope,
                                bool a1,double t1,bool a6,double t6) {
        out.transaction=true; out.leg1=a1; out.leg6=a6; out.target1=t1; out.target6=t6;
        return true;
    };
    auto apply_planned_return_outputs=[]() {};
#include "motion.inc"
    out.follow={pos[0],pos[4],pos[5]};
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
    require(close(motion_sync::axis1_window_left_abs(ctx),108),"axis1 left reference");
    require(close(motion_sync::axis1_window_right_abs(ctx),134),"axis1 delivery width 26");
    double lo=0,hi=0;
    motion_sync::calculate_axis6_window_from_axis5_abs(ctx,915,lo,hi);
    require(close(lo,1115) && close(hi-lo,26),"wire delivery reference and width");
    reverse=true;
    require(close(motion_sync::axis1_window_left_abs(ctx),108) &&
            close(motion_sync::axis1_window_right_abs(ctx),144),"axis1 retraction width 36, same left");
    motion_sync::calculate_axis6_window_from_axis5_abs(ctx,915,lo,hi);
    require(close(lo,1115) && close(hi-lo,36),"wire retraction width 36, same left");
    ctx.dual_reverse=nullptr;
    require(close(motion_sync::axis6_window_size_mm(ctx),26),"single-handle helper compatibility");
}

static void test_examples() {
    auto r=tick({18,615,615},-1,-1);
    require(!r.transaction && close(r.after.a1,17) && close(r.after.a6,614),"equal push at near boundary");
    r=tick({18,615,644},-0.1,-0.1);
    require(!r.transaction && close(r.after.a6-r.after.a5,29),"equal push at far boundary does not yield");
    r=tick({18,615,644},-0.05,-0.1);
    require(!r.transaction && close(r.after.a6-r.after.a5,28.95),"wire faster after return must not reverse handoff");
    r=tick({18,615,644},-0.1,0);
    require(r.leg6 && !r.leg1 && close(r.after.a6-r.after.a5,13),"passive far relocation keeps midpoint");
    r=tick({18,615,615},1,0);
    require(!r.transaction && close(r.after.a1,18) && close(r.after.a6,615),"passive near blocks only catheter pull");
    r=tick({18,615,615},-1,0);
    require(!r.transaction && close(r.after.a1,17) && close(r.after.a6,615),"passive near permits renewed catheter push");
    r=tick({8.2,615,628},-0.1,0);
    require(r.leg1 && !r.leg6 && close(r.target1,37),"delivery axis1 handoff target preserves 3mm margin");
    r=tick({18,615,615.2},0,-0.1);
    require(!r.leg1 && r.leg6 && close(r.target6-615,29),"active wire delivery handoff");
    r=tick(r.after,-0.05,-0.1);
    require(!r.transaction,"active wire handoff cannot immediately become opposite passive handoff");
    r=tick({8.2,615,616.4},-0.1,-0.2);
    require(r.leg1 && r.leg6,"merge uses actual gap approach direction");
    r=tick({8.2,615,616.4},-0.2,-0.1);
    require(r.leg1 && !r.leg6,"do not merge wire while moving away from near end");
    r=tick({8,615,644},-0.1,0.1);
    require(r.leg1 && !r.leg6,"blocked catheter at near end cannot cause passive wire far relocation");
    r=tick({44,615,615},0.1,-0.1,true);
    require(r.leg1 && !r.leg6,"blocked catheter at far end cannot cause passive wire near relocation");
    r=tick({18,615,644},0,0,false,1);
    require(!r.transaction && close(r.after.a6,644),"axis4 jog cannot override global direction");
    r=tick({18,615,644},1,1);
    require(!r.transaction && close(r.after.a6-r.after.a5,29),"equal pull at delivery far boundary");
    r=tick({44,615,640},1,0,true);
    require(r.leg1 && close(r.target1,5),"retraction axis1 target 5, trigger 44");
    r=tick({18,615,651},0,1,true);
    require(r.leg6 && close(r.target6-615,0),"retraction wire trigger 36");
    r=tick({18,615,615},1,0,true);
    require(r.leg6 && close(r.target6-615,18),"retraction passive near midpoint 18");
    r=tick({18,615,651},0.1,0.05,true);
    require(!r.transaction && close(r.after.a6-r.after.a5,35.95),"retraction gap decreasing must not hit far");
    r=tick({18,615,615},0.1,0.1,true);
    require(!r.transaction && close(r.after.a6-r.after.a5,0),"equal pull at retraction near end");
    r=tick({40,615,650},0,0);
    require(!r.transaction && close(r.after.a1,40) && close(r.after.a6,650),"shrinking windows cannot cause movement");
    r=tick({40,615,650},-1,-2);
    require(!r.transaction && close(r.after.a1,39) && close(r.after.a6-r.after.a5,34),"out-of-window input can return toward bounds");
    r=tick({40,615,650},1,0,false,0,26,26,false);
    require(!r.transaction && close(r.after.a1,40),"reconnect outside smaller window cannot move further outside");
    for (bool reverse : {false,true}) {
        const double hi=reverse?36:29;
        r=tick({18,615,615+hi},0,0,reverse);
        require(!r.transaction,"stationary far boundary never hands off");
        r=tick({18,615,615},0,0,reverse);
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
    require(!sample(0x40,0) && close(axis1_crawl.end_abs-axis1_crawl.start_abs,26),"handoff freezes delivery direction and window");
    planned_return.running=false;
    require(sample(0x40,0) && close(axis1_crawl.end_abs-axis1_crawl.start_abs,36),"pending B6 applies after handoff");
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
    for (double a1 : {5.,8.,8.1,10.,18.,34.,37.,44.,46.})
    for (double a5 : {615.,650.})
    for (double gap : {-2.,0.,0.1,1.4,13.,28.9,29.,35.9,36.,40.})
    for (double d1 : {-3.,-0.2,0.,0.2,3.})
    for (double d6 : {-3.,-0.2,0.,0.2,3.}) {
        Plant p{a1,a5,a5+gap};
        auto r=tick(p,d1,d6,rev);
        double x=r.follow.a1-p.a1,y=r.follow.a6-p.a6,g=r.follow.a6-r.follow.a5;
        require(std::isfinite(x) && std::isfinite(y),"finite outputs");
        require(x>=std::min(0.,d1)-1e-7 && x<=std::max(0.,d1)+1e-7,"catheter input cannot be amplified or reversed");
        require(y>=std::min(0.,d6)-1e-7 && y<=std::max(0.,d6)+1e-7,"wire input cannot be amplified or reversed");
        require(close(r.follow.a5-p.a5,x),"axis5 mirrors accepted catheter displacement");
        require(g>=std::min(0.,gap)-1e-7 && g<=std::max(rev?36.:29.,gap)+1e-7,"joint gap bound and outside recovery");
        require(r.follow.a1>=std::min(rev?5.:8.,a1)-1e-7 && r.follow.a1<=std::max(rev?44.:37.,a1)+1e-7,"axis1 bounds");
        require(r.follow.a6<=std::max(p.a6,670.)+1e-7,"absolute axis6 soft limit");
        if (r.leg1) require(rev?r.target1<p.a1:r.target1>p.a1,"axis1 handoff cannot oppose global mode");
        if (r.leg6 && !rev && r.target6<p.a6-1e-7)
            require(d1<0 && gap>=27.5,"delivery far-to-near is only passive catheter relief");
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
    for (double width : {20.,26.})
    for (auto inputs : {std::array<double,2>{0.1,0}, {0,0.1}, {0.1,0.1}, {0.1,0.2}, {0.2,0.1}}) {
        Plant p{28,100,113};
        int count=0;
        double total1=0,total6=0;
        double sign=rev?1.:-1.;
        for (int i=0;i<1000;++i) {
            auto r=tick(p,sign*inputs[0],sign*inputs[1],rev,0,width,26);
            if (r.transaction) ++count;
            else { total1+=r.after.a1-p.a1; total6+=r.after.a6-p.a6; }
            p=r.after;
        }
        require(count<100,"trajectory cannot oscillate into repeated handoffs");
        require((inputs[0]==0 || sign*total1>70) && (inputs[1]==0 || sign*total6>70),"requested trajectories make progress");
        csv<<(rev?"retraction":"delivery")<<','<<width<<','<<inputs[0]<<','<<inputs[1]<<','<<count<<','<<total1<<','<<total6<<'\n';
    }
}
int main() {
    test_windows();
    test_examples();
    test_direction_and_recovery();
    test_direction_filter();
    test_constraints();
    test_trajectories();
    std::cout<<"PASS "<<cases<<" assertions; production motion/windows/B6/recovery branches\n";
}
