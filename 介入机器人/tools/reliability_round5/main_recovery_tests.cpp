// 抽取生产主循环的恢复门控、规划入口和续做分支；PLC/ADS只用内存替身。
#define main baseline_return_tests
#include "../../B数据端/工程/ADS项目/tools/return_recovery_test.cpp"
#undef main
#include "return_recovery_crawl.inc"
#include <fstream>
enum class AdsConnectionState { Running=2, SoftHold=3 };
struct AdsPlannedReturnCommand {
    AdsPlannedReturnOperation operation{}; int leg_count=0;
    struct Leg { int axis_index=0; } legs[2];
};
namespace motion_sync {
    void calculate_axis6_window_from_axis5_abs(int,double axis5,double& left,double& right) { left=axis5+1; right=axis5+33; }
}
static double clamp_double(double x,double a,double b) { return std::clamp(x,a,b); }
static bool is_within_range(double x,double a,double b,double tol) { return x>=a-tol && x<=b+tol; }
struct Round5 : Fixture {
    CrawlState axis1_crawl,axis6_crawl;
    bool resuming_return=false;
    PlannedReturnMode interrupted_return_mode=PlannedReturnMode::Dual;
    PlannedReturnRebaseScope interrupted_return_scope=PlannedReturnRebaseScope::Both;
    DualHandoffRequest original_return_intent;
    bool has_new_ads_snapshot=true;
    struct { bool valid=true,estop_hold_req=false,host_comm_timeout=false,handle_reinit_done=true;
        bool return_req[2]={},return_busy[2]={}; std::uint64_t attempt_sequence=0; } ads_snapshot;
    struct { bool axis4_manual_busy=false; } ads_events;
    struct { AdsConnectionState state=AdsConnectionState::Running; } ads_stats;
    struct : decltype(Fixture::ads_communication) {
        std::uint64_t seq=0; int submissions=0; bool queue_full=false;
        bool planned_return_command_result(std::uint64_t,bool& done,bool& success,std::uint8_t& mask)
        { done=true; success=true; mask=0; return true; }
        std::uint64_t submit_planned_return_command(const AdsPlannedReturnCommand&) { ++submissions; return queue_full?0:++seq; }
    } ads_communication;
    int syncs=0, force_clears=0;
    bool estop_hold_req=false;
    std::ostringstream plc_estop_diagnostic;
    const char* diagnostic_path="round5_estop_test.log";
    Round5(bool reverse=false,bool both=true) : Fixture(reverse,both) {
        axis1_crawl.start_abs=5; axis1_crawl.end_abs=30;
        axis6_crawl.start_abs=101; axis6_crawl.end_abs=133;
        return_recovery.original_trigger(both?3:1);
        original_return_intent.reverse=reverse;
    }
    void recover() {
        now_ms+=10; ++ads_snapshot.attempt_sequence;
        auto GetTickCount64=[&]() { return static_cast<ULONGLONG>(now_ms); };
        auto sync_all=[&](int) { ++syncs; return true; };
#include "return_recovery_recovery_gate.inc"
    }
    void estop(bool requested) {
        estop_hold_req=requested;
        auto clear_axis1_delivery_mapping=[](int=0){};
        auto load_pos_from_actual=[&](){std::copy(plc_act_pos,plc_act_pos+7,pos);};
        auto clear_force_output=[&](){++force_clears;};
        auto clear_planned_return_ads_command_tracking=[&](){planned_return.ads_command_purpose=PlannedReturnAdsCommandPurpose::None;};
        auto submit_planned_return_ads_command=[&](AdsPlannedReturnOperation,PlannedReturnAdsCommandPurpose,bool){++clears;return true;};
#include "return_recovery_cancel.inc"
#include "return_recovery_estop.inc"
    }
    bool resume() {
        const double axis1_abs=plc_act_pos[0]+plc_init_pos[0],axis5_abs=plc_act_pos[4]+plc_init_pos[4],axis6_abs=plc_act_pos[5]+plc_init_pos[5];
        auto axis6_target_exceeds_soft_limit=[&](double target) { return target-plc_leftlimit[5]>cfg.axis6_soft_limit_from_left_mm+1e-6; };
        auto clear_axis1_delivery_mapping=[](int=0) {};
        auto clear_cylinder_manual_overrides=[]() {};
        auto apply_planned_return_outputs=[]() {};
#include "return_recovery_safety.inc"
#include "return_recovery_begin.inc"
#include "return_recovery_resume.inc"
        return planned_return.active();
    }
};
int main() {
    for(int failure=0;failure<4;++failure) {
        Round5 f;
        if(failure==0) { f.plc_busy=false; f.planned_return.phase=PlannedReturnPhase::AwaitAccepted; f.planned_return.retry_count=1; f.planned_return.legs[0].acknowledged=f.planned_return.legs[1].acknowledged=false;
            f.planned_return.execution_t0_ms=0; f.planned_return.request_t0_ms=1; }
        if(failure==1) f.planned_return.execution_t0_ms=1;
        if(failure==2) f.plc_error=true;
        if(failure==3) { f.plc_busy=false; for(auto& leg:f.planned_return.legs) { leg.started=false; leg.acknowledged=false; leg.possibly_started=false; }
            f.planned_return.phase=PlannedReturnPhase::RetryClear; f.planned_return.phase_t0_ms=1;
            f.planned_return.ads_command_purpose=PlannedReturnAdsCommandPurpose::RetryClear; f.command_result=PlannedReturnAdsPoll::Failure; }
        f.now_ms=10000; f.tick(false);
        check(f.return_ads_fault_hold&&!f.control_active&&!f.planned_return.active()&&f.clears>0,"V1 actual failure branch stops and cancels");
        f.ads_communication.queue_full=true; f.recover(); int submitted=f.ads_communication.submissions;
        for(int n=0;n<24;++n) f.recover();
        check(f.ads_communication.submissions==submitted && f.return_ads_fault_hold,"V1 failed Clear enqueue respects250ms");
        f.ads_communication.queue_full=false;
        for(int n=0;n<5&&f.return_ads_fault_hold;++n) f.recover();
        check(!f.return_ads_fault_hold&&f.control_active&&f.syncs==1,"V1 fresh Clear restores control with sync_all");
        f.plc_error=false; check(f.resume()&&f.return_recovery.resumes()==1,"V1 original planning entry resumes once");
        f.planned_return.phase=PlannedReturnPhase::AwaitDone; f.plc_error=true;
        for(int leg=0;leg<f.planned_return.leg_count;++leg) f.planned_return.legs[leg].request_armed=true;
        f.tick(false);
        check(f.return_ads_fault_hold&&!f.control_active,"V1 repeated failure stops again");
        for(int n=0;n<4&&f.return_ads_fault_hold;++n) f.recover();
        check(!f.return_ads_fault_hold&&f.control_active&&!f.return_recovery.resume_pending(),"V1 second failure restores without second resume");
    }
    for(bool reverse:{false,true}) {
        Round5 f(reverse);
        f.estop(true);
        check(!f.planned_return.active()&&f.clears==1&&f.force_clears==1,"V2 original estop branch cancels and clears force");
        f.recover(); f.recover();
        check(f.return_ads_fault_hold&&!f.control_active,"V2 estop gates restoration");
        f.estop(false); f.recover();
        check(f.control_active&&f.resume(),"V2 recovery replans with original intent");
        check(f.planned_return.legs[0].target_abs==(reverse?5:30),"V2 original direction retained");
        f.planned_return.phase=PlannedReturnPhase::PostHandoffClampSettle;
        f.planned_return.restore_stage=1; f.planned_return.release_applied=true; f.planned_return.phase_t0_ms=1;
        f.tick(false);
        check(!f.planned_return.active()&&f.return_recovery.resumes()==0,"V1 successful production handoff clears original counter");
    }
    Round5 unsafe; unsafe.return_recovery.interrupted("error"); unsafe.return_recovery.restored(); unsafe.planned_return.reset();
    unsafe.original_return_intent.wire_caused=true; unsafe.cfg.axis6_active_delivery_gap_mm=100;
    check(!unsafe.resume() && !unsafe.return_recovery.resume_pending(),"V1 original target safety rejects unsafe replan");
    Round5 pending; pending.return_recovery.interrupted("error"); pending.return_ads_fault_hold=true;
    pending.planned_return.reset(); pending.ads_snapshot.handle_reinit_done=false;
    pending.ads_motion_cycle_valid=false; pending.ads_snapshot.return_req[0]=true;
    pending.recover(); const int first_clear=pending.ads_communication.submissions;
    for(int n=0;n<25;++n) pending.recover();
    check(pending.ads_communication.submissions>first_clear && pending.return_ads_fault_hold,
        "V1 Clear retries during PLC reinitialization without restoring control");
    std::cout<<"PASS V1/V2 extracted main recovery/continuation branches\n";
}
