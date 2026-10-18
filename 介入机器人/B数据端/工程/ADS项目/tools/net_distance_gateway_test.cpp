// 截获生产网关的网络输出，验证认证协商及状态编码，不连接设备。
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <iostream>
#include <cassert>
#define private public
#include "../remote_gateway.h"
#undef private

static std::string tcp_sent;
static std::vector<unsigned char> udp_sent;
static int capture_send(SOCKET, const char* data, int len, int) {
    tcp_sent.append(data, len);
    return len;
}
static int capture_sendto(SOCKET, const char* data, int len, int, const sockaddr*, int) {
    udp_sent.assign(data, data + len);
    return len;
}
#define send capture_send
#define sendto capture_sendto
#include "../remote_gateway.cpp"
#undef send
#undef sendto

int main()
{
    RemoteGateway gateway;
    BCRYPT_ALG_HANDLE alg = nullptr;
    assert(BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM,
        nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG)));
    gateway.bcrypt_alg_ = alg;
    gateway.token_.assign(32, 'x');
    unsigned char nc[16] = {}, ns[16] = {};
    std::vector<RemoteGateway::Pending> pending;
    auto authenticate = [&](const std::string& extension) {
        RemoteGateway::AuthState auth = RemoteGateway::AuthState::Hello;
        assert(gateway.handle_message(0,
            "{\"t\":\"hello\",\"proto\":1,\"nonce_c\":\"00000000000000000000000000000000\"}",
            auth, nc, ns, pending));
        unsigned char message[36] = { 'a', 'u', 't', 'h' }, mac[32];
        std::memcpy(message + 4, nc, 16); std::memcpy(message + 20, ns, 16);
        assert(gateway.hmac_sha256(gateway.token_.data(), gateway.token_.size(), message, sizeof(message), mac));
        assert(gateway.handle_message(0, "{\"t\":\"auth\",\"mac\":\"" + hex_encode(mac, 32) +
            "\"" + extension + "}", auth, nc, ns, pending));
        assert(auth == RemoteGateway::AuthState::Authed);
        gateway.peer_valid_ = true;
    };
    auto publish = [&]() {
        VisState state{};
        RemoteExtraState extra{};
        state.axis1_reverse = true;
        state.axis_pos[0] = 23.5;
        extra.axis1_net_distance_mm = 12.25;
        extra.axis6_net_distance_mm = -3.125;
        gateway.publish_state(state, extra);
        gateway.tick_udp(0);
        unsigned char mac[32];
        const size_t body = udp_sent.size() - rp::kMacLen;
        assert(gateway.hmac_sha256(gateway.session_key_, 16, udp_sent.data(), body, mac));
        assert(std::memcmp(mac, udp_sent.data() + body, rp::kMacLen) == 0);
        rp::StatusPayload status{};
        std::memcpy(&status, udp_sent.data() + rp::kHeaderLen, sizeof(status));
        assert(status.axis_pos[0] == 23.5 && (status.flags & rp::kFlagGlobalReverse));
    };
    authenticate(",\"net_distance\":true");
    assert(gateway.net_distance_enabled_);
    publish();
    assert(udp_sent.size() == rp::kStatusDistanceFrameLen);
    rp::NetDistancePayload distance{};
    std::memcpy(&distance, udp_sent.data() + rp::kHeaderLen + sizeof(rp::StatusPayload), sizeof(distance));
    assert(distance.axis1_mm == 12.25 && distance.axis6_mm == -3.125);
    authenticate("");
    assert(!gateway.net_distance_enabled_);
    publish();
    assert(udp_sent.size() == rp::kStatusFrameLen);
    std::cout << "PASS: optional authenticated telemetry, signed doubles, legacy length, MAC\n";
    gateway.bcrypt_alg_ = nullptr;
    BCryptCloseAlgorithmProvider(alg, 0);
}
