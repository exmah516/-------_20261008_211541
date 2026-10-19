#include "hik_camera_relay.h"
#include <memory>

// ScreenCut 在进程内持有转发实例，关闭窗口时先停止线程再释放。
extern "C" __declspec(dllexport) void* __cdecl HikRelayStart(
    const char* listen_ip, std::uint16_t listen_port, const char* master_ip,
    const char* camera_ip, std::uint16_t camera_port, const char* camera_local_ip)
{
    try
    {
        if (!listen_ip || !master_ip || !camera_ip || !camera_local_ip) return nullptr;
        HikCameraRelayConfig cfg;
        cfg.listen_ip = listen_ip;
        cfg.listen_port = listen_port;
        cfg.master_ip = master_ip;
        cfg.camera_ip = camera_ip;
        cfg.camera_port = camera_port;
        cfg.camera_local_ip = camera_local_ip;
        auto relay = std::make_unique<HikCameraRelay>();
        if (!relay->start(cfg)) return nullptr;
        return relay.release();
    }
    catch (...) { return nullptr; }
}

extern "C" __declspec(dllexport) void __cdecl HikRelayStop(void* relay)
{
    delete static_cast<HikCameraRelay*>(relay);
}
