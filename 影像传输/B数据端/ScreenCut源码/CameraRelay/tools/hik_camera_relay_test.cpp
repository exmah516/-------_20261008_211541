// 使用迁入影像数据端的真实转发实现和回环替身；默认不连接摄像头、PLC 或手柄。
#include <winsock2.h>
#include <ws2tcpip.h>
#include "hik_camera_relay.h"
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

void require(bool ok, const char* message)
{
	if (!ok) throw std::runtime_error(message);
}

struct Socket
{
	SOCKET value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	Socket() { require(value != INVALID_SOCKET, "socket"); }
	~Socket() { if (value != INVALID_SOCKET) closesocket(value); }
	Socket(const Socket&) = delete;
	Socket& operator=(const Socket&) = delete;
};

std::uint16_t listen_local(SOCKET s)
{
	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	require(bind(s, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind");
	require(listen(s, 16) == 0, "listen");
	int length = sizeof(address);
	require(getsockname(s, reinterpret_cast<sockaddr*>(&address), &length) == 0, "getsockname");
	return ntohs(address.sin_port);
}

std::uint16_t free_port()
{
	Socket socket;
	return listen_local(socket.value);
}

void connect_local(SOCKET socket, std::uint16_t port)
{
	DWORD timeout = 5000;
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
	setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port = htons(port);
	require(connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "connect");
}

void send_all(SOCKET socket, const char* data, size_t size)
{
	while (size)
	{
		int count = send(socket, data, static_cast<int>(size > 4093 ? 4093 : size), 0);
		require(count > 0, "send");
		data += count; size -= count;
	}
}

class FakeCamera
{
public:
	Socket server;
	std::uint16_t port = listen_local(server.value);
	std::atomic<int> accepted{ 0 };
	std::atomic<bool> stopping{ false };
	std::future<void> worker;
	FakeCamera()
	{
		worker = std::async(std::launch::async, [this] {
			std::vector<std::future<void>> jobs;
			while (!stopping)
			{
				fd_set read{}; FD_SET(server.value, &read);
				timeval wait{ 0, 20000 };
				if (select(0, &read, nullptr, nullptr, &wait) <= 0) continue;
				sockaddr_in peer{}; int length = sizeof(peer);
				SOCKET client = accept(server.value, reinterpret_cast<sockaddr*>(&peer), &length);
				if (client == INVALID_SOCKET) continue;
				++accepted;
				jobs.push_back(std::async(std::launch::async, [client, peer] {
					Socket owned; closesocket(owned.value); owned.value = client;
					require(peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK), "wrong outgoing source");
					DWORD timeout = 5000;
					setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
					setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
					char data[7919];
					while (true)
					{
						int count = recv(client, data, sizeof(data), 0);
						if (count <= 0) break;
						send_all(client, data, count);
					}
					shutdown(client, SD_SEND);
				}));
			}
			for (auto& job : jobs) job.get();
		});
	}
	~FakeCamera() { stopping = true; if (worker.valid()) worker.wait(); }
};

void roundtrip(std::uint16_t port, int seed)
{
	Socket socket;
	connect_local(socket.value, port);
	int receive_buffer_bytes = 2048;
	setsockopt(socket.value, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&receive_buffer_bytes), sizeof(receive_buffer_bytes));
	std::vector<char> sent(1024 * 1024 + 137), received(sent.size());
	for (size_t i = 0; i < sent.size(); ++i) sent[i] = static_cast<char>((i * 31 + seed) & 255);
	auto sending = std::async(std::launch::async, [&] {
		send_all(socket.value, sent.data(), sent.size());
		shutdown(socket.value, SD_SEND);
	});
	// 延迟读取迫使转发处理背压和部分发送，数据必须逐字节一致。
	Sleep(100);
	size_t count = 0;
	while (count < received.size())
	{
		int got = recv(socket.value, received.data() + count, static_cast<int>(received.size() - count), 0);
		require(got > 0, "truncated relay data");
		count += got;
	}
	sending.get();
	require(sent == received, "relay changed bytes");
	char tail;
	require(recv(socket.value, &tail, 1, 0) == 0, "half-close not propagated");
}

void expect_closed(std::uint16_t port)
{
	Socket socket;
	connect_local(socket.value, port);
	char data;
	int got = recv(socket.value, &data, 1, 0);
	int error = got == SOCKET_ERROR ? WSAGetLastError() : 0;
	require(got == 0 || (got == SOCKET_ERROR && error == WSAECONNRESET), "connection was not rejected promptly");
}

int main(int argc, char* argv[])
{
	SetConsoleOutputCP(CP_UTF8);
	WSADATA wsa{};
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
	try
	{
		HikCameraRelayConfig cfg;
		cfg.listen_ip = cfg.master_ip = cfg.camera_local_ip = cfg.camera_ip = "127.0.0.1";
		HikCameraRelay relay;
		if (argc == 3 && std::string(argv[1]) == "--camera-probe")
		{
			// 显式人工取流诊断，只运行真实转发模块，不载入 ADS/手柄代码。
			cfg.camera_local_ip = argv[2];
			cfg.camera_ip = "192.168.1.64";
			require(relay.start(cfg), "camera probe start");
			Sleep(30000);
			relay.stop();
			WSACleanup();
			return 0;
		}
		FakeCamera camera;
		cfg.camera_port = camera.port;
		cfg.listen_port = free_port();
		require(relay.start(cfg), "relay start");
		std::vector<std::future<void>> clients;
		for (int i = 0; i < 3; ++i) clients.push_back(std::async(std::launch::async, [cfg, i] { roundtrip(cfg.listen_port, i); }));
		for (auto& client : clients) client.get();
		require(camera.accepted == 3, "SDK parallel connection count");
		std::cout << "PASS: 3 concurrent SDK-like connections, >3 MiB exact bytes, backpressure, half-close, source binding\n";

		relay.stop();
		relay.stop();
		cfg.listen_port = free_port();
		require(relay.start(cfg), "restart");
		roundtrip(cfg.listen_port, 7);
		std::cout << "PASS: stop is idempotent and restart works\n";
		relay.stop();

		cfg.listen_port = camera.port;
		require(!relay.start(cfg), "occupied listener must fail");
		cfg.listen_port = free_port();
		cfg.master_ip = "127.0.0.2";
		require(relay.start(cfg), "denied peer listener");
		int before = camera.accepted;
		expect_closed(cfg.listen_port);
		require(camera.accepted == before, "unauthorized peer reached camera");
		relay.stop();
		std::cout << "PASS: occupied port and unauthorized master rejected\n";

		cfg.listen_port = free_port();
		cfg.master_ip = "127.0.0.1";
		cfg.camera_local_ip = "192.0.2.96";
		require(relay.start(cfg), "missing USB listener");
		expect_closed(cfg.listen_port);
		require(camera.accepted == before, "missing USB used fallback interface");
		relay.stop();
		cfg.listen_port = free_port();
		cfg.camera_local_ip = "127.0.0.1";
		cfg.camera_port = free_port();
		require(relay.start(cfg), "offline camera listener");
		expect_closed(cfg.listen_port);
		relay.stop();
		std::cout << "PASS: missing USB has no fallback; offline camera does not block\n";

		cfg.listen_port = free_port();
		cfg.camera_port = camera.port;
		require(relay.start(cfg), "bounded connections listener");
		std::vector<std::unique_ptr<Socket>> idle;
		before = camera.accepted;
		for (int i = 0; i < 8; ++i)
		{
			idle.emplace_back(new Socket);
			connect_local(idle.back()->value, cfg.listen_port);
		}
		auto deadline = GetTickCount64() + 3000;
		while (camera.accepted < before + 8 && GetTickCount64() < deadline) Sleep(10);
		require(camera.accepted == before + 8, "8 connection bound");
		expect_closed(cfg.listen_port);
		auto started = GetTickCount64();
		relay.stop();
		require(GetTickCount64() - started < 1500, "stop blocked on live sockets");
		std::cout << "PASS: ninth connection rejected; active sockets stop within 1.5 seconds\n";
		cfg.listen_ip = "bad-ip";
		require(!relay.start(cfg), "invalid IP accepted");
		idle.clear();
		camera.stopping = true;
		camera.worker.get();
		std::cout << "ALL CAMERA RELAY CHECKS PASSED (loopback only)\n";
	}
	catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << std::endl; WSACleanup(); return 1; }
	WSACleanup();
	return 0;
}
