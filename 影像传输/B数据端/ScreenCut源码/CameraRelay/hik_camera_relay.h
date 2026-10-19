#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

struct HikCameraRelayConfig
{
	std::string listen_ip = "192.168.50.2";
	std::uint16_t listen_port = 18000;
	std::string master_ip = "192.168.50.1";
	std::string camera_ip = "192.168.1.64";
	std::uint16_t camera_port = 8000;
	std::string camera_local_ip = "192.168.1.96";
};

// 仅转发海康 TCP 字节流，由影像数据端承载，不调用 PLC。
class HikCameraRelay
{
public:
	HikCameraRelay() = default;
	~HikCameraRelay() { stop(); }
	HikCameraRelay(const HikCameraRelay&) = delete;
	HikCameraRelay& operator=(const HikCameraRelay&) = delete;
	bool start(const HikCameraRelayConfig& config = {});
	void stop();

private:
	void run();
	HikCameraRelayConfig config_;
	std::atomic<bool> stop_{ true };
	std::thread worker_;
	std::uintptr_t listener_ = ~std::uintptr_t(0);
	bool wsa_started_ = false;
};
