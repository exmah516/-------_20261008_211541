void baseline_record_event(ExperimentEventCode, int, std::uint64_t);
inline SessionClock::SessionClock() { reset(); }
inline void SessionClock::reset() { anchor_qpc_=static_cast<std::int64_t>(baseline::now_ms*1000); frequency_=1000000; }
inline std::int64_t SessionClock::now_qpc() const { return static_cast<std::int64_t>(baseline::now_ms*1000); }
inline std::uint64_t SessionClock::elapsed_us(std::int64_t qpc) const { return static_cast<std::uint64_t>(qpc-anchor_qpc_); }
class ExperimentRecorder {
    SessionClock clock_;
public:
    bool start(const std::string&,const ExperimentStartInfo&) { return true; }
    void stop_async(const char*) {}
    void stop_and_wait(const char*) {}
    void poll_health() {}
    bool is_recording() const { return true; }
    const SessionClock& clock() const { return clock_; }
    bool enqueue_force(const ForceCsvRow&) { return true; }
    bool enqueue_motion(const MotionCsvRow&) { return true; }
    bool enqueue_event(ExperimentEventCode code,int value,std::uint64_t sequence=0,std::int64_t=0) { baseline_record_event(code,value,sequence);return true; }
    void update_ads_communication_stats(const AdsCommunicationStats&) {}
    ExperimentRecorderSnapshot snapshot() const { return {}; }
};
