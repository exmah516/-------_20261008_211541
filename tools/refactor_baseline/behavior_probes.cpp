// 临时内存夹具：动作和判定使用生产片段，设备、同步结果及实际反馈由夹具提供。
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
using DWORD = std::uint32_t;
using ULONGLONG = std::uint64_t;
constexpr double Rad = 57.29577951308232;
#include "types.inc"
enum class StartupPhase { Done };
enum class CylinderManualMode { Automatic };
enum class ModeSelection { CatheterDelivery=1, CatheterRetraction, GuidewireDelivery, GuidewireRetraction };
enum class ExperimentEventCode { SpacingRecoveryStart };
static void record_experiment_event(ExperimentEventCode, int) {}
namespace AdsSymbol { constexpr int axis6_return = 6; }
namespace motion_sync { static double axis6_window_size_mm(int) { return 32.0; } }
static double clamp_double(double x, double lo, double hi) { return std::clamp(x, lo, hi); }
static int assertions = 0;
static void check(bool ok, const char* why) {
    ++assertions;
    if (!ok) { std::cerr << "FAIL " << why << '\n'; std::exit(1); }
}
static bool equal(double a, double b) { return std::abs(a-b) < 1e-8; }
struct Fixture {
    ControlConfig cfg;
    CylinderPreset cyl;
    PlannedReturnCoordinator planned_return;
    SpacingRecoveryState spacing_recovery;
    GuidewireMode guidewire_mode=GuidewireMode::None;
    struct { bool completed=true; StartupPhase phase=StartupPhase::Done; } startup;
    struct { void disable_compensation() {} } tracking_controller;
    bool control_active=true, initial_sync_done=true;
    bool ads_soft_hold_active=false, handle_soft_hold_active=false, connection_hold_active_prev=false;
    bool estop_hold_active=false, return_ads_fault_hold=false, startup_sequence_active=false;
    bool emergency_retract_active=false, axis6_soft_limit_hold=false, ads_motion_cycle_valid=true;
    bool single_handle_mode=false, axis1_reverse_pressed=false, axis6_effective_reverse_pressed=false;
    bool vis_reverse_override_active=false, vis_reverse_override_value=false;
    int vis_reverse_override_target=0;
    bool spacing_recovery_button_pressed_prev=false;
    bool axis1_fast_return=false, axis6_fast_retract=false;
    bool axis1_delivery_stop_latched=false, axis1_delivery_stop_prompted=false, axis1_push_rearm_after_hold=false;
    bool axis1_delivery_mapping_active=false;
    double axis1_delivery_mapping_progress_mm=0, axis1_delivery_mapping_applied_extra_mm=0;
    double axis1_follow_cmd_abs=30, axis6_follow_cmd_abs=628;
    double axis3_base_rel=610, axis5_base_rel=615;
    double plc_act_pos[7]={30,12,610,0,615,628,34};
    double plc_init_pos[7]={}, plc_leftlimit[7]={};
    double pos[7]={30,12,610,0,615,628,34};
    double axis2_hold_rel=12, axis7_hold_rel=34, independent_axis2_hold_rel=12;
    unsigned short cylinder1_cmd=0,cylinder2_cmd=0,cylinder3_cmd=0,cylinder4_cmd=0;
    CylinderManualMode cylinder_manual_mode[4]={};
    std::string recovery_event_text="fixture";
    DWORD tick_ms=1000;
    int ctx=0, sync_attempts=0, sync_failures=0, clears=0;

    void arm() {
#include "mapping_helpers.inc"
        arm_axis1_delivery_mapping();
    }
    double forward_dual(double nominal) {
        const bool reverse=false;
        const double inc1=-nominal, inc6=0;
        const double cmd1_start=axis1_follow_cmd_abs, cmd6_start=axis6_follow_cmd_abs;
        const double lo1=5,hi1=30,abs6_soft=660,gmin=1,gmax=33;
        const double g_start=13;
#include "dual_mapping.inc"
        (void)d1_before_gap;
        (void)d6_before_gap;
        axis1_follow_cmd_abs=cmd1;
        axis6_follow_cmd_abs=cmd6;
        plc_act_pos[0]=cmd1;
        return nominal == 0 ? 0 : cmd1_start-cmd1-nominal;
    }
    double forward_single(double nominal) {
        const bool ignored = false;
        (void)ignored;
        const double axis1_nominal_forward_mm=nominal;
        double axis1_cmd_abs=axis1_follow_cmd_abs-nominal;
        const double nominal_target=axis1_cmd_abs;
        auto constrain_axis1_follow_cmd_abs=[](double v) { return std::clamp(v,5.0,30.0); };
#include "single_mapping.inc"
        axis1_follow_cmd_abs=axis1_cmd_abs;
        plc_act_pos[0]=axis1_cmd_abs;
        return nominal_target-axis1_cmd_abs;
    }
    void tick(unsigned char buttons, bool hold, double increment=0, DWORD dt=10) {
        tick_ms+=dt;
        handle_soft_hold_active=hold;
        const bool motion_startup_active=false;
        auto GetTickCount=[&]() { return tick_ms; };
        auto clear_cylinder_manual_overrides=[]() {};
        auto clear_force_output=[]() {};
        auto load_pos_from_actual=[&]() { std::copy(plc_act_pos,plc_act_pos+7,pos); };
#include "mapping_helpers.inc"
#include "connection.inc"
        const unsigned char physical_587_buttons=buttons;
#include "button.inc"
#include "save.inc"
#include "mapping_guard.inc"
        auto read_plc_state=[]() { return true; };
        auto clear_axis1_group_return_requests=[&]() { ++clears; };
        auto clear_axis_return_request=[&](int) { ++clears; return true; };
        if (!ads_soft_hold_active && !handle_soft_hold_active && !return_ads_fault_hold && !estop_hold_active &&
            (control_active || motion_startup_active || emergency_retract_active) && read_plc_state()) {
            const DWORD now_ms=GetTickCount();
            const double axis5_abs=plc_act_pos[4]+plc_init_pos[4];
            const double axis6_abs=plc_act_pos[5]+plc_init_pos[5];
            const double axis3_from_left_mm=plc_act_pos[2]+plc_init_pos[2]-plc_leftlimit[2];
            const double axis1_linear_increment_mm=increment;
#include "entry.inc"
            if (false) {}
#include "action.inc"
        }
        auto sync_all=[&](int samples) {
            check(samples==20,"ExitSync uses 20-sample service call");
            ++sync_attempts;
            if(sync_failures>0) { --sync_failures; return false; }
            return true;
        };
        auto apply_mode_selection=[](ModeSelection) {};
#include "exit.inc"
    }
    void activate() {
        tick(0x07,false);
        check(spacing_recovery.phase==SpacingRecoveryPhase::ClampWait,"press enters clamp wait");
        tick(0x07,false,0,149);
        check(spacing_recovery.phase==SpacingRecoveryPhase::ClampWait,"149ms does not activate");
        tick(0x07,false,0,1);
        check(spacing_recovery.phase==SpacingRecoveryPhase::Active,"150ms activates");
    }
    void settle_actual() {
        plc_act_pos[2]=spacing_recovery.axis3_cmd_rel;
        plc_act_pos[4]=spacing_recovery.axis5_cmd_rel;
        plc_act_pos[5]=spacing_recovery.axis6_cmd_rel;
    }
};
int main() {
    {
        Fixture f; f.activate();
        f.tick(0x07,true);
        check(f.spacing_recovery.phase==SpacingRecoveryPhase::Idle&&!f.spacing_recovery.requested,
              "hold resets active recovery and request");
        check(f.spacing_recovery_button_pressed_prev,"hold retains pressed edge memory");
        for(int n=0;n<60;++n) f.tick(0x07,true);
        f.tick(0x07,false);
        check(f.spacing_recovery.phase==SpacingRecoveryPhase::Idle&&!f.spacing_recovery.requested,
              "held key after hold does not reenter");
        f.tick(0,false);
        f.tick(0x47,false);
        check(f.spacing_recovery.phase==SpacingRecoveryPhase::ClampWait,
              "release then press including B6 rearms");
        std::cout << "SCENARIO 1 PASS held=Idle release_press=ClampWait\n";
    }
    {
        Fixture f; f.activate();
        double before3=f.spacing_recovery.axis3_cmd_rel, before5=f.spacing_recovery.axis5_cmd_rel,
               before6=f.spacing_recovery.axis6_cmd_rel;
        for (const auto pair : {std::pair<double,DWORD>{1.3,10},{0.05,25},{3.0,250}}) {
            double prev=f.spacing_recovery.axis3_cmd_rel;
            f.tick(0x07,false,pair.first,pair.second);
            double d=f.spacing_recovery.axis3_cmd_rel-prev;
            check(d>=0 && d<=40.0*std::min(pair.second,DWORD(50))/1000.0+1e-8,"40mm/s and 50ms cap");
            check(equal(f.spacing_recovery.axis3_cmd_rel-before3,f.spacing_recovery.axis5_cmd_rel-before5)&&
                  equal(f.spacing_recovery.axis3_cmd_rel-before3,f.spacing_recovery.axis6_cmd_rel-before6),
                  "three axes advance equally");
        }
        check(equal(f.spacing_recovery.moved_mm,3.35),"sequence advances 0.4+0.95+2.0 mm");
        check(f.spacing_recovery.pending_delta_mm>0,"positive excess remains pending");
        double prev=f.spacing_recovery.moved_mm;
        f.tick(0x07,false,0);
        check(equal(f.spacing_recovery.pending_delta_mm,0)&&equal(prev,f.spacing_recovery.moved_mm),"zero clears backlog");
        f.tick(0x07,false,2);
        prev=f.spacing_recovery.moved_mm;
        f.tick(0x07,false,-1);
        check(equal(f.spacing_recovery.pending_delta_mm,0)&&equal(prev,f.spacing_recovery.moved_mm),"negative clears backlog");
        check(equal(f.pos[0],30)&&equal(f.pos[1],12)&&equal(f.pos[6],34),"axes 1 2 7 held");
        check(f.cylinder1_cmd==450&&f.cylinder2_cmd==1000&&f.cylinder3_cmd==550&&f.cylinder4_cmd==100,"recovery clamp outputs");
        f.settle_actual(); f.tick(0,false);
        check(f.spacing_recovery.phase==SpacingRecoveryPhase::ExitSync,"release enters ExitSync");
        f.tick(0,false,0,99);
        check(f.spacing_recovery.phase==SpacingRecoveryPhase::ExitSync&&f.sync_attempts==0,"minimum 100ms exit settle");
        f.tick(0,false,0,1);
        check(f.spacing_recovery.phase==SpacingRecoveryPhase::Idle&&f.sync_attempts==1,"settled exit synchronizes to Idle");
        std::cout << "SCENARIO 2A PASS equal_steps=3.35 zero_negative_clear=1 settled_exit=100ms\n";
    }
    {
        Fixture f;
        f.plc_act_pos[2]=640;f.plc_act_pos[4]=645;f.plc_act_pos[5]=659.7;
        f.activate(); f.tick(0x07,false,5);
        check(equal(f.spacing_recovery.moved_mm,0.3)&&equal(f.spacing_recovery.remaining_mm,0),"common remaining 0.3mm limits step");
        check(equal(f.spacing_recovery.pending_delta_mm,0)&&f.spacing_recovery.phase==SpacingRecoveryPhase::Active,"end of travel clears pending but remains Active");
        f.tick(0x07,false,5);
        check(equal(f.spacing_recovery.moved_mm,0.3),"cannot advance beyond common limit");
        f.plc_act_pos[5]-=1;
        f.tick(0,false);
        f.tick(0,false,0,1999);
        check(f.spacing_recovery.phase==SpacingRecoveryPhase::ExitSync,"unsettled exit waits before 2000ms");
        f.tick(0,false,0,1);
        check(f.spacing_recovery.phase==SpacingRecoveryPhase::Idle&&f.sync_attempts==1,"unsettled exit uses actual after 2000ms");
        std::cout << "SCENARIO 2B PASS common_limit=0.3 timeout_exit=2000ms\n";
    }
    {
        Fixture f; f.activate(); f.tick(0x07,false,1); f.settle_actual(); f.tick(0,false);
        f.sync_failures=1;
        f.tick(0,false,0,100);
        check(f.spacing_recovery.phase==SpacingRecoveryPhase::ExitSync&&f.sync_attempts==1,"failed sync keeps ExitSync");
        f.tick(0,true);
        check(f.spacing_recovery.phase==SpacingRecoveryPhase::ExitSync&&f.sync_attempts==1,"hold preserves ExitSync and blocks sync");
        f.tick(0,false);
        check(f.spacing_recovery.phase==SpacingRecoveryPhase::Idle&&f.sync_attempts==2,"next valid cycle retries sync");
        std::cout << "SCENARIO 2C PASS sync_failure_retry=1 ExitSync_hold_preserved=1\n";
    }
    for(bool single : {false,true}) {
        Fixture f;f.single_handle_mode=single;f.arm();
        const double extra=single?f.forward_single(4):f.forward_dual(4);
        check(equal(extra,0.8)&&equal(f.axis1_delivery_mapping_progress_mm,4),"4mm nominal applies 0.8mm extra");
        check(f.axis1_delivery_mapping_active,"mapping was active halfway");
        f.tick(0,true);
        check(!f.axis1_delivery_mapping_active&&equal(f.axis1_delivery_mapping_progress_mm,0)&&
              equal(f.axis1_delivery_mapping_applied_extra_mm,0),"short hold clears all lead state");
        f.tick(0,false);
        const double after=single?f.forward_single(1):f.forward_dual(1);
        check(equal(after,0)&&!f.axis1_delivery_mapping_active,"continued push has no remaining lead");
        std::cout << "SCENARIO 3 PASS mode="<<(single?"single":"dual")<<" before_progress=4 before_extra=0.8 after_extra="<<after<<"\n";
    }
    std::cout << "PASS assertions="<<assertions<<" no hardware or network calls\n";
}
