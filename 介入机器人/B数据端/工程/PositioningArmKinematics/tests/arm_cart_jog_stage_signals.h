#pragma once
#include "arm_cart_jog_reference.h"
#include "arm_cart_jog_st_generated.h"
#include <cstdint>
#include <type_traits>

struct AxisSignal { struct { bool Error = false; } Status; int index = 0; };
struct FbFrame {
    bool Done = false, Error = false, Busy = false, Enabled = false;
    std::uint32_t ErrorID = 0;
};
struct ScriptedFb : FbFrame {
    FbFrame script;
    bool execute = false;
    double position = 0, deceleration = 0, jerk = 0;
    void call(AxisSignal&, bool request, double p = 0, double option = 0) {
        static_cast<FbFrame&>(*this) = script;
        execute = request; position = p; deceleration = p; jerk = option;
    }
};
struct JogGlobals {
    int arm_cjog_mode = 0, arm_cjog_dir = 0;
    double arm_cjog_speed = 0, arm_cjog_path_speed = 0;
    std::uint32_t arm_cjog_alive = 0, arm_cjog_state = 0, arm_cjog_error = 0;
    std::uint32_t arm_curve_request = 0, arm_curve_ack = 0;
    bool arm_cjog_owned = false, arm_cjog_limited = false, arm_curve_owned = false;
    bool arm_manual_enable = true, host_comm_timeout = false, estop_hold_req = false;
    std::array<AxisSignal,6> arm_axis{};
    std::array<FbFrame,6> arm_power_output{};
    std::array<bool,6> arm_enable_req{}, arm_reset_req{}, arm_jog_pos_req{}, arm_jog_neg_req{};
    std::array<double,6> arm_act_pos{}, arm_jog_dec{}, arm_jog_jerk{};
};
template<class Model> struct JogGenAdapter {
    Model core;
    std::array<double,6> p{},v{},a{};
    double path_speed = 0, target_speed = 0;
    bool limited = false;
    void step(bool start, bool held, int mode, int dir, double speed,
        const std::array<double,6>& seed, double dt) {
        if constexpr (std::is_same_v<Model,ArmCartJogReference>) {
            const std::array<double,5> q{{seed[1],seed[2],seed[3],seed[4],seed[5]}};
            core.step(start,held,mode,dir,speed,q,dt);
            for(int i=1;i<=5;++i){p[i]=core.p[i-1];v[i]=core.v[i-1];a[i]=core.a[i-1];}
        } else {
            core.step(start,held,mode,dir,speed,seed,dt);
            p=core.p;v=core.v;a=core.a;
        }
        path_speed=core.path_speed;target_speed=core.target_speed;limited=core.limited;
    }
};
template<class Model> struct JogStageSignals {
    JogGlobals G;
    bool first_cycle = false;
    std::uint32_t cycle_time_100ns = 10000;
    JogGenAdapter<Model> gen;
    std::array<ScriptedFb,6> fb_enable{},fb_disable{},fb_stop{};
    std::array<std::array<double,4>,6> last_feed{};
    std::array<int,6> feed_count{};
    void feed(AxisSignal& axis,double p,double v,double a,int dir) {
        last_feed[axis.index]={{p,v,a,static_cast<double>(dir)}};
        ++feed_count[axis.index];
    }
    JogStageSignals() {
        G.arm_act_pos={{0,100,-40,-90,37,12}};
        for(int i=1;i<=5;++i){G.arm_axis[i].index=i;G.arm_power_output[i].Done=true;
            G.arm_enable_req[i]=true;G.arm_jog_dec[i]=50;G.arm_jog_jerk[i]=500;}
    }
};
