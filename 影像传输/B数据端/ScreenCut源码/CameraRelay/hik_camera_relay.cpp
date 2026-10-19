#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "hik_camera_relay.h"

#include <array>
#include <iostream>
#include <list>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

namespace
{
	constexpr size_t kMaxConnections = 8; // 一路海康预览会同时建立登录、控制及媒体连接。
	constexpr ULONGLONG kConnectTimeoutMs = 3000;
	constexpr ULONGLONG kStallTimeoutMs = 30000;

	struct Buffer
	{
		std::array<char, 65536> data;
		int offset = 0, size = 0;
		bool eof = false, shutdown_sent = false;
		ULONGLONG progress_ms = 0;
	};

	struct Connection
	{
		SOCKET master = INVALID_SOCKET, camera = INVALID_SOCKET;
		bool connecting = true;
		ULONGLONG started_ms = GetTickCount64();
		Buffer to_camera, to_master;
		~Connection()
		{
			if (master != INVALID_SOCKET) closesocket(master);
			if (camera != INVALID_SOCKET) closesocket(camera);
		}
	};

	bool address(const std::string& ip, std::uint16_t port, sockaddr_in& out)
	{
		out = {};
		out.sin_family = AF_INET;
		out.sin_port = htons(port);
		return InetPtonA(AF_INET, ip.c_str(), &out.sin_addr) == 1;
	}

	bool nonblocking(SOCKET socket)
	{
		u_long enabled = 1;
		return ioctlsocket(socket, FIONBIO, &enabled) == 0;
	}

	bool bind_camera_interface(SOCKET socket, const sockaddr_in& local)
	{
		ULONG bytes = 0;
		DWORD result = GetIpAddrTable(nullptr, &bytes, FALSE);
		if (result != ERROR_INSUFFICIENT_BUFFER) { WSASetLastError(static_cast<int>(result)); return false; }
		std::vector<unsigned char> storage(bytes);
		auto table = reinterpret_cast<MIB_IPADDRTABLE*>(storage.data());
		result = GetIpAddrTable(table, &bytes, FALSE);
		if (result != NO_ERROR) { WSASetLastError(static_cast<int>(result)); return false; }
		for (DWORD i = 0; i < table->dwNumEntries; ++i)
		{
			if (table->table[i].dwAddr != local.sin_addr.s_addr) continue;
			// 两张网卡同属 192.168.1.x 时，仅约束此连接的出口，不改系统路由或 PLC 网卡。
			DWORD index = htonl(table->table[i].dwIndex);
			return setsockopt(socket, IPPROTO_IP, IP_UNICAST_IF,
				reinterpret_cast<const char*>(&index), sizeof(index)) == 0 &&
				bind(socket, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) == 0;
		}
		WSASetLastError(WSAEADDRNOTAVAIL);
		return false;
	}

	bool receive(SOCKET socket, Buffer& buffer)
	{
		int count = recv(socket, buffer.data.data(), static_cast<int>(buffer.data.size()), 0);
		if (count == SOCKET_ERROR) return WSAGetLastError() == WSAEWOULDBLOCK;
		if (count == 0) buffer.eof = true;
		else { buffer.offset = 0; buffer.size = count; buffer.progress_ms = GetTickCount64(); }
		return true;
	}

	bool send_pending(SOCKET socket, Buffer& buffer)
	{
		int count = send(socket, buffer.data.data() + buffer.offset, buffer.size - buffer.offset, 0);
		if (count == SOCKET_ERROR) return WSAGetLastError() == WSAEWOULDBLOCK;
		if (count == 0) return false;
		buffer.offset += count;
		buffer.progress_ms = GetTickCount64();
		if (buffer.offset == buffer.size) buffer.offset = buffer.size = 0;
		return true;
	}

	void finish_direction(SOCKET destination, Buffer& buffer)
	{
		if (buffer.eof && buffer.size == 0 && !buffer.shutdown_sent)
		{
			shutdown(destination, SD_SEND);
			buffer.shutdown_sent = true;
		}
	}
}

bool HikCameraRelay::start(const HikCameraRelayConfig& config)
{
	stop();
	config_ = config;
	sockaddr_in listen_address{}, target{}, source{}, peer{};
	if (!address(config.listen_ip, config.listen_port, listen_address) ||
		!address(config.camera_ip, config.camera_port, target) ||
		!address(config.camera_local_ip, 0, source) || !address(config.master_ip, 0, peer) ||
		!config.listen_port || !config.camera_port || !listen_address.sin_addr.s_addr ||
		!target.sin_addr.s_addr || !source.sin_addr.s_addr || !peer.sin_addr.s_addr)
	{
		std::cerr << "[Camera] 转发未启动：IP 或端口无效。" << std::endl;
		return false;
	}
	WSADATA wsa{};
	int error = WSAStartup(MAKEWORD(2, 2), &wsa);
	if (error != 0) { std::cerr << "[Camera] WSAStartup失败：" << error << std::endl; return false; }
	wsa_started_ = true;
	listener_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	BOOL exclusive = TRUE;
	if (listener_ == INVALID_SOCKET ||
		setsockopt(listener_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) != 0 ||
		bind(listener_, reinterpret_cast<const sockaddr*>(&listen_address), sizeof(listen_address)) != 0 ||
		listen(listener_, static_cast<int>(kMaxConnections)) != 0 || !nonblocking(listener_))
	{
		std::cerr << "[Camera] 监听失败 " << config.listen_ip << ':' << config.listen_port
			<< "，错误码 " << WSAGetLastError() << "；截图服务继续运行。" << std::endl;
		stop();
		return false;
	}
	stop_ = false;
	try { worker_ = std::thread(&HikCameraRelay::run, this); }
	catch (const std::exception& e) { std::cerr << "[Camera] 线程启动失败：" << e.what() << std::endl; stop(); return false; }
	std::cout << "[Camera] 监听 " << config.listen_ip << ':' << config.listen_port
		<< " -> " << config.camera_ip << ':' << config.camera_port
		<< "，USB源地址 " << config.camera_local_ip << "，仅允许主端 " << config.master_ip
		<< "；等待主端取流。" << std::endl;
	return true;
}

void HikCameraRelay::stop()
{
	stop_ = true;
	if (worker_.joinable()) worker_.join();
	if (listener_ != INVALID_SOCKET) { closesocket(listener_); listener_ = INVALID_SOCKET; }
	if (wsa_started_) { WSACleanup(); wsa_started_ = false; }
}

void HikCameraRelay::run()
{
	try
	{
		sockaddr_in target{}, source{}, allowed{};
		address(config_.camera_ip, config_.camera_port, target);
		address(config_.camera_local_ip, 0, source);
		address(config_.master_ip, 0, allowed);
		std::list<Connection> connections;
		while (!stop_)
		{
			fd_set reads{}, writes{}, errors{};
			FD_SET(listener_, &reads);
			for (auto& c : connections)
			{
				if (c.connecting) { FD_SET(c.camera, &writes); FD_SET(c.camera, &errors); continue; }
				if (!c.to_camera.eof && c.to_camera.size == 0) FD_SET(c.master, &reads);
				if (!c.to_master.eof && c.to_master.size == 0) FD_SET(c.camera, &reads);
				if (c.to_camera.size != 0) FD_SET(c.camera, &writes);
				if (c.to_master.size != 0) FD_SET(c.master, &writes);
			}
			timeval wait{ 0, 100000 };
			if (select(0, &reads, &writes, &errors, &wait) == SOCKET_ERROR)
			{
				std::cerr << "[Camera] 转发等待失败：" << WSAGetLastError() << std::endl;
				break;
			}
			if (stop_) break;
			if (FD_ISSET(listener_, &reads))
			{
				sockaddr_in peer{};
				int length = sizeof(peer);
				SOCKET client = accept(listener_, reinterpret_cast<sockaddr*>(&peer), &length);
				if (client != INVALID_SOCKET)
				{
					if (peer.sin_addr.s_addr != allowed.sin_addr.s_addr || connections.size() >= kMaxConnections || !nonblocking(client))
						closesocket(client);
					else
					{
						connections.emplace_back();
						auto& c = connections.back();
						c.master = client;
						c.camera = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
						bool ok = c.camera != INVALID_SOCKET && bind_camera_interface(c.camera, source) && nonblocking(c.camera);
						if (ok)
						{
							int result = connect(c.camera, reinterpret_cast<const sockaddr*>(&target), sizeof(target));
							c.connecting = result != 0;
							ok = result == 0 || WSAGetLastError() == WSAEWOULDBLOCK;
						}
						if (!ok)
						{
							std::cerr << "[Camera] USB绑定/连接摄像头失败，错误码 " << WSAGetLastError() << std::endl;
							connections.pop_back();
						}
					}
				}
			}
			for (auto it = connections.begin(); it != connections.end();)
			{
				auto& c = *it;
				bool ok = true;
				ULONGLONG now = GetTickCount64();
				if (c.connecting)
				{
					if (FD_ISSET(c.camera, &writes) || FD_ISSET(c.camera, &errors))
					{
						int error = 0, length = sizeof(error);
						ok = getsockopt(c.camera, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) == 0 && error == 0;
						if (!ok) std::cerr << "[Camera] 连接摄像头失败：" << (error ? error : WSAGetLastError()) << std::endl;
						c.connecting = false;
					}
					else if (now - c.started_ms >= kConnectTimeoutMs)
					{
						std::cerr << "[Camera] 连接摄像头超时。" << std::endl;
						ok = false;
					}
				}
				else
				{
					if (FD_ISSET(c.master, &reads)) ok = receive(c.master, c.to_camera);
					if (ok && FD_ISSET(c.camera, &reads)) ok = receive(c.camera, c.to_master);
					if (ok && FD_ISSET(c.camera, &writes)) ok = send_pending(c.camera, c.to_camera);
					if (ok && FD_ISSET(c.master, &writes)) ok = send_pending(c.master, c.to_master);
					finish_direction(c.camera, c.to_camera);
					finish_direction(c.master, c.to_master);
					if (c.to_camera.shutdown_sent && c.to_master.shutdown_sent) ok = false;
					now = GetTickCount64();
					if ((c.to_camera.size && now - c.to_camera.progress_ms >= kStallTimeoutMs) ||
						(c.to_master.size && now - c.to_master.progress_ms >= kStallTimeoutMs)) ok = false;
				}
				if (!ok) it = connections.erase(it); else ++it;
			}
		}
	}
	catch (const std::exception& e) { std::cerr << "[Camera] 转发停止：" << e.what() << std::endl; }
	stop_ = true;
}
