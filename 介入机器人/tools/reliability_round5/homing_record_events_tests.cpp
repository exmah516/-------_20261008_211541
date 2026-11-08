#define NOMINMAX
#include "../../B数据端/工程/ADS项目/homing_record.h"
#include <cstdlib>
#include <iostream>
static int count=0;
static void check(bool ok,const char* label) { ++count;if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);} }
struct Events {
    homing::Record homing_record;
    std::filesystem::path homing_record_path="homing_events_test.json";
    bool homing_record_pending=true,ads_self_check_valid=true,self_check_done=true,plc_app_name_read=true;
    bool homing_reuse_pending=false,homing_restore_submitted=false;
    std::uint64_t homing_record_refresh_generation=1,homing_record_update_ms=0,tick=1000;
    std::string plc_application_name="RobotPLC";
    struct { std::uint64_t generation=1; std::uint64_t coordinate_refresh_generation() const { return generation; } } ads_communication;
    struct { bool valid=true; double leftlimit[7]={},init_pos[7]={},act_pos_rel[7]={}; } ads_snapshot;
    void cycle() {
        auto GetTickCount64=[&]{return tick;};
#include "homing_record_events.inc"
    }
};
int main(){
    Events f; std::filesystem::remove(f.homing_record_path);
    f.cycle(); check(!std::filesystem::exists(f.homing_record_path),"V11 rising edge waits for successful coordinate refresh");
    f.ads_communication.generation=2; f.ads_self_check_valid=false;f.cycle();
    check(!std::filesystem::exists(f.homing_record_path),"V11 invalid ADS snapshot never writes record");
    f.ads_self_check_valid=true;f.ads_snapshot.valid=false;f.cycle();
    check(!std::filesystem::exists(f.homing_record_path),"V11 position-only snapshot with invalid force never writes record");
    f.ads_snapshot.valid=true;
    f.ads_self_check_valid=true; f.ads_snapshot.init_pos[0]=12;f.ads_snapshot.act_pos_rel[0]=3; f.cycle();
    auto record=homing::load(f.homing_record_path);
    check(record.complete&&record.last_abs[0]==15&&!f.homing_record_pending,"V11 background coordinate refresh completes new record");
    f.ads_snapshot.act_pos_rel[0]=4;f.tick=1999;f.cycle();
    check(homing::load(f.homing_record_path).last_abs[0]==15,"V11 positions not updated before1s");
    f.tick=2000;f.cycle();check(homing::load(f.homing_record_path).last_abs[0]==16,"V11 positions update at1s");
    check(homing::load(f.homing_record_path).completed_time_unix_ms==record.completed_time_unix_ms,"V11 periodic update preserves homing completion time");
    for(int gate=0;gate<4;++gate){
        f.ads_snapshot.act_pos_rel[0]=20;f.tick+=1000;
        if(gate==0)f.self_check_done=false;if(gate==1)f.ads_self_check_valid=false;
        if(gate==2)f.homing_reuse_pending=true;if(gate==3)f.homing_restore_submitted=true;
        f.cycle();check(homing::load(f.homing_record_path).last_abs[0]==16,"V11 disconnect/restart/incomplete homing freezes last_abs");
        f.self_check_done=f.ads_self_check_valid=true;f.homing_reuse_pending=f.homing_restore_submitted=false;
    }
    f.homing_record_pending=true;f.homing_record_refresh_generation=2;f.cycle();
    check(homing::load(f.homing_record_path).last_abs[0]==16,"V11 new SelfCheck waits for its own refresh");
    f.ads_communication.generation=3;f.ads_snapshot.leftlimit[0]=-10;f.cycle();
    check(homing::load(f.homing_record_path).leftlimit[0]==-10,"V11 new SelfCheck replaces old homing result");
    std::filesystem::remove(f.homing_record_path);
    std::cout<<"PASS V11 extracted main record event rules checks="<<count<<'\n';
}
