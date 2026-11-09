#include "arm_ads_mock.h"

int main()
{
    try {
        const std::array<double, 5> velocity{{5,5,5,5,5}}, acceleration{{50,50,50,50,50}};
        auto plan = plan_arm_curve(actual, velocity, acceleration, 0, 0);
        check(plan.already_home && plan.error == 0, "Home recognition in PLC units");
        auto q = actual; q[4] = 2;
        plan = plan_arm_curve(q, velocity, acceleration, 0, 0);
        check(plan.error == 0 && plan.curve.values[9] == 0, "Home target uses degrees");
        check(plan.curve.values[5] == 100, "Home changed lift");
        const std::array<double, 5> home_start{{350, -30, 45, 25, -10}};
        plan = plan_arm_curve(home_start, velocity, acceleration, 0, 0);
        check(plan.error == 0 && !plan.already_home && plan.curve.values[5] == 350,
            "Home without legacy parameters rejected or moved lift");
        for (int i = 6; i <= 9; ++i)
            check(plan.curve.values[i] == 0, "Home did not return to Beckhoff zero");
        check(plan.curve.values[16] == 0, "Home must keep the quintic curve format");
        auto invalid_acceleration = acceleration; invalid_acceleration[0] = 0;
        check(plan_arm_curve(actual, velocity, invalid_acceleration, 0, 0).error == 1001,
            "Invalid home acceleration not rejected");
        CADSComm transport;
        AdsCommunicationService ads(transport);
        ArmManualAdsService service(ads);
        for (int i = 1; i <= 5; ++i) service.set_axis_enable(i, true);
        check(service.start(), "Service start");
        until([&] { return service.snapshot().valid; });
        for (double scale : { 1.0, 2.0, 2.0, 0.5, 1.0 }) {
            for (int axis = 1; axis <= 5; ++axis) {
                const int direction = axis % 2 ? 1 : -1;
                service.set_jog_direction(axis, direction, scale);
                until([&] {
                    const auto state = service.snapshot();
                    return state.cmd_dir[axis - 1] == direction
                        && std::all_of(state.jog_velocity.begin(), state.jog_velocity.end(),
                            [=](double value) { return value == 5.0 * scale; });
                });
                {
                    std::lock_guard<std::mutex> lock(fake_mutex);
                    check(started_velocity[axis - 1] == 5.0 * scale, "Jog started before the selected velocity was written");
                    check(jog_acc == acceleration && jog_dec == acceleration && jog_jerk[0] == 500,
                        "Speed selection changed acceleration/deceleration/jerk");
                }
                service.set_jog_direction(axis, 0, std::numeric_limits<double>::quiet_NaN());
                until([&] { return service.snapshot().cmd_dir[axis - 1] == 0; });
            }
        }
        for (double invalid : { 0.0, -1.0, 0.75, 3.0, std::numeric_limits<double>::quiet_NaN() })
            service.set_jog_direction(1, 1, invalid);
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        check(service.snapshot().cmd_dir[0] == 0 && service.snapshot().jog_velocity == velocity,
            "Invalid speed changed jog requests or velocity");
        service.set_jog_direction(1, 1, 0.5);
        until([&] { return service.snapshot().cmd_dir[0] == 1; });
        until([&] { return service.snapshot().cmd_dir[0] == 0; });
        service.set_jog_direction(1, 1);
        until([&] { return service.snapshot().cmd_dir[0] == 1 && service.snapshot().jog_velocity == velocity; });
        service.set_jog_direction(1, 0);
        until([&] { return service.snapshot().cmd_dir[0] == 0; });
        service.request_program_zero();
        until([&] { return service.snapshot().cartesian_status == 3; });
        check(count("G.arm_curve_request") == 0 && service.snapshot().cartesian_error == 0, "Already-home caused movement");
        std::cout << "E10 already_home status=3 error=0 requests=0\n";

        const auto cancel0 = count("G.arm_curve_cancel");
        const auto params0 = count("G.arm_jog_velocity");
        service.set_cartesian_jog(3, 3, std::numeric_limits<double>::quiet_NaN());
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex); return cjog_mode==3 && cjog_dir==1 && cjog_speed==3 && cjog_alive>0; });
        const auto first_alive = [&] { std::lock_guard<std::mutex> lock(fake_mutex); return cjog_alive; }();
        for(int n=0;n<8;++n) {
            service.set_cartesian_jog(3,3,2);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        {
            std::lock_guard<std::mutex> lock(fake_mutex);
            check(cjog_alive>=first_alive+8 && cjog_mode==3,"Continuous alive forwarding failed");
            check(jog_velocity==velocity && jog_acc==acceleration,"Cartesian jog changed joint parameters");
            for(const auto& batch:batches) if(std::find(batch.begin(),batch.end(),"G.arm_cjog_mode")!=batch.end()) {
                for(const char* name:{"G.arm_cjog_dir","G.arm_cjog_speed","G.arm_cjog_alive"})
                    check(std::find(batch.begin(),batch.end(),name)!=batch.end(),"Jog request split across sum writes");
            }
        }
        { std::lock_guard<std::mutex> lock(fake_mutex); fail_read=true; }
        for (int n=0; n<4; ++n) {
            service.set_cartesian_jog(3,3);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        {
            std::lock_guard<std::mutex> lock(fake_mutex);
            check(cjog_mode==3 && cjog_alive>=first_alive+12,"Snapshot read failure stopped jog alive forwarding");
            fail_read=false;
        }
        until([&] { return service.snapshot().valid; });
        check(count("G.arm_curve_request")==0 && count("G.arm_curve_cancel")==cancel0
            && count("G.arm_jog_velocity")==params0,"Cartesian jog touched the home curve or joint parameters");
        service.set_cartesian_jog(1,-15,0.75);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==1 && cjog_dir==-1 && cjog_speed==15; });
        service.set_cartesian_jog(2,30,3);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==2 && cjog_dir==1 && cjog_speed==30; });
        service.set_jog_direction(1,1);
        service.request_program_zero();
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        check(count("G.arm_curve_request")==0 && service.snapshot().cmd_dir[0]==0,"Active jog allowed home or single-axis jog");
        check(service.snapshot().cartesian_status==5 && service.snapshot().cartesian_error==1008,
            "Pending Cartesian jog did not report home rejection 1008");
        check(service.snapshot().cjog_state==2 && service.snapshot().cjog_limited
            && service.snapshot().cjog_path_speed==30 && service.snapshot().cjog_if_version==1,"Jog status snapshot mapping");
        service.stop_cartesian();
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==0 && cjog_dir==0 && cjog_speed==0; });
        const auto stopped_writes=count("G.arm_cjog_mode");
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        check(count("G.arm_cjog_mode")==stopped_writes && count("G.arm_curve_cancel")==cancel0,"Release continued writes or canceled home interface");
        const auto lease_start=std::chrono::steady_clock::now();
        service.set_cartesian_jog(3,2.5);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==3; });
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==0; });
        const auto lease_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-lease_start).count();
        check(lease_ms>=500,"Jog lease expired before 500 ms");
        std::cout << "ADS lease observed_ms=" << lease_ms << '\n';
        const auto expired_writes=count("G.arm_cjog_mode");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        check(count("G.arm_cjog_mode")==expired_writes,"Lease expiry continued alive writes");
        const auto before_invalid=count("G.arm_cjog_mode");
        for(int mode:{0,6})service.set_cartesian_jog(mode,1);
        for(double speed:{0.,101.,std::numeric_limits<double>::quiet_NaN()})service.set_cartesian_jog(3,speed);
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        check(count("G.arm_cjog_mode")==before_invalid,"Invalid request reached PLC");
        { std::lock_guard<std::mutex> lock(fake_mutex);cjog_owned=true; }
        until([&]{return service.snapshot().cjog_owned;});
        service.request_program_zero();
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        check(count("G.arm_curve_request")==0,"Home ignored PLC jog ownership");
        check(service.snapshot().cartesian_status==5 && service.snapshot().cartesian_error==1008,
            "PLC jog ownership did not report home rejection 1008");
        std::cout << "E10 occupied status=5 error=1008 requests=0 (request and PLC ownership)\n";
        { std::lock_guard<std::mutex> lock(fake_mutex);cjog_owned=false;actual[4]=2; }
        until([&]{return !service.snapshot().cjog_owned && service.snapshot().act_pos[4]==2;});
        service.request_program_zero();
        until([&]{service.keep_cartesian_alive();return count("G.arm_curve_request")==1;});
        check(service.snapshot().cartesian_status==1 && service.snapshot().cartesian_error==0,
            "Normal home did not report running status");
        service.set_cartesian_jog(3,3);
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        check(count("G.arm_cjog_mode")==before_invalid,"Jog interrupted home");
        {
            std::lock_guard<std::mutex> lock(fake_mutex);
            check(executing_curve.values[16]==0,"Home changed curve format");
            for(int i=0;i<5;++i)actual[i]=executing_curve.values[5+i];
            plc_state=2;plc_owned=false;
        }
        until([&]{service.keep_cartesian_alive();return service.snapshot().cartesian_status==2;});
        check(count("G.arm_curve_request")==1,"Home repeated");
        check(service.snapshot().cartesian_error==0,"Normal home completed with an error");
        std::cout << "E10 normal status=1 -> 2 error=0 curve_mode=0 requests=1\n";
        { std::lock_guard<std::mutex> lock(fake_mutex);actual[4]=61; }
        until([&]{return service.snapshot().act_pos[4]==61;});
        service.request_program_zero();
        until([&]{service.keep_cartesian_alive();return service.snapshot().cartesian_status==5;});
        check(service.snapshot().cartesian_error==1002 && count("G.arm_curve_request")==1,
            "Planning rejection did not report 1002 or submitted motion");
        std::cout << "E10 planning_failure status=5 error=1002 requests=1\n";
        { std::lock_guard<std::mutex> lock(fake_mutex);actual[4]=2; }
        until([&]{return service.snapshot().act_pos[4]==2;});
        service.request_program_zero();
        until([&]{service.keep_cartesian_alive();return count("G.arm_curve_request")==2;});
        { std::lock_guard<std::mutex> lock(fake_mutex);plc_state=4;plc_error=2003;plc_owned=false; }
        until([&]{service.keep_cartesian_alive();return service.snapshot().cartesian_status==5;});
        check(service.snapshot().cartesian_error==2003,"PLC rejection did not preserve its error code");
        std::cout << "E10 plc_rejection status=5 error=2003 requests=2\n";
        service.set_cartesian_jog(3,3);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==3; });
        service.set_axis_enable(1,false);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==0; });
        service.set_axis_enable(1,true);
        service.set_cartesian_jog(3,3);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==3; });
        service.request_reset(1);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==0; });
        service.set_cartesian_jog(3,3);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==3; });
        service.set_manual_enable(false);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==0; });
        service.set_manual_enable(true);
        service.set_cartesian_jog(3,3);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==3; });
        { std::lock_guard<std::mutex> lock(fake_mutex);host_unhealthy=true; }
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==0; });
        { std::lock_guard<std::mutex> lock(fake_mutex);host_unhealthy=false; }
        service.set_cartesian_jog(3,3);
        until([&] { std::lock_guard<std::mutex> lock(fake_mutex);return cjog_mode==3; });
        service.stop();
        {std::lock_guard<std::mutex> lock(fake_mutex);check(cjog_mode==0 && cjog_dir==0 && cjog_speed==0,"Shutdown did not clear jog");}
        std::cout<<"PASS ADS: single-axis tiers/deadman, atomic jog forwarding/alive despite snapshot read failure, speed and mode changes, release/lease, unhealthy gate, home mutual exclusion/regression, reset/disable/shutdown; no hardware.\n";
    } catch (const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';
        return 1;
    }
}
