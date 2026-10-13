// 直接调用生产网关；截获发送函数，不创建套接字、不连接 PLC。
#include <winsock2.h>
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
#define private public
#include "../remote_gateway.h"
#undef private

static std::string sent;
static int capture_send(SOCKET, const char* data, int len, int) {
    sent.append(data, len);
    return len;
}
#define send capture_send
#include "../remote_gateway.cpp"
#undef send

static void check(bool ok, const char* label) {
    std::cout << (ok ? "PASS: " : "FAIL: ") << label << std::endl;
    if (!ok) std::exit(1);
}

int main() {
    RemoteGateway gateway;
    gateway.lease_held_ = true;
    VisState state{};
    RemoteExtraState extra{};
    extra.initial_sync_done = true;
    std::vector<RemoteGateway::Pending> pending;
    RemoteJsonObject fields;
    fields["catheter_mm"].type = RemoteJsonValue::Number;
    fields["catheter_mm"].n = 40;
    fields["wire_mm"].type = RemoteJsonValue::Number;
    fields["wire_mm"].n = 500;
    auto command = [&](const char* name) {
        sent.clear(); pending.clear(); gateway.cmd_q_.clear();
        gateway.publish_state(state, extra);
        gateway.handle_command(0, 1, name, fields, pending);
    };
    state.ads_state = 4;
    state.self_check_done = false; // 断线时通信层清空缓存
    extra.ads_soft_hold = true;
    command("start_control");
    check(sent.find("进入器械准备位置") == std::string::npos &&
          sent.find("ADS") != std::string::npos && gateway.cmd_q_.empty(),
          "outage is reported as communication recovery, never a prepare-position instruction");
    command("prepare_position");
    check(sent.find("ADS") != std::string::npos && gateway.cmd_q_.empty(),
          "prepare is blocked while ADS is unavailable");
    state.ads_state = 2;
    state.self_check_done = true;
    command("start_control");
    check(gateway.cmd_q_.empty(), "running transport alone cannot bypass baseline rebuilding");
    extra.ads_soft_hold = false;
    command("start_control");
    check(gateway.cmd_q_.size() == 1 && gateway.cmd_q_.front().type == VisCommandType::SelectDirectControl &&
          sent.find("accepted") != std::string::npos,
          "recovered self-checked PLC can resume directly without preparation");
    command("prepare_position");
    check(gateway.cmd_q_.empty() && sent.find("开始控制") != std::string::npos,
          "already-prepared PLC points operator to start control");
    // 已移除的旧刷新命令不能重新建基准或排入执行队列。
    fields["success_mask"].type = RemoteJsonValue::Number;
    fields["success_mask"].n = 3;
    command("refresh_handles");
    check(gateway.cmd_q_.empty() && pending.empty() &&
          sent.find("rejected") != std::string::npos && sent.find("未知命令") != std::string::npos,
          "removed handle refresh is rejected without queued commands");
    state.self_check_done = false;
    state.selfcheck_status = 1;
    command("start_control");
    check(gateway.cmd_q_.empty() && sent.find("进入器械准备位置") != std::string::npos,
          "confirmed incomplete self-check still requires preparation");
    command("prepare_position");
    check(gateway.cmd_q_.size() == 5 && gateway.cmd_q_.back().type == VisCommandType::StartSelfCheck,
          "confirmed incomplete self-check permits prepare command");
    state.self_check_done = true;
    extra.return_ads_fault_hold = true;
    command("start_control");
    check(gateway.cmd_q_.empty() && sent.find("回退故障已锁止") != std::string::npos,
          "latched return fault explains the lock instead of suggesting preparation");
    gateway.lease_held_ = false;
    command("start_control");
    check(gateway.cmd_q_.empty() && sent.find("未持有控制权") != std::string::npos,
          "recovery never bypasses control ownership");
}
