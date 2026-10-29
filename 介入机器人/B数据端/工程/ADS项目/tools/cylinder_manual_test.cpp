// 抽取真实电缸门控和输出分支；只使用内存替身，不加载 ADS 或设备 DLL。
#include <algorithm>
#include <cstring>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <iostream>
enum class AdsConnectionState { Running, Reconnecting };
enum class CylinderManualMode { Automatic, Open, Closed, Position };
#include "cylinder_type.inc"
#include "cylinder_handles.inc"
#include "cylinder_changed.inc"
namespace AdsSymbol { const char *cylinder1_value="1", *cylinder2_value="2", *cylinder3_value="3", *cylinder4_value="4"; }
static int checks = 0;
static void check(bool ok, const char* text) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << text << '\n'; std::exit(1); }
}
struct Fixture {
    bool ads_motion_cycle_valid=true, has_new_ads_snapshot=true;
    bool connection_hold_active=false, control_active=false, estop_hold_active=false;
    bool return_ads_fault_hold=false, motion_startup_active=false, emergency_retract_active=false;
    bool axis6_soft_limit_hold=false, selfcheck_start_pending=false, y_valve_open=false;
    struct { bool position_valid=true, host_comm_timeout=false, estop_hold_req=false; } ads_snapshot;
    struct { AdsConnectionState state=AdsConnectionState::Running; } ads_stats;
    struct {
        bool host_comm_timeout=false, estop_hold_req=false, axis1_return_busy=false, axis6_return_busy=false;
        int selfcheck_status=0;
    } ads_events;
    struct { bool on=false; bool active() const { return on; } } planned_return, dual_handoff;
    struct { bool on=false; bool is_active() const { return on; } } startup;
    struct { bool on=false, requested=false; bool active() const { return on; } } spacing_recovery;
    struct { unsigned short cyl1_open=450, cyl1_clamp=5, cyl2_open=5, cyl2_clamp=1000,
        cyl3_open=450, cyl3_clamp=50, cyl4_open=5, cyl4_clamp=1000; } cyl;
    CylinderManualMode cylinder_manual_mode[4]={};
    unsigned short cylinder_manual_position[4]={}, cylinder_manual_restore[4]={100,200,300,400};
    std::uint8_t cylinder_manual_written_mask=0;
    struct {
        int reads=0;
        bool ok=true;
        bool read(const char* symbol, unsigned long size, void* value, unsigned long) {
            ++reads; if(!ok) return false;
            const unsigned short original=static_cast<unsigned short>((symbol[0]-'0')*100);
            std::memcpy(value,&original,size); return true;
        }
    } ads_communication;
    AdsOutputCommand output;
    bool allowed=false;
    void tick() {
#include "cylinder_clear.inc"
        unsigned short cylinder1_cmd=450, cylinder2_cmd=1000, cylinder3_cmd=400, cylinder4_cmd=100;
#include "cylinder_gates.inc"
        auto& ads_output=output;
#include "cylinder_output.inc"
        allowed=cylinder_manual_allowed;
    }
    bool open(int index) {
#include "cylinder_prepare.inc"
        if(!prepare_cylinder_manual_override(index, true, 0)) return false;
        cylinder_manual_mode[index]=CylinderManualMode::Position;
        cylinder_manual_position[index]=(index==0||index==2)?2000:10;
        return true;
    }
    std::vector<int> writes() const {
        int fast_write_handles_[kFastWriteHandleCount]={};
        for(int i=0;i<kFastWriteHandleCount;++i) fast_write_handles_[i]=i;
        std::vector<int> result;
        auto append=[&](int handle, unsigned long, const void*) { result.push_back(handle); };
        const auto& output=this->output;
#include "cylinder_write.inc"
        return result;
    }
};
int main() {
    for (int index=0;index<4;++index) {
        Fixture f;
        f.tick();
        check(f.allowed, "four-cylinder manual control works before handle control starts");
        check(!f.output.cylinder_valid, "idle connection never writes default positions to all cylinders");
        f.connection_hold_active=true;
        f.ads_motion_cycle_valid=false;
        f.return_ads_fault_hold=true;
        f.open(index); f.tick();
        const auto bit=static_cast<std::uint8_t>(1u<<index);
        check(f.allowed && f.output.cylinder_valid, "missing handles or a local return-fault latch do not block a stopped cylinder");
        check(f.output.cylinder_write_mask==bit, "independent manual control writes only the selected cylinder");
        check(f.writes()==std::vector<int>{static_cast<int>(kWriteCylinder1)+index}, "real ADS writer does not address other cylinders");
        check(f.output.cylinder[index]==((index==0||index==2)?2000:10), "open sends the existing UI target");
        check(!f.output.cylinder5_press_req && !f.control_active, "manual cylinder control never activates motion or Y-valve output");
        f.cylinder_manual_mode[index]=CylinderManualMode::Automatic;
        f.tick();
        check(f.output.cylinder_write_mask==bit && f.output.cylinder[index]==f.cylinder_manual_restore[index], "restore returns only the selected cylinder to its saved PLC value");
        f.open(index);
        check(f.ads_communication.reads==1, "repeated opening retains the initial PLC value without extra ADS reads");
    }
    for (int gate=0;gate<15;++gate) {
        Fixture f; f.open(0);
        switch(gate) {
        case 0: f.has_new_ads_snapshot=false; break;
        case 1: f.ads_snapshot.position_valid=false; break;
        case 2: f.ads_stats.state=AdsConnectionState::Reconnecting; break;
        case 3: f.ads_snapshot.host_comm_timeout=true; break;
        case 4: f.ads_events.host_comm_timeout=true; break;
        case 5: f.estop_hold_active=true; break;
        case 6: f.ads_events.estop_hold_req=true; break;
        case 7: f.startup.on=true; break;
        case 8: f.ads_events.selfcheck_status=2; break;
        case 9: f.planned_return.on=true; break;
        case 10: f.dual_handoff.on=true; break;
        case 11: f.ads_events.axis1_return_busy=true; break;
        case 12: f.ads_events.axis6_return_busy=true; break;
        case 13: f.spacing_recovery.requested=true; break;
        case 14: f.axis6_soft_limit_hold=true; break;
        }
        f.tick();
        check(!f.allowed && !f.output.cylinder_valid, "PLC unavailable or an automatic cylinder owner still blocks manual output");
        check(f.cylinder_manual_written_mask==0, "rejected manual work is discarded rather than replayed later");
    }
    Fixture normal; normal.control_active=true; normal.open(1); normal.tick();
    check(normal.output.cylinder_valid && normal.output.cylinder_write_mask==0x0f, "normal control keeps the existing four-cylinder automatic output");
    normal.cylinder_manual_mode[1]=CylinderManualMode::Automatic; normal.tick();
    check(normal.output.cylinder[1]==1000, "normal control restores its current automatic target");
    check(normal.writes().size()==4, "normal ADS output still writes all four cylinders");
    Fixture failed; failed.ads_communication.ok=false;
    check(!failed.open(0)&&failed.cylinder_manual_written_mask==0, "failed original-value read does not schedule any cylinder change");
    auto changed=normal.output; changed.cylinder_write_mask=1;
    check(!same_discrete_output(normal.output,changed), "a write-mask change alone triggers a new ADS transaction");
    std::cout << "PASS " << checks << " assertions; independent cylinders, saved values, output masks and PLC/automatic ownership gates\n";
}
