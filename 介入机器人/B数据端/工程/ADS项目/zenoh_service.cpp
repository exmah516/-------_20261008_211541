#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#include <windows.h>
#include <mmsystem.h>

#include "zenoh_service.h"
#include "protocol/cpp/zenoh_validation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "winmm.lib")

namespace
{
    constexpr std::size_t kMaxPayloadBytes = 1024 * 1024;
    constexpr std::size_t kMaxCommandQueue = 256;
    constexpr std::size_t kMaxPendingTasks = 8;
    constexpr ULONGLONG kCommandMaxAgeMs = 2000;
    constexpr ULONGLONG kHandleReleaseMs = 3000;
    constexpr ULONGLONG kHoldRenewMs = 100;
    constexpr ULONGLONG kStateFreshMs = 500;

    template<class T>
    std::vector<std::uint8_t> encode_array(const T& value)
    {
        msgpack::sbuffer buffer;
        msgpack::pack(buffer, value);
        return std::vector<std::uint8_t>(buffer.data(), buffer.data() + buffer.size());
    }

    bool finite(const vi_protocol::HandleFrame& f)
    {
        return std::isfinite(f.joint582_0) && std::isfinite(f.joint582_1) &&
            std::isfinite(f.velocity582_0) && std::isfinite(f.velocity582_1) &&
            std::isfinite(f.joint587_0) && std::isfinite(f.joint587_1) &&
            std::isfinite(f.velocity587_0) && std::isfinite(f.velocity587_1) &&
            f.injector1_direction >= -1 && f.injector1_direction <= 1 &&
            f.injector2_direction >= -1 && f.injector2_direction <= 1 &&
            f.axis4_direction >= -1 && f.axis4_direction <= 1;
    }

    bool valid_command(const vi_protocol::CommandFrame& f)
    {
        if (!std::isfinite(f.value1) || !std::isfinite(f.value2) || f.payload.size() > 4096 ||
            f.nonce_client.size() > 64 || f.nonce_server.size() > 64 || f.proof.size() > 64 || f.auth_tag.size() > 32)
            return false;
        return f.command <= static_cast<std::uint8_t>(vi_protocol::Command::YValve) && f.command_id >= 0;
    }

    std::vector<std::uint8_t> bytes_of(const std::string& value)
    {
        return std::vector<std::uint8_t>(value.begin(), value.end());
    }

    std::wstring utf8_to_wide(const std::string& value)
    {
        if (value.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (size <= 0) return {};
        std::wstring out(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size);
        return out;
    }

    std::wstring executable_directory()
    {
        wchar_t path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return {};
        return std::filesystem::path(path).parent_path().wstring();
    }

    bool parse_tcp_endpoint(const std::string& endpoint, std::string& host, std::string& port)
    {
        constexpr char prefix[] = "tcp/";
        if (endpoint.compare(0, sizeof(prefix) - 1, prefix) != 0) return false;
        const std::size_t colon = endpoint.rfind(':');
        if (colon <= sizeof(prefix) - 1 || colon + 1 >= endpoint.size()) return false;
        host = endpoint.substr(sizeof(prefix) - 1, colon - (sizeof(prefix) - 1));
        port = endpoint.substr(colon + 1);
        if (host.find_first_of("'\"/[]\\ \t\r\n") != std::string::npos ||
            port.find_first_not_of("0123456789") != std::string::npos || port.size() > 5) return false;
        const unsigned long number = std::stoul(port);
        return number > 0 && number <= 65535;
    }

    bool endpoint_ready(const std::string& endpoint, int timeout_ms)
    {
        std::string host, port;
        if (!parse_tcp_endpoint(endpoint, host, port)) return false;
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* addresses = nullptr;
        if (getaddrinfo(host.c_str(), port.c_str(), &hints, &addresses) != 0) return false;
        bool ready = false;
        for (addrinfo* address = addresses; address && !ready; address = address->ai_next)
        {
            SOCKET socket_handle = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
            if (socket_handle == INVALID_SOCKET) continue;
            u_long nonblocking = 1;
            ioctlsocket(socket_handle, FIONBIO, &nonblocking);
            const int result = connect(socket_handle, address->ai_addr, static_cast<int>(address->ai_addrlen));
            if (result == 0) ready = true;
            else if (WSAGetLastError() == WSAEWOULDBLOCK)
            {
                fd_set write_set;
                FD_ZERO(&write_set); FD_SET(socket_handle, &write_set);
                timeval timeout{ timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
                if (select(0, nullptr, &write_set, nullptr, &timeout) > 0)
                {
                    int error = 0; int length = sizeof(error);
                    getsockopt(socket_handle, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length);
                    ready = error == 0;
                }
            }
            closesocket(socket_handle);
        }
        freeaddrinfo(addresses);
        return ready;
    }

    bool constant_time_equal(const std::uint8_t* left, const std::uint8_t* right, std::size_t size)
    {
        std::uint8_t difference = 0;
        for (std::size_t i = 0; i < size; ++i) difference |= static_cast<std::uint8_t>(left[i] ^ right[i]);
        return difference == 0;
    }

    bool valid_bool_int(std::int32_t value) { return value == 0 || value == 1; }
}

ZenohService::~ZenohService() { stop(); }

bool ZenohService::start(const ZenohServiceConfig& config)
{
    if (running_.load()) return true;
    config_ = config;
    stop_requested_.store(false);
    std::string host, port;
    if (!parse_tcp_endpoint(config_.endpoint, host, port)) return false;
    if (!std::isfinite(config_.startup_g_mm) || config_.startup_g_mm < 0.0 || config_.startup_g_mm > 100.0)
        return false;
    if (!load_token(config_.token_path)) return false;

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cout << "Zenoh: WSAStartup failed." << std::endl;
        return false;
    }
    wsa_started_ = true;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG)))
    {
        std::cout << "Zenoh: unable to initialize HMAC-SHA256." << std::endl;
        WSACleanup(); wsa_started_ = false;
        return false;
    }
    bcrypt_algorithm_ = algorithm;

    if (config_.start_router && !start_router_process())
    {
        stop();
        return false;
    }
    if (!open_zenoh() || !declare_entities())
    {
        std::cout << "Zenoh: session or topic setup failed." << std::endl;
        stop();
        return false;
    }
    stop_requested_.store(false);
    try { worker_ = std::thread(&ZenohService::worker_loop, this); }
    catch (...)
    {
        stop();
        return false;
    }
    running_.store(true);
    std::cout << "Zenoh: session ready at " << config_.endpoint << "." << std::endl;
    return true;
}

void ZenohService::stop()
{
    stop_requested_.store(true);
    if (worker_.joinable()) worker_.join();
    close_zenoh();
    stop_router_process();
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        authenticated_ = lease_held_ = false;
        session_id_ = challenge_session_id_ = 0;
        have_command_sequence_ = have_handle_sequence_ = false;
        challenge_client_nonce_.clear(); challenge_server_nonce_.clear();
        client_nonce_.clear(); server_nonce_.clear(); session_key_.clear();
        pending_tasks_.clear();
        force_zeroing_ = false;
        have_state_ = false;
    }
    remote_handle_bridge::clear_samples();
    remote_handle_bridge::clear_outputs();
    {
        std::lock_guard<std::mutex> lock(command_mutex_);
        command_queue_.clear(); control_actions_.clear(); dispatched_action_ = ControlAction();
        for (int i = 0; i < 3; ++i) { hold_dirty_[i] = false; hold_direction_[i] = 0; }
    }
    if (bcrypt_algorithm_)
    {
        BCryptCloseAlgorithmProvider(static_cast<BCRYPT_ALG_HANDLE>(bcrypt_algorithm_), 0);
        bcrypt_algorithm_ = nullptr;
    }
    if (wsa_started_)
    {
        WSACleanup();
        wsa_started_ = false;
    }
    running_.store(false);
}

bool ZenohService::load_token(const std::string& configured_path)
{
    const std::wstring app_dir = executable_directory();
    std::vector<std::filesystem::path> candidates;
    const std::wstring relative = utf8_to_wide(configured_path.empty() ? "remote.token" : configured_path);
    if (!relative.empty() && std::filesystem::path(relative).is_absolute()) candidates.emplace_back(relative);
    else if (!relative.empty()) candidates.emplace_back(std::filesystem::path(app_dir) / relative);

    std::filesystem::path parent(app_dir);
    for (int i = 0; i < 8 && !parent.empty(); ++i)
    {
        candidates.push_back(parent / L"config" / L"remote.token");
        candidates.push_back(parent / L"remote.token");
        const auto next = parent.parent_path();
        if (next == parent) break;
        parent = next;
    }

    for (const auto& path : candidates)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) continue;
        std::string raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF &&
            static_cast<unsigned char>(raw[1]) == 0xBB && static_cast<unsigned char>(raw[2]) == 0xBF) raw.erase(0, 3);
        const auto begin = raw.find_first_not_of(" \t\r\n");
        const auto end = raw.find_last_not_of(" \t\r\n");
        if (begin == std::string::npos) continue;
        raw = raw.substr(begin, end - begin + 1);
        if (raw.size() < 16 || raw.size() > 4096) continue;
        if (std::any_of(raw.begin(), raw.end(), [](unsigned char c) { return c < 0x21 || c > 0x7e; })) continue;
        token_ = bytes_of(raw);
        std::cout << "Zenoh: authentication token loaded." << std::endl;
        return true;
    }
    std::cout << "Zenoh: no valid remote.token found; service disabled." << std::endl;
    return false;
}

bool ZenohService::start_router_process()
{
    if (endpoint_ready(config_.endpoint, 150))
    {
        std::cout << "Zenoh: reusing existing router at " << config_.endpoint << "." << std::endl;
        return true;
    }
    std::filesystem::path router = config_.router_path.empty()
        ? std::filesystem::path(executable_directory()) / L"zenohd.exe"
        : std::filesystem::path(utf8_to_wide(config_.router_path));
    if (!std::filesystem::exists(router))
    {
        std::cout << "Zenoh: router executable is missing." << std::endl;
        return false;
    }
    std::wstring command = L"\"" + router.wstring() + L"\" --listen " + utf8_to_wide(config_.endpoint);
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(router.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
        nullptr, router.parent_path().c_str(), &startup, &process))
    {
        std::cout << "Zenoh: failed to start zenohd.exe (" << GetLastError() << ")." << std::endl;
        return false;
    }
    CloseHandle(process.hThread);
    router_process_ = process.hProcess;
    router_started_ = true;
    for (int i = 0; i < 40 && !endpoint_ready(config_.endpoint, 100); ++i)
    {
        if (WaitForSingleObject(router_process_, 0) == WAIT_OBJECT_0) break;
        Sleep(100);
    }
    if (!endpoint_ready(config_.endpoint, 250))
    {
        std::cout << "Zenoh: the managed router did not open " << config_.endpoint << "." << std::endl;
        stop_router_process();
        return false;
    }
    return true;
}

void ZenohService::stop_router_process()
{
    if (!router_process_) return;
    if (router_started_ && WaitForSingleObject(router_process_, 0) == WAIT_TIMEOUT)
    {
        TerminateProcess(router_process_, 0);
        WaitForSingleObject(router_process_, 2000);
    }
    CloseHandle(router_process_);
    router_process_ = nullptr;
    router_started_ = false;
}

bool ZenohService::open_zenoh()
{
    std::string endpoint = config_.endpoint;
    if (endpoint.find_first_of("'\"\\") != std::string::npos) return false;
    const std::string json = "{mode:'client',connect:{endpoints:['" + endpoint + "']}}";
    z_owned_config_t config{};
    if (zc_config_from_str(&config, json.c_str()) < 0) return false;
    if (z_open(&session_, z_move(config), nullptr) < 0) return false;
    zenoh_open_ = true;
    return true;
}

bool ZenohService::declare_entities()
{
    if (z_keyexpr_from_str(&handle_key_, vi_protocol::kHandleTopic) < 0 ||
        z_keyexpr_from_str(&command_key_, vi_protocol::kCommandTopic) < 0 ||
        z_keyexpr_from_str(&state_key_, vi_protocol::kStateTopic) < 0 ||
        z_keyexpr_from_str(&force_key_, vi_protocol::kForceTopic) < 0) return false;

    z_publisher_options_t publisher_options{};
    z_publisher_options_default(&publisher_options);
    if (z_declare_publisher(z_loan(session_), &state_publisher_, z_loan(state_key_), &publisher_options) < 0 ||
        z_declare_publisher(z_loan(session_), &force_publisher_, z_loan(force_key_), &publisher_options) < 0) return false;

    z_owned_closure_sample_t handle_closure{};
    z_closure(&handle_closure, handle_callback, nullptr, this);
    z_subscriber_options_t subscriber_options{};
    z_subscriber_options_default(&subscriber_options);
    if (z_declare_subscriber(z_loan(session_), &handle_subscriber_, z_loan(handle_key_), z_move(handle_closure), &subscriber_options) < 0)
        return false;
    z_owned_closure_sample_t command_closure{};
    z_closure(&command_closure, command_callback, nullptr, this);
    if (z_declare_subscriber(z_loan(session_), &command_subscriber_, z_loan(command_key_), z_move(command_closure), &subscriber_options) < 0)
        return false;
    return true;
}

void ZenohService::close_zenoh()
{
    if (zenoh_open_)
    {
        z_drop(z_move(handle_subscriber_));
        z_drop(z_move(command_subscriber_));
        z_drop(z_move(state_publisher_));
        z_drop(z_move(force_publisher_));
        z_drop(z_move(handle_key_)); z_drop(z_move(command_key_));
        z_drop(z_move(state_key_)); z_drop(z_move(force_key_));
        z_drop(z_move(session_));
        zenoh_open_ = false;
    }
}

void ZenohService::publish_state(const VisState& state, const RemoteExtraState& extra)
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_ = state;
    extra_ = extra;
    have_state_ = true;
    const std::uint64_t sequence = ++state_snapshot_sequence_;
    const ULONGLONG now = GetTickCount64();
    state_snapshot_tick_ms_ = now;
    for (std::size_t i = 0; i < pending_tasks_.size();)
    {
        PendingTask& task = pending_tasks_[i];
        bool finished = false, success = false;
        std::string reason;
        switch (task.kind)
        {
        case PendingKind::Prepare:
            if (state.self_check_done) { finished = success = true; }
            else if (sequence >= task.start_state_sequence + 2 && state.selfcheck_status == 4)
            { finished = true; reason = "PLC rejected the prepare-position request."; }
            else if (now >= task.deadline_ms) { finished = true; reason = "Prepare-position request timed out."; }
            break;
        case PendingKind::ForceZero:
            if (extra.force_zero_fail_count != task.zero_fail_count)
            { finished = true; reason = "Force-sensor zeroing failed."; force_zeroing_ = false; }
            else if (extra.force_zero_ok_count != task.zero_ok_count)
            {
                queue_command(VisCommand{ VisCommandType::ToggleForceFeedback }, 0, false);
                task.kind = PendingKind::ForceEnable;
                task.start_state_sequence = sequence;
                task.deadline_ms = now + 3000;
            }
            else if (now >= task.deadline_ms)
            { finished = true; reason = "Force-sensor zeroing timed out."; force_zeroing_ = false; }
            break;
        case PendingKind::ForceEnable:
            if (state.ff_enabled) { finished = success = true; force_zeroing_ = false; }
            else if (now >= task.deadline_ms)
            { finished = true; reason = "Force feedback did not turn on."; force_zeroing_ = false; }
            break;
        case PendingKind::ForceDisable:
            if (!state.ff_enabled) { finished = success = true; }
            else if (now >= task.deadline_ms)
            { finished = true; reason = "Force feedback did not turn off."; }
            break;
        case PendingKind::CylinderOn:
        case PendingKind::CylinderOff:
        {
            const bool engaged = (state.cylinder_manual_mask & (1u << task.index)) != 0;
            const bool reached = task.kind == PendingKind::CylinderOn ? engaged : !engaged;
            if (reached) { finished = success = true; }
            else if (now >= task.deadline_ms)
            { finished = true; reason = "Cylinder state did not change."; }
            break;
        }
        case PendingKind::YValve:
            if (extra.y_valve_open == !task.flag) { finished = success = true; }
            else if (now >= task.deadline_ms) { finished = true; reason = "Y-valve state did not change."; }
            break;
        case PendingKind::StartControl:
            if (state.control_active) { finished = success = true; }
            else if (now >= task.deadline_ms)
            { finished = true; reason = "Start-control request was not applied."; }
            break;
        }
        if (finished)
        {
            if (task.kind == PendingKind::Prepare) { prepare_task_state_ = success ? 2 : 3; prepare_task_command_id_ = task.command_id; }
            if (task.kind == PendingKind::StartControl) { start_task_state_ = success ? 2 : 3; start_task_command_id_ = task.command_id; }
            add_result_locked(task.command_id, success ? 0 : 1, reason);
            pending_tasks_.erase(pending_tasks_.begin() + i);
        }
        else ++i;
    }
}

bool ZenohService::poll_command(VisCommand& command)
{
    // 只有控制线程取出命令后才展开控制域动作，整批准备动作不会被 FIFO 溢出拆开。
    for (std::size_t consumed = 0; consumed < kMaxCommandQueue; ++consumed)
    {
        std::lock_guard<std::mutex> state_lock(state_mutex_);
        {
            std::lock_guard<std::mutex> queue_lock(command_mutex_);
            for (int i = 0; i < 3; ++i)
            {
                if (!hold_dirty_[i]) continue;
                hold_dirty_[i] = false;
                command = hold_actions_[i].command;
                dispatched_action_ = ControlAction();
                return true;
            }
            while (!control_actions_.empty())
            {
                auto action = std::move(control_actions_.front());
                control_actions_.pop_front();
                if (action.session_id != session_id_ || action.lease_epoch != lease_epoch_) continue;
                command = action.command;
                dispatched_action_ = std::move(action);
                return true;
            }
        }
        QueuedCommand item;
        {
            std::lock_guard<std::mutex> queue_lock(command_mutex_);
            if (command_queue_.empty()) return false;
            item = std::move(command_queue_.front());
            command_queue_.pop_front();
        }
        const auto type = static_cast<vi_protocol::Command>(item.frame.command);
        if (type == vi_protocol::Command::ArmAxisJog || type == vi_protocol::Command::ArmCartesianJog ||
            type == vi_protocol::Command::ArmCartesianAlive) item.frame.command_id = 0;
        if (item.frame.session_id != session_id_ || !authenticated_ ||
            (type != vi_protocol::Command::Acquire && item.lease_epoch != lease_epoch_))
        {
            add_result_locked(item.frame.command_id, 1, "Control session or lease changed before command consumption.");
            continue;
        }
        if (GetTickCount64() - item.queued_ms > kCommandMaxAgeMs)
        {
            add_result_locked(item.frame.command_id, 1, "Command expired before ADS consumption.");
            expired_commands_.fetch_add(1);
            continue;
        }
        consume_command_locked(item.frame);
    }
    return false;
}

void ZenohService::command_processed()
{
    std::lock_guard<std::mutex> state_lock(state_mutex_);
    std::lock_guard<std::mutex> queue_lock(command_mutex_);
    if (dispatched_action_.session_id == session_id_ && dispatched_action_.lease_epoch == lease_epoch_ &&
        dispatched_action_.has_result)
        add_result_locked(dispatched_action_.request_id, 0, "");
    dispatched_action_ = ControlAction();
}

void ZenohService::queue_command(const VisCommand& command, std::int32_t request_id, bool has_result)
{
    std::lock_guard<std::mutex> queue_lock(command_mutex_);
    control_actions_.push_back(ControlAction{ command, request_id, has_result, session_id_, lease_epoch_ });
}

void ZenohService::add_result_locked(std::int32_t command_id, std::uint8_t result, const std::string& reason)
{
    if (command_id <= 0) return;
    auto existing = std::find_if(results_.begin(), results_.end(), [command_id](const auto& item) { return item.command_id == command_id; });
    if (existing != results_.end()) results_.erase(existing);
    if (results_.size() >= vi_protocol::kMaxResults) results_.erase(results_.begin());
    results_.push_back(vi_protocol::CommandResult{ command_id, result, reason });
}

void ZenohService::handle_callback(z_loaned_sample_t* sample, void* context)
{
    try { if (context) static_cast<ZenohService*>(context)->receive_handle(sample); }
    catch (...) { /* 不允许 C++ 异常跨过原生回调边界。 */ }
}

void ZenohService::command_callback(z_loaned_sample_t* sample, void* context)
{
    try { if (context) static_cast<ZenohService*>(context)->receive_command(sample); }
    catch (...) { /* 不允许 C++ 异常跨过原生回调边界。 */ }
}

bool ZenohService::unpack_bytes(z_loaned_sample_t* sample, std::vector<std::uint8_t>& bytes)
{
    const z_loaned_bytes_t* payload = z_sample_payload(sample);
    if (!payload) return false;
    const std::size_t length = z_bytes_len(payload);
    if (length == 0 || length > kMaxPayloadBytes) return false;
    bytes.resize(length);
    z_bytes_reader_t reader = z_bytes_get_reader(payload);
    return z_bytes_reader_read(&reader, bytes.data(), length) == length;
}

bool ZenohService::decode(const std::vector<std::uint8_t>& bytes, vi_protocol::HandleFrame& frame)
{
    return vi_protocol::decode(bytes, frame) &&
        frame.version == vi_protocol::kVersion && frame.session_id != 0 && frame.sequence != 0 && finite(frame);
}

bool ZenohService::decode(const std::vector<std::uint8_t>& bytes, vi_protocol::CommandFrame& frame)
{
    return vi_protocol::decode(bytes, frame) &&
        frame.version == vi_protocol::kVersion && frame.sequence != 0 && valid_command(frame);
}

void ZenohService::receive_handle(z_loaned_sample_t* sample)
{
    std::vector<std::uint8_t> bytes;
    vi_protocol::HandleFrame frame;
    if (!unpack_bytes(sample, bytes) || !decode(bytes, frame)) return;
    on_handle(frame);
}

void ZenohService::receive_command(z_loaned_sample_t* sample)
{
    std::vector<std::uint8_t> bytes;
    vi_protocol::CommandFrame frame;
    if (!unpack_bytes(sample, bytes) || !decode(bytes, frame)) return;
    on_command(frame);
}

std::vector<std::uint8_t> ZenohService::pack(const vi_protocol::HandleFrame& frame) { return encode_array(frame); }
std::vector<std::uint8_t> ZenohService::pack(const vi_protocol::CommandFrame& frame) { return encode_array(frame); }
std::vector<std::uint8_t> ZenohService::pack(const vi_protocol::StateFrame& frame) { return encode_array(frame); }
std::vector<std::uint8_t> ZenohService::pack(const vi_protocol::ForceFrame& frame) { return encode_array(frame); }

bool ZenohService::hmac_sha256(const std::vector<std::uint8_t>& key, const std::uint8_t* data, std::size_t size, std::uint8_t out[32]) const
{
    if (!bcrypt_algorithm_ || key.empty() || size > ULONG_MAX) return false;
    BCRYPT_HASH_HANDLE hash = nullptr;
    NTSTATUS status = BCryptCreateHash(static_cast<BCRYPT_ALG_HANDLE>(bcrypt_algorithm_), &hash, nullptr, 0,
        const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0);
    if (!BCRYPT_SUCCESS(status)) return false;
    status = BCryptHashData(hash, const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0);
    if (BCRYPT_SUCCESS(status)) status = BCryptFinishHash(hash, out, 32, 0);
    BCryptDestroyHash(hash);
    return BCRYPT_SUCCESS(status);
}

std::vector<std::uint8_t> ZenohService::sign(const vi_protocol::HandleFrame& frame) const
{
    auto clean = frame; clean.auth_tag.clear();
    std::uint8_t mac[32]{};
    const auto bytes = pack(clean);
    if (!hmac_sha256(session_key_, bytes.data(), bytes.size(), mac)) return {};
    return std::vector<std::uint8_t>(mac, mac + 16);
}

std::vector<std::uint8_t> ZenohService::sign(const vi_protocol::CommandFrame& frame) const
{
    auto clean = frame; clean.auth_tag.clear();
    const auto bytes = pack(clean);
    std::uint8_t mac[32]{};
    if (!hmac_sha256(session_key_, bytes.data(), bytes.size(), mac)) return {};
    return std::vector<std::uint8_t>(mac, mac + 16);
}

bool ZenohService::verify_tag(const vi_protocol::HandleFrame& frame) const
{
    const auto expected = sign(frame);
    return frame.auth_tag.size() == 16 && expected.size() == 16 && constant_time_equal(frame.auth_tag.data(), expected.data(), 16);
}

bool ZenohService::verify_tag(const vi_protocol::CommandFrame& frame) const
{
    const auto expected = sign(frame);
    return frame.auth_tag.size() == 16 && expected.size() == 16 && constant_time_equal(frame.auth_tag.data(), expected.data(), 16);
}

void ZenohService::on_handle(const vi_protocol::HandleFrame& frame)
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!authenticated_ || !lease_held_ || frame.session_id != session_id_ || !verify_tag(frame)) return;
    if (have_handle_sequence_ && frame.sequence <= last_handle_sequence_) return;
    last_handle_sequence_ = frame.sequence;
    have_handle_sequence_ = true;
    echo_main_monotonic_ms_ = frame.monotonic_ms;
    last_handle_rx_ms_ = GetTickCount64();

    for (int slot = 0; slot < 2; ++slot)
    {
        remote_handle_bridge::Sample sample;
        if (slot == 0)
        {
            sample.valid = frame.valid582; sample.buttons = frame.buttons582;
            sample.encoders[0] = frame.encoder582_0; sample.encoders[1] = frame.encoder582_1;
            sample.joints[0] = frame.joint582_0; sample.joints[1] = frame.joint582_1;
            sample.vels[0] = frame.velocity582_0; sample.vels[1] = frame.velocity582_1;
        }
        else
        {
            sample.valid = frame.valid587; sample.buttons = frame.buttons587;
            sample.encoders[0] = frame.encoder587_0; sample.encoders[1] = frame.encoder587_1;
            sample.joints[0] = frame.joint587_0; sample.joints[1] = frame.joint587_1;
            sample.vels[0] = frame.velocity587_0; sample.vels[1] = frame.velocity587_1;
        }
        remote_handle_bridge::put_sample(slot, sample);
    }

    const int directions[3] = { frame.injector1_direction, frame.injector2_direction, frame.axis4_direction };
    std::lock_guard<std::mutex> queue_lock(command_mutex_);
    for (int i = 0; i < 3; ++i)
    {
        const ULONGLONG now = last_handle_rx_ms_;
        const int direction = directions[i];
        if (direction == hold_direction_[i] && (direction == 0 || now - hold_last_push_ms_[i] < kHoldRenewMs)) continue;
        VisCommand command;
        if (i < 2) { command.type = VisCommandType::SetInjectorManualJog; command.param1 = i + 1; command.param2 = direction; }
        else { command.type = VisCommandType::SetAxis4ManualJog; command.param1 = direction; }
        hold_actions_[i] = ControlAction{ command, 0, false, session_id_, lease_epoch_ };
        hold_dirty_[i] = true;
        hold_direction_[i] = direction; hold_last_push_ms_[i] = now;
    }
}

void ZenohService::on_command(const vi_protocol::CommandFrame& frame)
{
    const auto type = static_cast<vi_protocol::Command>(frame.command);
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (type == vi_protocol::Command::Hello)
    {
        if (frame.session_id != 0 || frame.nonce_client.size() != 16 || !frame.nonce_server.empty() ||
            !frame.proof.empty() || !frame.auth_tag.empty()) return;
        // 挑战与当前控制会话分开，未经认证的 Hello 不能撤销有效租约。
        if (challenge_session_id_ != 0 && challenge_client_nonce_ == frame.nonce_client &&
            GetTickCount64() - challenge_tick_ms_ <= 10000) return;
        std::uint64_t id = 0;
        if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&id), sizeof(id), BCRYPT_USE_SYSTEM_PREFERRED_RNG))) return;
        if (id == 0) id = 1;
        std::vector<std::uint8_t> server_nonce(16);
        if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, server_nonce.data(), static_cast<ULONG>(server_nonce.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG))) return;
        challenge_session_id_ = id; challenge_sequence_ = frame.sequence;
        challenge_client_nonce_ = frame.nonce_client; challenge_server_nonce_ = std::move(server_nonce);
        challenge_tick_ms_ = GetTickCount64();
        return;
    }
    if (type == vi_protocol::Command::Authenticate)
    {
        if (frame.session_id == 0 || frame.session_id != challenge_session_id_ ||
            frame.nonce_client != challenge_client_nonce_ || frame.nonce_server != challenge_server_nonce_ ||
            frame.proof.size() != 32 || !frame.auth_tag.empty() || frame.sequence <= challenge_sequence_ ||
            GetTickCount64() - challenge_tick_ms_ > 10000) return;
        std::vector<std::uint8_t> message = bytes_of("auth");
        message.insert(message.end(), challenge_client_nonce_.begin(), challenge_client_nonce_.end());
        message.insert(message.end(), challenge_server_nonce_.begin(), challenge_server_nonce_.end());
        std::uint8_t expected[32]{};
        if (!hmac_sha256(token_, message.data(), message.size(), expected) ||
            !constant_time_equal(expected, frame.proof.data(), 32)) return;
        std::vector<std::uint8_t> key_message = bytes_of("key");
        key_message.insert(key_message.end(), challenge_client_nonce_.begin(), challenge_client_nonce_.end());
        key_message.insert(key_message.end(), challenge_server_nonce_.begin(), challenge_server_nonce_.end());
        std::uint8_t full_key[32]{};
        if (!hmac_sha256(token_, key_message.data(), key_message.size(), full_key)) return;
        release_lease_locked("Authenticated client session changed.");
        session_id_ = challenge_session_id_; client_nonce_ = challenge_client_nonce_; server_nonce_ = challenge_server_nonce_;
        session_key_.assign(full_key, full_key + 16); authenticated_ = true;
        last_command_sequence_ = frame.sequence; have_command_sequence_ = true;
        last_handle_sequence_ = 0; have_handle_sequence_ = false;
        echo_main_monotonic_ms_ = frame.monotonic_ms;
        challenge_session_id_ = 0; challenge_client_nonce_.clear(); challenge_server_nonce_.clear();
        {
            std::lock_guard<std::mutex> queue_lock(command_mutex_);
            command_queue_.clear(); control_actions_.clear(); dispatched_action_ = ControlAction();
        }
        results_.clear(); prepare_task_state_ = start_task_state_ = 0;
        std::cout << "Zenoh: authenticated client session." << std::endl;
        return;
    }

    if (!authenticated_ || frame.session_id != session_id_ ||
        (have_command_sequence_ && frame.sequence <= last_command_sequence_) || !verify_tag(frame)) return;
    last_command_sequence_ = frame.sequence;
    have_command_sequence_ = true;
    echo_main_monotonic_ms_ = frame.monotonic_ms;

    {
        std::lock_guard<std::mutex> queue_lock(command_mutex_);
        if (command_queue_.size() >= kMaxCommandQueue)
        {
            const auto dropped = command_queue_.front().frame;
            command_queue_.pop_front();
            if (dropped.command != static_cast<std::uint8_t>(vi_protocol::Command::ArmAxisJog) &&
                dropped.command != static_cast<std::uint8_t>(vi_protocol::Command::ArmCartesianJog) &&
                dropped.command != static_cast<std::uint8_t>(vi_protocol::Command::ArmCartesianAlive))
                add_result_locked(dropped.command_id, 1, "ADS command FIFO overflow: oldest command dropped.");
            dropped_commands_.fetch_add(1);
        }
        command_queue_.push_back(QueuedCommand{ frame, GetTickCount64(), lease_epoch_ });
    }
}

void ZenohService::consume_command_locked(const vi_protocol::CommandFrame& received_frame)
{
    auto frame = received_frame;
    if (frame.command == static_cast<std::uint8_t>(vi_protocol::Command::ArmAxisJog) ||
        frame.command == static_cast<std::uint8_t>(vi_protocol::Command::ArmCartesianJog) ||
        frame.command == static_cast<std::uint8_t>(vi_protocol::Command::ArmCartesianAlive)) frame.command_id = 0;
    const auto type = static_cast<vi_protocol::Command>(frame.command);
    if (type != vi_protocol::Command::Release &&
        (!have_state_ || GetTickCount64() - state_snapshot_tick_ms_ > kStateFreshMs))
    { add_result_locked(frame.command_id, 1, "ADS state is stale."); return; }

    if (type == vi_protocol::Command::Acquire)
    {
        lease_held_ = true;
        last_handle_rx_ms_ = GetTickCount64();
        add_result_locked(frame.command_id, 0, "");
        std::cout << "Zenoh: remote control lease acquired." << std::endl;
        return;
    }
    if (type == vi_protocol::Command::Release)
    {
        release_lease_locked("Control lease was released.");
        add_result_locked(frame.command_id, 0, "");
        return;
    }
    if (!lease_held_)
    { add_result_locked(frame.command_id, 1, "Control lease is not held."); return; }
    if (pending_tasks_.size() >= kMaxPendingTasks &&
        (type == vi_protocol::Command::PreparePosition || type == vi_protocol::Command::StartControl ||
         type == vi_protocol::Command::ForceFeedback || type == vi_protocol::Command::Cylinder ||
         type == vi_protocol::Command::YValve))
    { add_result_locked(frame.command_id, 1, "A long-running command is still pending."); return; }

    const auto enqueue = [&](VisCommandType command_type, int p1 = 0, int p2 = 0, bool receipt = true, double jog_speed_scale = 1.0)
    {
        VisCommand command; command.type = command_type; command.param1 = p1; command.param2 = p2;
        command.arm_jog_speed_scale = jog_speed_scale;
        queue_command(command, receipt ? frame.command_id : 0, receipt);
    };
    const ULONGLONG now = GetTickCount64();

    switch (type)
    {
    case vi_protocol::Command::PreparePosition:
    {
        if (std::any_of(pending_tasks_.begin(), pending_tasks_.end(), [](const auto& task)
            { return task.kind == PendingKind::Prepare; }))
        { add_result_locked(frame.command_id, 1, "Prepare-position is already pending."); return; }
        const double catheter = frame.value1, wire = frame.value2;
        if (catheter < 5.0 || catheter > 95.0 || wire < 10.0 || wire > 639.0 || catheter > wire)
        { add_result_locked(frame.command_id, 1, "Prepare-position values are outside their allowed range."); return; }
        if (state_.ads_state != 2 || state_.self_check_done ||
            (state_.selfcheck_status != 1 && state_.selfcheck_status != 3))
        { add_result_locked(frame.command_id, 1, "PLC is not ready for prepare-position."); return; }
        const int p1 = static_cast<int>(std::llround(catheter * 100.0));
        const int p3 = static_cast<int>(std::llround(wire * 100.0));
        if (p3 + 500 + static_cast<int>(std::llround(config_.startup_g_mm * 100.0)) > 67000)
        { add_result_locked(frame.command_id, 1, "Prepare-position exceeds the axis6 travel limit."); return; }
        enqueue(VisCommandType::SetSelfCheckAxisPos, 1, p1, false);
        enqueue(VisCommandType::SetSelfCheckAxisPos, 3, p3, false);
        enqueue(VisCommandType::SetSelfCheckAxisPos, 5, p3 + 500, false);
        enqueue(VisCommandType::SetSelfCheckAxisPos, 6, p3 + 500 + static_cast<int>(std::llround(config_.startup_g_mm * 100.0)), false);
        enqueue(VisCommandType::StartSelfCheck, 0, 0, false);
        prepare_task_state_ = 1; prepare_task_command_id_ = frame.command_id;
        pending_tasks_.push_back(PendingTask{ PendingKind::Prepare, frame.command_id, now + 300000, state_snapshot_sequence_ });
        break;
    }
    case vi_protocol::Command::StartControl:
        if (std::any_of(pending_tasks_.begin(), pending_tasks_.end(), [](const auto& task)
            { return task.kind == PendingKind::StartControl; }))
        { add_result_locked(frame.command_id, 1, "Start-control is already pending."); return; }
        if (state_.ads_state != 2 || extra_.ads_soft_hold || !state_.self_check_done || state_.estop_hold ||
            extra_.return_ads_fault_hold || extra_.handle_soft_hold || !extra_.initial_sync_done)
        { add_result_locked(frame.command_id, 1, "Start-control safety gates are not clear."); return; }
        if (state_.control_active) { add_result_locked(frame.command_id, 0, ""); return; }
        enqueue(VisCommandType::SelectDirectControl, 0, 0, false);
        start_task_state_ = 1; start_task_command_id_ = frame.command_id;
        pending_tasks_.push_back(PendingTask{ PendingKind::StartControl, frame.command_id, now + 3000, state_snapshot_sequence_ });
        break;
    case vi_protocol::Command::ForceFeedback:
    {
        if (!valid_bool_int(frame.param1)) { add_result_locked(frame.command_id, 1, "Invalid force-feedback value."); return; }
        if (std::any_of(pending_tasks_.begin(), pending_tasks_.end(), [](const auto& task)
            { return task.kind == PendingKind::ForceZero || task.kind == PendingKind::ForceEnable || task.kind == PendingKind::ForceDisable; }))
        { add_result_locked(frame.command_id, 1, "Force-feedback transition is already pending."); return; }
        if (frame.param1 != 0)
        {
            if (state_.ff_enabled) { add_result_locked(frame.command_id, 0, ""); return; }
            enqueue(VisCommandType::ZeroForceSensor, 0, 0, false);
            force_zeroing_ = true;
            pending_tasks_.push_back(PendingTask{ PendingKind::ForceZero, frame.command_id, now + 3000,
                state_snapshot_sequence_, 0, false, extra_.force_zero_ok_count, extra_.force_zero_fail_count });
        }
        else
        {
            if (!state_.ff_enabled) { add_result_locked(frame.command_id, 0, ""); return; }
            enqueue(VisCommandType::ToggleForceFeedback, 0, 0, false);
            pending_tasks_.push_back(PendingTask{ PendingKind::ForceDisable, frame.command_id, now + 3000, state_snapshot_sequence_ });
        }
        break;
    }
    case vi_protocol::Command::Cylinder:
    {
        if (frame.param1 < 1 || frame.param1 > 4 || !valid_bool_int(frame.param2))
        { add_result_locked(frame.command_id, 1, "Invalid cylinder command."); return; }
        const int index = frame.param1 - 1;
        if (std::any_of(pending_tasks_.begin(), pending_tasks_.end(), [index](const auto& task)
            { return (task.kind == PendingKind::CylinderOn || task.kind == PendingKind::CylinderOff) && task.index == index; }))
        { add_result_locked(frame.command_id, 1, "This cylinder already has a pending transition."); return; }
        const bool engaged = (state_.cylinder_manual_mask & (1u << index)) != 0;
        const bool enable = frame.param2 != 0;
        if (engaged == enable) { add_result_locked(frame.command_id, 0, ""); return; }
        if (enable)
        {
            if (!state_.cylinder_manual_allowed) { add_result_locked(frame.command_id, 1, "Cylinder manual control is unavailable."); return; }
            enqueue(VisCommandType::SetCylinderManualPosition, index, (index == 0 || index == 2) ? 2000 : 10, false);
        }
        else enqueue(VisCommandType::ResetCylinderManual, index, 0, false);
        pending_tasks_.push_back(PendingTask{ enable ? PendingKind::CylinderOn : PendingKind::CylinderOff,
            frame.command_id, now + 3000, state_snapshot_sequence_, index });
        break;
    }
    case vi_protocol::Command::ArmManualEnable:
        if (!valid_bool_int(frame.param1)) { add_result_locked(frame.command_id, 1, "Invalid manual-enable value."); return; }
        enqueue(VisCommandType::SetArmManualEnable, frame.param1); break;
    case vi_protocol::Command::ArmAxisEnable:
        if (frame.param1 < 1 || frame.param1 > 5 || !valid_bool_int(frame.param2)) { add_result_locked(frame.command_id, 1, "Invalid positioning-arm axis enable."); return; }
        enqueue(VisCommandType::SetArmAxisEnable, frame.param1, frame.param2); break;
    case vi_protocol::Command::ArmAxisReset:
        if (frame.param1 < 1 || frame.param1 > 5) { add_result_locked(frame.command_id, 1, "Invalid positioning-arm axis."); return; }
        enqueue(VisCommandType::RequestArmAxisReset, frame.param1); break;
    case vi_protocol::Command::ArmAxisJog:
        if (frame.param1 < 1 || frame.param1 > 5 || frame.param2 < -1 || frame.param2 > 1 ||
            (frame.param2 != 0 && frame.value1 != 0 && frame.value1 != 0.5 && frame.value1 != 1.0 && frame.value1 != 2.0))
        { add_result_locked(frame.command_id, 1, "Invalid positioning-arm jog."); return; }
        // 旧主端的零倍率按中档处理；停止指令不受档位影响。
        enqueue(VisCommandType::SetArmAxisJog, frame.param1, frame.param2, false,
            frame.param2 == 0 || frame.value1 == 0 ? 1.0 : frame.value1); break;
    case vi_protocol::Command::ArmCartesianJog:
        if (frame.param1 < 1 || frame.param1 > 5 || frame.param2 < -100000 || frame.param2 > 100000) { add_result_locked(frame.command_id, 1, "Invalid Cartesian jog."); return; }
        enqueue(VisCommandType::SetArmCartesianJog, frame.param1, frame.param2, false); break;
    case vi_protocol::Command::ArmStop:
        enqueue(VisCommandType::StopArmCartesian, 0, 0, false);
        for (int axis = 1; axis <= 5; ++axis) enqueue(VisCommandType::SetArmAxisJog, axis, 0, axis == 5);
        break;
    case vi_protocol::Command::ArmProgramZero:
        enqueue(VisCommandType::ReturnArmProgramZero); break;
    case vi_protocol::Command::ArmCartesianParameter:
        if (frame.param1 < 0 || frame.param1 > 3 || frame.param2 < -100000 || frame.param2 > 100000)
        { add_result_locked(frame.command_id, 1, "Invalid Cartesian parameter."); return; }
        enqueue(VisCommandType::SetArmCartesianParameter, frame.param1, frame.param2); break;
    case vi_protocol::Command::ArmCartesianAlive:
        enqueue(VisCommandType::KeepArmCartesianAlive, 0, 0, false); break;
    case vi_protocol::Command::YValve:
        if (!valid_bool_int(frame.param1)) { add_result_locked(frame.command_id, 1, "Invalid Y-valve value."); return; }
        if (std::any_of(pending_tasks_.begin(), pending_tasks_.end(), [](const auto& task)
            { return task.kind == PendingKind::YValve; }))
        { add_result_locked(frame.command_id, 1, "Y-valve transition is already pending."); return; }
        if (extra_.y_valve_open == (frame.param1 == 0)) { add_result_locked(frame.command_id, 0, ""); return; }
        enqueue(VisCommandType::SetYValveOpen, frame.param1 == 0 ? 1 : 0, 0, false);
        pending_tasks_.push_back(PendingTask{ PendingKind::YValve, frame.command_id, now + 3000,
            state_snapshot_sequence_, 0, frame.param1 != 0 });
        break;
    default:
        add_result_locked(frame.command_id, 1, "Unsupported command.");
        break;
    }
}

void ZenohService::release_lease_locked(const std::string& reason)
{
    lease_held_ = false; ++lease_epoch_; force_zeroing_ = false;
    remote_handle_bridge::clear_samples(); remote_handle_bridge::clear_outputs();
    for (const auto& task : pending_tasks_)
    {
        if (task.kind == PendingKind::Prepare) prepare_task_state_ = 3;
        if (task.kind == PendingKind::StartControl) start_task_state_ = 3;
        add_result_locked(task.command_id, 1, reason);
    }
    pending_tasks_.clear();
    std::lock_guard<std::mutex> queue_lock(command_mutex_);
    control_actions_.clear();
    for (int i = 0; i < 3; ++i)
    {
        VisCommand command;
        if (i < 2) { command.type = VisCommandType::SetInjectorManualJog; command.param1 = i + 1; command.param2 = 0; }
        else { command.type = VisCommandType::SetAxis4ManualJog; command.param1 = 0; }
        hold_actions_[i] = ControlAction{ command, 0, false, session_id_, lease_epoch_ };
        hold_dirty_[i] = true; hold_direction_[i] = 0;
    }
}

void ZenohService::worker_loop()
{
    // 发布线程退出时恢复计时精度，避免 Windows 默认时钟粒度压低 100 Hz 频率。
    struct TimerResolution
    {
        bool enabled = timeBeginPeriod(1) == TIMERR_NOERROR;
        ~TimerResolution() { if (enabled) timeEndPeriod(1); }
    } timer_resolution;
    using Clock = std::chrono::steady_clock;
    const auto state_period = std::chrono::milliseconds(33);
    const auto force_period = std::chrono::milliseconds(10);
    auto next_state = Clock::now(), next_force = next_state;
    while (!stop_requested_.load())
    {
        const ULONGLONG now = GetTickCount64();
        const auto dropped = dropped_commands_.exchange(0);
        if (dropped != 0)
            std::cout << "Zenoh: command FIFO full; dropped " << dropped << " oldest commands." << std::endl;
        const auto expired = expired_commands_.exchange(0);
        if (expired != 0)
            std::cout << "Zenoh: dropped " << expired << " commands older than 2 seconds in the ADS queue." << std::endl;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (lease_held_ && now - last_handle_rx_ms_ > kHandleReleaseMs)
                release_lease_locked("Handle stream timed out; control lease released.");
        }
        const auto publish_tick = Clock::now();
        if (publish_tick >= next_state)
        {
            publish_state_frame(); next_state += state_period;
            if (next_state <= publish_tick) next_state = publish_tick + state_period;
        }
        if (publish_tick >= next_force)
        {
            publish_force_frame(); next_force += force_period;
            if (next_force <= publish_tick) next_force = publish_tick + force_period;
        }
        Sleep(1);
    }
}

void ZenohService::publish_state_frame()
{
    vi_protocol::StateFrame frame;
    std::vector<std::uint8_t> key;
    bool authed = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!have_state_ || GetTickCount64() - state_snapshot_tick_ms_ > kStateFreshMs) return;
        frame.version = vi_protocol::kVersion;
        frame.session_id = session_id_;
        frame.sequence = ++state_sequence_;
        const bool challenge = challenge_session_id_ != 0 && GetTickCount64() - challenge_tick_ms_ <= 10000 &&
            (!authenticated_ || (frame.sequence & 1u) == 0);
        frame.auth_state = challenge ? 1 : (authenticated_ ? 2 : 0);
        if (challenge) frame.session_id = challenge_session_id_;
        frame.nonce_server = challenge ? challenge_server_nonce_ : server_nonce_;
        frame.echo_main_monotonic_ms = echo_main_monotonic_ms_;
        frame.flags = 0;
        if (state_.control_active) frame.flags |= vi_protocol::ControlActive;
        if (state_.estop_hold) frame.flags |= vi_protocol::EstopHold;
        if (state_.self_check_done) frame.flags |= vi_protocol::SelfCheckDone;
        if (state_.ff_enabled) frame.flags |= vi_protocol::ForceFeedbackEnabled;
        if (state_.cal_zeroed) frame.flags |= vi_protocol::CalZeroed;
        if (state_.host_comm_timeout) frame.flags |= vi_protocol::HostCommTimeout;
        if (!extra_.y_valve_open) frame.flags |= vi_protocol::YValveClosed;
        if (lease_held_) frame.flags |= vi_protocol::LeaseHeld;
        if (state_.startup_completed) frame.flags |= vi_protocol::StartupCompleted;
        if (state_.ads_state == 2) frame.flags |= vi_protocol::AdsHealthy;
        if (force_zeroing_) frame.flags |= vi_protocol::ForceZeroing;
        if (state_.clean_force_valid && std::isfinite(static_cast<float>(state_.clean_force_n)))
            frame.flags |= vi_protocol::CleanForceValid;
        if ((state_.guidewire_mode == 0 ? state_.axis1_reverse : state_.axis6_reverse)) frame.flags |= vi_protocol::GlobalReverse;

        frame.mode = state_.guidewire_mode; frame.phase = state_.startup_phase;
        frame.self_check_status = state_.selfcheck_status; frame.ads_state = state_.ads_state;
        frame.cylinder1 = state_.cylinder_cmd[0]; frame.cylinder2 = state_.cylinder_cmd[1];
        frame.cylinder3 = state_.cylinder_cmd[2]; frame.cylinder4 = state_.cylinder_cmd[3];
        frame.cylinder_manual_mask = state_.cylinder_manual_mask;
        frame.injector1_direction = extra_.injector_dir[0]; frame.injector2_direction = extra_.injector_dir[1];
        frame.axis4_direction = extra_.axis4_dir;
        frame.force582_f = (std::isfinite(static_cast<float>(state_.force_582_f)) ? static_cast<float>(state_.force_582_f) : 0.0f); frame.force582_n = (std::isfinite(static_cast<float>(state_.force_582_n)) ? static_cast<float>(state_.force_582_n) : 0.0f);
        frame.force587_f = (std::isfinite(static_cast<float>(state_.force_587_f)) ? static_cast<float>(state_.force_587_f) : 0.0f); frame.force587_n = (std::isfinite(static_cast<float>(state_.force_587_n)) ? static_cast<float>(state_.force_587_n) : 0.0f);
        frame.clean_force_n = (state_.clean_force_valid && std::isfinite(static_cast<float>(state_.clean_force_n)) ? static_cast<float>(state_.clean_force_n) : 0.0f); frame.ads_actual_hz = (std::isfinite(static_cast<float>(state_.ads_actual_hz)) ? static_cast<float>(state_.ads_actual_hz) : 0.0f);
        frame.axis_pos0 = std::isfinite(static_cast<float>(state_.axis_pos[0])) ? static_cast<float>(state_.axis_pos[0]) : 0.0f;
        frame.axis_from_left0 = std::isfinite(static_cast<float>(state_.axis_pos_from_left[0])) ? static_cast<float>(state_.axis_pos_from_left[0]) : 0.0f;
        frame.axis_pos1 = std::isfinite(static_cast<float>(state_.axis_pos[1])) ? static_cast<float>(state_.axis_pos[1]) : 0.0f;
        frame.axis_from_left1 = std::isfinite(static_cast<float>(state_.axis_pos_from_left[1])) ? static_cast<float>(state_.axis_pos_from_left[1]) : 0.0f;
        frame.axis_pos2 = std::isfinite(static_cast<float>(state_.axis_pos[2])) ? static_cast<float>(state_.axis_pos[2]) : 0.0f;
        frame.axis_from_left2 = std::isfinite(static_cast<float>(state_.axis_pos_from_left[2])) ? static_cast<float>(state_.axis_pos_from_left[2]) : 0.0f;
        frame.axis_pos3 = std::isfinite(static_cast<float>(state_.axis_pos[3])) ? static_cast<float>(state_.axis_pos[3]) : 0.0f;
        frame.axis_from_left3 = std::isfinite(static_cast<float>(state_.axis_pos_from_left[3])) ? static_cast<float>(state_.axis_pos_from_left[3]) : 0.0f;
        frame.axis_pos4 = std::isfinite(static_cast<float>(state_.axis_pos[4])) ? static_cast<float>(state_.axis_pos[4]) : 0.0f;
        frame.axis_from_left4 = std::isfinite(static_cast<float>(state_.axis_pos_from_left[4])) ? static_cast<float>(state_.axis_pos_from_left[4]) : 0.0f;
        frame.axis_pos5 = std::isfinite(static_cast<float>(state_.axis_pos[5])) ? static_cast<float>(state_.axis_pos[5]) : 0.0f;
        frame.axis_from_left5 = std::isfinite(static_cast<float>(state_.axis_pos_from_left[5])) ? static_cast<float>(state_.axis_pos_from_left[5]) : 0.0f;
        frame.axis_pos6 = std::isfinite(static_cast<float>(state_.axis_pos[6])) ? static_cast<float>(state_.axis_pos[6]) : 0.0f;
        frame.axis_from_left6 = std::isfinite(static_cast<float>(state_.axis_pos_from_left[6])) ? static_cast<float>(state_.axis_pos_from_left[6]) : 0.0f;
        frame.has_net_distance = true;
        frame.axis1_net_distance_mm = std::isfinite(extra_.axis1_net_distance_mm) ? extra_.axis1_net_distance_mm : 0.0;
        frame.axis6_net_distance_mm = std::isfinite(extra_.axis6_net_distance_mm) ? extra_.axis6_net_distance_mm : 0.0;
        frame.prepare_task_state = prepare_task_state_; frame.prepare_task_command_id = prepare_task_command_id_;
        frame.start_task_state = start_task_state_; frame.start_task_command_id = start_task_command_id_;
        frame.results = results_; frame.event_text.clear();
        authed = authenticated_ && frame.auth_state == 2; key = session_key_;
    }
    if (authed)
    {
        auto clean = frame; clean.auth_tag.clear(); const auto data = pack(clean); std::uint8_t mac[32]{};
        if (!hmac_sha256(key, data.data(), data.size(), mac)) return;
        frame.auth_tag.assign(mac, mac + 16);
    }
    auto bytes = pack(frame);
    z_owned_bytes_t payload{};
    if (z_bytes_copy_from_buf(&payload, bytes.data(), bytes.size()) < 0) return;
    z_publisher_put_options_t options{}; z_publisher_put_options_default(&options);
    z_publisher_put(z_loan(state_publisher_), z_move(payload), &options);
}

void ZenohService::publish_force_frame()
{
    vi_protocol::ForceFrame frame;
    std::vector<std::uint8_t> key;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!authenticated_ || session_id_ == 0) return;
        frame.version = vi_protocol::kVersion; frame.session_id = session_id_;
        frame.sequence = ++force_sequence_; frame.echo_main_monotonic_ms = echo_main_monotonic_ms_;
        if (lease_held_)
        {
            const auto a = remote_handle_bridge::get_output(0);
            const auto b = remote_handle_bridge::get_output(1);
            frame.enable582 = a.enable; frame.axis582 = static_cast<std::int8_t>(std::clamp(a.axis, 0, 2));
            frame.force582_n = a.enable && std::isfinite(static_cast<float>(a.force)) ? static_cast<float>(a.force) : 0.0f;
            frame.torque582_nm = a.enable && std::isfinite(static_cast<float>(a.torque)) ? static_cast<float>(a.torque) : 0.0f;
            frame.enable587 = b.enable; frame.axis587 = static_cast<std::int8_t>(std::clamp(b.axis, 0, 2));
            frame.force587_n = b.enable && std::isfinite(static_cast<float>(b.force)) ? static_cast<float>(b.force) : 0.0f;
            frame.torque587_nm = b.enable && std::isfinite(static_cast<float>(b.torque)) ? static_cast<float>(b.torque) : 0.0f;
        }
        key = session_key_;
    }
    auto clean = frame; clean.auth_tag.clear(); const auto data = pack(clean); std::uint8_t mac[32]{};
    if (!hmac_sha256(key, data.data(), data.size(), mac)) return;
    frame.auth_tag.assign(mac, mac + 16);
    auto bytes = pack(frame);
    z_owned_bytes_t payload{};
    if (z_bytes_copy_from_buf(&payload, bytes.data(), bytes.size()) < 0) return;
    z_publisher_put_options_t options{}; z_publisher_put_options_default(&options);
    z_publisher_put(z_loan(force_publisher_), z_move(payload), &options);
}
