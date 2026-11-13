#pragma once
#include <cstdint>
#include <string>

struct ReturnRecoveryConditions
{
    bool ads_running = false, snapshot_valid = false;
    bool estop = false, host_timeout = false, handle_reinit_done = false;
    bool handles_valid = false, startup_active = false, exclusive_task = false, handoff_idle = false;
    std::uint64_t snapshot_sequence = 0;
};
class ReturnRecovery
{
public:
    void original_trigger(std::uint8_t mask) { *this = ReturnRecovery{}; mask_ = mask; }
    void interrupted(const char* reason, bool unfinished = true)
    { holding_ = true; reason_ = reason; unfinished_ = unfinished; }
    bool can_restore(const ReturnRecoveryConditions& c) const
    {
        return holding_ && c.handoff_idle && c.ads_running && c.snapshot_valid && !c.estop && !c.host_timeout &&
            c.handle_reinit_done && c.handles_valid && !c.startup_active && !c.exclusive_task;
    }
    void restored() { holding_ = false; resume_pending_ = unfinished_ && resumes_ == 0; }
    bool resume_pending() const { return resume_pending_; }
    void consume_resume() { resume_pending_ = false; ++resumes_; }
    void completed() { *this = ReturnRecovery{}; }
    unsigned int resumes() const { return resumes_; }
    std::uint8_t mask() const { return mask_; }
    const std::string& reason() const { return reason_; }
private:
    bool holding_ = false, resume_pending_ = false, unfinished_ = true;
    std::uint8_t mask_ = 0;
    unsigned int resumes_ = 0;
    std::string reason_;
};
