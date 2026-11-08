// 抽取生产PLC复用回执分支，验证恢复前的控制资格而非只测决策函数。
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <cstring>
namespace AdsSymbol { constexpr auto app_name="app"; }
enum class StartupPhase { WaitForEnter,Done };
struct Fixture {
    bool homing_restore_submitted=true,ads_self_check_valid=true,homing_reuse_pending=true;
    bool homing_restore_applied=false,last_self_check_done=true,control_before_plc_restart=true;
    bool ads_motion_cycle_valid=false,plc_app_name_read=false;
    struct { bool valid=true; } ads_snapshot;
    std::string plc_application_name;
    std::string recovery_event_text;
    struct { bool completed=false; StartupPhase phase=StartupPhase::WaitForEnter; } startup;
    struct {
        int status=1,reason=0; bool ok=true;
        bool read(const char*,std::size_t,void* value,int) { std::strcpy(static_cast<char*>(value),"RobotPLC");return ok; }
        bool read_sum(const char* const*,const unsigned long*,void* const* values,unsigned long) {
            *static_cast<std::int32_t*>(values[0])=status;
            *static_cast<std::int32_t*>(values[1])=reason;return ok;
        }
    } ads_communication;
    void cycle() {
#include "homing_app_name.inc"
#include "homing_restore_ack.inc"
    }
};
static void check(bool ok,const char* label) { if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);} }
int main() {
    for(bool controlled:{false,true}) {
        Fixture f;f.control_before_plc_restart=controlled;f.cycle();
        check(f.homing_restore_applied&&!f.homing_restore_submitted&&!f.homing_reuse_pending,"V11 PLC applied clears restore transaction");
        check(!f.last_self_check_done,"V11 applied re-arms normal self-check completion/sync path");
        check(f.startup.completed==controlled&&f.startup.phase==(controlled?StartupPhase::Done:StartupPhase::WaitForEnter),
            "V11 only previously controlled session regains automatic-control eligibility");
    }
    Fixture refused;refused.ads_communication.status=2;refused.ads_communication.reason=2;refused.cycle();
    check(!refused.homing_restore_applied&&!refused.startup.completed&&refused.recovery_event_text.find("PLC原因码 2")!=std::string::npos,
        "V11 PLC rejection retains waiting state and displays reason");
    Fixture pending;pending.ads_communication.ok=false;pending.cycle();
    check(pending.homing_restore_submitted&&!pending.homing_restore_applied,"V11 failed reply cannot claim restore applied");
    Fixture waiting;waiting.homing_restore_submitted=false;waiting.cycle();
    check(waiting.plc_app_name_read&&waiting.plc_application_name=="RobotPLC","V11 application name is readable in self-check waiting state before motion-ready");
    Fixture normal;normal.homing_reuse_pending=false;normal.homing_restore_submitted=false;normal.cycle();
    check(!normal.plc_app_name_read,"V11 ordinary startup keeps original application-name read gate");
    std::cout<<"PASS V11 extracted restore acknowledgement checks=10\n";
}
