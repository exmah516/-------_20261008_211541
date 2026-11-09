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
    static_assert(StCartJog::TOOL_LATERAL_OFFSET_MM == ArmCartJogReference::TOOL_LATERAL_OFFSET_MM);
    static_assert(StCartJog::TOOL_LENGTH_MM == ArmCartJogReference::TOOL_LENGTH_MM);
    static_assert(StCartJog::PI == ArmCartJogReference::PI);
    static_assert(StCartJog::RAD == ArmCartJogReference::RAD);
    for (int i=0;i<5;++i) if(StCartJog::JOINT_VCAP[i+1]!=ArmCartJogReference::JOINT_VCAP[i]) return 1;
    double difference=0;
    int cycles=0;
    auto compare_step = [&](ArmCartJogReference& ref, StCartJog& st, const std::array<double,5>& seed,
        bool held, int mode, int dir, double speed) {
        const std::array<double,6> st_seed{{0,seed[0],seed[1],seed[2],seed[3],seed[4]}};
        ref.step(false,held,mode,dir,speed,seed,.001);
        st.step(false,held,mode,dir,speed,st_seed,.001);
        for(int i=0;i<5;++i) {
            if (!std::isfinite(ref.p[i]) || !std::isfinite(st.p[i+1])
                || !std::isfinite(ref.v[i]) || !std::isfinite(st.v[i+1])
                || !std::isfinite(ref.a[i]) || !std::isfinite(st.a[i+1])) return false;
            difference=std::max(difference,std::abs(ref.p[i]-st.p[i+1]));
            difference=std::max(difference,std::abs(ref.v[i]-st.v[i+1]));
            difference=std::max(difference,std::abs(ref.a[i]-st.a[i+1]));
        }
        for(const auto& pair : {std::pair<double,double>{ref.path_v,st.path_v},
            {ref.v0,st.v0},{ref.v1,st.v1},{ref.tau,st.tau},{ref.duration,st.duration},
            {ref.sigma,st.sigma},{ref.wx,st.wx},{ref.wy,st.wy},{ref.psi0,st.psi0},
            {ref.d0x,st.d0x},{ref.d0y,st.d0y},{ref.tcp0x,st.tcp0x},{ref.tcp0y,st.tcp0y},
            {ref.candidate_psi,st.candidate_psi}})
            difference=std::max(difference,std::abs(pair.first-pair.second));
        if (ref.latched_mode == 5) for(const auto& pair : {
            std::pair<double,double>{ref.candidate_radius,st.candidate_radius},
            {ref.candidate_cosine,st.candidate_cosine},{ref.candidate_elbow,st.candidate_elbow},
            {ref.candidate_q2,st.candidate_q2},{ref.candidate_q3,st.candidate_q3},
            {ref.candidate_q4,st.candidate_q4}})
            difference=std::max(difference,std::abs(pair.first-pair.second));
        if (!std::isfinite(ref.path_speed) || !std::isfinite(st.path_speed)
            || !std::isfinite(ref.target_speed) || !std::isfinite(st.target_speed)) return false;
        difference=std::max(difference,std::abs(ref.path_speed-st.path_speed));
        difference=std::max(difference,std::abs(ref.target_speed-st.target_speed));
        if(ref.limited!=st.limited || ref.branch!=st.branch
            || ref.latched_mode!=st.latched_mode || ref.latched_dir!=st.latched_dir) return false;
        ++cycles;
        return true;
    };
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
            if(!compare_step(ref,st,seed,held,mode,dir,speed)) return 1;
        }
    }
    const auto yaw_seed = [](double elbow, double q5, bool outward_boundary = false) {
        const double e=elbow*ArmCartJogReference::RAD;
        const double q2=-std::atan(ArmCartJogReference::L2*std::sin(e)
            /(ArmCartJogReference::L1+ArmCartJogReference::L2*std::cos(e)))/ArmCartJogReference::RAD;
        const double q3=elbow-180.0;
        const double wx=ArmCartJogReference::L1*std::sin(q2*ArmCartJogReference::RAD)
            +ArmCartJogReference::L2*std::sin((q2+q3)*ArmCartJogReference::RAD+ArmCartJogReference::PI);
        const double wy=-ArmCartJogReference::L1*std::cos(q2*ArmCartJogReference::RAD)
            -ArmCartJogReference::L2*std::cos((q2+q3)*ArmCartJogReference::RAD+ArmCartJogReference::PI);
        const double wrist_angle=std::atan2(wy,wx)/ArmCartJogReference::RAD;
        const double d_y=-ArmCartJogReference::TOOL_LENGTH_MM*std::cos(q5*ArmCartJogReference::RAD);
        const double d_angle=std::atan2(d_y,ArmCartJogReference::TOOL_LATERAL_OFFSET_MM)/ArmCartJogReference::RAD;
        const double psi=wrist_angle+(outward_boundary?90.0:0.0)-d_angle;
        const double q4=psi-q2-q3+90.0;
        return std::array<double,5>{{100,q2,q3,q4,q5}};
    };
    const auto yaw_case = [&](const std::array<double,5>& seed, int dir, double speed,
        int held_cycles, int release_cycles) {
        ArmCartJogReference ref; StCartJog st;
        const std::array<double,6> st_seed{{0,seed[0],seed[1],seed[2],seed[3],seed[4]}};
        ref.step(true,false,5,dir,speed,seed,.001);
        st.step(true,false,5,dir,speed,st_seed,.001);
        for(int n=0;n<held_cycles;++n) if(!compare_step(ref,st,seed,true,5,dir,speed)) return false;
        for(int n=0;n<release_cycles;++n) if(!compare_step(ref,st,seed,false,0,0,0)) return false;
        return true;
    };
    for(double elbow:{30.,90.,150.}) for(double q5:{0.,30.,-30.}) for(int dir:{-1,1})
        if(!yaw_case(yaw_seed(elbow,q5),dir,.8,3000,500)) return 1; // Y1
    for(double elbow:{30.,90.,150.}) for(double q5:{0.,30.,-30.}) for(double speed:{2.4,1.6,.8})
        if(!yaw_case(yaw_seed(elbow,q5),1,speed,3000,0)) return 1; // Y3
    {
        auto p=yaw_seed(10,0,true);ArmCartJogReference ref;StCartJog st;
        const std::array<double,6> st_seed{{0,p[0],p[1],p[2],p[3],p[4]}};
        ref.step(true,false,5,1,.8,p,.001);st.step(true,false,5,1,.8,st_seed,.001);
        int limit=0;
        for(int n=0;n<6000&&limit==0;++n){if(!compare_step(ref,st,p,true,5,1,.8))return 1;
            if(ref.path_speed<.001&&ref.limited)limit=n+1;}
        if(limit==0)return 1;
        for(int n=0;n<3000;++n)if(!compare_step(ref,st,p,true,5,-1,.8))return 1;
    } // Y4 boundary stop and reversal
    if(!yaw_case(std::array<double,5>{{100,0,0,0,0}},1,.8,2000,0)
        || !yaw_case(std::array<double,5>{{100,0,0,0,0}},-1,.8,2000,0)) return 1; // Y5
    {
        const auto seed= yaw_seed(90,0);
        ArmCartJogReference ref; StCartJog st;
        const std::array<double,6> st_seed{{0,seed[0],seed[1],seed[2],seed[3],seed[4]}};
        ref.step(true,false,1,1,5,seed,.001); st.step(true,false,1,1,5,st_seed,.001);
        for(int n=0;n<2000;++n) if(!compare_step(ref,st,seed,true,1,1,5)) return 1;
        for(int n=0;n<3500;++n) if(!compare_step(ref,st,seed,true,5,1,.8)) return 1;
        for(int n=0;n<3500;++n) if(!compare_step(ref,st,seed,true,2,1,5)) return 1;
    } // Y6
    std::cout<<std::setprecision(17)<<(difference==0?"PASS":"FAIL")
        <<" ST translated statements versus handwritten reference, cycles="<<cycles
        <<", max_position_velocity_acceleration_path_difference="<<difference<<'\n';
    return difference==0?0:1;
}
