#include "arm_cart_jog_reference.h"
#include "arm_cart_jog_st_generated.h"
#include <iostream>
#include <iomanip>

int main()
{
    static_assert(StCartJog::T_ACC == ArmCartJogReference::T_ACC);
    static_assert(StCartJog::T_DEC == ArmCartJogReference::T_DEC);
    static_assert(StCartJog::INITIAL_BRANCH == ArmCartJogReference::INITIAL_BRANCH);
    static_assert(StCartJog::BISECTION_COUNT == ArmCartJogReference::BISECTION_COUNT);
    static_assert(StCartJog::L1 == ArmCartJogReference::L1);
    static_assert(StCartJog::L2 == ArmCartJogReference::L2);
    static_assert(StCartJog::PI == ArmCartJogReference::PI);
    static_assert(StCartJog::RAD == ArmCartJogReference::RAD);
    for (int i=0;i<5;++i) if(StCartJog::JOINT_VCAP[i+1]!=ArmCartJogReference::JOINT_VCAP[i]) return 1;
    double difference=0;
    int cycles=0;
    for (int branch : {-2,-1,0,1,2}) for (int first_mode=1;first_mode<=5;++first_mode) for(int initial_dir:{-1,1}) {
        ArmCartJogReference ref;
        StCartJog st;
        const std::array<double,5> seed = branch==0 ? std::array<double,5>{{100,0,0,0,0}}
            : std::array<double,5>{{100,37,branch*90.0,720,12}};
        const std::array<double,6> st_seed{{0,seed[0],seed[1],seed[2],seed[3],seed[4]}};
        ref.step(true,false,first_mode,initial_dir,5,seed,.001);
        st.step(true,false,first_mode,initial_dir,5,st_seed,.001);
        for(int n=0;n<12000;++n) {
            const bool held=n<11000 && !(n>=7000 && n<7200);
            const int mode=n<5000?first_mode:first_mode%5+1;
            const int dir=n<8500?initial_dir:-initial_dir;
            const double speed=n<2000?5:n<4000?15:n<6000?30:5;
            ref.step(false,held,mode,dir,speed,seed,.001);
            st.step(false,held,mode,dir,speed,st_seed,.001);
            for(int i=0;i<5;++i) {
                if (!std::isfinite(ref.p[i]) || !std::isfinite(st.p[i+1])
                    || !std::isfinite(ref.v[i]) || !std::isfinite(st.v[i+1])
                    || !std::isfinite(ref.a[i]) || !std::isfinite(st.a[i+1])) return 1;
                difference=std::max(difference,std::abs(ref.p[i]-st.p[i+1]));
                difference=std::max(difference,std::abs(ref.v[i]-st.v[i+1]));
                difference=std::max(difference,std::abs(ref.a[i]-st.a[i+1]));
            }
            for(const auto& pair : {std::pair<double,double>{ref.path_v,st.path_v},
                {ref.v0,st.v0},{ref.v1,st.v1},{ref.tau,st.tau},{ref.duration,st.duration},
                {ref.sigma,st.sigma},{ref.wx,st.wx},{ref.wy,st.wy}})
                difference=std::max(difference,std::abs(pair.first-pair.second));
            if (!std::isfinite(ref.path_speed) || !std::isfinite(st.path_speed)
                || !std::isfinite(ref.target_speed) || !std::isfinite(st.target_speed)) return 1;
            difference=std::max(difference,std::abs(ref.path_speed-st.path_speed));
            difference=std::max(difference,std::abs(ref.target_speed-st.target_speed));
            if(ref.limited!=st.limited
                || ref.branch!=st.branch || ref.latched_mode!=st.latched_mode || ref.latched_dir!=st.latched_dir) return 1;
            ++cycles;
        }
    }
    std::cout<<std::setprecision(17)<<(difference==0?"PASS":"FAIL")
        <<" ST translated statements versus handwritten reference, cycles="<<cycles
        <<", max_position_velocity_acceleration_path_difference="<<difference<<'\n';
    return difference==0?0:1;
}
