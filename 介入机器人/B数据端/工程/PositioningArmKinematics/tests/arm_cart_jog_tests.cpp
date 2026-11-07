#include "arm_cart_jog_reference.h"
#include "positioning_arm/kinematics.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <string>

using Gen = ArmCartJogReference;
using Axes = std::array<double, 5>;
constexpr double DT = 0.001;
int failures = 0;
double velocity_difference = 0, acceleration_difference = 0, maximum_path_acceleration_ratio = 0;
bool path_acceleration_ok = true;
std::array<double, 2> nominal_bounds{};
std::array<double, 3> peak_bounds{};
std::string peak_case;
double peak_v0=0,peak_v1=0,peak_duration=0,peak_previous_path_speed=0,peak_path_speed=0;
double peak_previous_nominal=0,peak_nominal=0,peak_tau=0;
int peak_case_cycle=0;
std::map<std::string,int> cycle_count;
std::map<std::string,double> acceleration_ratio_by_case;
bool cycles_finite = true;

positioning_arm::ArmKinematicConfig config()
{
    positioning_arm::ArmKinematicConfig c;
    c.lower.setConstant(-std::numeric_limits<double>::infinity());
    c.upper.setConstant(std::numeric_limits<double>::infinity());
    c.lower[4] = -60*Gen::RAD; c.upper[4] = 60*Gen::RAD;
    for (int i = 0; i < 5; ++i) {
        c.calibration[i] = {0, i == 0 ? 0.001 : Gen::RAD, 1};
        c.max_velocity[i] = Gen::JOINT_VCAP[i]*(i == 0 ? 0.001 : Gen::RAD);
    }
    return c;
}
positioning_arm::JointVector joints(const Axes& p)
{
    positioning_arm::JointVector q;
    for (int i = 0; i < 5; ++i) q[i] = p[i]*(i == 0 ? 0.001 : Gen::RAD);
    return q;
}
Axes seed(int mode, double effective_elbow)
{
    const double e = effective_elbow*Gen::RAD;
    const double q2 = -std::atan(Gen::L2*std::sin(e)/(Gen::L1+Gen::L2*std::cos(e)))/Gen::RAD;
    return {{100, q2+(mode == 2 ? 90.0 : 0.0), effective_elbow-180, 37, 12}};
}
std::array<double,2> wrist_xy(const Axes& p)
{
    return {{Gen::L1*std::sin(p[1]*Gen::RAD)+Gen::L2*std::sin((p[1]+p[2])*Gen::RAD+Gen::PI),
        -Gen::L1*std::cos(p[1]*Gen::RAD)-Gen::L2*std::cos((p[1]+p[2])*Gen::RAD+Gen::PI)}};
}
double psi_deg(const Axes& p) { return p[1]+p[2]+p[3]-90.0; }
Axes yaw_seed(double effective_elbow, double q5, bool outward_boundary = false)
{
    Axes p=seed(1,effective_elbow);
    p[4]=q5;
    const auto wrist=wrist_xy(p);
    const double wrist_angle=std::atan2(wrist[1],wrist[0])/Gen::RAD;
    const double d_y=-Gen::TOOL_LENGTH_MM*std::cos(q5*Gen::RAD);
    const double d_angle=std::atan2(d_y,Gen::TOOL_LATERAL_OFFSET_MM)/Gen::RAD;
    const double psi=wrist_angle+(outward_boundary?90.0:0.0)-d_angle;
    p[3]=psi-p[1]-p[2]+90.0;
    return p;
}
bool yaw_candidate(const Gen& g, const Axes& current, double sigma_base, int dir, double step, Axes& q)
{
    const double psi=g.psi0+dir*(sigma_base+step);
    const double wx=g.tcp0x-Gen::TOOL_LATERAL_OFFSET_MM*std::cos(psi*Gen::RAD)
        +g.d0y*std::sin(psi*Gen::RAD);
    const double wy=g.tcp0y-Gen::TOOL_LATERAL_OFFSET_MM*std::sin(psi*Gen::RAD)
        -g.d0y*std::cos(psi*Gen::RAD);
    const double radius=std::hypot(wx,wy);
    if(radius<Gen::L1-Gen::L2 || radius>Gen::L1+Gen::L2) return false;
    const double cosine=std::clamp((radius*radius-Gen::L1*Gen::L1-Gen::L2*Gen::L2)/(2*Gen::L1*Gen::L2),-1.0,1.0);
    const double elbow=g.branch*std::acos(cosine);
    const double azimuth=std::atan2(wx,-wy);
    double q2=(azimuth-std::atan(Gen::L2*std::sin(elbow)/(Gen::L1+Gen::L2*std::cos(elbow))))/Gen::RAD;
    double q3=(elbow-Gen::PI)/Gen::RAD;
    while(q2-current[1]>180.0) q2-=360.0;
    while(q2-current[1]<-180.0) q2+=360.0;
    while(q3-current[2]>180.0) q3-=360.0;
    while(q3-current[2]<-180.0) q3+=360.0;
    const double q4=g.anchor[3]+(psi-g.psi0)-((q2+q3)-(g.anchor[1]+g.anchor[2]));
    q=current;
    q[1]=q2; q[2]=q3; q[3]=q4;
    return true;
}
double curve_integral(const Gen& g,double tau_begin,double tau_end)
{
    const auto primitive=[&](double t){
        const double u=std::min(t,g.duration)/g.duration;
        const double transition=g.v0*std::min(t,g.duration)
            +(g.v1-g.v0)*g.duration*(u*u*u-.5*u*u*u*u);
        return transition+g.v1*std::max(0.0,t-g.duration);
    };
    return primitive(tau_end)-primitive(tau_begin);
}
Gen begin(int mode, int dir, double speed, Axes p)
{
    nominal_bounds = {{0,0}};
    Gen g; g.step(true, false, mode, dir, speed, p, DT); return g;
}
void tick(Gen& g, bool held, int mode, int dir, double speed, const char* name)
{
    const auto prev_p = g.p, prev_v = g.v;
    const double previous_path_speed = g.path_speed, previous_nominal = g.path_v;
    const int cycle=++cycle_count[name];
    g.step(false, held, mode, dir, speed, g.p, DT);
    for (int i = 0; i < 5; ++i)
        cycles_finite = cycles_finite && std::isfinite(g.p[i]) && std::isfinite(g.v[i]) && std::isfinite(g.a[i]);
    cycles_finite = cycles_finite && std::isfinite(g.path_speed);
    for (int i = 0; i < 5; ++i) {
        velocity_difference = std::max(velocity_difference, std::abs(g.v[i]-(g.p[i]-prev_p[i])/DT));
        acceleration_difference = std::max(acceleration_difference, std::abs(g.a[i]-(g.v[i]-prev_v[i])/DT));
    }
    // 路径速度差分跨三个名义速度样本，使用三者所属过渡的最大上限。
    const double current_bound = 1.5*std::abs(g.v1-g.v0)/g.duration;
    const std::array<double, 3> bounds{{nominal_bounds[0],nominal_bounds[1],current_bound}};
    nominal_bounds = {{nominal_bounds[1],current_bound}};
    if (!g.limited) {
        const double bound = std::max({bounds[0],bounds[1],bounds[2]});
        const double acceleration = std::abs((g.path_speed-previous_path_speed)/DT);
        path_acceleration_ok = path_acceleration_ok && acceleration <= bound*1.01;
        const double ratio = bound > 0 ? acceleration/bound : acceleration == 0 ? 0 : std::numeric_limits<double>::infinity();
        acceleration_ratio_by_case[name] = std::max(acceleration_ratio_by_case[name], ratio);
        if (std::strcmp(name,"T7")==0 && cycle==2201)
            std::cout<<"T11 T7 boundary_cycle="<<cycle<<" B_k_minus_2="<<bounds[0]<<" B_k_minus_1="<<bounds[1]
                <<" B_k="<<bounds[2]<<" max_B="<<bound<<" previous_path_speed="<<previous_path_speed
                <<" path_speed="<<g.path_speed<<" acceleration="<<acceleration
                <<" allowed="<<1.01*bound<<" ratio="<<ratio<<'\n';
        if(ratio>maximum_path_acceleration_ratio){
            maximum_path_acceleration_ratio=ratio;peak_case=name;peak_case_cycle=cycle;peak_bounds=bounds;
            peak_v0=g.v0;peak_v1=g.v1;peak_duration=g.duration;
            peak_previous_path_speed=previous_path_speed;peak_path_speed=g.path_speed;
            peak_previous_nominal=previous_nominal;peak_nominal=g.path_v;peak_tau=g.tau;
        }
    }
}
void result(const char* id, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << id << " " << detail << '\n';
    if (!ok) ++failures;
}
double angle_error(double a, double b) { return std::abs(std::remainder(a-b, 2*Gen::PI)); }
double pitch(const positioning_arm::ArmPose& p)
{
    const Eigen::Vector3d axis = p.quaternion*Eigen::Vector3d(0,-1,0);
    return std::atan2(axis.z(), std::hypot(axis.x(), axis.y()));
}
double yaw(const positioning_arm::ArmPose& p)
{
    const Eigen::Vector3d axis = p.quaternion*Eigen::Vector3d::UnitX();
    return std::atan2(axis.y(), axis.x());
}
int main()
{
    std::cout << std::setprecision(12);
    positioning_arm::PositioningArmKinematics truth(config());
    std::mt19937 random(20261104);
    double wrist_error = 0, inverse_error = 0; bool t1_finite = true;
    for (int n = 0; n < 1000; ++n) {
        auto uniform = [&]() { return static_cast<double>(random())/4294967295.0; };
        Axes p{{100, -1080+2160*uniform(), (n%2 ? -1 : 1)*(1+178*uniform()), -1080+2160*uniform(), 12}};
        Gen g = begin(1, 1, 5, p);
        const auto fk = truth.forward(joints(p));
        const Eigen::Vector3d tool = fk.quaternion*Eigen::Vector3d(config().lateral_offset,-config().tool_length,0);
        wrist_error = std::max(wrist_error, std::hypot(g.wx-(fk.position.x()-tool.x())*1000,
            g.wy-(fk.position.y()-tool.y())*1000));
        // 独立atan2逆解验证原点回代，再验证发生器实际非零步的逆解。
        const auto inverse = [&](double wx, double wy) {
            const double cosine = std::clamp((wx*wx+wy*wy-Gen::L1*Gen::L1-Gen::L2*Gen::L2)/(2*Gen::L1*Gen::L2),-1.0,1.0);
            const double alpha = g.branch*std::acos(cosine);
            return std::array<double,2>{{std::atan2(wx,-wy)-std::atan(Gen::L2*std::sin(alpha)/(Gen::L1+Gen::L2*std::cos(alpha))), alpha-Gen::PI}};
        };
        const auto roundtrip = inverse(g.wx,g.wy);
        inverse_error = std::max(inverse_error,std::max(angle_error(roundtrip[0],p[1]*Gen::RAD),angle_error(roundtrip[1],p[2]*Gen::RAD)));
        tick(g,true,1,1,5,"T1");
        const auto moved_inverse = inverse(g.wx,g.wy);
        inverse_error = std::max(inverse_error,std::max(angle_error(moved_inverse[0],g.p[1]*Gen::RAD),angle_error(moved_inverse[1],g.p[2]*Gen::RAD)));
        t1_finite = t1_finite && std::isfinite(g.p[1]) && std::isfinite(g.p[2]) && g.path_speed>0;
    }
    std::cout << "T1 wrist_mm=" << wrist_error << " inverse_rad=" << inverse_error << '\n';
    result("T1", t1_finite && wrist_error <= 1e-6 && inverse_error <= 1e-9);

    bool t2 = true; double line_error = 0, invariants = 0;
    for (int mode : {1,2}) for (double speed : {30.,15.,5.}) for (double elbow : {30.,90.,150.}) {
        const auto p = seed(mode, elbow); auto g = begin(mode,1,speed,p);
        const auto initial = truth.forward(joints(p));
        for (int n = 1; n <= 3501; ++n) {
            tick(g,n<=3000,mode,1,speed,"T2");
            const auto fk = truth.forward(joints(g.p));
            line_error = std::max(line_error,std::abs(fk.position[mode == 1 ? 1 : 0]
                - initial.position[mode == 1 ? 1 : 0])*1000);
            invariants = std::max(invariants,std::max({std::abs(fk.position.z()-initial.position.z())*1000,
                angle_error(yaw(fk),yaw(initial))/Gen::RAD, std::abs(pitch(fk)-pitch(initial))/Gen::RAD}));
            t2 = t2 && std::hypot(g.wx,g.wy)>=Gen::L1-Gen::L2
                && std::hypot(g.wx,g.wy)<=Gen::L1+Gen::L2 && std::memcmp(&g.p[0],&p[0],sizeof(double)) == 0
                && std::memcmp(&g.p[4],&p[4],sizeof(double)) == 0;
        }
    }
    std::cout << "T2 line_mm=" << line_error << " invariant_max=" << invariants << '\n';
    result("T2",t2 && line_error<=0.01 && invariants<=1e-6);

    auto g = begin(1,1,5,seed(1,90));
    double peak = 0, prev = 0; bool monotonic = true; int acc_ticks = 0, stop_ticks = 0;
    for (int n=1;n<=2000;++n) {
        tick(g,true,1,1,5,"T3"); monotonic = monotonic && g.path_speed >= prev;
        peak = std::max(peak,(g.path_speed-prev)/DT); prev = g.path_speed;
        if (g.path_speed == 5 && acc_ticks == 0) acc_ticks=n;
    }
    const double sigma0=g.sigma;
    for (int n=1;n<=501;++n) {
        tick(g,false,0,0,0,"T3");
        if (g.path_speed==0 && stop_ticks==0) stop_ticks=n;
    }
    const double stopping=g.sigma-sigma0;
    std::cout<<"T3 acc_s="<<acc_ticks*DT<<" stop_s="<<stop_ticks*DT<<" peak_mm_s2="<<peak<<" stop_mm="<<stopping<<'\n';
    result("T3",monotonic && std::abs(acc_ticks-1000)<=1 && std::abs(stop_ticks-500)<=1
        && std::abs(peak-7.5)<=0.075 && std::abs(stopping-1.25)<=1.25*0.005);

    bool t4=true;
    for (int mode=1;mode<=5;++mode) {
        const Axes speeds = mode<=2 ? Axes{{30,15,5,0,0}} : mode==3 ? Axes{{3,2.5,1.25,0,0}}
            : mode==4 ? Axes{{1.2,.8,.4,0,0}} : Axes{{2.4,1.6,.8,0,0}};
        for (int tier=0;tier<3;++tier) {
            const Axes p=mode==5?yaw_seed(90,0):seed(mode,90);
            g=begin(mode,1,speeds[tier],p);
            for(int n=0;n<2000;++n) tick(g,true,mode,1,speeds[tier],"T4");
            const double start=g.sigma;
            bool constrained=g.limited;
            for(int n=0;n<501;++n) { tick(g,false,0,0,0,"T4"); constrained=constrained||g.limited; }
            const double distance=g.sigma-start;
            const double expected=speeds[tier]*.25;
            std::cout<<"T4 mode="<<mode<<" tier="<<tier<<" stop_distance="<<distance
                <<" expected="<<expected<<" limited="<<constrained<<'\n';
            t4=t4 && (mode<=2 ? distance<=15 : constrained || std::abs(distance-expected)<=expected*.005);
        }
    }
    result("T4",t4);

    g=begin(1,1,15,seed(1,90)); bool positive=true; int up=0,down=0;
    for(int n=1;n<=5000;++n) {
        const double speed=n<=2000?15:n<=4000?30:5;
        tick(g,true,1,1,speed,"T5"); positive=positive && g.path_speed>0;
        if(n>2000 && n<=4000 && g.path_speed==30 && up==0) up=n-2000;
        if(n>4000 && g.path_speed==5 && down==0) down=n-4000;
    }
    std::cout<<"T5 up_s="<<up*DT<<" down_s="<<down*DT<<'\n';
    result("T5",positive && std::abs(up-1000)<=1 && std::abs(down-500)<=1);

    g=begin(1,1,15,seed(1,90)); int switched=0; bool switched_at_zero=false;
    for(int n=1;n<=4000;++n) {
        tick(g,true,n<=2000?1:2,1,15,"T6");
        if(g.latched_mode==2 && switched==0) { switched=n-2000; switched_at_zero=g.path_v==0; }
    }
    std::cout<<"T6 switch_after_s="<<switched*DT<<" path_speed="<<g.path_speed<<'\n';
    result("T6",switched_at_zero && std::abs(switched-500)<=1 && g.path_speed>0,
        "Gen only; stage 25 ownership checked in ST static audit");

    g=begin(1,1,5,seed(1,90)); double minimum=5; double at_repress=0;
    for(int n=1;n<=4000;++n) {
        tick(g,n<=2000 || n>2200,1,1,5,"T7");
        if(n==2200)at_repress=g.path_speed;
        if(n>2000)minimum=std::min(minimum,g.path_speed);
    }
    std::cout<<"T7 minimum="<<minimum<<" repressed="<<at_repress<<" final="<<g.path_speed<<'\n';
    result("T7",minimum>0 && g.path_speed==5);

    g=begin(1,1,5,seed(1,90)); double alive_age=0,at_timeout=0;int timeout_tick=0;
    for(int n=1;n<=3100;++n) {
        if(n<=2000)alive_age=0;else alive_age+=DT;
        const bool held=alive_age<0.5;
        if(!held && timeout_tick==0){timeout_tick=n-2000;at_timeout=g.sigma;}
        tick(g,held,1,1,5,"T8");
    }
    std::cout<<"T8 age_cycles="<<timeout_tick<<" stop_mm="<<g.sigma-at_timeout<<'\n';
    result("T8",timeout_tick==500 && g.path_speed==0 && std::abs(g.sigma-at_timeout-1.25)<=1.25*.005,
        "alive-age predicate matches ArmCartJog; no error path");

    // 有效肘角10度、腕点朝+X；慢档径向伸直3秒，再反向3秒。
    const auto radial=seed(2,10);g=begin(1,1,5,radial);
    bool cap=true,flag=true,finite=true,near_straight=false,branch_unchanged=true;
    double max_joint=0,min_radius=std::hypot(g.wx,g.wy),max_radius=min_radius,outward_final_speed=0;
    double outward_elbow=0,reverse_full_speed_s=0;
    const int old_branch=g.branch;
    for(int n=1;n<=6000;++n){
        const auto before=g.p;
        const double previous_nominal_speed=g.path_v;
        tick(g,true,1,n<=3000?1:-1,5,n<=3000?"T9":"T9 reverse");
        for(int i=0;i<5;++i){
            const double joint_speed=std::abs(g.p[i]-before[i])/DT;
            max_joint=std::max(max_joint,joint_speed);
            cap=cap && joint_speed<=Gen::JOINT_VCAP[i]*(1+1e-9);
            finite=finite && std::isfinite(g.p[i]) && std::isfinite(g.v[i]) && std::isfinite(g.a[i]);
        }
        const double radius=std::hypot(g.wx,g.wy);
        min_radius=std::min(min_radius,radius);max_radius=std::max(max_radius,radius);
        flag=flag && (g.path_speed*DT>=0.5*(g.path_v+previous_nominal_speed)*DT || g.limited);
        branch_unchanged=branch_unchanged && g.branch==old_branch;
        if(n==3000){outward_final_speed=g.path_speed;near_straight=g.limited;
            outward_elbow=std::abs(std::remainder(g.p[2]+180,360.));}
        if(n>3000 && g.path_speed==5 && reverse_full_speed_s==0)reverse_full_speed_s=(n-3000)*DT;
    }
    std::cout<<"T9 max_joint_deg_s="<<max_joint<<" radius_min_mm="<<min_radius<<" radius_max_mm="<<max_radius
        <<" outward_final_speed="<<outward_final_speed<<" outward_effective_elbow_deg="<<outward_elbow
        <<" reverse_full_speed_s="<<reverse_full_speed_s<<" reverse_final_speed="<<g.path_speed<<'\n';
    result("T9",cap && flag && finite && near_straight && outward_final_speed<.001
        && min_radius>=Gen::L1-Gen::L2 && max_radius<=Gen::L1+Gen::L2
        && reverse_full_speed_s>0 && reverse_full_speed_s<=3 && g.path_speed==5 && branch_unchanged);

    bool t10=true;
    for(int mode=3;mode<=4;++mode){
        const double speed=mode==3?3:1.2; const auto p=seed(mode,90);
        g=begin(mode,1,speed,p); const auto before=truth.forward(joints(p));
        for(int n=0;n<2000;++n)tick(g,true,mode,1,speed,"T10");
        const int j=mode==3?0:4; bool others=true;
        for(int i=0;i<5;++i)if(i!=j)others=others && std::memcmp(&g.p[i],&p[i],sizeof(double))==0;
        const auto after=truth.forward(joints(g.p));
        const double integral_error=std::abs(std::abs(g.p[j]-p[j])-1.5*speed);
        const bool sign=mode==3?g.p[0]<p[0]:pitch(after)>pitch(before);
        std::cout<<"T10 mode="<<mode<<" distance="<<std::abs(g.p[j]-p[j])<<" analytic_integral_error="<<integral_error<<'\n';
        t10=t10 && others && sign && integral_error<=1e-4;
    }
    result("T10",t10,"trapezoidal step versus continuous cubic integral, tolerance 1e-4");
    bool t12=true;
    for(double s:{3.,2.5,1.25})t12=t12 && s<=Gen::JOINT_VCAP[0];
    for(double s:{1.2,.8,.4})t12=t12 && s<=Gen::JOINT_VCAP[4];
    result("T12",t12,"UI speed table checked separately");

    bool y1=true; double y1_tcp=0,y1_z=0,y1_pitch=0,y1_psi_error=0,y1_integral_error=0;int y1_cases=0,y1_cycles=0;
    for(double elbow:{30.,90.,150.})for(double q5:{0.,30.,-30.})for(int dir:{1,-1}){
        const Axes p=yaw_seed(elbow,q5);g=begin(5,dir,.8,p);
        const auto initial=truth.forward(joints(p));const double initial_psi=psi_deg(p);
        double max_psi_sigma=0,expected_sigma=0,max_integral_error=0;bool finite=true,limited_seen=false;
        for(int n=1;n<=3500;++n){
            const double old_v1=g.v1,old_tau=g.tau;tick(g,n<=3000,5,dir,.8,"Y1");
            expected_sigma+=curve_integral(g,g.v1!=old_v1?0:old_tau,g.tau);
            const auto fk=truth.forward(joints(g.p));
            y1_tcp=std::max(y1_tcp,(fk.position-initial.position).norm()*1000);
            y1_z=std::max(y1_z,std::abs(fk.position.z()-initial.position.z())*1000);
            y1_pitch=std::max(y1_pitch,std::abs(pitch(fk)-pitch(initial))/Gen::RAD);
            max_psi_sigma=std::max(max_psi_sigma,std::abs(psi_deg(g.p)-initial_psi-dir*g.sigma));
            limited_seen=limited_seen||g.limited;
            if(!g.limited)max_integral_error=std::max(max_integral_error,std::abs(g.sigma-expected_sigma));
            finite=finite&&std::isfinite(fk.position.x())&&std::isfinite(fk.position.y())&&std::isfinite(fk.position.z())
                &&std::isfinite(g.path_speed)&&g.p[0]==p[0]&&g.p[4]==p[4];
        }
        const double yaw_change=psi_deg(g.p)-initial_psi;
        y1_integral_error=std::max(y1_integral_error,max_integral_error);
        y1_psi_error=std::max(y1_psi_error,max_psi_sigma);
        y1=y1&&finite&&!limited_seen&&y1_z<=1e-6&&y1_pitch<=1e-6&&max_psi_sigma<=1e-9
            &&std::abs(yaw_change)>0&&(dir>0?yaw_change>0:yaw_change<0);
        std::cout<<"Y1 config elbow="<<elbow<<" q5="<<q5<<" dir="<<dir<<" yaw_change="<<yaw_change
            <<" limited="<<limited_seen<<" sigma="<<g.sigma<<" integral_error="<<max_integral_error<<'\n';
        y1_cycles+=3500;++y1_cases;
    }
    std::cout<<"Y1 cases="<<y1_cases<<" cycles="<<y1_cycles<<" tcp_max_mm="<<y1_tcp
        <<" z_max_mm="<<y1_z<<" pitch_max_deg="<<y1_pitch<<" psi_sigma_error_deg="<<y1_psi_error
        <<" integral_error_deg="<<y1_integral_error<<"\n";
    result("Y1",y1&&y1_tcp<=.01&&y1_integral_error<=1e-4);

    g=begin(5,1,.8,yaw_seed(90,0));double y2_peak=0,y2_previous=0,y2_stop=0;int y2_acc=0,y2_dec=0;
    bool y2_monotonic=true;double y2_sigma=0;
    for(int n=1;n<=2000;++n){tick(g,true,5,1,.8,"Y2");y2_monotonic=y2_monotonic&&g.path_speed>=y2_previous;
        y2_peak=std::max(y2_peak,(g.path_speed-y2_previous)/DT);y2_previous=g.path_speed;if(g.path_speed==.8&&y2_acc==0)y2_acc=n;}
    y2_sigma=g.sigma;
    for(int n=1;n<=501;++n){tick(g,false,0,0,0,"Y2");if(g.path_speed==0&&y2_dec==0)y2_dec=n;}
    const double y2_stopping=g.sigma-y2_sigma;
    std::cout<<"Y2 acc_s="<<y2_acc*DT<<" stop_s="<<y2_dec*DT<<" stop_deg="<<y2_stopping<<" peak_deg_s2="<<y2_peak<<'\n';
    result("Y2",y2_monotonic&&std::abs(y2_acc-1000)<=1&&std::abs(y2_dec-500)<=1
        &&std::abs(y2_stopping-.2)<=.001);

    bool y3=true;double y3_max_joint=0,y3_tcp=0;std::uint64_t y3_cycles=0;
    for(double elbow:{30.,90.,150.})for(double q5:{0.,30.,-30.})for(double speed:{2.4,1.6,.8}){
        const Axes p=yaw_seed(elbow,q5);g=begin(5,1,speed,p);const auto initial=truth.forward(joints(p));bool constrained=false;
        Axes full_rate{};const bool derivative_valid=yaw_candidate(g,p,0,1,speed*DT,full_rate);
        const double required_q2=derivative_valid?std::abs(full_rate[1]-p[1])/DT:std::numeric_limits<double>::infinity();
        const double required_q3=derivative_valid?std::abs(full_rate[2]-p[2])/DT:std::numeric_limits<double>::infinity();
        const double required_q4=derivative_valid?std::abs(full_rate[3]-p[3])/DT:std::numeric_limits<double>::infinity();
        const double required_joint=std::max({required_q2,required_q3,required_q4});
        const double wrist_speed=speed*Gen::RAD*std::hypot(g.d0x,g.d0y);
        for(int n=0;n<3000;++n){const auto previous=g.p;tick(g,true,5,1,speed,"Y3");
            for(int i=0;i<5;++i){const double joint_speed=std::abs(g.p[i]-previous[i])/DT;y3_max_joint=std::max(y3_max_joint,joint_speed);
                y3=y3&&joint_speed<=Gen::JOINT_VCAP[i]*(1+1e-9);}
            const auto fk=truth.forward(joints(g.p));y3_tcp=std::max(y3_tcp,(fk.position-initial.position).norm()*1000);
            constrained=constrained||g.limited;y3_cycles++;}
        std::cout<<"Y3 elbow="<<elbow<<" q5="<<q5<<" requested_deg_s="<<speed
            <<" required_wrist_mm_s="<<wrist_speed<<" required_q2_deg_s="<<required_q2
            <<" required_q3_deg_s="<<required_q3<<" required_q4_deg_s="<<required_q4
            <<" limited="<<constrained<<" actual_yaw_speed_deg_s="<<g.path_speed
            <<" tcp_max_mm="<<y3_tcp<<'\n';
        y3=y3&&derivative_valid&&std::isfinite(required_joint);
    }
    result("Y3",y3&&y3_tcp<=.01,"cases=9x3 cycles="+std::to_string(y3_cycles)+" max_joint_deg_s="+std::to_string(y3_max_joint));

    g=begin(5,1,.8,yaw_seed(10,0,true));const Axes y4_start=g.p;
    bool y4_boundary=false,y4_finite=true;double y4_min=std::hypot(g.wx,g.wy),y4_max=y4_min;
    int y4_reverse_full_cycle=0,y4_stop_cycle=0;
    while(y4_stop_cycle==0&&cycle_count["Y4"]<6000){const int n=cycle_count["Y4"];
        tick(g,true,5,1,.8,"Y4");
        const auto current_wrist=wrist_xy(g.p);const double actual_radius=std::hypot(current_wrist[0],current_wrist[1]);
        y4_min=std::min(y4_min,actual_radius);y4_max=std::max(y4_max,actual_radius);y4_boundary=y4_boundary||g.limited;
        y4_finite=y4_finite&&std::isfinite(g.path_speed)&&std::isfinite(actual_radius);
        if(g.path_speed<.001&&g.limited)y4_stop_cycle=n+1;
    }
    double y4_reverse_peak=0,y4_max_joint=0;bool y4_limited_reverse=false;
    for(int n=1;n<=3000;++n){const auto previous=g.p;tick(g,true,5,-1,.8,"Y4 reverse");
        const auto current_wrist=wrist_xy(g.p);const double actual_radius=std::hypot(current_wrist[0],current_wrist[1]);
        y4_min=std::min(y4_min,actual_radius);y4_max=std::max(y4_max,actual_radius);
        y4_finite=y4_finite&&std::isfinite(g.path_speed)&&std::isfinite(actual_radius);
        y4_reverse_peak=std::max(y4_reverse_peak,g.path_speed);y4_limited_reverse=y4_limited_reverse||g.limited;
        for(int i=1;i<=3;++i)y4_max_joint=std::max(y4_max_joint,std::abs(g.p[i]-previous[i])/DT);
        if(g.path_speed>=.8&&y4_reverse_full_cycle==0)y4_reverse_full_cycle=n;}
    const auto y4_end_wrist=wrist_xy(g.p);
    std::cout<<"Y4 radius_min="<<y4_min<<" radius_max="<<y4_max<<" boundary_limited="<<y4_boundary
        <<" stop_cycle="<<y4_stop_cycle<<" reverse_reach_s="<<y4_reverse_full_cycle*DT
        <<" reverse_peak_speed="<<y4_reverse_peak<<" reverse_final_speed="<<g.path_speed
        <<" reverse_limited="<<y4_limited_reverse<<" max_joint_deg_s="<<y4_max_joint
        <<" start_q="<<y4_start[1]<<','<<y4_start[2]<<','<<y4_start[3]
        <<" end_wrist_r="<<std::hypot(y4_end_wrist[0],y4_end_wrist[1])<<'\n';
    result("Y4",y4_finite&&y4_boundary&&y4_min>=Gen::L1-Gen::L2-1e-10&&y4_max<=Gen::L1+Gen::L2+1e-10
        &&y4_reverse_peak<=.8&&y4_limited_reverse&&y4_max_joint<=Gen::JOINT_VCAP[3]*(1+1e-9),
        "boundary stop and reversal verified; JOINT_VCAP limits recovery rate when 0.8 deg/s is infeasible");

    bool y5=true;for(int dir:{1,-1}){const Axes zero{{100,0,0,0,0}};g=begin(5,dir,.8,zero);const auto initial=truth.forward(joints(zero));
        double max_tcp=0;for(int n=0;n<2000;++n){tick(g,true,5,dir,.8,"Y5");const auto fk=truth.forward(joints(g.p));
            max_tcp=std::max(max_tcp,(fk.position-initial.position).norm()*1000);}
        const bool no_motion=g.sigma==0;
        std::cout<<"Y5 dir="<<dir<<" yaw_deg="<<g.sigma*dir<<" final_path_speed="<<g.path_speed
            <<" tcp_error_mm="<<max_tcp<<" limited="<<g.limited<<" no_motion="<<no_motion<<'\n';
        y5=y5&&max_tcp<=.01&&std::isfinite(g.path_speed)&&(!no_motion||g.limited);}
    result("Y5",y5,"from zero q=(100,0,0,0,0)");

    g=begin(1,1,5,yaw_seed(90,0));bool y6=true,y6_yaw_captured=false;int y6_x_anchor=0,y6_y_anchor=0;
    Eigen::Vector3d y6_yaw_tcp_anchor=Eigen::Vector3d::Zero();double y6_tcp=0;bool y6_no_error=true;
    for(int n=1;n<=9000;++n){const int mode=n<=2000?1:n<=5500?5:2;const double speed=mode==1||mode==2?5:.8;
        tick(g,true,mode,1,speed,"Y6");if(mode==5&&g.latched_mode==5&&y6_x_anchor==0)y6_x_anchor=n;
        if(mode==2&&g.latched_mode==2&&y6_y_anchor==0)y6_y_anchor=n;
        if(g.latched_mode==5){
            if(!y6_yaw_captured){y6_yaw_tcp_anchor=truth.forward(joints(g.anchor)).position;y6_yaw_captured=true;}
            y6_tcp=std::max(y6_tcp,(truth.forward(joints(g.p)).position-y6_yaw_tcp_anchor).norm()*1000);
        }
        y6_no_error=y6_no_error&&std::isfinite(g.path_speed);
    }
    y6=y6_x_anchor>=2500&&y6_y_anchor>=6000&&y6_yaw_captured&&y6_tcp<=.01&&g.latched_mode==2&&y6_no_error;
    std::cout<<"Y6 yaw_reanchor_cycle="<<y6_x_anchor<<" y_reanchor_cycle="<<y6_y_anchor<<" yaw_tcp_error_mm="<<y6_tcp<<" stage25_model=true\n";
    result("Y6",y6);

    bool branch_retained=true;
    for(double known_q3:{-90.,90.}){
        g=begin(1,1,5,Axes{{100,0,known_q3,0,0}});
        const int prior_branch=g.branch;
        for(double ambiguous_q3:{0.,180.,-180.,360.,-360.,540.,-540.}){
            g.step(true,false,1,1,5,Axes{{100,0,ambiguous_q3,0,0}},DT);
            branch_retained=branch_retained && g.branch==prior_branch;
        }
    }
    result("BRANCH",branch_retained,"folded/straight and multi-turn equivalents retain both previous branches");

    bool t13=true;
    for(int mode:{1,2})for(int dir:{1,-1}){
        const Axes zero{{100,0,0,0,0}};g=begin(mode,dir,5,zero);
        const auto initial=truth.forward(joints(zero));
        double straight_error=0,max_increment_speed=0;bool finite=true,limited_seen=false,negative_bend=true;
        for(int n=0;n<2000;++n){
            const auto previous=g.p;
            tick(g,true,mode,dir,5,"T13");
            const auto fk=truth.forward(joints(g.p));
            straight_error=std::max(straight_error,std::abs(fk.position[mode==1?1:0]-initial.position[mode==1?1:0])*1000);
            for(int i=0;i<5;++i){
                const double joint_speed=std::abs(g.p[i]-previous[i])/DT;
                max_increment_speed=std::max(max_increment_speed,joint_speed);
                t13=t13 && joint_speed<=Gen::JOINT_VCAP[i]*(1+1e-9);
                finite=finite && std::isfinite(g.p[i]) && std::isfinite(g.v[i]) && std::isfinite(g.a[i]);
            }
            finite=finite && std::isfinite(g.path_speed);
            limited_seen=limited_seen || g.limited;
            negative_bend=negative_bend && g.p[2]<=0 && g.branch==Gen::INITIAL_BRANCH;
        }
        const double displacement=(truth.forward(joints(g.p)).position-initial.position).norm()*1000;
        const bool inward=mode==2 && dir==1;
        std::cout<<"T13 mode="<<mode<<" dir="<<dir<<" displacement_mm="<<displacement
            <<" final_path_speed="<<g.path_speed<<" line_mm="<<straight_error<<" max_joint_deg_s="<<max_increment_speed
            <<" q3_deg="<<g.p[2]<<" limited="<<g.limited<<'\n';
        t13=t13 && finite && negative_bend && straight_error<=.01 && (inward?limited_seen:displacement>0);
    }
    result("T13",t13,"program zero wrist=(0,-20 mm); Y+ is toward axis2 pivot; Gen has no error output");

    for(const auto& item:acceleration_ratio_by_case)
        std::cout<<"T11 case="<<item.first<<" path_acceleration_ratio="<<item.second<<'\n';
    std::cout<<"T11 velocity_difference="<<velocity_difference<<" acceleration_difference="<<acceleration_difference
        <<" maximum_path_acceleration_ratio="<<maximum_path_acceleration_ratio<<'\n';
    std::cout<<"T11 peak_case="<<peak_case<<" cycle="<<peak_case_cycle<<" v0="<<peak_v0<<" v1="<<peak_v1
        <<" T="<<peak_duration<<" tau="<<peak_tau<<" previous_nominal="<<peak_previous_nominal<<" nominal="<<peak_nominal
        <<" previous_path_speed="<<peak_previous_path_speed<<" path_speed="<<peak_path_speed
        <<" acceleration="<<std::abs(peak_path_speed-peak_previous_path_speed)/DT
        <<" B_k_minus_2="<<peak_bounds[0]<<" B_k_minus_1="<<peak_bounds[1]<<" B_k="<<peak_bounds[2]
        <<" allowed="<<1.01*std::max({peak_bounds[0],peak_bounds[1],peak_bounds[2]})<<'\n';
    result("T11",cycles_finite && velocity_difference<=1e-9 && acceleration_difference<=1e-9 && path_acceleration_ok);

    auto shifted=config();shifted.base_offset_mm=Eigen::Vector3d(20,-30,40);
    positioning_arm::PositioningArmKinematics translated(shifted);auto p=seed(1,90);
    const auto base=truth.forward(joints(p)), moved=translated.forward(joints(p));
    const auto j=translated.computeFrameJacobian(joints(p)),j0=truth.computeFrameJacobian(joints(p));
    const auto ik0=truth.solveCartesianJog(joints(p),{{.0001,0,0},0,0,.05});
    const auto ik1=translated.solveCartesianJog(joints(p),{{.0001,0,0},0,0,.05});
    result("OFFSET",(moved.position-base.position-shifted.base_offset_mm*.001).norm()<1e-12
        && (j-j0).norm()<1e-12 && ik0.success() && ik1.success() && (ik0.target-ik1.target).norm()<1e-12);
    result("FINITE",cycles_finite);
    std::cout<<"TOTAL failures="<<failures<<'\n';
    return failures?1:0;
}
