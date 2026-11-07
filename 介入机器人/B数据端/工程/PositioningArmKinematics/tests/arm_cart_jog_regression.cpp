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
    if (argc < 2 || argc > 3) return 2;
    const char magic[8] = {'A','R','M','J','O','G','3','\0'};
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

    const double e = 10 * Gen::RAD;
    const double q2 = -std::atan(Gen::L2 * std::sin(e) / (Gen::L1 + Gen::L2 * std::cos(e))) / Gen::RAD;
    g = begin(1, 1, 5, Axes{{100, q2 + 90, -170, 37, 12}});
    record(105, 6000, g, [](Gen& state, std::uint32_t n) {
        state.step(false, true, 1, n <= 3000 ? 1 : -1, 5, state.p, 0.001);
    });

    for (int mode = 3; mode <= 4; ++mode) {
        const double speed = mode == 3 ? 3 : 1.2;
        g = begin(mode, 1, speed, seed(mode, 90));
        record(static_cast<std::uint32_t>(110 + mode), 2000, g, [=](Gen& state, std::uint32_t) {
            state.step(false, true, mode, 1, speed, state.p, 0.001);
        });
    }

    for (int mode : {1, 2}) for (int dir : {1, -1}) {
        g = begin(mode, dir, 5, Axes{{100, 0, 0, 0, 0}});
        record(130 + static_cast<std::uint32_t>((mode - 1) * 2 + (dir < 0)), 2000, g,
            [=](Gen& state, std::uint32_t) { state.step(false, true, mode, dir, 5, state.p, 0.001); });
    }

    const std::vector<unsigned char> bytes = baseline;
    const bool capture = argc == 3 && std::strcmp(argv[2], "--capture") == 0;
    if (argc == 3 && !capture) return 2;
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
    std::cout << (equal ? "PASS" : "FAIL") << " second-round bitwise regression scenarios=30 cycles=" << total_cycles
        << " bytes=" << bytes.size() << " baseline_bytes=" << expected.size() << '\n';
    if (!equal) std::cout << "first differing byte=" << std::distance(bytes.begin(), mismatch.first) << '\n';
    return equal ? 0 : 1;
}
