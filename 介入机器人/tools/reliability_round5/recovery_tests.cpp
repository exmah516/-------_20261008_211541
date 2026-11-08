#include "return_recovery.h"
#include "homing_record.h"
#include <cstdlib>
#include <iostream>

static int checks=0;
static void check(bool value, const char* label)
{ ++checks; if(!value) { std::cerr<<"FAIL "<<label<<'\n'; std::exit(1); } }
int main()
{
    ReturnRecoveryConditions c;
    c.ads_running=c.snapshot_valid=c.handle_reinit_done=c.handles_valid=true;
    c.snapshot_sequence=10;
    for(const char* reason : {"ACK timeout","execution timeout","PLC error","Clear queue full","estop"})
    {
        ReturnRecovery recovery; recovery.original_trigger(3); recovery.interrupted(reason);
        check(!recovery.can_restore(c), "V1/V2 no recovery without confirmed Clear");
        c.req[0]=true;
        check(recovery.clear_due(0,c.req) && !recovery.clear_due(249,c.req) && recovery.clear_due(250,c.req), "V1 Clear every250ms");
        recovery.clear_submitted(0);
        check(recovery.clear_due(500,c.req), "V1 failed Clear enqueue is retried");
        recovery.clear_submitted(1); recovery.clear_result(true,10);
        c.req[0]=false; c.snapshot_sequence=10;
        check(!recovery.can_restore(c), "V1 old snapshot cannot confirm Clear");
        c.snapshot_sequence=11;
        for(int gate=0;gate<10;++gate)
        {
            auto blocked=c;
            switch(gate) {
            case 0: blocked.ads_running=false; break; case 1: blocked.snapshot_valid=false; break;
            case 2: blocked.estop=true; break; case 3: blocked.host_timeout=true; break;
            case 4: blocked.handle_reinit_done=false; break; case 5: blocked.handles_valid=false; break;
            case 6: blocked.startup_active=true; break; case 7: blocked.exclusive_task=true; break;
            case 8: blocked.req[1]=true; break; case 9: blocked.busy[0]=true; break;
            }
            check(!recovery.can_restore(blocked), "V1 each required recovery gate");
        }
        check(recovery.can_restore(c), "V1/V2 recovery allowed at current position");
        recovery.restored(); check(recovery.resume_pending(), "V1/V2 resume first interruption");
        recovery.consume_resume(); check(recovery.resumes()==1 && !recovery.resume_pending(), "V1 resume exactly once");
        recovery.interrupted("second failure"); recovery.clear_submitted(2); recovery.clear_result(true,11); c.snapshot_sequence=12;
        check(recovery.can_restore(c), "V1 second failure still restores control");
        recovery.restored(); check(!recovery.resume_pending(), "V1 second failure never auto-resumes");
        recovery.completed(); recovery.original_trigger(1); recovery.interrupted("new trigger");
        recovery.clear_submitted(3); recovery.clear_result(true,12); c.snapshot_sequence=13;
        check(recovery.can_restore(c), "V1 next original trigger can recover");
        recovery.restored(); check(recovery.resume_pending() && recovery.resumes()==0, "V1 normal completion clears resume counter");
    }
    const auto directory=std::filesystem::current_path();
    ReturnRecovery finished;
    finished.original_trigger(1); finished.interrupted("handoff interrupted",false);
    finished.clear_submitted(1); finished.clear_result(true,1); finished.restored();
    check(!finished.resume_pending(), "V2 completed motion is not repeated when handoff interrupted");
    const auto path=directory/L"homing_test_record.json";
    homing::Record record; record.complete=true; record.application="RobotPLC";
    record.completed_time_unix_ms=record.last_abs_time_unix_ms=1000;
    for(int axis=0;axis<7;++axis) { record.leftlimit[axis]=axis-10.25; record.init_pos[axis]=axis*2.25; record.last_abs[axis]=axis*3.5; }
    check(homing::save(path,record), "V11 atomic initial write");
    auto loaded=homing::load(path);
    check(loaded.complete && loaded.leftlimit==record.leftlimit && loaded.init_pos==record.init_pos && loaded.last_abs==record.last_abs, "V11 complete record exact roundtrip");
    double current[7]; std::copy(record.last_abs.begin(),record.last_abs.end(),current);
    check(homing::reject_reason(true,loaded,"RobotPLC",current).empty(), "V11 reuse pass");
    check(!homing::reject_reason(false,loaded,"RobotPLC",current).empty(), "V11 switch disabled refuses");
    check(!homing::reject_reason(true,loaded,"OtherPLC",current).empty(), "V11 application differs refuses");
    for(int axis=0;axis<7;++axis)
    {
        const double tolerance=axis==1||axis==3||axis==6?1.0:0.5;
        current[axis]+=tolerance; check(homing::reject_reason(true,loaded,"RobotPLC",current).empty(), "V11 tolerance boundary accepted");
        current[axis]+=0.001; check(!homing::reject_reason(true,loaded,"RobotPLC",current).empty(), "V11 every axis outside tolerance refused");
        current[axis]=record.last_abs[axis];
    }
    record.last_abs[0]=123.5; record.last_abs_time_unix_ms=2000;
    check(homing::save(path,record) && homing::load(path).last_abs[0]==123.5 && !std::filesystem::exists(path.wstring()+L".tmp"), "V11 replaces previous record atomically");
    { std::ofstream broken(path); broken<<"{\"leftlimit\":[1,2]}"; }
    check(!homing::load(path).complete, "V11 incomplete record refused");
    const auto config=directory/L"homing_test_config.json";
    {std::ofstream file(config); file<<"{\"homing_reuse_enabled\":false}";}
    check(!homing::enabled(config), "V11 default config false");
    {std::ofstream file(config); file<<"{\"homing_reuse_enabled\":true}";}
    check(homing::enabled(config), "V11 config true");
    std::cout<<"PASS V1/V2/V11 production recovery and homing tests checks="<<checks<<'\n';
}
