#include "arm_ads_mock.h"
#include "zenoh_service.h"
#include <atomic>
#include <filesystem>

int wmain(int, wchar_t** argv)
{
    ZenohServiceConfig config;
    config.endpoint = "tcp/127.0.0.1:17448";
    config.token_path = std::filesystem::path(argv[1]).u8string();
    config.router_path = std::filesystem::path(argv[2]).u8string();
    CADSComm transport;
    AdsCommunicationService ads(transport);
    ArmManualAdsService arm(ads);
    for (int i=1;i<=5;++i) arm.set_axis_enable(i,true);
    arm.start();
    ZenohService zenoh;
    if (!zenoh.start(config)) return 1;
    std::atomic<bool> stopped{false};
    std::atomic<int> next_case{-1};
    std::thread control([&] {
        VisState state{};
        state.ads_state=2; state.ads_actual_hz=100;
        state.arm_snapshot_valid=true; state.self_check_done=true;
        state.startup_completed=true; state.control_active=true;
        RemoteExtraState extra; extra.initial_sync_done=true;
        int scenario=-1;
        bool announced=false;
        auto configured=std::chrono::steady_clock::now(), moving=configured;
        std::uint32_t moving_sequence=0;
        while (!stopped) {
            const auto now=std::chrono::steady_clock::now();
            const int requested=next_case.exchange(-1);
            if (requested>=0) {
                arm.stop_cartesian();
                std::lock_guard<std::mutex> lock(fake_mutex);
                scenario=requested; announced=false; configured=now;
                actual.fill(0); actual[0]=100;
                actual[4]=scenario==1?0:scenario==2?61:2;
                plc_owned=false; plc_state=0; plc_error=0; cjog_owned=scenario==4;
            }
            VisCommand command;
            while (zenoh.poll_command(command)) {
                switch (command.type) {
                case VisCommandType::ReturnArmProgramZero:
                    arm.request_program_zero();
                    std::cout<<"HOME_DISPATCH case="<<scenario<<'\n'<<std::flush;
                    break;
                case VisCommandType::KeepArmCartesianAlive: arm.keep_cartesian_alive(); break;
                case VisCommandType::StopArmCartesian: arm.stop_cartesian(); break;
                default: break;
                }
                zenoh.command_processed();
            }
            {
                std::lock_guard<std::mutex> lock(fake_mutex);
                if (plc_owned && acknowledged!=moving_sequence) {
                    moving_sequence=acknowledged; moving=now;
                    std::cout<<"HOME_COMMIT case="<<scenario<<" format="<<executing_curve.values[16]
                        <<" target="<<executing_curve.values[5]<<','<<executing_curve.values[6]<<','
                        <<executing_curve.values[7]<<','<<executing_curve.values[8]<<','<<executing_curve.values[9]
                        <<'\n'<<std::flush;
                }
                if (plc_owned && now-moving>std::chrono::milliseconds(300)) {
                    // 仅模拟PLC的完成/拒绝反馈；曲线由生产规划器生成并通过真实ADS服务提交。
                    plc_owned=false;
                    if (scenario==3) {plc_state=4;plc_error=2003;}
                    else {
                        for(int i=0;i<5;++i)actual[i]=executing_curve.values[5+i];
                        plc_state=2;
                    }
                }
            }
            const auto snapshot=arm.snapshot();
            if (scenario>=0 && !announced && snapshot.valid && now-configured>std::chrono::milliseconds(200)) {
                announced=true;
                std::cout<<"CASE_READY "<<scenario<<'\n'<<std::flush;
            }
            state.arm_cartesian_status=snapshot.cartesian_status;
            state.arm_cartesian_error=snapshot.cartesian_error;
            zenoh.publish_state(state,extra);
            Sleep(10);
        }
    });
    std::cout<<"HOME_SIM_READY\n"<<std::flush;
    std::string input;
    while (std::getline(std::cin,input)) {
        if (input=="quit") break;
        next_case=std::stoi(input);
    }
    stopped=true; control.join(); arm.stop(); zenoh.stop();
    return 0;
}
