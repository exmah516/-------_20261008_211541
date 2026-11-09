#include "arm_cart_jog_reference.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

using Gen = ArmCartJogReference;
using Axes = std::array<double, 5>;

std::vector<unsigned char> baseline;
std::uint64_t total_cycles = 0;

void append(const void* value, std::size_t size)
{
    const auto* bytes = static_cast<const unsigned char*>(value);
    baseline.insert(baseline.end(), bytes, bytes + size);
}

void header(std::uint32_t id, std::uint32_t cycles)
{
    append(&id, sizeof(id));
    append(&cycles, sizeof(cycles));
}

void sample(const Gen& g)
{
    for (double value : g.p) append(&value, sizeof(value));
    for (double value : g.v) append(&value, sizeof(value));
    for (double value : g.a) append(&value, sizeof(value));
    append(&g.path_speed, sizeof(g.path_speed));
    const std::uint8_t limited = g.limited ? 1 : 0;
    append(&limited, sizeof(limited));
}

Axes seed(int mode, double effective_elbow)
{
    const double e = effective_elbow * Gen::RAD;
    const double q2 = -std::atan(Gen::L2 * std::sin(e) / (Gen::L1 + Gen::L2 * std::cos(e))) / Gen::RAD;
    return {{100, q2 + (mode == 2 ? 90.0 : 0.0), effective_elbow - 180, 37, 12}};
}

Axes yaw_seed(double elbow, double q5)
{
    Axes p = seed(1, elbow);
    p[4] = q5;
    const double wx = Gen::L1*std::sin(p[1]*Gen::RAD)+Gen::L2*std::sin((p[1]+p[2])*Gen::RAD+Gen::PI);
    const double wy = -Gen::L1*std::cos(p[1]*Gen::RAD)-Gen::L2*std::cos((p[1]+p[2])*Gen::RAD+Gen::PI);
    const double psi = std::atan2(wy, wx)/Gen::RAD
        - std::atan2(-Gen::TOOL_LENGTH_MM*std::cos(q5*Gen::RAD), Gen::TOOL_LATERAL_OFFSET_MM)/Gen::RAD;
    p[3] = psi-p[1]-p[2]+90.0;
    return p;
}

Gen begin(int mode, int dir, double speed, Axes p)
{
    Gen g;
    g.step(true, false, mode, dir, speed, p, 0.001);
    return g;
}

template <class Update>
void record(std::uint32_t id, std::uint32_t cycles, Gen& g, Update update)
{
    header(id, cycles);
    total_cycles += cycles;
    for (std::uint32_t n = 1; n <= cycles; ++n) {
        update(g, n);
        sample(g);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 4) return 2;
    bool capture = false, round4 = false;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--capture") == 0) capture = true;
        else if (std::strcmp(argv[i], "--round4") == 0) round4 = true;
        else return 2;
    }
    const char magic[8] = {'A','R','M','J','O','G',round4 ? '4' : '3','\0'};
    append(magic, sizeof(magic));
    std::uint32_t case_id = 0;

    for (int mode : {1, 2}) for (double speed : {30., 15., 5.}) for (double elbow : {30., 90., 150.}) {
        Gen g = begin(mode, 1, speed, seed(mode, elbow));
        record(++case_id, 3501, g, [=](Gen& state, std::uint32_t n) {
            state.step(false, n <= 3000, mode, 1, speed, state.p, 0.001);
        });
    }

    Gen g = begin(1, 1, 5, seed(1, 90));
    record(100, 2501, g, [](Gen& state, std::uint32_t n) {
        state.step(false, n <= 2000, 1, n <= 2000 ? 1 : 0, n <= 2000 ? 5 : 0, state.p, 0.001);
    });

    g = begin(1, 1, 15, seed(1, 90));
    record(101, 5000, g, [](Gen& state, std::uint32_t n) {
        const double speed = n <= 2000 ? 15 : n <= 4000 ? 30 : 5;
        state.step(false, true, 1, 1, speed, state.p, 0.001);
    });

    g = begin(1, 1, 15, seed(1, 90));
    record(102, 4000, g, [](Gen& state, std::uint32_t n) {
        state.step(false, true, n <= 2000 ? 1 : 2, 1, 15, state.p, 0.001);
    });

    g = begin(1, 1, 5, seed(1, 90));
    record(103, 4000, g, [](Gen& state, std::uint32_t n) {
        state.step(false, n <= 2000 || n > 2200, 1, 1, 5, state.p, 0.001);
    });

    g = begin(1, 1, 5, seed(1, 90));
    double alive_age = 0;
    record(104, 3100, g, [&alive_age](Gen& state, std::uint32_t n) {
        if (n <= 2000) alive_age = 0; else alive_age += 0.001;
        const bool held = alive_age < 0.5;
        state.step(false, held, 1, held ? 1 : 0, held ? 5 : 0, state.p, 0.001);
    });

    if (!round4) {
    const double e = 10 * Gen::RAD;
    const double q2 = -std::atan(Gen::L2 * std::sin(e) / (Gen::L1 + Gen::L2 * std::cos(e))) / Gen::RAD;
    g = begin(1, 1, 5, Axes{{100, q2 + 90, -170, 37, 12}});
    record(105, 6000, g, [](Gen& state, std::uint32_t n) {
        state.step(false, true, 1, n <= 3000 ? 1 : -1, 5, state.p, 0.001);
    });
    }

    for (int mode = 3; mode <= 4; ++mode) {
        const double speed = mode == 3 ? 3 : 1.2;
        g = begin(mode, 1, speed, seed(mode, 90));
        record(static_cast<std::uint32_t>(110 + mode), 2000, g, [=](Gen& state, std::uint32_t) {
            state.step(false, true, mode, 1, speed, state.p, 0.001);
        });
    }

    if (!round4) for (int mode : {1, 2}) for (int dir : {1, -1}) {
        g = begin(mode, dir, 5, Axes{{100, 0, 0, 0, 0}});
        record(130 + static_cast<std::uint32_t>((mode - 1) * 2 + (dir < 0)), 2000, g,
            [=](Gen& state, std::uint32_t) { state.step(false, true, mode, dir, 5, state.p, 0.001); });
    }

    if (round4) {
        std::uint32_t id = 200;
        for (double elbow : {30.,90.,150.}) for (double q5 : {0.,30.,-30.}) for (int dir : {1,-1}) {
            g = begin(5, dir, .8, yaw_seed(elbow,q5));
            record(id++, 3500, g, [=](Gen& state, std::uint32_t n) {
                state.step(false, n <= 3000, 5, dir, .8, state.p, .001);
            });
        }
        g = begin(5, 1, .8, yaw_seed(90,0));
        record(220, 2501, g, [](Gen& state, std::uint32_t n) {
            state.step(false, n <= 2000, n <= 2000 ? 5 : 0, n <= 2000 ? 1 : 0,
                n <= 2000 ? .8 : 0, state.p, .001);
        });
        g = begin(1, 1, 5, yaw_seed(90,0));
        record(221, 9000, g, [](Gen& state, std::uint32_t n) {
            const int mode = n <= 2000 ? 1 : n <= 5500 ? 5 : 2;
            state.step(false, true, mode, 1, mode == 5 ? .8 : 5, state.p, .001);
        });
    }

    const std::vector<unsigned char> bytes = baseline;
    if (capture) {
        std::ofstream output(argv[1], std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        std::cout << "CAPTURE baseline bytes=" << bytes.size() << " cycles=";
        std::cout << total_cycles << '\n';
        return output ? 0 : 1;
    }

    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<unsigned char> expected((std::istreambuf_iterator<char>(input)), {});
    const auto mismatch = std::mismatch(bytes.begin(), bytes.end(), expected.begin(), expected.end());
    const bool equal = bytes.size() == expected.size() && mismatch.first == bytes.end();
    std::cout << (equal ? "PASS" : "FAIL") << (round4 ? " pre-round4 bitwise regression scenarios=45 cycles="
        : " second-round bitwise regression scenarios=30 cycles=") << total_cycles
        << " bytes=" << bytes.size() << " baseline_bytes=" << expected.size() << '\n';
    if (!equal) std::cout << "first differing byte=" << std::distance(bytes.begin(), mismatch.first) << '\n';
    return equal ? 0 : 1;
}
