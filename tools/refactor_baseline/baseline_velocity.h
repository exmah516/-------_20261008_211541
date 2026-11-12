#include <filesystem>
class HandleVelocityRecorder {
public:
    bool start(const std::filesystem::path&) { return true; }
    void stop() {}
};
