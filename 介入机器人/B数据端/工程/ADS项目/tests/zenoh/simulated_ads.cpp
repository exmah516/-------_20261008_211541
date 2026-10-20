#include "../../zenoh_service.h"
#include <atomic>
#include <iostream>
#include <string>
#include <thread>

namespace
{
    std::string utf8(const wchar_t* text)
    {
        const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        std::string output(length, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, -1, output.data(), length, nullptr, nullptr);
        output.pop_back();
        return output;
    }
    void simulate_command(VisState& state, RemoteExtraState& extra, const VisCommand& command)
    {
        switch (command.type)
        {
        case VisCommandType::SetSelfCheckAxisPos:
            if (command.param1 >= 1 && command.param1 <= 7) state.axis_pos[command.param1 - 1] = command.param2 / 100.0;
            break;
        case VisCommandType::StartSelfCheck:
            state.self_check_done = true; state.selfcheck_status = 2; state.startup_completed = true; break;
        case VisCommandType::SelectDirectControl: state.control_active = true; break;
        case VisCommandType::ZeroForceSensor: ++extra.force_zero_ok_count; state.cal_zeroed = true; break;
        case VisCommandType::ToggleForceFeedback: state.ff_enabled = !state.ff_enabled; break;
        case VisCommandType::SetCylinderManualPosition:
            if (command.param1 >= 0 && command.param1 < 4)
            {
                state.cylinder_manual_mask |= static_cast<std::uint8_t>(1u << command.param1);
                state.cylinder_cmd[command.param1] = static_cast<unsigned short>(command.param2);
            }
            break;
        case VisCommandType::ResetCylinderManual:
            if (command.param1 >= 0 && command.param1 < 4) state.cylinder_manual_mask &= static_cast<std::uint8_t>(~(1u << command.param1));
            break;
        case VisCommandType::SetYValveOpen: extra.y_valve_open = command.param1 != 0; break;
        case VisCommandType::SetArmManualEnable: state.arm_manual_enable = command.param1 != 0; break;
        case VisCommandType::SetArmAxisEnable:
            if (command.param1 >= 1 && command.param1 <= 5) state.arm_enable_req[command.param1 - 1] = command.param2 != 0;
            break;
        case VisCommandType::SetInjectorManualJog:
            if (command.param1 >= 1 && command.param1 <= 2) extra.injector_dir[command.param1 - 1] = static_cast<std::int8_t>(command.param2);
            break;
        case VisCommandType::SetAxis4ManualJog: extra.axis4_dir = static_cast<std::int8_t>(command.param1); break;
        case VisCommandType::SetArmCartesianJog: state.arm_cartesian_status = command.param2 == 0 ? 0 : 1; break;
        case VisCommandType::StopArmCartesian: state.arm_cartesian_status = 0; break;
        default: break;
        }
    }
}

int wmain(int argc, wchar_t** argv)
{
    // 此程序只驱动内存快照，未链接 TcAdsDll，也不会连接 PLC 或手柄 SDK。
    ZenohServiceConfig config;
    config.endpoint = "tcp/127.0.0.1:17447";
    for (int i = 1; i + 1 < argc; i += 2)
    {
        const std::wstring argument(argv[i]);
        if (argument == L"--endpoint") config.endpoint = utf8(argv[i + 1]);
        else if (argument == L"--token") config.token_path = utf8(argv[i + 1]);
        else if (argument == L"--router") config.router_path = utf8(argv[i + 1]);
        else { std::cerr << "Unknown argument\n"; return 2; }
    }
    ZenohService service;
    if (!service.start(config)) { std::cerr << "SIM_START_FAILED\n"; return 1; }
    std::atomic<bool> paused{false}, stopped{false};
    std::thread control([&]
    {
        VisState state{};
        state.ads_state = 2; state.ads_actual_hz = 100; state.selfcheck_status = 1;
        state.cylinder_manual_allowed = true; state.arm_snapshot_valid = true;
        state.clean_force_valid = true; state.clean_force_n = .3;
        state.force_582_f = .25; state.force_582_n = .01; state.force_587_f = .5; state.force_587_n = .02;
        RemoteExtraState extra;
        extra.initial_sync_done = true;
        while (!stopped)
        {
            VisCommand command;
            for (int budget = 0; budget < 64 && service.poll_command(command); ++budget)
            {
                simulate_command(state, extra, command);
                service.command_processed();
            }
            remote_handle_bridge::set_output(0, {true, 1, .5, .01});
            remote_handle_bridge::set_output(1, {true, 2, .25, .02});
            if (!paused) service.publish_state(state, extra);
            Sleep(10);
        }
    });
    std::cout << "SIM_READY\n" << std::flush;
    std::string input;
    while (std::getline(std::cin, input))
    {
        if (input == "pause") { paused = true; std::cout << "SIM_PAUSED\n" << std::flush; }
        else if (input == "resume") { paused = false; std::cout << "SIM_RESUMED\n" << std::flush; }
        else if (input == "quit") break;
    }
    stopped = true;
    control.join();
    service.stop();
    std::cout << "SIM_STOPPED\n" << std::flush;
    return 0;
}

