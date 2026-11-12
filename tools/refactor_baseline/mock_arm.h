class ArmManualAdsService {
public:
    explicit ArmManualAdsService(AdsCommunicationService&) {}
    bool start() { return true; }
    void stop() {}
    ArmManualSnapshot snapshot() const { return {}; }
    void set_manual_enable(bool) {}
    void set_axis_enable(int,bool) {}
    void request_reset(int) {}
    void set_jog_direction(int,int,double) {}
    bool set_jog_parameter(int,int,double) { return true; }
    void set_cartesian_jog(int,double,double) {}
    void request_program_zero() {}
    void stop_cartesian() {}
    void keep_cartesian_alive() {}
};
