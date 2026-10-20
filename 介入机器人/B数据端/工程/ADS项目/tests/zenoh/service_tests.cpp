#include <winsock2.h>
#include <bcrypt.h>
#include "zenoh_service.h"
#include <algorithm>
#include <cassert>
#include <iostream>

struct ZenohServiceTestAccess
{
    static void initialize(ZenohService& service)
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        assert(BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
            BCRYPT_ALG_HANDLE_HMAC_FLAG)));
        service.bcrypt_algorithm_ = algorithm;
        service.token_.assign(32, 't');
        service.session_key_.assign(16, 0x42);
        service.session_id_ = 41;
        service.authenticated_ = true;
        service.lease_held_ = true;
        VisState state{}; state.ads_state = 2; state.selfcheck_status = 1; state.cylinder_manual_allowed = true;
        RemoteExtraState extra; extra.initial_sync_done = true;
        service.publish_state(state, extra);
    }

    static vi_protocol::CommandFrame command(ZenohService& service, vi_protocol::Command type,
        std::uint64_t sequence, int id, int p1 = 0, int p2 = 0)
    {
        vi_protocol::CommandFrame frame;
        frame.version = 1; frame.session_id = service.session_id_; frame.sequence = sequence;
        frame.command_id = id; frame.command = static_cast<std::uint8_t>(type);
        frame.param1 = p1; frame.param2 = p2; frame.monotonic_ms = 100 + sequence;
        frame.auth_tag = service.sign(frame);
        return frame;
    }

    static void send(ZenohService& service, vi_protocol::CommandFrame frame)
    {
        frame.auth_tag = service.sign(frame);
        service.on_command(frame);
    }

    static bool result(const ZenohService& service, int id, int value)
    {
        return std::any_of(service.results_.begin(), service.results_.end(), [=](const auto& r)
            { return r.command_id == id && r.result == value; });
    }

    static void fifo_and_high_rate()
    {
        ZenohService service; initialize(service);
        for (int id = 1; id <= 257; ++id)
            send(service, command(service, vi_protocol::Command::ArmManualEnable, id, id, id % 2));
        assert(service.command_queue_.size() == 256);
        assert(service.command_queue_.front().frame.command_id == 2);
        assert(result(service, 1, 1));
        vi_protocol::HandleFrame handle; handle.version = 1; handle.session_id = 41; handle.sequence = 1;
        handle.valid582 = handle.valid587 = true; handle.injector1_direction = 1;
        handle.auth_tag = service.sign(handle); service.on_handle(handle);
        assert(service.command_queue_.size() == 256);
        VisCommand control; assert(service.poll_command(control));
        assert(control.type == VisCommandType::SetInjectorManualJog && control.param2 == 1);
        service.command_processed();
        int count = 0;
        while (service.poll_command(control))
        {
            assert(control.type == VisCommandType::SetArmManualEnable);
            assert(control.param1 == ((count + 2) % 2));
            service.command_processed(); ++count;
        }
        assert(count == 256 && service.results_.size() == 32);
        assert(service.results_.front().command_id == 226 && service.results_.back().command_id == 257);
    }

    static void expiry_and_replay()
    {
        ZenohService service; initialize(service);
        const auto first = command(service, vi_protocol::Command::ArmManualEnable, 10, 10, 1);
        send(service, first); send(service, first);
        send(service, command(service, vi_protocol::Command::ArmManualEnable, 9, 9, 0));
        auto other = command(service, vi_protocol::Command::ArmManualEnable, 11, 11, 0);
        other.session_id = 999; send(service, other);
        assert(service.command_queue_.size() == 1);
        service.command_queue_.front().queued_ms = GetTickCount64() - 2001;
        VisCommand control; assert(!service.poll_command(control));
        assert(result(service, 10, 1));
        send(service, command(service, vi_protocol::Command::ArmManualEnable, 12, 12, 1));
        service.release_lease_locked("test revoke");
        while (service.poll_command(control)) service.command_processed();
        assert(result(service, 12, 1));
    }

    static void results_exclude_jog_and_alive()
    {
        ZenohService service; initialize(service);
        for (int i = 1; i <= 40; ++i)
        {
            const auto type = i % 3 == 0 ? vi_protocol::Command::ArmCartesianAlive :
                (i % 3 == 1 ? vi_protocol::Command::ArmAxisJog : vi_protocol::Command::ArmCartesianJog);
            send(service, command(service, type, i, i, 1, 1));
        }
        VisCommand control;
        while (service.poll_command(control)) service.command_processed();
        assert(service.results_.empty());
        send(service, command(service, vi_protocol::Command::ArmAxisJog, 41, 41, 0, 7));
        assert(!service.poll_command(control));
        assert(service.results_.empty());
    }

    static void task_association()
    {
        ZenohService service; initialize(service);
        auto prepare = command(service, vi_protocol::Command::PreparePosition, 1, 101);
        prepare.value1 = 10.0; prepare.value2 = 20.0; send(service, prepare);
        VisCommand control; int count = 0;
        while (service.poll_command(control)) { service.command_processed(); ++count; }
        assert(count == 5 && service.prepare_task_state_ == 1 && service.prepare_task_command_id_ == 101);
        assert(!result(service, 101, 0));
        VisState state = service.state_; RemoteExtraState extra = service.extra_;
        state.self_check_done = true; service.publish_state(state, extra);
        assert(service.prepare_task_state_ == 2 && result(service, 101, 0));
        send(service, command(service, vi_protocol::Command::StartControl, 2, 102));
        assert(service.poll_command(control) && control.type == VisCommandType::SelectDirectControl);
        service.command_processed();
        assert(service.start_task_state_ == 1 && service.start_task_command_id_ == 102);
        state.control_active = true; service.publish_state(state, extra);
        assert(service.start_task_state_ == 2 && result(service, 102, 0));
    }

    static void challenge_cannot_revoke()
    {
        ZenohService service; initialize(service);
        send(service, command(service, vi_protocol::Command::ArmManualEnable, 1, 1, 1));
        vi_protocol::CommandFrame hello; hello.version = 1; hello.sequence = 2;
        hello.nonce_client.assign(16, 0x31);
        service.on_command(hello);
        assert(service.session_id_ == 41 && service.authenticated_ && service.lease_held_);
        assert(service.command_queue_.size() == 1 && service.challenge_session_id_ != 0);
        vi_protocol::CommandFrame proof; proof.version = 1; proof.command = 1; proof.sequence = 3;
        proof.session_id = service.challenge_session_id_; proof.nonce_client = service.challenge_client_nonce_;
        proof.nonce_server = service.challenge_server_nonce_;
        std::vector<std::uint8_t> data{ 'a', 'u', 't', 'h' };
        data.insert(data.end(), proof.nonce_client.begin(), proof.nonce_client.end());
        data.insert(data.end(), proof.nonce_server.begin(), proof.nonce_server.end());
        proof.proof.resize(32);
        assert(service.hmac_sha256(service.token_, data.data(), data.size(), proof.proof.data()));
        service.on_command(proof);
        assert(service.session_id_ == proof.session_id && service.authenticated_ && !service.lease_held_);
        assert(service.command_queue_.empty());
        VisCommand control;
        while (service.poll_command(control))
        {
            assert(control.type == VisCommandType::SetInjectorManualJog || control.type == VisCommandType::SetAxis4ManualJog);
            service.command_processed();
        }
    }

    static void handle_replay()
    {
        ZenohService service; initialize(service);
        vi_protocol::HandleFrame handle; handle.version = 1; handle.session_id = 41; handle.sequence = 10;
        handle.valid582 = handle.valid587 = true; handle.injector1_direction = 1;
        handle.auth_tag = service.sign(handle); service.on_handle(handle);
        VisCommand control;
        assert(service.poll_command(control) && control.param2 == 1); service.command_processed();
        handle.injector1_direction = -1;
        handle.auth_tag = service.sign(handle); service.on_handle(handle);
        assert(!service.poll_command(control));
        --handle.sequence; handle.auth_tag = service.sign(handle); service.on_handle(handle);
        assert(!service.poll_command(control));
        handle.sequence = 11; handle.session_id = 999;
        handle.auth_tag = service.sign(handle); service.on_handle(handle);
        assert(!service.poll_command(control));
        handle.session_id = 41; handle.auth_tag = service.sign(handle); service.on_handle(handle);
        assert(service.poll_command(control) && control.param2 == -1); service.command_processed();
    }

    static void task_waits_for_observed_state()
    {
        ZenohService service; initialize(service);
        auto state = service.state_; state.self_check_done = true;
        auto extra = service.extra_; service.publish_state(state, extra);
        send(service, command(service, vi_protocol::Command::StartControl, 1, 201));
        VisCommand control; assert(service.poll_command(control)); service.command_processed();
        service.publish_state(state, extra); service.publish_state(state, extra);
        assert(service.start_task_state_ == 1 && !result(service, 201, 1));
        state.control_active = true; service.publish_state(state, extra);
        assert(service.start_task_state_ == 2 && result(service, 201, 0));

        send(service, command(service, vi_protocol::Command::ForceFeedback, 2, 202, 1));
        assert(service.poll_command(control) && control.type == VisCommandType::ZeroForceSensor);
        service.command_processed(); ++extra.force_zero_ok_count; service.publish_state(state, extra);
        assert(service.poll_command(control) && control.type == VisCommandType::ToggleForceFeedback);
        service.command_processed();
        service.publish_state(state, extra); service.publish_state(state, extra);
        assert(!result(service, 202, 1));
        state.ff_enabled = true; service.publish_state(state, extra);
        assert(result(service, 202, 0));
    }

    static void resource_transition_rejection()
    {
        ZenohService service; initialize(service);
        send(service, command(service, vi_protocol::Command::Cylinder, 1, 301, 1, 1));
        VisCommand control; assert(service.poll_command(control)); service.command_processed();
        send(service, command(service, vi_protocol::Command::Cylinder, 2, 302, 1, 0));
        assert(!service.poll_command(control) && result(service, 302, 1));
        auto state = service.state_; state.cylinder_manual_mask = 1;
        service.publish_state(state, service.extra_); assert(result(service, 301, 0));
        send(service, command(service, vi_protocol::Command::YValve, 3, 303, 0));
        assert(service.poll_command(control)); service.command_processed();
        send(service, command(service, vi_protocol::Command::YValve, 4, 304, 1));
        assert(!service.poll_command(control) && result(service, 304, 1));
    }
};

int main()
{
    ZenohServiceTestAccess::fifo_and_high_rate();
    ZenohServiceTestAccess::expiry_and_replay();
    ZenohServiceTestAccess::results_exclude_jog_and_alive();
    ZenohServiceTestAccess::task_association();
    ZenohServiceTestAccess::challenge_cannot_revoke();
    ZenohServiceTestAccess::handle_replay();
    ZenohServiceTestAccess::task_waits_for_observed_state();
    ZenohServiceTestAccess::resource_transition_rejection();
    std::cout << "PASS: FIFO256/overflow/order, independent handle, 2s expiry, session/sequence replay, "
        "results32 excluding jog/alive, task IDs/state observation, authentication isolation, handle replay, resource transitions.\n";
}
