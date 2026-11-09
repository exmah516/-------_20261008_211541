#include "../../ADS项目/arm_cartesian_motion.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>

std::uint64_t bits(double v) { std::uint64_t b; std::memcpy(&b, &v, sizeof(b)); return b; }
double value(std::uint64_t b) { double v; std::memcpy(&v, &b, sizeof(v)); return v; }

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    const bool capture = std::string(argv[1]) == "--capture";
    std::fstream file(argv[2], capture ? std::ios::out : std::ios::in);
    if (!file) return 2;
    std::mt19937 rng(20261104);
    const std::array<double, 5> velocity{{5,5,5,5,5}}, acceleration{{50,50,50,50,50}};
    int mismatches = 0;
    for (int n = 0; n < 100; ++n) {
        std::array<double, 5> q;
        for (int i = 0; i < 5; ++i) {
            const double u = static_cast<double>(rng()) / 4294967295.0;
            q[i] = i == 0 ? 100 + 300*u : i == 4 ? -60 + 120*u : -360 + 720*u;
            if (capture) file << std::hex << bits(q[i]) << ' ';
            else { std::uint64_t b; file >> std::hex >> b; q[i] = value(b); }
        }
        const auto r = plan_arm_curve(q, velocity, acceleration, 0, 0);
        if (capture) {
            file << std::dec << r.error << ' ' << r.already_home;
            for (double v : r.curve.values) file << ' ' << std::hex << bits(v);
            file << '\n';
        } else {
            int error, home; file >> std::dec >> error >> home;
            mismatches += error != r.error || (home != 0) != r.already_home;
            for (double v : r.curve.values) {
                std::uint64_t b; file >> std::hex >> b; mismatches += bits(v) != b;
            }
        }
    }
    if (!file) return 2;
    std::cout << (mismatches ? "FAIL" : "PASS") << " HOME 100 cases, 1700 doubles + error/already_home, mismatches=" << mismatches << '\n';
    return mismatches ? 1 : 0;
}
