#pragma once
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <windows.h>

namespace homing
{
struct Record
{
    std::array<double, 7> leftlimit{}, init_pos{}, last_abs{};
    std::int64_t completed_time_unix_ms = 0, last_abs_time_unix_ms = 0;
    std::string application;
    bool complete = false;
};
inline std::int64_t now_ms()
{
    FILETIME file{}; GetSystemTimeAsFileTime(&file);
    ULARGE_INTEGER value{}; value.LowPart = file.dwLowDateTime; value.HighPart = file.dwHighDateTime;
    return static_cast<std::int64_t>(value.QuadPart / 10000 - 11644473600000ULL);
}
inline bool token(std::istream& input, char expected)
{ char value = 0; return static_cast<bool>(input >> value) && value == expected; }
inline bool key(std::istream& input, const char* expected)
{ std::string name; return static_cast<bool>(input >> std::quoted(name)) && name == expected && token(input, ':'); }
inline bool array(std::istream& input, std::array<double, 7>& values)
{
    if (!token(input, '[')) return false;
    for (int i = 0; i < 7; ++i)
    {
        if (!(input >> values[i]) || !std::isfinite(values[i])) return false;
        if (i < 6 && !token(input, ',')) return false;
    }
    return token(input, ']');
}
inline Record load(const std::filesystem::path& path)
{
    Record record;
    std::ifstream input(path);
    record.complete = token(input, '{') && key(input, "leftlimit") && array(input, record.leftlimit) && token(input, ',') &&
        key(input, "init_pos") && array(input, record.init_pos) && token(input, ',') &&
        key(input, "completed_time_unix_ms") && static_cast<bool>(input >> record.completed_time_unix_ms) && token(input, ',') &&
        key(input, "last_abs") && array(input, record.last_abs) && token(input, ',') &&
        key(input, "last_abs_time_unix_ms") && static_cast<bool>(input >> record.last_abs_time_unix_ms) && token(input, ',') &&
        key(input, "application") && static_cast<bool>(input >> std::quoted(record.application)) && token(input, '}') &&
        (input >> std::ws).eof() && record.completed_time_unix_ms > 0 &&
        record.last_abs_time_unix_ms >= record.completed_time_unix_ms && !record.application.empty();
    return record;
}
inline bool enabled(const std::filesystem::path& path)
{
    std::ifstream input(path);
    bool value = false;
    return token(input, '{') && key(input, "homing_reuse_enabled") &&
        static_cast<bool>(input >> std::boolalpha >> value) && token(input, '}') && (input >> std::ws).eof() && value;
}
inline bool save(const std::filesystem::path& path, const Record& record)
{
    const auto temporary = std::filesystem::path(path.wstring() + L".tmp");
    std::ofstream output(temporary, std::ios::trunc);
    output << std::setprecision(17);
    auto write_array = [&](const std::array<double, 7>& values)
    {
        output << '[';
        for (int i = 0; i < 7; ++i) { if (i) output << ','; output << values[i]; }
        output << ']';
    };
    output << "{\n\"leftlimit\":"; write_array(record.leftlimit);
    output << ",\n\"init_pos\":"; write_array(record.init_pos);
    output << ",\n\"completed_time_unix_ms\":" << record.completed_time_unix_ms;
    output << ",\n\"last_abs\":"; write_array(record.last_abs);
    output << ",\n\"last_abs_time_unix_ms\":" << record.last_abs_time_unix_ms;
    output << ",\n\"application\":" << std::quoted(record.application) << "\n}\n";
    output.flush();
    if (!output) return false;
    output.close();
    if (!output) return false;
    return MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}
inline std::string reject_reason(bool reuse_enabled, const Record& record, const std::string& application, const double* current_abs)
{
    if (!reuse_enabled) return "homing_reuse_enabled=FALSE";
    if (!record.complete) return "记录不完整";
    if (record.application != application) return "PLC应用名不同";
    for (int axis = 0; axis < 7; ++axis)
    {
        const double tolerance = axis == 1 || axis == 3 || axis == 6 ? 1.0 : 0.5;
        if (!std::isfinite(current_abs[axis]) || std::abs(current_abs[axis] - record.last_abs[axis]) > tolerance)
            return "轴" + std::to_string(axis + 1) + "绝对位置超过容差";
    }
    return "";
}
}
