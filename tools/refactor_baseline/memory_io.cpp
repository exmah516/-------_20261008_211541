// 完整生产主循环的离线输入端；不链接 ADS、Zenoh、手柄 SDK 或 NC。
#include "baseline_support.h"
#include "st_plc_fixture.h"
#include "control_types.h"
#include "zenoh_service.h"
#include "experiment_recorder.h"
#include "手柄/remote_handle_bridge.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {
struct Finished {};
struct Restarted {};
const std::string scenario=std::getenv("BASELINE_SCENARIO");
int tick=0, commits=0, preparations=0, clears=0, last_phase=0, completed_returns=0;
std::uint64_t generation=0, motion_generation=0, command_sequence=0;
std::uint64_t applied_generation=0;
AdsFastSnapshot snapshot;
AdsEventState events;
AdsCommunicationStats memory_stats;
AdsOutputCommand output;
PlcFixture plc;
std::uint64_t sent_token = 0;
int transactions = 0;
std::deque<VisCommand> commands;
double actual[7]={18,12,100,0,110,123,34}, velocity[7]={}, joints[2][2]={};
bool req[2]={};
bool injected_fault=false;
bool injected_pause=false, injected_restart=false;
int transaction_hold_end=0;
int lead_hold_tick=0;
int homing_requested_at=0;
double observed_mapping_progress=0;
bool observed_mapping_active=false;
std::ofstream trace;
std::ofstream event_trace;
std::array<int,10> phases{};
bool saw_mapping=false,saw_hold=false,saw_spacing_active=false,saw_fault=false;

void command(VisCommandType type,int a=0,int b=0) {
    VisCommand c; c.type=type;c.param1=a;c.param2=b;commands.push_back(c);
}
void samples() {
    for(int slot=0;slot<2;++slot) {
        const bool gap = (scenario=="handle_gap_short" && tick>=30 && tick<60) ||
            (scenario=="handle_gap_rebase" && tick>=30 && tick<200) ||
            (scenario=="handle_disconnect" && tick>=30 && tick<640);
        if(gap && slot==1) continue;
        if(scenario=="txn_handle_hold" && transaction_hold_end>tick && slot==1) continue;
        if(scenario=="event_missing582" && slot==0 && tick>=30 && tick<640) continue;
        remote_handle_bridge::Sample s;
        s.valid=true;s.device_epoch=(scenario=="device_epoch" && tick>=40)?2:1;
        s.joints[0]=static_cast<float>(joints[slot][0]);s.joints[1]=static_cast<float>(joints[slot][1]);
        if(scenario=="follow_retraction") s.buttons=0x40;
        if((scenario=="spacing" || scenario=="spacing_hold") && ((tick>=30 && tick<100)||(tick>=170 && tick<230)))
            if(slot==1) s.buttons=0x07;
        if(scenario=="event_rejected_hold" && tick>=42 && tick<44 && slot==1) s.buttons=0x07;
        remote_handle_bridge::put_sample(slot,s);
    }
}
struct Initialize {
    Initialize() {
        events.self_check_done=events.handle_reinit_done=true;events.selfcheck_status=0;
        memory_stats.state=AdsConnectionState::Running; memory_stats.handoff_if_version=scenario=="txn_version"?2:kHandoffInterfaceVersion;
        if(scenario.find("handoff")==0 || scenario.find("return")==0 || scenario.find("txn_")==0 || scenario=="lead_hold" || scenario=="lead_axis6") {
            actual[0]=5;
            if(scenario=="handoff_axis1") actual[0]=7;
            if(scenario=="handoff_active" || scenario=="return_axis6" || (scenario.find("return_")==0 && scenario.find("_axis6")!=std::string::npos)) { actual[0]=18;actual[5]=111; }
            if(scenario=="handoff_passive") { actual[0]=18;actual[5]=142; }
            if(scenario=="handoff_active") actual[5]=112;
            if(scenario=="handoff_merge" || scenario=="return_both" || scenario.find("_both")!=std::string::npos) actual[5]=141;
            if(scenario.find("txn_")==0) actual[5]=141;
        }
        if(scenario=="follow_retraction") actual[0]=29;
        samples();
        if(scenario=="txn_start_wait") { actual[0]=20; actual[5]=120; plc.request(3,90); plc.until(30); }
    }
} initialize;
void advance() {
    if(++tick>800) throw Finished{};
    baseline::now_ms+=10;
    if(scenario=="txn_restart" && plc.phase()==30 && !injected_restart) { injected_restart=true; throw Restarted{}; }
    if((scenario=="txn_ads_hold" || scenario=="txn_handle_hold") && plc.phase()==30 && !transaction_hold_end) {
        transaction_hold_end=tick+160;
        if(scenario=="txn_handle_hold") remote_handle_bridge::clear_samples();
    }
    if(scenario=="txn_pause" && plc.phase()==30 && !injected_pause) {
        injected_pause=true; baseline::now_ms+=2000;
        for(int i=0;i<2000;++i) plc.cycle();
        if(plc.phase()!=60 || plc.result()!=1) throw std::runtime_error("PLC did not independently complete during main pause");
        for(int axis : {0,5}) actual[axis]=double(plc.G["axis"][axis+1]["NcToPlc"]["ActPos"]);
    }
    applied_generation=generation;
    if(output.motion_enabled) motion_generation=generation;
    const bool pushing=tick>=12 && tick<260 && !(scenario.find("txn_")==0 && (commits>0 || scenario=="txn_start_wait"));
    if(pushing) {
        double step=0.12;
        if(scenario=="handoff_active" || scenario=="return_axis6" || (scenario.find("return_")==0 && scenario.find("_axis6")!=std::string::npos)) joints[0][0]+=step/750.0;
        else if(scenario=="handoff_passive") joints[1][0]+=step/750.0;
        else if(scenario=="follow_retraction") { joints[0][0]-=step/750.0;joints[1][0]-=step/750.0; }
        else if(scenario!="axis4" && scenario!="manual" && scenario.find("restart")!=0) {
            joints[1][0]+=step/750.0;
            if(scenario=="follow_delivery") joints[0][0]+=step/750.0;
        }
        if(scenario.find("restart")!=0) { joints[0][1]+=0.0003;joints[1][1]-=0.0004; }
    }
    if(scenario=="lead_axis6" && observed_mapping_active && observed_mapping_progress>=4 && completed_returns<2) {
        joints[1][0]-=pushing?0.12/750.0:0;
        joints[0][0]+=0.5/750.0;
    }
    if((scenario=="spacing" || scenario=="spacing_hold") && tick>=40 && tick<95)
        joints[1][0]-=0.002; // 587 的正轴1增量，向增大3/5/6位置的方向恢复。
    if((scenario=="spacing" || scenario=="spacing_hold") && tick>=95 && tick<100)
        joints[1][0]+=0.002;
    samples();
    memory_stats.state=((scenario=="txn_ads_hold" && transaction_hold_end>tick) || (scenario=="ads_hold" && tick>=40 && tick<45) ||
        (scenario=="spacing_hold" && tick>=70 && tick<75) ||
        (scenario=="event_rejected_hold" && tick>=40 && tick<45) ||
        (scenario=="lead_hold" && lead_hold_tick && tick>=lead_hold_tick && tick<lead_hold_tick+3))
        ?AdsConnectionState::SoftHold:AdsConnectionState::Running;
    if(scenario=="ads_hold" && tick==45) ++memory_stats.reconnect_count;
    events.estop_hold_req=scenario=="return_estop" && tick>=100 && tick<110;
    if(scenario.find("restart")==0 && tick==40) ++memory_stats.plc_restart_count;
    if(scenario.find("restart")==0 && tick>=40) {
        const bool accepted=homing_requested_at && tick>=homing_requested_at+2;
        events.self_check_done=events.handle_reinit_done=accepted;
        events.gen_state=accepted?8:7;events.selfcheck_status=accepted?2:1;
    }
    for(int axis=0;axis<7;++axis) {
        double old=actual[axis];
        bool returning=(axis==0 && plc.phase()>=30 && (int(plc.G["handoff_status"]["leg_mask"])&1)) || (axis==5 && plc.phase()>=30 && (int(plc.G["handoff_status"]["leg_mask"])&2));
        if(output.motion_enabled && !returning) actual[axis]+=std::clamp(output.refer[axis]-actual[axis],-0.25,0.25);
        velocity[axis]=(actual[axis]-old)*100;
    }
    for(int axis=0;axis<7;++axis) {
        plc.G["axis"][axis+1]["NcToPlc"]["ActPos"]=actual[axis];
        plc.G["refer"][axis+1]=output.motion_enabled?output.refer[axis]:actual[axis];
    }
    if(events.estop_hold_req && !plc.interrupted) plc.interrupted=true;
    if(!events.estop_hold_req && plc.interrupted) plc.recover();
    const int cycles=10;
    for(int ms=0;ms<cycles;++ms) {
        if(scenario=="txn_always_fail" && plc.phase()==10) injected_fault=false;
        if((scenario.find("return_fault")==0 || scenario=="txn_always_fail") && !injected_fault && plc.phase()==30) {
            injected_fault=true;plc.fault_kind=1;plc.fault_axis=(int(plc.G["handoff_cmd"]["leg_mask"])&1)?1:6;
        }
        plc.cycle();
        if(plc.phase()>=40) plc.fault_kind=0;
    }
    for(int slot=0;slot<2;++slot) {
        const int axis=slot?5:0;
        actual[axis]=double(plc.G["axis"][axis+1]["NcToPlc"]["ActPos"]);
        req[slot]=plc.phase()>=10 && plc.phase()<60 && (int(plc.G["handoff_status"]["leg_mask"])&(1<<slot));
    }
    snapshot={};
    snapshot.attempt_sequence=static_cast<std::uint64_t>(tick);snapshot.qpc_ticks=static_cast<std::int64_t>(baseline::now_ms*1000);
    snapshot.position_valid=memory_stats.state==AdsConnectionState::Running;
    snapshot.force_valid=snapshot.valid=snapshot.position_valid;
    snapshot.self_check_done=events.self_check_done;snapshot.handle_reinit_done=events.handle_reinit_done;
    snapshot.estop_hold_req=events.estop_hold_req;
    for(int a=0;a<7;++a) snapshot.act_pos_rel[a]=snapshot.act_pos_from_left[a]=actual[a];
    snapshot.axis1_act_velocity_mm_s=velocity[0];snapshot.axis3_act_velocity_mm_s=velocity[2];
    snapshot.axis5_act_velocity_mm_s=velocity[4];snapshot.axis6_act_velocity_mm_s=velocity[5];
    snapshot.handoff_if_version=kHandoffInterfaceVersion;
    auto& h=plc.G["handoff_status"];
    snapshot.handoff.ack_id=static_cast<std::uint32_t>(double(h["ack_id"]));
    snapshot.handoff.phase=static_cast<std::int16_t>(double(h["phase"]));
    snapshot.handoff.phase_seq=static_cast<std::uint32_t>(double(h["phase_seq"]));
    snapshot.handoff.done_id=static_cast<std::uint32_t>(double(h["done_id"]));
    snapshot.handoff.result=static_cast<std::int16_t>(double(h["result"]));
    snapshot.handoff.error_code=static_cast<std::uint32_t>(double(h["error_code"]));
    snapshot.handoff.error_axis=static_cast<std::int16_t>(double(h["error_axis"]));
    snapshot.handoff.nc_error_code=static_cast<std::uint32_t>(double(h["nc_error_code"]));
    snapshot.handoff.leg_mask=static_cast<std::uint8_t>(double(h["leg_mask"]));
    for(int i=0;i<8;++i) snapshot.handoff.phase_ms[i]=static_cast<std::uint32_t>(double(h["phase_ms"][i+1]));
    snapshot.fn_1_value=static_cast<short>(100+(tick%31));snapshot.ft_1_value=static_cast<short>(50+(tick%17));
    snapshot.fn_2_value=static_cast<short>(120+(tick%23));snapshot.ft_2_value=static_cast<short>(70+(tick%19));
    if(tick==5) command(VisCommandType::SelectDirectControl);
    if(scenario=="axis4" && tick>=20 && tick<=80 && tick%10==0) command(VisCommandType::SetAxis4ManualJog,1);
    if(scenario=="manual") {
        if(tick==20) command(VisCommandType::SetCylinderManualPosition,2,780);
        if(tick==30) command(VisCommandType::ResetCylinderManual,2);
        if(tick==40) command(VisCommandType::SetYValveOpen,1);
        if(tick==50) command(VisCommandType::SetYValveOpen,0);
        if(tick==60) command(VisCommandType::SetInjectorManualJog,1,1);
        if(tick==120) command(VisCommandType::SetInjectorManualJog,2,-1);
    }
    if(scenario=="feedback" && tick==20) command(VisCommandType::ZeroForceSensor);
    if(scenario=="feedback" && tick==22) command(VisCommandType::SetForceFeedback,1);
    if(scenario=="feedback" && tick==24) command(VisCommandType::SetForceFeedbackHold,1);
}
}
void baseline_record_event(ExperimentEventCode code,int value,std::uint64_t sequence) {
    if(!event_trace.is_open()) {
        auto path=std::filesystem::path(_wgetenv(L"BASELINE_OUTPUT"));
        path+=L".events.csv";
        event_trace.open(path,std::ios::binary);
        if(!event_trace) throw std::runtime_error("event trace open failed");
        event_trace<<"tick,ads_snapshot_sequence,event_code,value\n";
    }
    event_trace<<tick<<','<<sequence<<','<<int(code)<<','<<value<<'\n';
}
bool AdsCommunicationService::wait_for_snapshot(std::uint64_t,DWORD,AdsFastSnapshot& value) { advance();value=snapshot;return true; }
bool AdsCommunicationService::latest_snapshot(AdsFastSnapshot& value) const { value=snapshot;return tick>0; }
void AdsCommunicationService::drain_snapshots(std::vector<AdsFastSnapshot>& out) { out={snapshot}; }
std::uint64_t AdsCommunicationService::publish_output(const AdsOutputCommand& value) {
    output=value;++generation;
    if(value.cylinder_valid) for(int i=0;i<4;++i) if(value.cylinder_write_mask&(1<<i)) plc.G["cyl_cmd"][i+1]=value.cylinder[i];
    if(value.handoff_send_token && value.handoff_send_token!=sent_token) {
        sent_token=value.handoff_send_token;++commits;
        const bool fail=(scenario.find("return_retry")==0 && commits==1) || scenario=="txn_no_ack";
        if(!fail) {
            const auto& c=value.handoff;auto& dest=plc.G["handoff_cmd"];
            dest["leg_mask"]=c.leg_mask;
            for(int i=0;i<2;++i) {
                dest["target_abs"][i+1]=c.target_abs[i];dest["velocity"][i+1]=c.velocity[i];
                dest["acc"][i+1]=c.acc[i];dest["dec"][i+1]=c.dec[i];dest["jerk"][i+1]=c.jerk[i];
                for(int j=0;j<4;++j) dest["wait_ms"][i+1][j+1]=c.wait_ms[i][j];
            }
            for(int i=0;i<4;++i) { dest["cyl_clamp"][i+1]=c.cyl_clamp[i];dest["cyl_open"][i+1]=c.cyl_open[i]; }
            dest["request_id"]=c.request_id;
            ++transactions;
        }
    }
    if(value.handoff_takeover_id) plc.G["handoff_takeover_id"]=value.handoff_takeover_id;
    return generation;
}
std::uint64_t AdsCommunicationService::applied_output_generation() const { return applied_generation; }
std::uint64_t AdsCommunicationService::applied_motion_output_generation() const { return motion_generation; }
bool AdsCommunicationService::read(const char* raw,unsigned long size,void* dst,DWORD) {
    std::string name(raw);std::memset(dst,0,size);
    if(name.find("AppName")!=std::string::npos) { std::strncpy(static_cast<char*>(dst),"MemoryPLC",size-1);return true; }
    if(name=="G.v_limit") { std::fill_n(static_cast<double*>(dst),7,110.0);return true; }
    if(name.find("cylinder")!=std::string::npos) { *static_cast<unsigned short*>(dst)=500;return true; }
    if(name.find("power_output")!=std::string::npos && name.find(".Done")!=std::string::npos) *static_cast<bool*>(dst)=true;
    if(name.find(".NcToPlc.ActPos")!=std::string::npos) {
        const int axis=std::stoi(name.substr(name.find('[')+1))-1;
        *static_cast<double*>(dst)=actual[axis]+(scenario=="restart_reject"?2.0:0.0);
    }
    if(name=="G.homing_restore_status") *static_cast<std::int32_t*>(dst)=homing_requested_at?1:0;
    return true;
}
bool AdsCommunicationService::write(const char* name,unsigned long,const void*,DWORD) {
    if(std::string(name)=="G.homing_restore_req") homing_requested_at=tick;
    return true;
}
bool AdsCommunicationService::read_sum(const char* const* names,const unsigned long* sizes,void* const* dst,unsigned long n,DWORD ms) {
    for(unsigned long i=0;i<n;++i) read(names[i],sizes[i],dst[i],ms);return true;
}
bool AdsCommunicationService::write_sum(const char* const* names,const unsigned long* sizes,const void* const* src,unsigned long n,DWORD ms) {
    for(unsigned long i=0;i<n;++i) write(names[i],sizes[i],src[i],ms);return true;
}
AdsEventState AdsCommunicationService::event_state() const { return events; }
AdsCommunicationStats AdsCommunicationService::stats() const { return memory_stats; }
bool ZenohService::poll_command(VisCommand& c) { if(commands.empty()) return false;c=commands.front();commands.pop_front();return true; }
void baseline_capture(const AdsOutputCommand& o,std::initializer_list<double> state) {
    if(!trace.is_open()) {
        trace.open(std::filesystem::path(_wgetenv(L"BASELINE_OUTPUT")),std::ios::binary);
        if(!trace) throw std::runtime_error("trace open failed");
        trace<<"tick,refer1,refer2,refer3,refer4,refer5,refer6,refer7,cyl1,cyl2,cyl3,cyl4,fast1,fast6,req1,req6,clear_mask,motion,cylinder,mask,yvalve,axis4f,axis4r,inject1p,inject1r,inject2p,inject2r,prepare_count,commit_count,clear_count";
        for(size_t i=0;i<state.size();++i) trace<<",state"<<i;
        trace<<"\n";trace<<std::hexfloat;
    }
    trace<<tick;
    for(double v:o.refer) trace<<','<<v;
    for(auto v:o.cylinder) trace<<','<<v;
    trace<<','<<false<<','<<false<<','<<req[0]<<','<<req[1]<<','<<0
         <<','<<o.motion_enabled<<','<<o.cylinder_valid<<','<<int(o.cylinder_write_mask)<<','<<o.cylinder5_press_req
         <<','<<o.axis4_forward_req<<','<<o.axis4_reverse_req<<','<<o.inject_push_req[0]<<','<<o.inject_pull_req[0]
         <<','<<o.inject_push_req[1]<<','<<o.inject_pull_req[1]<<','<<preparations<<','<<commits<<','<<clears;
    for(double v:state) trace<<','<<v;
    trace<<'\n';
    auto it=state.begin();
    int phase=int(*(it+9));
    if(phase>=0 && phase<int(phases.size())) ++phases[phase];
    if(last_phase==3 && phase==0) ++completed_returns;
    last_phase=phase;saw_mapping|=*(it+20)!=0;saw_hold|=*(it+2)!=0;saw_spacing_active|=*(it+16)==2;saw_fault|=*(it+6)!=0;
    if(scenario=="lead_hold" && !lead_hold_tick && *(it+20)!=0 && *(it+21)>=4) lead_hold_tick=tick+1;
    observed_mapping_active=*(it+20)!=0;observed_mapping_progress=*(it+21);
}
int control_main();
int main() {
    try { for(;;) { try { control_main(); break; } catch(const Restarted&) { commands.clear(); output={}; command(VisCommandType::SelectDirectControl); } } }
    catch(const Finished&) {
        trace.close();
        event_trace.close();
        std::cout<<"COVERAGE "<<scenario<<" commits="<<commits<<" complete="<<completed_returns
                 <<" starts="<<(plc.starts[1]+plc.starts[6])<<" transactions="<<transactions<<" mapping="<<saw_mapping<<" hold="<<saw_hold<<" spacing_active="<<saw_spacing_active<<" fault="<<saw_fault<<" phases=";
        for(int count:phases) std::cout<<count<<',';
        std::cout<<" pause="<<injected_pause<<" restart="<<injected_restart<<" plc_phase="<<plc.phase()<<" plc_result="<<plc.result();
        std::cout<<'\n';return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
    return 2;
}
