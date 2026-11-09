#define main dls_acceptance_main
#include "dls_acceptance.cpp"
#undef main
#include <cstring>
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
        Run r("T1",p,1,1,5); Gen& g=r.g;
        const auto fk = truth.forward(joints(p));
        const Eigen::Vector3d tool = fk.quaternion*Eigen::Vector3d(config().lateral_offset,-config().tool_length,0);
        wrist_error = std::max(wrist_error, std::hypot(wrist_xy(g.p)[0]-(fk.position.x()-tool.x())*1000,
            wrist_xy(g.p)[1]-(fk.position.y()-tool.y())*1000));
        // 独立atan2逆解验证原点回代，再验证发生器实际非零步的逆解。
        const auto inverse = [&](double wx, double wy) {
            const double cosine = std::clamp((wx*wx+wy*wy-Gen::L1*Gen::L1-Gen::L2*Gen::L2)/(2*Gen::L1*Gen::L2),-1.0,1.0);
            const double alpha = (std::sin(p[2]*Gen::RAD+Gen::PI)>=0?1.0:-1.0)*std::acos(cosine);
            return std::array<double,2>{{std::atan2(wx,-wy)-std::atan(Gen::L2*std::sin(alpha)/(Gen::L1+Gen::L2*std::cos(alpha))), alpha-Gen::PI}};
        };
        const auto roundtrip = inverse(wrist_xy(g.p)[0],wrist_xy(g.p)[1]);
        inverse_error = std::max(inverse_error,std::max(angle_error(roundtrip[0],p[1]*Gen::RAD),angle_error(roundtrip[1],p[2]*Gen::RAD)));
        r.tick(true,1,1,5);
        const auto moved_inverse = inverse(wrist_xy(g.p)[0],wrist_xy(g.p)[1]);
        inverse_error = std::max(inverse_error,std::max(angle_error(moved_inverse[0],g.p[1]*Gen::RAD),angle_error(moved_inverse[1],g.p[2]*Gen::RAD)));
        t1_finite = t1_finite && std::isfinite(g.p[1]) && std::isfinite(g.p[2]) && g.path_speed>0;
    }
    std::cout << "T1 wrist_mm=" << wrist_error << " inverse_rad=" << inverse_error << '\n';
    result("T1", t1_finite && wrist_error <= 1e-6 && inverse_error <= 1e-9);

    bool t2 = true; double line_error = 0, invariants = 0;
    for (int mode : {1,2}) for (double speed : {30.,15.,5.}) for (double elbow : {30.,90.,150.}) {
        const auto p = seed(mode, elbow); Run r("T2",p,mode,1,speed); Gen& g=r.g;
        const auto initial = truth.forward(joints(p));
        double case_line=0,case_yaw=0,minimum_s=1;
        for (int n = 1; n <= 3501; ++n) {
            r.tick(n<=3000,mode,1,speed);
            const auto fk = truth.forward(joints(g.p));
            case_line=std::max(case_line,std::abs(fk.position[mode==1?1:0]-initial.position[mode==1?1:0])*1000);
            case_yaw=std::max(case_yaw,angle_error(yaw(fk),yaw(initial))/Gen::RAD);
            minimum_s=std::min(minimum_s,g.s);
            line_error = std::max(line_error,std::abs(fk.position[mode == 1 ? 1 : 0]
                - initial.position[mode == 1 ? 1 : 0])*1000);
            invariants = std::max(invariants,std::max({std::abs(fk.position.z()-initial.position.z())*1000,
                angle_error(yaw(fk),yaw(initial))/Gen::RAD, std::abs(pitch(fk)-pitch(initial))/Gen::RAD}));
            t2 = t2 && std::hypot(wrist_xy(g.p)[0],wrist_xy(g.p)[1])>=Gen::L1-Gen::L2
                && std::hypot(wrist_xy(g.p)[0],wrist_xy(g.p)[1])<=Gen::L1+Gen::L2 && std::memcmp(&g.p[0],&p[0],sizeof(double)) == 0
                && std::memcmp(&g.p[4],&p[4],sizeof(double)) == 0;
        }
        std::cout << "T2_CASE mode="<<mode<<" speed="<<speed<<" effective_elbow="<<elbow<<" q2="<<p[1]<<" q3="<<p[2]<<" q4="<<p[3]<<" q5="<<p[4]<<" line_mm="<<case_line<<" yaw_deg="<<case_yaw<<" damped="<<r.damped<<" minimum_s="<<minimum_s<<"\n";
    }
    std::cout << "T2 line_mm=" << line_error << " invariant_max=" << invariants << '\n';
    result("T2",t2 && line_error<=0.01 && invariants<=1e-6);

    bool t10=true;
    for(int mode=3;mode<=4;++mode){
        const double speed=mode==3?3:1.2; const auto p=seed(mode,90);
        Run r("T10",p,mode,1,speed); Gen& g=r.g; const auto before=truth.forward(joints(p));
        for(int n=0;n<2000;++n)r.tick(true,mode,1,speed);
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


    std::cout << "E11 cycles=" << cycles << " ST_max_difference=" << st_difference << " max_acc=" << max_acceleration << " failures=" << failures << "\n";
    result("E11_ST_EQ",st_difference==0&&discrete);
    return failures?1:0;
}
