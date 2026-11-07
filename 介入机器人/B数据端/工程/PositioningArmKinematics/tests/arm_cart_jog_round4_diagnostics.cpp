#include "arm_cart_jog_reference.h"
#include "arm_cart_jog_st_generated.h"
#include "positioning_arm/kinematics.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>

using Gen=ArmCartJogReference;
using Axes=std::array<double,5>;
constexpr double DT=.001, SING_MARGIN=5.;
int failures=0;
std::uint64_t compared_cycles=0;
double difference=0;
bool equivalent=true;
double diagnostic_path_ratio=0,diagnostic_velocity_difference=0,diagnostic_acceleration_difference=0;
bool diagnostic_path_ok=true;

void result(bool ok,const char* label){std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';if(!ok)++failures;}
double alpha(const Axes& p){double value=p[2]+180.;while(value>180.)value-=360.;while(value<=-180.)value+=360.;return value;}
double delta(const Axes& p){const double e=std::abs(alpha(p));return std::min(e-SING_MARGIN,180.-SING_MARGIN-e);}
std::array<double,2> wrist(const Axes& p){return {{Gen::L1*std::sin(p[1]*Gen::RAD)+Gen::L2*std::sin((p[1]+p[2])*Gen::RAD+Gen::PI),
    -Gen::L1*std::cos(p[1]*Gen::RAD)-Gen::L2*std::cos((p[1]+p[2])*Gen::RAD+Gen::PI)}};}
positioning_arm::JointVector joints(const Axes& p){positioning_arm::JointVector q;for(int i=0;i<5;++i)q[i]=p[i]*(i==0?.001:Gen::RAD);return q;}
positioning_arm::ArmKinematicConfig config(){positioning_arm::ArmKinematicConfig c;
    c.lower.setConstant(-std::numeric_limits<double>::infinity());c.upper.setConstant(std::numeric_limits<double>::infinity());
    c.lower[4]=-60*Gen::RAD;c.upper[4]=60*Gen::RAD;
    for(int i=0;i<5;++i){c.calibration[i]={0,i==0?.001:Gen::RAD,1};c.max_velocity[i]=Gen::JOINT_VCAP[i]*(i==0?.001:Gen::RAD);}return c;}

// 本文件只诊断未改动的第三轮发生器；不把候选奇异规则接入运动模型。
struct Run {
    Gen g;StCartJog st;Axes initial;
    positioning_arm::PositioningArmKinematics truth{config()};
    Eigen::Vector3d tcp_initial;
    double min_delta=std::numeric_limits<double>::infinity(),max_acceleration=0,max_speed=0,line_error=0;
    double radius_min=std::numeric_limits<double>::infinity(),radius_max=0,peak_previous_v=0,peak_v=0;
    bool finite=true,cap=true,branch_same=true;
    int cycle=0,peak_cycle=0,peak_axis=0,first_stop=0,initial_branch=0;
    std::array<double,2> nominal_bounds{};
    double maximum_path_ratio=0;int path_peak_cycle=0;
    explicit Run(Axes p,int mode,int dir,double speed):initial(p){
        g.step(true,false,mode,dir,speed,p,DT);
        const std::array<double,6> seed{{0,p[0],p[1],p[2],p[3],p[4]}};
        st.step(true,false,mode,dir,speed,seed,DT);tcp_initial=truth.forward(joints(p)).position;
        initial_branch=g.branch;min_delta=delta(p);
    }
    void tick(int mode,int dir,double speed){
        const Axes before=g.p,previous_v=g.v;const double previous_path_speed=g.path_speed;
        const std::array<double,6> seed{{0,before[0],before[1],before[2],before[3],before[4]}};
        g.step(false,true,mode,dir,speed,g.p,DT);st.step(false,true,mode,dir,speed,seed,DT);++cycle;++compared_cycles;
        const double bound=1.5*std::abs(g.v1-g.v0)/g.duration;
        const double maximum_bound=std::max({nominal_bounds[0],nominal_bounds[1],bound});
        nominal_bounds={{nominal_bounds[1],bound}};
        if(!g.limited){const double acceleration=std::abs(g.path_speed-previous_path_speed)/DT;
            const double ratio=maximum_bound>0?acceleration/maximum_bound:acceleration==0?0:std::numeric_limits<double>::infinity();
            diagnostic_path_ok=diagnostic_path_ok&&acceleration<=1.01*maximum_bound;
            diagnostic_path_ratio=std::max(diagnostic_path_ratio,ratio);
            if(ratio>maximum_path_ratio){maximum_path_ratio=ratio;path_peak_cycle=cycle;}}

        for(int i=0;i<5;++i){
            diagnostic_velocity_difference=std::max(diagnostic_velocity_difference,std::abs(g.v[i]-(g.p[i]-before[i])/DT));
            diagnostic_acceleration_difference=std::max(diagnostic_acceleration_difference,std::abs(g.a[i]-(g.v[i]-previous_v[i])/DT));
            finite=finite&&std::isfinite(g.p[i])&&std::isfinite(g.v[i])&&std::isfinite(g.a[i]);
            cap=cap&&std::abs(g.p[i]-before[i])/DT<=Gen::JOINT_VCAP[i]*(1+1e-9);
            difference=std::max({difference,std::abs(g.p[i]-st.p[i+1]),std::abs(g.v[i]-st.v[i+1]),std::abs(g.a[i]-st.a[i+1])});
        }
        difference=std::max({difference,std::abs(g.path_speed-st.path_speed),std::abs(g.path_v-st.path_v),std::abs(g.sigma-st.sigma)});
        equivalent=equivalent&&g.limited==st.limited&&g.branch==st.branch&&g.latched_mode==st.latched_mode&&g.latched_dir==st.latched_dir;
        finite=finite&&std::isfinite(g.path_speed);branch_same=branch_same&&g.branch==initial_branch;
        for(int i=1;i<=3;++i){max_speed=std::max(max_speed,std::abs(g.v[i]));if(std::abs(g.a[i])>max_acceleration){
            max_acceleration=std::abs(g.a[i]);peak_cycle=cycle;peak_axis=i+1;peak_previous_v=previous_v[i];peak_v=g.v[i];}}
        min_delta=std::min(min_delta,delta(g.p));const auto w=wrist(g.p);const double r=std::hypot(w[0],w[1]);
        radius_min=std::min(radius_min,r);radius_max=std::max(radius_max,r);
        const auto tcp=truth.forward(joints(g.p)).position;
        line_error=std::max(line_error,std::abs(tcp[mode==1?1:0]-tcp_initial[mode==1?1:0])*1000.);
        if(g.path_speed==0&&g.limited&&first_stop==0)first_stop=cycle;
    }
    double displacement(){return (truth.forward(joints(g.p)).position-tcp_initial).norm()*1000.;}
    void print(const char* id,int mode,int dir,double speed){
        std::cout<<id<<" mode="<<mode<<" dir="<<dir<<" speed="<<speed<<" cycles="<<cycle<<" displacement_mm="<<displacement()
            <<" final_alpha_deg="<<alpha(g.p)<<" min_delta_deg="<<min_delta<<" final_path_speed="<<g.path_speed<<" limited="<<g.limited
            <<" line_mm="<<line_error<<" finite="<<finite<<" joint_cap="<<cap<<" max_joint_speed="<<max_speed
            <<" max_joint_acceleration="<<max_acceleration<<" peak_cycle="<<peak_cycle<<" peak_axis="<<peak_axis
            <<" peak_previous_v="<<peak_previous_v<<" peak_v="<<peak_v<<" first_stop_cycle="<<first_stop
            <<" radius_min="<<radius_min<<" radius_max="<<radius_max<<" T11_ratio="<<maximum_path_ratio<<" T11_peak_cycle="<<path_peak_cycle<<'\n';
    }
};

int main(){
    std::cout<<std::setprecision(17);
    const Axes probe{{100,-31.568598207665932,-4,0,0}};
    const auto w=wrist(probe);std::cout<<"B_NONMONOTONE q2="<<probe[1]<<" q3="<<probe[2]<<" wx="<<w[0]<<" wy="<<w[1]
        <<" alpha_now="<<alpha(probe)<<" delta_now="<<delta(probe)<<'\n';
    const auto candidate=[&](double step){
        Axes q=probe;const double x=w[0]+step,y=w[1];const double r=std::hypot(x,y);
        const double cosine=std::clamp((r*r-Gen::L1*Gen::L1-Gen::L2*Gen::L2)/(2.*Gen::L1*Gen::L2),-1.,1.);
        const double elbow=std::acos(cosine);
        q[1]=(std::atan2(x,-y)-std::atan(Gen::L2*std::sin(elbow)/(Gen::L1+Gen::L2*std::cos(elbow))))/Gen::RAD;
        q[2]=(elbow-Gen::PI)/Gen::RAD;q[3]=probe[3]+(probe[1]+probe[2])-(q[1]+q[2]);return q;
    };
    const auto old_accept=[&](const Axes& q){const auto xy=wrist(q);const double r=std::hypot(xy[0],xy[1]);
        bool ok=r>=Gen::L1-Gen::L2&&r<=Gen::L1+Gen::L2;
        for(int i=1;i<=3;++i)ok=ok&&std::abs(q[i]-probe[i])<=Gen::JOINT_VCAP[i]*DT;return ok;};
    const auto proposed_accept=[&](double step){const auto q=candidate(step);return old_accept(q)&&delta(q)>=delta(probe);};
    for(double step:{.001,.0025,.004,.005}){
        const auto q=candidate(step);
        std::cout<<"B_TRIAL step_mm="<<step<<" alpha_try="<<alpha(q)<<" delta_try="<<delta(q)<<" old_conditions="<<old_accept(q)
            <<" proposed_accept="<<proposed_accept(step)<<" dq2="<<q[1]-probe[1]<<" dq3="<<q[2]-probe[2]<<" dq4="<<q[3]-probe[3]<<'\n';
    }
    double lower=0,upper=.005;
    for(int n=0;n<Gen::BISECTION_COUNT;++n){const double middle=(lower+upper)/2.;if(proposed_accept(middle))lower=middle;else upper=middle;}
    std::cout<<"B_BISECTION desired_mm=0.005 accepted_mm="<<lower<<" known_acceptable_mm=0.004\n";
    result(!proposed_accept(.0025)&&proposed_accept(.004)&&!proposed_accept(.005)&&lower==0,
        "B nonmonotone witness confirmed; 12 bisections miss an acceptable positive step; implementation stopped by user rule");
    bool s1=true,s2=true,s3=true,s4=true;
    for(int mode:{1,2})for(int dir:{1,-1})for(double speed:{5.,30.}){
        Run r({{100,-9.9812,167.5076,-70.5273,8.0475}},mode,dir,speed);
        for(int n=0;n<5000;++n)r.tick(mode,dir,speed);
        r.print("S1",mode,dir,speed);
        const bool ok=r.finite&&r.cap&&r.line_error<=.01&&r.min_delta>=0&&r.max_acceleration<=200;
        s1=s1&&ok;s4=s4&&r.max_acceleration<=200;
    }
    result(s1,"S1 current third-round generator; B not implemented");
    const double e=10.*Gen::RAD;
    const double q2=90.-std::atan(Gen::L2*std::sin(e)/(Gen::L1+Gen::L2*std::cos(e)))/Gen::RAD;
    for(double speed:{5.,30.}){
        Run r({{100,q2,-170,37,12}},1,1,speed);
        for(int n=0;n<5000;++n)r.tick(1,1,speed);
        r.print("S2_OUT",1,1,speed);const double stop_alpha=std::abs(alpha(r.g.p));
        for(int n=0;n<3000;++n)r.tick(1,-1,speed);
        r.print("S2_REVERSE",1,-1,speed);
        s2=s2&&r.finite&&r.cap&&stop_alpha>=SING_MARGIN&&r.max_acceleration<=200&&r.branch_same&&std::abs(alpha(r.g.p))>stop_alpha;
        s4=s4&&r.max_acceleration<=200;
    }
    result(s2,"S2 current third-round generator; B not implemented");
    for(int mode:{1,2})for(int dir:{1,-1}){
        Run r({{100,0,0,0,0}},mode,dir,5);for(int n=0;n<2000;++n){r.tick(mode,dir,5);
            if(n==0)std::cout<<"S3_START mode="<<mode<<" dir="<<dir<<" delta_now=-5 delta_try="<<delta(r.g.p)
                <<" limited="<<r.g.limited<<" a2="<<r.g.a[1]<<" a3="<<r.g.a[2]<<" a4="<<r.g.a[3]<<'\n';}
        r.print("S3",mode,dir,5);
        const bool inward=mode==2&&dir==1;
        s3=s3&&r.finite&&r.cap&&(inward?r.displacement()==0&&r.g.limited:r.displacement()>0);
        s4=s4&&r.max_acceleration<=200;
    }
    result(s3,"S3 zero-position direction behavior only; acceleration is tested separately by S4");
    result(s4,"S4 includes every S1/S2/S3 cycle, including limited and start/reversal cycles");
    std::cout<<"ST_EQ new_diagnostic_cycles="<<compared_cycles<<" max_difference="<<difference<<" discrete_equal="<<equivalent<<'\n';
    result(difference==0&&equivalent,"ST/C++ diagnostic cycle equivalence");
    std::cout<<"T11_DIAGNOSTICS maximum_ratio="<<diagnostic_path_ratio<<" velocity_difference="<<diagnostic_velocity_difference
        <<" acceleration_difference="<<diagnostic_acceleration_difference<<'\n';
    result(diagnostic_path_ok&&diagnostic_velocity_difference<=1e-9&&diagnostic_acceleration_difference<=1e-9,"T11 diagnostics with the unchanged second-round rule");
    std::cout<<"TOTAL failures="<<failures<<"; B stopped, diagnostic failures are not waived\n";return failures?1:0;
}
