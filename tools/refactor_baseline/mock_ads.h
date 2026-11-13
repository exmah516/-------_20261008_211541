class AdsCommunicationService {
public:
    explicit AdsCommunicationService(CADSComm&) {}
    bool start(const double*,const double*) { return true; }
    void stop() {}
    bool wait_for_snapshot(std::uint64_t,DWORD,AdsFastSnapshot&);
    bool latest_snapshot(AdsFastSnapshot&) const;
    void drain_snapshots(std::vector<AdsFastSnapshot>&);
    std::uint64_t publish_output(const AdsOutputCommand&);
    std::uint64_t applied_output_generation() const;
    std::uint64_t applied_motion_output_generation() const;
    void request_coordinate_refresh() {}
    bool refresh_coordinates(DWORD=250) { return true; }
    std::uint64_t coordinate_refresh_generation() const { return 1; }
    void request_watchdog_recovery() {}
    bool read(const char*,unsigned long,void*,DWORD=250);
    bool write(const char*,unsigned long,const void*,DWORD=250);
    bool read_sum(const char* const*,const unsigned long*,void* const*,unsigned long,DWORD=250);
    bool write_sum(const char* const*,const unsigned long*,const void* const*,unsigned long,DWORD=250);
    AdsEventState event_state() const;
    AdsCommunicationStats stats() const;
    bool coordinate_cache(double*,double*) const { return true; }
    std::uint32_t host_session_id() const { return 1; }
};
