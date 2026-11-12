#pragma once
#include "control_messages.h"
struct ZenohServiceConfig { double startup_g_mm=13,axis6_soft_limit_from_left_mm=660; };
struct RemoteExtraState {
    double axis1_net_distance_mm=0,axis6_net_distance_mm=0;
    std::int8_t injector_dir[2]={},axis4_dir=0;
    bool y_valve_open=false;
    std::uint32_t force_zero_ok_count=0,force_zero_fail_count=0;
    bool initial_sync_done=false,handle_soft_hold=false,ads_soft_hold=false,connection_hold=false,return_ads_fault_hold=false;
};
class ZenohService {
public:
    bool start(const ZenohServiceConfig&) { return true; }
    bool is_running() const { return true; }
    void stop() {}
    void publish_state(const VisState&,const RemoteExtraState&) {}
    bool poll_command(VisCommand&);
    void command_processed() {}
};
