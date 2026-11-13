#pragma once
#include "st_runtime.h"
#include "plc_generated.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

// 原样执行ST；这里只模拟NC功能块的反馈与1ms时钟，不模拟驱动动力学。
struct PlcFixture
{
    Node G;
    PLCHandoff txn{G};
    PLChandle handle{G};
    std::array<bool, 8> external{};
    std::array<bool, 8> moving{};
    std::array<int, 8> starts{};
    int fault_axis = 0;
    int fault_kind = 0; // 1=NC错误，2=中止，3=不完成，4=停用失败，5=重新使能失败
    int stop_calls = 0;
    int aligned_enables = 0;
    bool interrupted = false;
    bool timeout_interrupt = false;
    bool ever_started = false;
    double maximum_enable_delta = 0;

    PlcFixture()
    {
        initPLC(G);
        G["gen_state"] = _handle;
        G["host_session_id"] = 1;
        G["host_comm_timeout"] = false;
        G["self_check_done"] = true;
        for (int i = 1; i <= 7; ++i)
        {
            G["init_pos"][i] = 0;
            G["axis"][i]["NcToPlc"]["ActPos"] = i == 6 ? 120 : 20;
            G["power_output"][i]["Done"] = true;
            G["SetPointGenDisable"][i]["Enabled"] = true;
            G["SetPointGenEnable"][i]["Done"] = true;
            external[i] = true;
            handle.fb_return_move_abs[i].hook = [this, i] { move(i); };
            handle.return_clear_stop[i].hook = [this, i] { stop(i, handle.return_clear_stop[i]); };
            handle.fb_host_return_stop[i].hook = [this, i] { stop(i, handle.fb_host_return_stop[i]); };
        }
        G["cylinder1_value"] = 450; G["cylinder2_value"] = 1000;
        G["cylinder3_value"] = 550; G["cylinder4_value"] = 1000;
        handle.handoff.hook = [this]
        {
            txn.ready = handle.handoff["ready"];
            txn.interrupted = handle.handoff["interrupted"];
            txn.leg_cmd = handle.handoff["leg_cmd"];
            txn.leg_error = handle.handoff["leg_error"];
            txn.cycle();
            handle.handoff["leg_cmd"] = txn.leg_cmd;
            handle.handoff["leg_error"] = txn.leg_error;
            handle.handoff["freeze"] = txn.freeze;
            handle.handoff["abort_motion"] = txn.abort_motion;
        };
        for (int i = 0; i < 4; ++i) cycle();
    }
    int phase() { return int(G["handoff_status"]["phase"]); }
    int result() { return int(G["handoff_status"]["result"]); }
    void request(int mask, unsigned id = 1)
    {
        auto& c = G["handoff_cmd"];
        c["leg_mask"] = mask;
        for (int slot = 1; slot <= 2; ++slot)
        {
            c["target_abs"][slot] = slot == 1 ? 25 : 125;
            c["velocity"][slot] = 110;
            c["acc"][slot] = 4800; c["dec"][slot] = 4800; c["jerk"][slot] = 70000;
            for (int j = 1; j <= 4; ++j) c["wait_ms"][slot][j] = 3 * j + slot;
        }
        c["cyl_clamp"][1] = 5; c["cyl_clamp"][2] = 1000;
        c["cyl_clamp"][3] = 50; c["cyl_clamp"][4] = 1000;
        c["cyl_open"][1] = 450; c["cyl_open"][2] = 5;
        c["cyl_open"][3] = 550; c["cyl_open"][4] = 5;
        c["request_id"] = id;
    }
    void move(int i)
    {
        auto& fb = handle.fb_return_move_abs[i];
        if (!fb["Execute"]) { fb["Done"] = false; fb["Error"] = false; fb["CommandAborted"] = false; return; }
        if (!moving[i] && !fb["Done"] && !fb["Error"] && !fb["CommandAborted"]) { moving[i] = true; ++starts[i]; ever_started = true; }
        if (fault_axis == i && fault_kind > 0 && fault_kind <= 3)
        {
            if (fault_kind == 1) { fb["Error"] = true; fb["ErrorID"] = 42; }
            if (fault_kind == 2) fb["CommandAborted"] = true;
            fb["Busy"] = true;
            return;
        }
        const double diff = fb["Position"] - G["axis"][i]["NcToPlc"]["ActPos"];
        const double step = std::clamp(diff, -double(fb["Velocity"]) * .001, double(fb["Velocity"]) * .001);
        auto& nc = G["axis"][i]["NcToPlc"];
        nc["ActPos"] = double(nc["ActPos"]) + step;
        nc["ActVelo"] = std::abs(diff) <= double(fb["Velocity"]) * .001 ? 0 : step * 1000;
        fb["Done"] = std::abs(diff) <= double(fb["Velocity"]) * .001;
        fb["Busy"] = !fb["Done"];
        if (fb["Done"]) moving[i] = false;
    }
    void stop(int i, Node& fb)
    {
        fb["Done"] = double(fb["Execute"]);
        if (fb["Execute"])
        {
            ++stop_calls;
            moving[i] = false;
            G["axis"][i]["NcToPlc"]["ActVelo"] = 0;
        }
    }
    void cycle()
    {
        ++st_tick;
        G["host_heartbeat_sequence"] = double(st_tick);
        if (interrupted && !timeout_interrupt)
        {
            G["gen_state"] = _err;
            call(handle.handoff, {{"ready", false}, {"interrupted", true}});
            for (int i = 1; i <= 7; ++i) { moving[i] = false; G["axis"][i]["NcToPlc"]["ActVelo"] = 0; }
            return;
        }
        G["gen_state"] = _handle;
        if (timeout_interrupt && interrupted) G["host_comm_timeout"] = true;
        handle.cycle();
        for (int i = 1; i <= 7; ++i)
        {
            auto& en = G["SetPointGenEnable"][i];
            auto& dis = G["SetPointGenDisable"][i];
            if (dis["Execute"] && !(i == fault_axis && fault_kind == 4)) external[i] = false;
            dis["Done"] = dis["Execute"] && !external[i];
            dis["Enabled"] = external[i];
            dis["Error"] = dis["Execute"] && (i == fault_axis && fault_kind == 4);
            if (en["Execute"] && !(i == fault_axis && fault_kind == 5))
            {
                if (!external[i] && ever_started && (i == 1 || i == 6))
                {
                    ++aligned_enables;
                    const double delta = std::abs(G["axis"][i]["PlcToNc"]["ExtSetPos"] - G["axis"][i]["NcToPlc"]["ActPos"]);
                    maximum_enable_delta = std::max(maximum_enable_delta, delta);
                }
                external[i] = true;
            }
            en["Done"] = en["Execute"] && external[i];
            en["Error"] = en["Execute"] && i == fault_axis && fault_kind == 5;
        }
    }
    void recover()
    {
        interrupted = false;
        fault_kind = 0;
        if (timeout_interrupt) G["host_recover_req"] = true;
        else G["handle_reinit_req"] = true;
    }
    void until(int wanted, int limit = 5000)
    {
        for (int i = 0; i < limit && phase() != wanted; ++i) cycle();
        if (phase() != wanted) throw std::runtime_error("phase timeout " + std::to_string(phase()) + " wanted " + std::to_string(wanted));
    }
};
