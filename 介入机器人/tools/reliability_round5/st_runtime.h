#pragma once
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>

// 替身显式保留FB输出，由脚本逐拍注入；不仿真NC、驱动器或现场实时性。
struct Node
{
    double value = 0;
    std::map<std::string, Node> fields;
    std::string kind;
    std::function<void()> hook;
    Node() = default;
    Node(double v) : value(v) {}
    operator double() const { return value; }
    Node& operator=(double v) { value = v; return *this; }
    Node& operator[](const char* name) { std::string key(name); for (char& c : key) c = char(std::tolower(static_cast<unsigned char>(c))); return fields[key]; }
    Node& operator[](double index) { return fields[std::to_string(int(index))]; }
    Node& operator++() { ++value; return *this; }
};
using Args = std::initializer_list<std::pair<std::string, Node>>;
inline double st_min(double a, double b) { return std::min(a,b); }
inline double st_max(double a, double b) { return std::max(a,b); }
inline std::uint64_t st_tick = 0;
inline void call(Node& fb, Args args)
{
    for (const auto& arg : args) fb[arg.first.c_str()] = arg.second;
    if (fb.kind == "TON")
    {
        if (fb["IN"] && !fb["prev"]) fb["start"] = double(st_tick);
        fb["elapsed"] = fb["IN"] ? double(st_tick) - fb["start"] : 0;
        fb["ET"] = double(fb["elapsed"]);
        fb["prev"] = double(fb["IN"]);
        fb["Q"] = fb["IN"] && fb["elapsed"] >= fb["PT"];
    }
    if (fb.hook) fb.hook();
}
inline void feed(Node& axis, Args args)
{
    for (const auto& arg : args)
    {
        if (arg.first == "Position") axis["PlcToNc"]["ExtSetPos"] = arg.second;
        if (arg.first == "Velocity") axis["PlcToNc"]["ExtSetVelo"] = arg.second;
        if (arg.first == "Acceleration") axis["PlcToNc"]["ExtSetAcc"] = arg.second;
    }
}
inline Node TwinCAT_SystemInfoVarList;
enum { _init, _jog1, _jog2, _jog3, _trans, _trans_wait, _handle, _self_check, _clear_err, _err, _reset };
