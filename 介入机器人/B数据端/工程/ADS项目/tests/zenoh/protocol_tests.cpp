#include "../../protocol/cpp/zenoh_validation.h"
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <algorithm>
#pragma comment(lib, "bcrypt.lib")

namespace
{
    int passed = 0, failed = 0;
    constexpr std::uint64_t session = 0x1122334455667788ULL;
    using Bytes = std::vector<std::uint8_t>;
    void check(const char* name, bool valid)
    {
        valid ? ++passed : ++failed;
        std::cout << (valid ? "[PASS] " : "[FAIL] ") << name << '\n';
    }
    Bytes from_hex(const std::string& text)
    {
        Bytes output;
        std::string cleaned;
        for (const unsigned char c : text) if (!std::isspace(c)) cleaned.push_back(c);
        if (cleaned.size() % 2) throw std::runtime_error("Odd Hex length");
        for (std::size_t i = 0; i < cleaned.size(); i += 2)
            output.push_back(static_cast<std::uint8_t>(std::stoul(cleaned.substr(i, 2), nullptr, 16)));
        return output;
    }
    Bytes read_hex(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Fixture missing");
        return from_hex(std::string(std::istreambuf_iterator<char>(input), {}));
    }
    Bytes hmac(const Bytes& key, const Bytes& bytes, std::size_t count = 16)
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        Bytes result(32);
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0)
            throw std::runtime_error("BCryptOpenAlgorithmProvider");
        NTSTATUS status = BCryptCreateHash(algorithm, &hash, nullptr, 0,
            const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0);
        if (status >= 0) status = BCryptHashData(hash, const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), 0);
        if (status >= 0) status = BCryptFinishHash(hash, result.data(), static_cast<ULONG>(result.size()), 0);
        if (hash) BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (status < 0) throw std::runtime_error("HMAC calculation");
        result.resize(count);
        return result;
    }
    template<class T> Bytes sign(T frame, const Bytes& key)
    { frame.auth_tag.clear(); return hmac(key, vi_protocol::encode(frame)); }
    template<class T> void golden(const std::filesystem::path& dir, const char* name, T expected, const Bytes& key)
    {
        const auto bytes = read_hex(dir / (std::string(name) + ".hex"));
        expected.auth_tag = sign(expected, key);
        check((std::string(name) + " serialize fixed Hex").c_str(), vi_protocol::encode(expected) == bytes);
        T decoded;
        const bool valid = vi_protocol::decode(bytes, decoded);
        check((std::string(name) + " strict decode").c_str(), valid);
        check((std::string(name) + " reserialize fixed Hex").c_str(), valid && vi_protocol::encode(decoded) == bytes);
        check((std::string(name) + " HMAC fixed value").c_str(), valid && sign(decoded, key) == decoded.auth_tag);
        if (valid)
        {
            decoded.auth_tag.back() ^= 1;
            check((std::string(name) + " tampered HMAC rejected").c_str(), sign(decoded, key) != decoded.auth_tag);
        }
    }
    vi_protocol::HandleFrame new_handle()
    {
        vi_protocol::HandleFrame f;
        f.version = 1; f.session_id = session; f.sequence = 100; f.monotonic_ms = 123456;
        f.buttons582 = 5; f.valid582 = true; f.encoder582_0 = 1234; f.encoder582_1 = -5678;
        f.joint582_0 = .5f; f.joint582_1 = -.25f; f.velocity582_0 = 1.5f; f.velocity582_1 = -2.5f;
        f.buttons587 = 2; f.valid587 = true; f.encoder587_0 = -1; f.encoder587_1 = 2;
        f.joint587_0 = 1; f.joint587_1 = 2; f.velocity587_0 = 0; f.velocity587_1 = .125f;
        f.injector1_direction = 1; f.injector2_direction = -1; f.axis4_direction = 0;
        return f;
    }
    vi_protocol::CommandFrame new_command()
    {
        vi_protocol::CommandFrame f;
        f.version = 1; f.session_id = session; f.sequence = 101; f.monotonic_ms = 123457;
        f.command_id = 42; f.command = 4; f.param1 = -1; f.param2 = 2;
        f.value1 = 12.25; f.value2 = 125.5; f.payload = u8"准备";
        return f;
    }
    vi_protocol::StateFrame new_state()
    {
        vi_protocol::StateFrame f;
        f.version = 1; f.session_id = session; f.sequence = 102; f.auth_state = 2;
        for (int i = 16; i < 32; ++i) f.nonce_server.push_back(static_cast<std::uint8_t>(i));
        f.echo_main_monotonic_ms = 123456; f.flags = 913;
        f.mode = 0; f.phase = 1; f.self_check_status = 4; f.ads_state = 2;
        f.cylinder1 = 2000; f.cylinder2 = 10; f.cylinder3 = 2000; f.cylinder4 = 10; f.cylinder_manual_mask = 5;
        f.injector1_direction = 1; f.injector2_direction = -1; f.axis4_direction = 0;
        f.force582_f = .25f; f.force582_n = .01f; f.force587_f = .5f; f.force587_n = -.02f; f.clean_force_n = .3f; f.ads_actual_hz = 100;
        f.axis_pos0 = 1; f.axis_pos1 = 2; f.axis_pos2 = 3; f.axis_pos3 = 4; f.axis_pos4 = 5; f.axis_pos5 = 6; f.axis_pos6 = 7;
        f.axis_from_left0 = 11; f.axis_from_left1 = 12; f.axis_from_left2 = 13; f.axis_from_left3 = 14; f.axis_from_left4 = 15; f.axis_from_left5 = 16; f.axis_from_left6 = 17;
        f.has_net_distance = true; f.axis1_net_distance_mm = 12.25; f.axis6_net_distance_mm = -3.125;
        f.prepare_task_state = 2; f.prepare_task_command_id = 40; f.start_task_state = 1; f.start_task_command_id = 41;
        f.results = { {39, 0, ""}, {38, 1, u8"拒绝"} }; f.event_text = "ready";
        f.axis6_window_start_from_left_mm = 16; f.axis6_window_end_from_left_mm = 48;
        return f;
    }
    vi_protocol::ForceFrame new_force()
    {
        vi_protocol::ForceFrame f;
        f.version = 1; f.session_id = session; f.sequence = 103; f.echo_main_monotonic_ms = 123456;
        f.enable582 = true; f.axis582 = 1; f.force582_n = .5f; f.torque582_nm = .01f;
        f.enable587 = false; f.axis587 = 0; f.force587_n = 0; f.torque587_nm = 0;
        return f;
    }
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc != 2) throw std::runtime_error("Usage: protocol_tests.exe <fixtures directory>");
        const std::filesystem::path dir(argv[1]);
        const Bytes token{'z','e','n','o','h','-','g','o','l','d','e','n','-','t','o','k','e','n'};
        Bytes nc, ns, material{'k','e','y'};
        for (int i = 0; i < 16; ++i) nc.push_back(static_cast<std::uint8_t>(i));
        for (int i = 16; i < 32; ++i) ns.push_back(static_cast<std::uint8_t>(i));
        material.insert(material.end(), nc.begin(), nc.end()); material.insert(material.end(), ns.begin(), ns.end());
        const auto key = hmac(token, material);
        golden(dir, "handle", new_handle(), key);
        golden(dir, "command", new_command(), key);
        golden(dir, "state", new_state(), key);
        golden(dir, "force", new_force(), key);
        vi_protocol::CommandFrame hello, auth;
        check("Hello strict decode", vi_protocol::decode(read_hex(dir / "hello.hex"), hello));
        check("Hello reserialize", vi_protocol::encode(hello) == read_hex(dir / "hello.hex"));
        check("Authenticate strict decode", vi_protocol::decode(read_hex(dir / "authenticate.hex"), auth));
        Bytes proof_material{'a','u','t','h'};
        proof_material.insert(proof_material.end(), nc.begin(), nc.end()); proof_material.insert(proof_material.end(), ns.begin(), ns.end());
        check("Authenticate proof matches independent Hex", hmac(token, proof_material, 32) == auth.proof);
        check("Authenticate reserialize", vi_protocol::encode(auth) == read_hex(dir / "authenticate.hex"));
        vi_protocol::StateFrame challenge;
        check("Challenge decode and reserialize", vi_protocol::decode(read_hex(dir / "challenge.hex"), challenge) &&
            challenge.auth_state == 1 && vi_protocol::encode(challenge) == read_hex(dir / "challenge.hex"));
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            const auto name = entry.path().stem().string();
            if (name.rfind("invalid_", 0) != 0) continue;
            const auto bytes = read_hex(entry.path());
            bool accepted = false;
            if (name.rfind("invalid_handle_", 0) == 0) { vi_protocol::HandleFrame f; accepted = vi_protocol::decode(bytes, f); }
            else if (name.rfind("invalid_command_", 0) == 0) { vi_protocol::CommandFrame f; accepted = vi_protocol::decode(bytes, f); }
            else if (name.rfind("invalid_state_", 0) == 0) { vi_protocol::StateFrame f; accepted = vi_protocol::decode(bytes, f); }
            else if (name.rfind("invalid_force_", 0) == 0) { vi_protocol::ForceFrame f; accepted = vi_protocol::decode(bytes, f); }
            else throw std::runtime_error("Unknown fixture");
            check(name.c_str(), !accepted);
        }
        auto truncated = read_hex(dir / "handle.hex"); truncated.pop_back();
        vi_protocol::HandleFrame handle;
        check("Truncated data rejected", !vi_protocol::decode(truncated, handle));
        bool batch_valid = true;
        for (int i = 0; i < 100; ++i)
        {
            auto sample = new_handle();
            sample.sequence += i; sample.monotonic_ms += i * 10; sample.auth_tag = sign(sample, key);
            vi_protocol::HandleFrame output;
            batch_valid = batch_valid && vi_protocol::decode(vi_protocol::encode(sample), output) &&
                output.sequence == sample.sequence && sign(output, key) == output.auth_tag;
        }
        check("100Hz monotonic samples preserve fields and authentication", batch_valid);
    }
    catch (const std::exception& e) { ++failed; std::cerr << e.what() << '\n'; }
    std::cout << "C++ protocol tests: passed " << passed << ", failed " << failed << '\n';
    return failed == 0 ? 0 : 1;
}

