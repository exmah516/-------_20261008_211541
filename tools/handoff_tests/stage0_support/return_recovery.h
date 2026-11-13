#pragma once
#include <cstdint>
#include <string>

struct ReturnRecoveryConditions
{
    bool ads_running = false, snapshot_valid = false;
    bool estop = false, host_timeout = false, handle_reinit_done = false;
    bool handles_valid = false, startup_active = false, exclusive_task = false;
    bool req[2] = {}, busy[2] = {};
    std::uint64_t snapshot_sequence = 0;
};

class ReturnRecovery
{
public:
    static constexpr std::uint64_t clear_interval_ms = 250;
    void original_trigger(std::uint8_t mask) { *this = ReturnRecovery{}; mask_ = mask; }
    void interrupted(const char* reason, bool unfinished = true)
    {
        holding_ = true;
        reason_ = reason;
        next_clear_ = 0;
        clear_applied_ = false;
        clear_sequence_ = 0;
        unfinished_ = unfinished;
    }
    bool clear_due(std::uint64_t now, const bool* req)
    {
        if (!holding_ || now < next_clear_) return false;
        bool needed = !clear_applied_;
        for (int slot = 0; slot < 2; ++slot)
            needed = needed || ((mask_ & (1 << slot)) && req[slot]);
        if (needed) next_clear_ = now + clear_interval_ms;
        return needed;
    }
    void clear_submitted(std::uint64_t sequence)
    {
        if (sequence != 0) { clear_sequence_ = sequence; clear_applied_ = false; }
    }
    std::uint64_t clear_sequence() const { return clear_sequence_; }
    void clear_result(bool success, std::uint64_t snapshot_sequence)
    {
        clear_applied_ = success;
        clear_sequence_ = 0;
        clear_observed_sequence_ = snapshot_sequence;
    }
    bool can_restore(const ReturnRecoveryConditions& c) const
    {
        if (!holding_ || !clear_applied_ || c.snapshot_sequence <= clear_observed_sequence_ || !c.ads_running || !c.snapshot_valid || c.estop || c.host_timeout ||
            !c.handle_reinit_done || !c.handles_valid || c.startup_active || c.exclusive_task) return false;
        for (int slot = 0; slot < 2; ++slot)
            if ((mask_ & (1 << slot)) && (c.req[slot] || c.busy[slot])) return false;
        return true;
    }
    void restored() { holding_ = false; resume_pending_ = unfinished_ && resumes_ == 0; }
    bool resume_pending() const { return resume_pending_; }
    void consume_resume() { resume_pending_ = false; ++resumes_; }
    void completed() { *this = ReturnRecovery{}; }
    unsigned int resumes() const { return resumes_; }
    std::uint8_t mask() const { return mask_; }
    const std::string& reason() const { return reason_; }
private:
    bool holding_ = false, resume_pending_ = false, clear_applied_ = false;
    bool unfinished_ = true;
    std::uint8_t mask_ = 0;
    unsigned int resumes_ = 0;
    std::uint64_t next_clear_ = 0, clear_sequence_ = 0, clear_observed_sequence_ = 0;
    std::string reason_;
};
