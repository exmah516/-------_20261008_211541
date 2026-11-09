#include "positioning_arm/kinematics.h"
#include "arm_cart_jog_reference.h"
#include "arm_cart_jog_st_generated.h"
#include <Eigen/Cholesky>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

using Gen = ArmCartJogReference;
using Axes = std::array<double, 5>;
constexpr double DT = .001;
int failures = 0;
std::uint64_t cycles = 0;
double st_difference = 0, velocity_difference = 0, max_velocity = 0, max_acceleration = 0;
double independent_error = 0;
bool finite = true, caps = true, discrete = true;
std::string acceleration_case;
int acceleration_axis = 0, acceleration_cycle = 0;

positioning_arm::ArmKinematicConfig config()
{
    positioning_arm::ArmKinematicConfig c;
    c.lower.setConstant(-std::numeric_limits<double>::infinity());
    c.upper.setConstant(std::numeric_limits<double>::infinity());
    c.max_velocity.setConstant(100);
    for (int i = 0; i < 5; ++i) c.calibration[i] = {0, i == 0 ? .001 : Gen::RAD, 1};
    return c;
}
positioning_arm::JointVector joints(const Axes& p)
{
    positioning_arm::JointVector q;
    for (int i = 0; i < 5; ++i) q[i] = p[i]*(i == 0 ? .001 : Gen::RAD);
    return q;
}
double yaw(const Axes& p) { return p[1]+p[2]+p[3]; }
void result(const std::string& id, bool pass)
{
    std::cout << (pass ? "PASS " : "FAIL ") << id << '\n';
    if (!pass) ++failures;
}
struct Run {
    Gen g;
    StCartJog st;
    Axes seed;
    std::array<double, 6> st_seed;
    std::string name;
    int n = 0;
    double nominal = 0, integral = 0, peak_a = 0, minimum_scale = 1;
    bool damped = false;
    int peak_axis = 0, peak_cycle = 0;
    double peak_s = 0, peak_lambda = 0, peak_previous_v = 0, peak_v = 0, peak_q3 = 0;
    Run(const std::string& label, const Axes& p, int mode, int dir, double speed)
        : seed(p), st_seed{{0,p[0],p[1],p[2],p[3],p[4]}}, name(label)
    {
        g.step(true, false, mode, dir, speed, seed, DT);
        st.step(true, false, mode, dir, speed, st_seed, DT);
    }
    void tick(bool held, int mode, int dir, double speed)
    {
        const auto before = g.p;
        const auto before_v = g.v;
        g.step(false, held, mode, dir, speed, seed, DT);
        st.step(false, held, mode, dir, speed, st_seed, DT);
        ++n; ++cycles;
        nominal += g.path_v*DT;
        integral += g.path_speed*DT;
        damped = damped || g.limited;
        minimum_scale = std::min(minimum_scale, g.scale);
        for (int i = 0; i < 5; ++i) {
            for (const auto& pair : {std::pair<double,double>{g.p[i],st.p[i+1]},
                {g.v[i],st.v[i+1]},{g.a[i],st.a[i+1]}})
                st_difference = std::max(st_difference,std::abs(pair.first-pair.second));
            velocity_difference = std::max(velocity_difference,std::abs(g.v[i]-(g.p[i]-before[i])/DT));
            caps = caps && std::abs(g.v[i]) <= Gen::JOINT_VCAP[i]*(1+1e-9);
            finite = finite && std::isfinite(g.p[i]) && std::isfinite(g.v[i]) && std::isfinite(g.a[i]);
            max_velocity = std::max(max_velocity,std::abs(g.v[i]));
            if (std::abs(g.a[i]) > peak_a) {
                peak_a = std::abs(g.a[i]); peak_axis=i+1; peak_cycle=n;
                peak_s=g.s; peak_lambda=g.lambda; peak_previous_v=before_v[i]; peak_v=g.v[i]; peak_q3=before[2];
            }
            if (std::abs(g.a[i]) > max_acceleration) {
                max_acceleration=std::abs(g.a[i]); acceleration_case=name; acceleration_axis=i+1; acceleration_cycle=n;
            }
        }
        if (mode == 1 || mode == 2 || mode == 5) {
            Eigen::Matrix3d j;
            j << g.j11,g.j12,g.j13,g.j21,g.j22,g.j23,
                 Gen::DLS_PSI_WEIGHT,Gen::DLS_PSI_WEIGHT,Gen::DLS_PSI_WEIGHT;
            const Eigen::Vector3d task(g.task_x,g.task_y,g.task_psi);
            const Eigen::Matrix3d m=j*j.transpose()+g.lambda*g.lambda*Eigen::Matrix3d::Identity();
            Eigen::Vector3d expected=j.transpose()*m.ldlt().solve(task);
            double scale=1;
            for(int axis=0;axis<3;++axis)
                if(std::abs(expected[axis])>Gen::JOINT_VCAP[axis+1]*Gen::RAD)
                    scale=std::min(scale,Gen::JOINT_VCAP[axis+1]*Gen::RAD/std::abs(expected[axis]));
            expected*=scale;
            for(int axis=0;axis<3;++axis)
                independent_error=std::max(independent_error,std::abs(g.qdot[axis+1]-expected[axis]));
            if (std::abs(g.a[2]) > 200 && std::abs(g.a[2]) == max_acceleration) {
                std::cout<<"ACC_EVIDENCE case="<<name<<" cycle="<<n<<" q3_before="<<before[2]
                    <<" q3_after="<<g.p[2]<<" seed_q2="<<seed[1]<<" seed_q3="<<seed[2]<<" seed_q4="<<seed[3]<<" seed_q5="<<seed[4]<<" s="<<g.s<<" lambda="<<g.lambda
                    <<" previous_v3="<<before_v[2]<<" v3="<<g.v[2]<<" a3="<<g.a[2]<<'\n';
            }
        }
        for (const auto& pair : {std::pair<double,double>{g.path_speed,st.path_speed},
            {g.path_v,st.path_v},{g.target_speed,st.target_speed},{g.lambda,st.lambda},
            {g.scale,st.scale},{g.sigma,st.sigma},{g.tau,st.tau},{g.duration,st.duration}})
            st_difference=std::max(st_difference,std::abs(pair.first-pair.second));
        discrete = discrete && g.limited == st.limited && g.latched_mode == st.latched_mode && g.latched_dir == st.latched_dir;
        finite = finite && std::isfinite(g.path_speed) && std::isfinite(g.det);
    }
};

int main(int argc, char** argv)
{
    std::cout << std::setprecision(17);
    static_assert(Gen::DLS_PSI_WEIGHT == StCartJog::DLS_PSI_WEIGHT);
    static_assert(Gen::DLS_S0 == StCartJog::DLS_S0);
    static_assert(Gen::DLS_LAMBDA_MAX == StCartJog::DLS_LAMBDA_MAX);
    static_assert(Gen::T_ACC == StCartJog::T_ACC && Gen::T_DEC == StCartJog::T_DEC);
    for (int i=0;i<5;++i) if (Gen::JOINT_VCAP[i] != StCartJog::JOINT_VCAP[i+1]) return 2;
    positioning_arm::PositioningArmKinematics truth(config());
    std::mt19937 rng(20261109);
    auto random = [&] { return (static_cast<double>(rng())/4294967295.0)*360-180; };
    double jacobian_error=0;
    for (int sample=0;sample<1000;++sample) {
        Axes p{{100,random(),random(),random(),random()/3}};
        Gen g; g.step(true,false,1,1,5,p,DT); g.step(false,false,1,1,5,p,DT);
        const double j[3][3]={{g.j11,g.j12,g.j13},{g.j21,g.j22,g.j23},{1,1,1}};
        for(int axis=0;axis<3;++axis) {
            auto plus=joints(p), minus=plus;
            constexpr double eps=1e-5;
            plus[axis+1]+=eps; minus[axis+1]-=eps;
            const auto a=truth.forward(plus), b=truth.forward(minus);
            const auto d=(a.position-b.position)/(2*eps)*1000;
            const Eigen::Vector3d ua=a.quaternion*Eigen::Vector3d::UnitX(), ub=b.quaternion*Eigen::Vector3d::UnitX();
            const double dpsi=std::remainder(std::atan2(ua.y(),ua.x())-std::atan2(ub.y(),ub.x()),2*Gen::PI)/(2*eps);
            jacobian_error=std::max({jacobian_error,std::abs(j[0][axis]-d.x()),std::abs(j[1][axis]-d.y()),std::abs(1-dpsi)});
        }
    }
    std::cout<<"E1 samples=1000 max_error="<<jacobian_error<<'\n';
    result("E1",jacobian_error<=1e-6);
    std::ofstream csv(argc>1?argv[1]:"dls_cases.csv");
    csv<<std::setprecision(17)<<"id,elbow,q2,q4,q5,mode,dir,speed,cycles,displacement_mm,line_mm,yaw_deg,actual_integral,nominal_integral,ratio,damped,min_scale,peak_acc\n";
    bool e2=true,e3=true,e4=true,e5=true;
    double max_e2_line=0,max_e2_yaw=0,max_e5_drift=0,all_e2_line=0,all_e2_yaw=0;
    int e2_far=0,e2_entered=0;
    for (int group=2;group<=5;++group) {
        const auto elbows=group==2?std::vector<double>{45,90,135}:group==3?std::vector<double>{0}:group==4?std::vector<double>{175}:std::vector<double>{90,0};
        for(double elbow:elbows) for(double q5:group==2?std::vector<double>{0,30,-30}:std::vector<double>{0})
        for(int sample=0;sample<(group==2?16:1);++sample) for(int mode:group==5?std::vector<int>{5}:std::vector<int>{1,2})
        for(int dir:{1,-1}) for(double speed:group==5?std::vector<double>{.8,2.4}:std::vector<double>{5,30}) {
            // 按任务中的实际关节q3角度取姿态；q3=0为机械零位。
            Axes p=elbow==0?Axes{{0,0,0,0,0}}:
                Axes{{100,group==2?random():0,elbow,group==2?random():-elbow,q5}};
            if (group==4) {
                const double wx=Gen::L2*std::sin(elbow*Gen::RAD+Gen::PI);
                const double wy=-Gen::L1-Gen::L2*std::cos(elbow*Gen::RAD+Gen::PI);
                p[1]=-std::atan2(wy,wx)/Gen::RAD;
                p[3]=-p[1]-p[2]; // 腕点径向沿+X；X+向外，Y为切向。
            }
            const std::string id="E"+std::to_string(group);
            Run r(id,p,mode,dir,speed);
            const auto initial=truth.forward(joints(p));
            double line=0,drift=0,yaw_error=0;
            const int steps=group==2?3500:3000;
            for(int n=1;n<=steps;++n) {
                r.tick(n<=3000,mode,dir,speed);
                const auto fk=truth.forward(joints(r.g.p));
                if(mode!=5)line=std::max(line,std::abs(fk.position[mode==1?1:0]-initial.position[mode==1?1:0])*1000);
                drift=std::max(drift,(fk.position-initial.position).norm()*1000);
                yaw_error=std::max(yaw_error,std::abs(yaw(r.g.p)-yaw(p)));
            }
            const auto end=truth.forward(joints(r.g.p));
            const double distance=mode==5?drift:dir*(end.position[mode-1]-initial.position[mode-1])*1000;
            const double ratio=mode==5?(yaw(r.g.p)-yaw(p))/r.integral:distance/r.integral;
            csv<<id<<','<<elbow<<','<<p[1]<<','<<p[3]<<','<<q5<<','<<mode<<','<<dir<<','<<speed<<','<<steps<<','
                <<distance<<','<<line<<','<<yaw(r.g.p)-yaw(p)<<','<<r.integral<<','<<r.nominal<<','<<ratio<<','
                <<r.damped<<','<<r.minimum_scale<<','<<r.peak_a<<'\n';
            if(group==2) {
                all_e2_line=std::max(all_e2_line,line);all_e2_yaw=std::max(all_e2_yaw,yaw_error);
                if (!r.damped) {
                    ++e2_far;
                    e2=e2&&line<=.05&&yaw_error<=.01&&std::abs(ratio-1)<=.005;
                    max_e2_line=std::max(max_e2_line,line);max_e2_yaw=std::max(max_e2_yaw,yaw_error);
                } else ++e2_entered;
            }
            if(group==3) {
                e3=e3&&distance/r.nominal>=.7;
                std::cout<<"E3 mode="<<mode<<" dir="<<dir<<" speed="<<speed<<" displacement_mm="<<distance
                    <<" actual_integral="<<r.integral<<" ratio="<<ratio<<" nominal_ratio="<<distance/r.nominal
                    <<" yaw_change_deg="<<yaw(r.g.p)-yaw(p)<<" peak_acc="<<r.peak_a<<'\n';
            }
            if(group==4)std::cout<<"E4 q3="<<p[2]<<" q2="<<p[1]<<" q4="<<p[3]<<" radial=X+ tangent=Y mode="<<mode<<" dir="<<dir<<" speed="<<speed<<" displacement_mm="<<distance<<'\n';
            if(group==5) {
                if(elbow==90) { e5=e5&&drift<=.05&&std::abs(ratio-1)<=.005;max_e5_drift=std::max(max_e5_drift,drift); }
                std::cout<<"E5 elbow="<<elbow<<" dir="<<dir<<" speed="<<speed<<" drift_mm="<<drift
                    <<" yaw_change_deg="<<yaw(r.g.p)-yaw(p)<<" integral_deg="<<r.integral<<'\n';
            }
        }
    }
    std::cout<<"E2 far_cases="<<e2_far<<" entered_damping_cases="<<e2_entered<<" max_line_mm="<<max_e2_line<<" max_yaw_deg="<<max_e2_yaw<<'\n';
    result("E2",e2);result("E3",e3&&caps&&finite);result("E4",e4&&caps&&finite);result("E5",e5);
    bool crossed=false;double boundary_acc=0;
    for(int mode:{1,2})for(int dir:{-1,1}) {
        Run r("E6",Axes{{100,0,40,0,0}},mode,dir,30);
        bool outside=false,inside=false;
        for(int n=0;n<6000;++n) {
            r.tick(true,mode,dir,30);
            outside=outside||r.g.s>=Gen::DLS_S0;
            inside=inside||r.g.s<Gen::DLS_S0;
        }
        if(outside&&inside) {crossed=true;boundary_acc=std::max(boundary_acc,r.peak_a);}
        std::cout<<"E6 mode="<<mode<<" dir="<<dir<<" crossed="<<(outside&&inside)<<" peak_acc="<<r.peak_a<<'\n';
    }
    int boundary_cases=0;
    for(int q2=-180;q2<180;q2+=30)for(int q4=-180;q4<180;q4+=30)
    for(int mode:{1,2})for(int dir:{-1,1}) {
        Run r("E6_SCAN",Axes{{100,double(q2),40,double(q4),0}},mode,dir,30);
        bool inside=false;
        for(int n=0;n<6000;++n) {r.tick(true,mode,dir,30);inside=inside||r.g.s<Gen::DLS_S0;}
        if(inside) {
            ++boundary_cases;
            if(r.peak_a>boundary_acc) {
                boundary_acc=r.peak_a;crossed=true;
                std::cout<<"E6_SCAN_PEAK q2="<<q2<<" q3=40 q4="<<q4<<" mode="<<mode<<" dir="<<dir
                    <<" peak_acc="<<r.peak_a<<" axis="<<r.peak_axis<<" cycle="<<r.peak_cycle<<" q3_before="<<r.peak_q3
                    <<" s="<<r.peak_s<<" lambda="<<r.peak_lambda<<" previous_v="<<r.peak_previous_v<<" v="<<r.peak_v<<'\n';
            }
        }
    }
    std::cout<<"E6_SCAN poses=144 runs=576 entered_damping="<<boundary_cases<<'\n';
    std::cout<<"E6 crossed="<<crossed<<" peak_acc_deg_s2="<<boundary_acc<<'\n';
    result("E6",crossed&&boundary_acc<=200);
    // 复用原T3/T5/T6/T7/T8的输入时序与物理阈值；浮点速度以1e-9识别到达目标。
    bool e7=true;
    const Axes curve_seed{{100,0,90,-90,0}};
    Run t3("T3",curve_seed,1,1,5);
    double previous=0,peak=0;bool monotonic=true;int rise=0,stop=0;
    for(int n=1;n<=2000;++n) {
        t3.tick(true,1,1,5);
        monotonic=monotonic&&t3.g.path_speed>=previous-1e-9;
        peak=std::max(peak,(t3.g.path_speed-previous)/DT);previous=t3.g.path_speed;
        if(std::abs(t3.g.path_speed-5)<1e-9&&rise==0)rise=n;
    }
    const double start_integral=t3.integral;
    for(int n=1;n<=501;++n) {t3.tick(false,0,0,0);if(std::abs(t3.g.path_speed)<1e-9&&stop==0)stop=n;}
    bool pass=monotonic&&std::abs(rise-1000)<=1&&std::abs(stop-500)<=1&&std::abs(peak-7.5)<=.075
        &&std::abs(t3.integral-start_integral-1.25)<=1.25*.005&&!t3.damped;
    e7=e7&&pass;result("T3",pass);
    std::cout<<"T3 rise="<<rise<<" stop="<<stop<<" peak_mm_s2="<<peak<<" stopping_mm="<<t3.integral-start_integral<<'\n';
    Run t5("T5",curve_seed,1,1,15);int up=0,down=0;bool positive=true;
    for(int n=1;n<=5000;++n) {
        const double speed=n<=2000?15:n<=4000?30:5;t5.tick(true,1,1,speed);
        positive=positive&&t5.g.path_speed>0;
        if(n>2000&&n<=4000&&std::abs(t5.g.path_speed-30)<1e-9&&up==0)up=n-2000;
        if(n>4000&&std::abs(t5.g.path_speed-5)<1e-9&&down==0)down=n-4000;
    }
    pass=positive&&std::abs(up-1000)<=1&&std::abs(down-500)<=1&&!t5.damped;
    e7=e7&&pass;result("T5",pass);std::cout<<"T5 up="<<up<<" down="<<down<<'\n';
    Run t6("T6",curve_seed,1,1,15);int switched=0;bool at_zero=false;
    for(int n=1;n<=4000;++n) {
        t6.tick(true,n<=2000?1:2,1,15);
        if(t6.g.latched_mode==2&&switched==0) {switched=n-2000;at_zero=t6.g.path_v==0;}
    }
    pass=at_zero&&std::abs(switched-500)<=1&&t6.g.path_speed>0&&!t6.damped;
    e7=e7&&pass;result("T6",pass);std::cout<<"T6 switched="<<switched<<" speed="<<t6.g.path_speed<<'\n';
    Run t7("T7",curve_seed,1,1,5);double minimum=5,repress=0;
    for(int n=1;n<=4000;++n) {
        t7.tick(n<=2000||n>2200,1,1,5);
        if(n==2200)repress=t7.g.path_speed;
        if(n>2000)minimum=std::min(minimum,t7.g.path_speed);
    }
    pass=minimum>0&&std::abs(t7.g.path_speed-5)<1e-9&&!t7.damped;
    e7=e7&&pass;result("T7",pass);std::cout<<"T7 minimum="<<minimum<<" repress="<<repress<<" final="<<t7.g.path_speed<<'\n';
    Run t8("T8",curve_seed,1,1,5);double age=0,timeout_integral=0;int timeout=0;
    for(int n=1;n<=3100;++n) {
        if(n<=2000)age=0;else age+=DT;
        const bool held=age<.5;
        if(!held&&timeout==0) {timeout=n-2000;timeout_integral=t8.integral;}
        t8.tick(held,1,1,5);
    }
    pass=timeout==500&&t8.g.path_speed==0&&std::abs(t8.integral-timeout_integral-1.25)<=1.25*.005&&!t8.damped;
    e7=e7&&pass;result("T8",pass);std::cout<<"T8 timeout="<<timeout<<" stopping_mm="<<t8.integral-timeout_integral<<'\n';
    result("E7",e7);
    // Z/俯仰原T10/T12的积分、方向和其它轴保持。
    bool t10=true;
    for(int mode:{3,4}) {
        const double speed=mode==3?3:1.2;
        const Axes p{{100,20,-90,70,0}};Run r("T10",p,mode,1,speed);
        for(int n=0;n<2000;++n)r.tick(true,mode,1,speed);
        const int axis=mode==3?0:4;
        for(int i=0;i<5;++i)if(i!=axis)t10=t10&&r.g.p[i]==p[i];
        t10=t10&&std::abs(p[axis]-r.g.p[axis]-1.5*speed)<=1e-4;
    }
    result("T10/T12",t10);
    std::cout<<"E8 cycles="<<cycles<<" finite="<<finite<<" caps="<<caps<<" max_velocity="<<max_velocity
        <<" difference="<<velocity_difference<<" max_acc="<<max_acceleration<<" at="<<acceleration_case
        <<" axis="<<acceleration_axis<<" cycle="<<acceleration_cycle<<'\n';
    result("E8",finite&&caps&&velocity_difference<=1e-9&&max_acceleration<=300);
    std::cout<<"E9 cycles="<<cycles<<" max_difference="<<st_difference<<" discrete_equal="<<discrete<<'\n';
    result("E9",st_difference==0&&discrete);
    std::cout<<"INDEPENDENT LDLT max_qdot_difference_rad_s="<<independent_error<<'\n';
    result("INDEPENDENT",independent_error<=1e-9);
    return failures?1:0;
}
