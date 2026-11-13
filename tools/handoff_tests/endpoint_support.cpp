// 新旧主循环共用的观测端，不参与控制计算。
static bool endpoint_started=false;
static void endpoint_capture(std::initializer_list<double> values)
{
    static std::ofstream file;
    if (!std::getenv("BASELINE_ENDPOINT")) return;
    if (!file.is_open()) {
        auto path=std::filesystem::path(_wgetenv(L"BASELINE_OUTPUT")); path+=L".endpoint.csv";
        file.open(path,std::ios::binary);
        if (!file) throw std::runtime_error("endpoint trace open failed");
        file<<"tick";
        for (size_t i=0;i<values.size();++i) file<<",v"<<i;
        file<<'\n'<<std::hexfloat;
    }
    file<<tick; for(double v:values) file<<','<<v; file<<'\n'; file.flush();
}
