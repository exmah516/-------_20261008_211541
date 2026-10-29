// 抽取真实取消、恢复进入/退出及完整回退推进分支；PLC 与 ADS 仅使用内存替身。
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
using DWORD = unsigned long;
using ULONGLONG = unsigned long long;
constexpr double Rad = 57.29577951308232;
#include "return_recovery_types.inc"
enum class StartupPhase { Done };
enum class CylinderManualMode { Automatic };
enum class ModeSelection { CatheterDelivery=1, CatheterRetraction, GuidewireDelivery, GuidewireRetraction };
enum class AdsPlannedReturnOperation { Prepare, Commit, Clear };
enum class PlannedReturnAdsPoll { Pending, Success, Failure };
namespace AdsSymbol { constexpr int axis6_return=6; }
namespace motion_sync {
    double axis6_window_size_mm(int) { return 32; }
    bool rebase_dual_after_return(int) { return true; }
    bool sync_axis1(int,int,bool) { return true; }
    bool sync_axis6(int,int,bool,bool,bool) { return true; }
}
struct PlannedReturnEventView {
    bool busy=false,done=false,error=false;
    std::uint32_t error_id=0,last_nonzero_error_id=0;
    std::uint64_t event_sequence=1,busy_true_sequence=0,done_false_sequence=0,done_true_sequence=0;
    std::uint64_t error_true_sequence=0,last_nonzero_error_id_sequence=0;
};
static int assertions=0;
static void check(bool ok, const char* text) {
    ++assertions;
    if (!ok) { std::cerr << "FAIL " << text << '\n'; std::exit(1); }
}
struct Fixture {
    ControlConfig cfg;
    CylinderPreset cyl;
    PlannedReturnCoordinator planned_return;
    DualHandoffRequest dual_handoff;
    SpacingRecoveryState spacing_recovery;
    GuidewireMode guidewire_mode=GuidewireMode::None;
    struct { bool completed=true; StartupPhase phase=StartupPhase::Done; bool is_active() const { return false; } } startup;
    struct {
        std::uint64_t applied=0,motion_applied=0;
        std::uint64_t applied_output_generation() const { return applied; }
        std::uint64_t applied_motion_output_generation() const { return motion_applied; }
        void request_coordinate_refresh() {}
        void request_watchdog_recovery() {}
        bool refresh_coordinates() { return true; }
    } ads_communication;
    bool control_active=true, connection_hold_active=false, estop_hold_active=false;
    bool return_ads_fault_hold=false, startup_sequence_active=false, emergency_retract_active=false, axis6_soft_limit_hold=false;
    bool single_handle_mode=false, vis_reverse_override_active=false, vis_reverse_override_value=false;
    int vis_reverse_override_target=0;
    bool spacing_recovery_button_pressed_prev=false, axis1_reverse_pressed=false, axis6_effective_reverse_pressed=false;
    bool axis1_fast_return=false, axis6_fast_retract=false, ads_motion_cycle_valid=true;
    bool initial_sync_done=true, ads_soft_hold_active=false, handle_soft_hold_active=false;
    bool handle_reinit_req=false,last_handle_reinit_req=false;
    bool connection_hold_active_prev=false, handle_rebase_requested=false;
    struct { unsigned int reconnect_count=0; } ads_stats;
    unsigned int handled_reconnect_count=0;
    struct { void set_following(bool) {} } axis1_net_distance, axis6_net_distance;
    struct { void disable_compensation() {} } tracking_controller;
    struct Filter {
        double axis0_filtered=0, axis1_filtered=0;
        void reset(double x,double y) { axis0_filtered=x; axis1_filtered=y; }
    } axis1_handle_filter, axis6_handle_filter;
    struct Input { double fJoints2[2]={}; } input1,input6;
    Input* axis1_input_handle=&input1;
    Input* axis6_input_handle=&input6;
    double axis1_prev_linear_filtered=0,axis6_prev_linear_filtered=0,axis1_prev_rot_filtered=0,axis6_prev_rot_filtered=0;
    bool axis1_delivery_stop_latched=false, axis1_delivery_stop_prompted=false, axis1_push_rearm_after_hold=false;
    bool plc_busy=true, plc_done=false, plc_req=true;
    bool plc_error=false;
    int pending_return_error_axis=-1;
    std::uint64_t pending_return_error_event_sequence=0;
    ULONGLONG pending_return_error_deadline_ms=0;
    PlannedReturnAdsPoll command_result=PlannedReturnAdsPoll::Success;
    unsigned int now_ms=1000;
    std::uint64_t ads_snapshot_sequence=1;
    int clears=0, commits=0, rebases=0, failed_rebases=0, ctx=0;
    double pos[7]={18,0,90,0,100,113,0}, plc_act_pos[7]={18,0,90,0,100,113,0};
    double plc_init_pos[7]={}, plc_leftlimit[7]={};
    double axis2_hold_rel=0, axis7_hold_rel=0;
    double independent_axis1_hold_rel=0, independent_axis2_hold_rel=0, independent_axis3_hold_rel=0, independent_axis5_hold_rel=0;
    double axis1_follow_cmd_abs=18, axis6_follow_cmd_abs=113;
    CylinderManualMode cylinder_manual_mode[4]={};
    explicit Fixture(bool reverse, bool both=false) {
        axis1_reverse_pressed=axis6_effective_reverse_pressed=reverse;
        planned_return.phase=PlannedReturnPhase::AwaitDone;
        planned_return.mode=PlannedReturnMode::Dual;
        planned_return.rebase_scope=PlannedReturnRebaseScope::Both;
        planned_return.hold_axis3_rel=90; planned_return.hold_axis5_rel=100; planned_return.hold_axis6_rel=113;
        planned_return.leg_count=both?2:1;
        planned_return.legs[0].active=planned_return.legs[0].started=true;
        planned_return.legs[0].request_armed=planned_return.legs[0].acknowledged=true;
        planned_return.legs[0].ack_event_sequence=1;
        planned_return.legs[0].axis_index=0;
        planned_return.legs[0].target_abs=reverse?5:30;
        planned_return.legs[0].entry_rel=reverse?30:5;
        if(both) { planned_return.legs[1]=planned_return.legs[0]; planned_return.legs[1].axis_index=5; planned_return.legs[1].target_abs=reverse?106:128; }
    }
    void tick(bool pressed) {
        now_ms+=10; ++ads_snapshot_sequence;
        auto GetTickCount=[&]() { return now_ms; };
        auto GetTickCount64=[&]() { return static_cast<ULONGLONG>(now_ms); };
        auto clear_force_output=[]() {};
        auto clear_axis1_delivery_mapping=[]() {};
        auto arm_axis1_delivery_mapping=[]() {};
        auto apply_mode_selection=[](ModeSelection) {};
        auto load_pos_from_actual=[&]() { std::copy(plc_act_pos,plc_act_pos+7,pos); };
        auto clear_planned_return_ads_command_tracking=[&]() { planned_return.ads_command_purpose=PlannedReturnAdsCommandPurpose::None; planned_return.ads_command_sequence=0; };
        auto submit_planned_return_ads_command=[&](AdsPlannedReturnOperation op,PlannedReturnAdsCommandPurpose purpose,bool) {
            if(op==AdsPlannedReturnOperation::Clear) { ++clears; plc_req=false; }
            if(op==AdsPlannedReturnOperation::Commit) { ++commits; plc_req=true; }
            planned_return.ads_command_purpose=purpose; planned_return.ads_command_sequence=1; return true;
        };
#include "return_recovery_cancel.inc"
        auto clear_cylinder_manual_overrides=[]() {};
        const bool snapshot_ok=ads_motion_cycle_valid;
        if(!snapshot_ok) ads_soft_hold_active=true;
        else {
#include "main_rebase.inc"
        }
#include "return_recovery_reconnect.inc"
#include "return_recovery_connection.inc"
        auto sync_all=[&](int) {
            ++rebases;
            if(failed_rebases>0) { --failed_rebases; return false; }
            return true;
        };
#include "return_recovery_reinit.inc"
        const bool spacing_recovery_button_press_edge=pressed&&!spacing_recovery_button_pressed_prev;
        const bool spacing_recovery_button_release_edge=!pressed&&spacing_recovery_button_pressed_prev;
        const bool spacing_recovery_button_pressed=pressed;
#include "return_recovery_save.inc"
#include "return_recovery_guard.inc"
        const double axis5_abs=plc_act_pos[4], axis6_abs=plc_act_pos[5], axis3_from_left_mm=plc_act_pos[2];
        auto clear_axis1_group_return_requests=[&]() { ++clears; plc_req=false; };
        auto clear_axis_return_request=[](int) { return true; };
#include "return_recovery_enter.inc"
        int cylinder1_cmd=0,cylinder2_cmd=0,cylinder3_cmd=0,cylinder4_cmd=0;
#include "return_recovery_output.inc"
        auto planned_return_settle_wait_ms=[](bool,unsigned char) { return 50u; };
        auto planned_return_event_view=[&](int) {
            PlannedReturnEventView e;
            e.busy=plc_busy; e.done=plc_done; e.error=plc_error;
            e.event_sequence=ads_snapshot_sequence;
            e.busy_true_sequence=plc_busy?ads_snapshot_sequence:0;
            e.done_true_sequence=plc_done?ads_snapshot_sequence:0;
            e.error_true_sequence=plc_error?ads_snapshot_sequence:0;
            e.error_id=plc_error?0x4466:0;
            return e;
        };
#include "return_recovery_refresh.inc"
#include "return_recovery_failure.inc"
#include "return_recovery_retry.inc"
        auto poll_planned_return_ads_command=[&]() { return command_result; };
        auto planned_return_ads_command_timed_out=[](ULONGLONG) { return false; };
        auto latch_planned_return_possibly_started=[](std::uint8_t) {};
        auto planned_return_active_axis_mask=[]() { return std::uint8_t(3); };
        auto planned_return_targets_safe=[]() { return true; };
        auto rebase_planned_return=[&]() {
            ++rebases;
            if(failed_rebases>0) { --failed_rebases; return false; }
            axis1_follow_cmd_abs=plc_act_pos[0]; axis6_follow_cmd_abs=plc_act_pos[5]; load_pos_from_actual(); return true;
        };
#include "return_recovery_step.inc"
        if(!ads_soft_hold_active&&planned_return.active()&&!spacing_recovery.active()) step_planned_return();
#include "return_recovery_exit.inc"
        if(planned_return.phase==PlannedReturnPhase::PublishHandoff) {
            plc_req=false; planned_return.handoff_generation=++ads_communication.applied;
            ads_communication.motion_applied=ads_communication.applied;
            planned_return.phase=PlannedReturnPhase::AwaitHandoffApplied;
        }
        if(planned_return.phase==PlannedReturnPhase::PostHandoffClampSettle&&planned_return.restore_stage==1&&
           planned_return.release_output_generation==0) {
            planned_return.release_output_generation=++ads_communication.applied;
            ads_communication.motion_applied=ads_communication.applied;
        }
    }
    void finish_plc() {
        for(int i=0;i<planned_return.leg_count;++i) plc_act_pos[planned_return.legs[i].axis_index]=planned_return.legs[i].target_abs;
        plc_busy=false; plc_done=true;
    }
};
int main() {
    for(auto phase:{PlannedReturnPhase::ClampSettle,PlannedReturnPhase::AwaitHandoffApplied,
                    PlannedReturnPhase::PostHandoffClampSettle}) {
        Fixture f(false,true);
        f.planned_return.phase=phase;
        f.planned_return.clamp_output_generation=10;
        f.planned_return.handoff_generation=10;
        f.planned_return.restore_stage=1;
        f.planned_return.release_output_generation=10;
        f.ads_communication.applied=11;
        f.tick(false);
        check(f.planned_return.phase==phase&&!f.planned_return.clamp_output_applied&&
              !f.planned_return.completion_clear_confirmed&&!f.planned_return.release_applied,
              "heartbeat-only writes cannot acknowledge a clamp or return handoff");
        f.ads_communication.motion_applied=11;
        f.tick(false);
        check(f.planned_return.clamp_output_applied||f.planned_return.completion_clear_confirmed||
              f.planned_return.release_applied,
              "the corresponding motion write acknowledges the original handoff");
    }
    for(bool both:{false,true}) for(bool already_done:{false,true}) {
        Fixture f(false,both);
        f.planned_return.phase=PlannedReturnPhase::SubmitRequest;
        f.planned_return.request_prepared=true;
        f.planned_return.ads_command_purpose=PlannedReturnAdsCommandPurpose::Commit;
        f.planned_return.ads_command_sequence=1;
        f.command_result=PlannedReturnAdsPoll::Failure;
        for(int i=0;i<f.planned_return.leg_count;++i) {
            f.planned_return.legs[i].acknowledged=false;
            f.planned_return.legs[i].request_event_sequence=1;
        }
        if(already_done) f.finish_plc();
        f.tick(false);
        check(f.planned_return.active()&&f.control_active&&!f.return_ads_fault_hold&&f.clears==0&&f.commits==0,
              "PLC acknowledgement wins over a lost ADS Commit result without cancel or replay");
    }
    {
        Fixture f(false,true);
        f.handle_reinit_req=true;
        f.tick(false);
        check(f.planned_return.active()&&f.clears==0&&f.rebases==0,
              "PLC rebase request cannot clear an unfinished return");
    }
    {
        Fixture f(false,true);
        f.plc_error=true;
        f.tick(false);
        check(f.return_ads_fault_hold&&!f.control_active&&f.clears==1,
              "real PLC return error retains existing fault handling");
    }
    for(bool stale:{false,true}) {
        Fixture f(false);
        auto& leg=f.planned_return.legs[0];
        leg.acknowledged=false;
        leg.done=false;
        leg.request_event_sequence=stale?100:1;
        f.plc_busy=false; f.plc_done=true;
        if(stale) f.plc_act_pos[0]=leg.target_abs;
        f.tick(false);
        check(!leg.acknowledged&&!leg.done&&f.planned_return.phase==PlannedReturnPhase::AwaitDone,
              "stale Done or feedback away from target cannot complete a return");
    }
    for(bool reverse:{false,true}) for(bool both:{false,true}) {
        Fixture f(reverse,both);
        const double target=f.planned_return.legs[0].target_abs;
        f.ads_motion_cycle_valid=false;
        f.tick(false);
        check(f.planned_return.active()&&f.planned_return.legs[0].target_abs==target&&f.clears==0,
              "snapshot loss preserves return ownership and never clears PLC Req");
        f.finish_plc();
        f.ads_motion_cycle_valid=true;
        ++f.ads_stats.reconnect_count;
        f.tick(false);
        check(f.planned_return.active()&&!f.ads_soft_hold_active&&f.clears==0&&f.commits==0,
              "ADS reconnect observes the same completed return without resending it");
        for(int n=0;n<40;++n) f.tick(false);
        check(!f.planned_return.active()&&f.control_active&&f.commits==0,
              "completed return finishes its original handoff after reconnect");
    }
    for(auto phase:{PlannedReturnPhase::AwaitFreshSnapshot,PlannedReturnPhase::PostHandoffClampSettle}) {
        Fixture f(false,true);
        f.finish_plc();
        f.planned_return.phase=phase;
        f.planned_return.restore_stage=1;
        f.planned_return.release_applied=true;
        f.failed_rebases=1;
        f.tick(false);
        check(f.planned_return.phase==phase&&!f.return_ads_fault_hold&&f.control_active,
              "one failed return rebase retains its phase without a permanent control lock");
        for(int n=0;n<40;++n) f.tick(false);
        check(!f.planned_return.active()&&f.control_active&&f.commits==0,
              "next valid rebase completes the same handoff without replay");
    }
    {
        Fixture f(false);
        f.planned_return.reset();
        f.spacing_recovery.phase=SpacingRecoveryPhase::ExitSync;
        f.spacing_recovery.axis3_cmd_rel=f.plc_act_pos[2];
        f.spacing_recovery.axis5_cmd_rel=f.plc_act_pos[4];
        f.spacing_recovery.axis6_cmd_rel=f.plc_act_pos[5];
        f.failed_rebases=1;
        f.tick(false);
        check(f.spacing_recovery.phase==SpacingRecoveryPhase::ExitSync&&!f.return_ads_fault_hold&&f.control_active,
              "one failed recovery exit sync remains retryable");
        f.tick(false);
        check(!f.spacing_recovery.active()&&f.control_active,
              "recovery exit retries successfully without a restart");
    }
    {
        Fixture f(false);
        f.planned_return.reset();
        f.plc_act_pos[2]=600; f.plc_act_pos[4]=640; f.plc_act_pos[5]=659;
        f.tick(true);
        check(std::abs(f.spacing_recovery.remaining_mm-1.0)<1e-9,
              "recovery travel also stops at axis6 from-left 660 mm");
    }
    for(bool reverse:{false,true}) for(bool both:{false,true}) for(bool hold_through_return:{false,true}) {
        Fixture f(reverse,both);
        f.tick(true);
        check(f.planned_return.active()&&!f.spacing_recovery.active(),"recovery press during return must preserve the PLC handoff owner");
        check(f.clears==0&&f.control_active&&f.plc_busy,"recovery key must not clear or interrupt an executing return");
        f.tick(false); f.tick(true); f.tick(hold_through_return);
        check(f.commits==0&&f.planned_return.leg_count==(both?2:1),"repeated recovery edges must not create another return");
        f.finish_plc();
        for(int n=0;n<40;++n) f.tick(hold_through_return);
        check(!f.planned_return.active()&&f.rebases>=2,"original return finishes its fresh-snapshot and clamp handoff");
        check(f.spacing_recovery.active()==hold_through_return,"only a still-held recovery button enters recovery after return");
        f.tick(false);
        for(int n=0;n<25;++n) f.tick(false);
        check(!f.spacing_recovery.active()&&!f.spacing_recovery.requested&&f.control_active,"release restores follow without restarting control");
        check(f.axis1_follow_cmd_abs==f.plc_act_pos[0]&&f.axis6_follow_cmd_abs==f.plc_act_pos[5],"both translation targets are rebased to current feedback");
        const double before=f.plc_act_pos[0];
        for(int repeat=0;repeat<3;++repeat) { f.tick(true); f.tick(false); for(int n=0;n<25;++n) f.tick(false); }
        check(f.commits==0&&f.plc_act_pos[0]==before&&!f.planned_return.active(),"subsequent recovery presses never replay the old full-window return");
    }
    std::cout<<"PASS "<<assertions<<" assertions; return/recovery crossover in both directions and dual-leg returns\n";
}
