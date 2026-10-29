#pragma once

#include "control_messages.h"
#include "protocol/cpp/zenoh_protocol.h"
#include "手柄/remote_handle_bridge.h"

#include <zenoh.h>
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

struct ZenohServiceConfig
{
    std::string endpoint = "tcp/192.168.60.1:7447";
    std::string token_path = "remote.token";
    std::string router_path;
    bool start_router = true;
    double startup_g_mm = 13.0;
    double axis6_soft_limit_from_left_mm = 660.0;
};

struct RemoteExtraState
{
    double axis1_net_distance_mm = 0.0;
    double axis6_net_distance_mm = 0.0;
    std::int8_t injector_dir[2] = {};
    std::int8_t axis4_dir = 0;
    bool y_valve_open = false;
    std::uint32_t force_zero_ok_count = 0;
    std::uint32_t force_zero_fail_count = 0;
    bool initial_sync_done = false;
    bool handle_soft_hold = false;
    bool ads_soft_hold = false;
    bool connection_hold = false;
    bool return_ads_fault_hold = false;
};

class ZenohService
{
public:
    ZenohService() = default;
    ~ZenohService();
    ZenohService(const ZenohService&) = delete;
    ZenohService& operator=(const ZenohService&) = delete;

    bool start(const ZenohServiceConfig& config);
    void stop();
    bool is_running() const { return running_.load(); }
    void publish_state(const VisState& state, const RemoteExtraState& extra);
    bool poll_command(VisCommand& command);
    void command_processed();

private:
    enum class PendingKind { Prepare, ForceZero, ForceEnable, ForceDisable, CylinderOn, CylinderOff, YValve, StartControl };
    struct PendingTask
    {
        PendingKind kind{};
        std::int32_t command_id{};
        ULONGLONG deadline_ms{};
        std::uint64_t start_state_sequence{};
        int index{};
        bool flag{};
        std::uint32_t zero_ok_count{}, zero_fail_count{};
    };
    struct QueuedCommand
    {
        vi_protocol::CommandFrame frame;
        ULONGLONG queued_ms{};
        std::uint64_t lease_epoch{};
    };
    struct ControlAction
    {
        VisCommand command;
        std::int32_t request_id{};
        bool has_result{};
        std::uint64_t session_id{};
        std::uint64_t lease_epoch{};
    };

    static void handle_callback(z_loaned_sample_t* sample, void* context);
    static void command_callback(z_loaned_sample_t* sample, void* context);
    void receive_handle(z_loaned_sample_t* sample);
    void receive_command(z_loaned_sample_t* sample);
    void on_handle(const vi_protocol::HandleFrame& frame);
    void on_command(const vi_protocol::CommandFrame& frame);
    void consume_command_locked(const vi_protocol::CommandFrame& frame);
    void release_lease_locked(const std::string& reason);
    void worker_loop();
    void publish_state_frame();
    void publish_force_frame();
    void queue_command(const VisCommand& command, std::int32_t request_id, bool has_result = true);
    void add_result_locked(std::int32_t command_id, std::uint8_t result, const std::string& reason);
    bool load_token(const std::string& path);
    bool start_router_process();
    void stop_router_process();
    bool open_zenoh();
    bool declare_entities();
    void close_zenoh();
    bool verify_tag(const vi_protocol::HandleFrame& frame) const;
    bool verify_tag(const vi_protocol::CommandFrame& frame) const;
    std::vector<std::uint8_t> sign(const vi_protocol::HandleFrame& frame) const;
    std::vector<std::uint8_t> sign(const vi_protocol::CommandFrame& frame) const;
    bool hmac_sha256(const std::vector<std::uint8_t>& key, const std::uint8_t* data, std::size_t size, std::uint8_t out[32]) const;
    static bool unpack_bytes(z_loaned_sample_t* sample, std::vector<std::uint8_t>& bytes);
    static bool decode(const std::vector<std::uint8_t>& bytes, vi_protocol::HandleFrame& frame);
    static bool decode(const std::vector<std::uint8_t>& bytes, vi_protocol::CommandFrame& frame);
    static std::vector<std::uint8_t> pack(const vi_protocol::HandleFrame& frame);
    static std::vector<std::uint8_t> pack(const vi_protocol::CommandFrame& frame);
    static std::vector<std::uint8_t> pack(const vi_protocol::StateFrame& frame);
    static std::vector<std::uint8_t> pack(const vi_protocol::ForceFrame& frame);

    ZenohServiceConfig config_;
    std::vector<std::uint8_t> token_, client_nonce_, server_nonce_, session_key_;
    std::atomic<bool> running_{ false };
    std::atomic<bool> stop_requested_{ false };
    std::atomic<std::uint64_t> dropped_commands_{ 0 };
    std::atomic<std::uint64_t> expired_commands_{ 0 };
    std::thread worker_;
    HANDLE router_process_ = nullptr;
    bool router_started_ = false;
    bool wsa_started_ = false;
    void* bcrypt_algorithm_ = nullptr;

    z_owned_session_t session_{};
    z_owned_keyexpr_t handle_key_{}, command_key_{}, state_key_{}, force_key_{};
    z_owned_publisher_t state_publisher_{}, force_publisher_{};
    z_owned_subscriber_t handle_subscriber_{}, command_subscriber_{};
    bool zenoh_open_ = false;

    mutable std::mutex state_mutex_;
    VisState state_{};
    RemoteExtraState extra_{};
    bool have_state_ = false;
    ULONGLONG state_snapshot_tick_ms_ = 0, challenge_tick_ms_ = 0;
    std::uint64_t state_sequence_ = 0, state_snapshot_sequence_ = 0, force_sequence_ = 0;
    std::uint64_t last_handle_sequence_ = 0, last_command_sequence_ = 0;
    bool have_handle_sequence_ = false, have_command_sequence_ = false;
    std::uint64_t echo_main_monotonic_ms_ = 0, session_id_ = 0;
    ULONGLONG last_handle_rx_ms_ = 0;
    bool authenticated_ = false, lease_held_ = false;
    std::uint64_t challenge_session_id_ = 0, challenge_sequence_ = 0, lease_epoch_ = 0;
    std::vector<std::uint8_t> challenge_client_nonce_, challenge_server_nonce_;
    int hold_direction_[3] = {};
    ULONGLONG hold_last_push_ms_[3] = {};
    bool force_zeroing_ = false;
    std::uint8_t prepare_task_state_ = 0, start_task_state_ = 0;
    std::int32_t prepare_task_command_id_ = 0, start_task_command_id_ = 0;
    std::vector<vi_protocol::CommandResult> results_;
    std::vector<PendingTask> pending_tasks_;

    std::mutex command_mutex_;
    std::deque<QueuedCommand> command_queue_;
    std::deque<ControlAction> control_actions_;
    ControlAction hold_actions_[3]{};
    bool hold_dirty_[3] = {};
    ControlAction dispatched_action_{};
    friend struct ZenohServiceTestAccess;
};
