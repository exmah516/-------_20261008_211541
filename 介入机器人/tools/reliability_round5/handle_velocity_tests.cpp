#include "handle_velocity_recorder.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
#pragma comment(lib, "winmm.lib")

static void check(bool ok, const char* label)
{
    std::cout << (ok ? "PASS: " : "FAIL: ") << label << std::endl;
    if (!ok) std::exit(1);
}
int main()
{
    using namespace remote_handle_bridge;
    clear_samples();
    Sample a, b;
    a.valid = b.valid = true;
    a.rx_ms = b.rx_ms = GetTickCount64() - 100;
    a.vels[0] = -3; a.vels[1] = 4; b.vels[0] = 5; b.vels[1] = -6;
    a.joints[0] = 1; a.joints[1] = 2; b.joints[0] = 3; b.joints[1] = 4;
    auto set = [&](const Sample& first, const Sample& second) {
        auto& state = detail::state(); std::lock_guard<std::mutex> lock(state.m);
        state.samples[0] = first; state.samples[1] = second;
    };
    set(a, b);
    HandleVelocityRecorder recorder;
    recorder.sample(GetTickCount64());
    a.rx_ms += 10; b.rx_ms += 10;
    a.joints[0] += .125f; a.joints[1] -= .25f; b.joints[0] -= .5f; b.joints[1] += 1;
    set(a, b);
    const auto row = recorder.sample(GetTickCount64());
    check(row.received[0] == -3 && row.received[1] == 4 && row.received[2] == 5 && row.received[3] == -6,
        "R3 records all four unmodified received velocities");
    check(row.joint_rate[0] == 12.5 && row.joint_rate[1] == -25 && row.joint_rate[2] == -50 && row.joint_rate[3] == 100,
        "R3 receive-interval joint differences have units per second");
    check(recorder.received_max() == std::array<double,4>{3,4,5,6} &&
        recorder.joint_max() == std::array<double,4>{12.5,25,50,100}, "R3 maxima use absolute magnitude per joint");
    const auto duplicate = recorder.sample(GetTickCount64());
    check(std::isnan(duplicate.joint_rate[0]), "R3 repeated cache is not a new joint speed");
    Sample after;
    check(get_sample(0, after, 5000) && after.rx_ms == a.rx_ms && after.joints[0] == a.joints[0] && after.vels[0] == a.vels[0],
        "R3 logging does not change received sample or timestamp");
    set_output(0, Output{true,1,2,.5}); recorder.sample(GetTickCount64());
    check(get_output(0).enable && get_output(0).force == 2, "R3 logger never changes force output");
    clear_samples();
    const auto missing = recorder.sample(GetTickCount64());
    check(!missing.valid[0] && std::isnan(missing.received[0]) && recorder.received_max()[0] == 3,
        "R3 missing sample writes invalid/NaN and preserves maxima");
    set(a,b);
    const auto path = std::filesystem::path(L"handle_velocity_test.csv");
    check(recorder.start(path), "R3 asynchronous velocity CSV starts");
    Sleep(1000); recorder.stop();
    std::ifstream csv(path); std::string line;
    std::getline(csv,line); check(line.find("sdk582_0") != std::string::npos && line.find("joint587_1_per_s") != std::string::npos,
        "R3 velocity CSV declares both raw and joint-rate columns");
    std::vector<unsigned long long> ticks;
    while (std::getline(csv,line)) { std::istringstream values(line); unsigned long long tick; values >> tick; ticks.push_back(tick); }
    check(ticks.size() >= 99 && ticks.size() <= 102 && ticks.back() - ticks.front() >= 980,
        "R3 one second writes 100Hz velocity rows independent of ADS/control");
    std::cout << "R3 CSV rows=" << ticks.size() << " span_ms=" << ticks.back()-ticks.front() << '\n';
}
