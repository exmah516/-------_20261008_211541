#include "st_plc_fixture.h"
#include <cstdlib>
#include <map>

static int checks = 0;
static void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { std::cerr << "FAIL " << label << '\n'; std::exit(1); }
}
static void normal(int mask)
{
    PlcFixture p;
    p.request(mask);
    std::map<int, int> duration;
    while (p.phase() != 60)
    {
        p.cycle();
        int phase = p.phase();
        ++duration[phase];
        for (int slot = 1; slot <= 2; ++slot)
        {
            const bool part = mask == 3 || mask == slot;
            const int holder = 2 * slot - 1, mover = holder + 1;
            const std::string hs = "cylinder" + std::to_string(holder) + "_value";
            const std::string ms = "cylinder" + std::to_string(mover) + "_value";
            const double h = part && phase < 50 ? (slot == 1 ? 5 : 50) : (slot == 1 ? 450 : 550);
            const double m = part && (phase == 20 || phase == 30) ? 5 : 1000;
            check(p.G[hs.c_str()] == h && p.G[ms.c_str()] == m, "four cylinder stage table");
        }
        if (phase == 30) { p.G["handoff_cmd"]["target_abs"][1] = 77; p.G["handoff_cmd"]["cyl_clamp"][2] = 17; }
        check(duration[phase] < 3000, "transaction bounded in model");
    }
    check(p.result() == 1, "normal success");
    for (int index : {1, 2, 4, 5})
    {
        int wait_slot = index <= 2 ? index : index - 1;
        int expected = 3 * wait_slot + (mask == 1 ? 1 : 2);
        check(p.G["handoff_status"]["phase_ms"][index] == expected, "wait exact milliseconds and maximum over legs");
    }
    check(p.maximum_enable_delta == 0, "aligned prefeed before enable");
    if (mask & 1) check(p.G["axis"][1]["NcToPlc"]["ActPos"] == 25, "latched target axis1");
    if (mask & 2) check(p.G["axis"][6]["NcToPlc"]["ActPos"] == 125, "latched target axis6");
    auto frozen = p.G["refer"];
    for (int n = 0; n < 10; ++n)
    {
        p.G["refer"][1] = 70; p.G["refer"][6] = 170;
        p.cycle();
        if (mask & 1) check(p.G["refer"][1] == frozen[1], "axis1 freeze");
        if (mask & 2) check(p.G["refer"][6] == frozen[6], "axis6 freeze");
        if (!(mask & 1)) check(p.G["refer"][1] == 70, "nonparticipant axis1 accepts refer");
        if (!(mask & 2)) check(p.G["refer"][6] == 170, "nonparticipant axis6 accepts refer");
    }
    check(p.starts[1] == ((mask & 1) ? 1 : 0) && p.starts[6] == ((mask & 2) ? 1 : 0), "idempotent same request");
    auto cylinders = p.txn.outputs;
    p.G["handoff_takeover_id"] = 1;
    p.cycle();
    check(p.phase() == 0, "takeover releases");
    for (int i = 1; i <= 4; ++i) check(p.G["cyl_cmd"][i] == cylinders[i], "takeover cylinder no jump");
}
int main()
{
    try
    {
        for (int mask : {1, 2, 3}) normal(mask);
        for (int kind : {1, 2, 3, 4})
        {
            PlcFixture p; p.request(3); p.fault_axis = 1; p.fault_kind = kind;
            p.until(60);
            check(p.result() == 2, "failure safely finalized");
            check(p.G["handoff_status"]["error_code"] == (kind == 3 ? 4 : kind == 4 ? 2 : 3), "classified failure");
            check(p.G["cylinder1_value"] == 450 && p.G["cylinder2_value"] == 1000 && p.G["cylinder3_value"] == 550 && p.G["cylinder4_value"] == 1000, "failure normal grip");
            check(p.external[1] && p.external[6], "failure external generator enabled");
            check(p.maximum_enable_delta == 0, "failure aligned enable");
            if (kind != 4) check(p.stop_calls > 0, "controlled stop on motion failure");
        }
        for (bool timeout : {false, true}) for (int phase : {10, 20, 30, 40, 50})
        {
            PlcFixture p; p.request(3); p.until(phase);
            auto outputs = p.txn.outputs;
            p.interrupted = true; p.timeout_interrupt = timeout;
            for (int i = 0; i < 10; ++i) p.cycle();
            for (int i = 1; i <= 4; ++i) check(p.txn.outputs[i] == outputs[i], "interrupt preserves cylinder ownership");
            p.recover(); p.until(60);
            check(p.result() == 2 && p.G["handoff_status"]["error_code"] == 5, "interruption cleanup result");
            check(p.G["cylinder1_value"] == 450 && p.G["cylinder3_value"] == 550, "interruption holder release");
        }
        {
            PlcFixture p; p.request(3); p.until(20); p.request(3, 2); p.cycle();
            check(p.G["handoff_status"]["ack_id"] == 2 && p.G["handoff_status"]["done_id"] == 2 && p.G["handoff_status"]["reject_reason"] == 1, "busy rejection");
            p.request(3, 1); p.cycle();
            check(p.txn.active_id == 1 && p.phase() == 20, "active id resend stays idempotent after another id rejection");
            p.until(60); check(p.G["handoff_status"]["done_id"] == 1 && p.result() == 1, "busy rejection preserves current transaction");
            p.G["handoff_takeover_id"] = 1; p.cycle();
            for (int n = 0; n < 20; ++n) p.cycle();
            check(p.phase() == 0 && p.starts[1] == 1 && p.starts[6] == 1, "acknowledged id cannot restart after busy rejection and takeover");
        }
        for (int mask : {0, 4})
        {
            PlcFixture p; p.request(mask); p.cycle();
            check(p.phase() == 0 && p.result() == 2 && p.G["handoff_status"]["error_code"] == 1, "invalid mask rejected");
        }
        {
            PlcFixture p; p.request(1); p.G["handoff_cmd"]["velocity"][1] = 0; p.cycle();
            check(p.phase() == 0 && p.result() == 2, "invalid dynamics rejected");
        }
        {
            PlcFixture p; p.request(1); p.interrupted = true; p.cycle();
            check(p.phase() == 0 && p.result() == 2 && p.G["handoff_status"]["reject_reason"] == 2, "wrong state rejected");
        }
        {
            PlcFixture p; p.request(1); p.txn.leg_cmd[6]["Busy"] = true; p.cycle();
            check(p.phase() == 10, "only participating leg must be idle");
        }
        {
            PlcFixture p; p.request(3); p.until(30);
            auto outputs = p.txn.outputs;
            double before = p.txn.tick;
            p.G["handle_reinit_req"] = true; p.cycle();
            check(p.txn.tick == before + 1, "reinit calls executor once per millisecond");
            for (int i = 1; i <= 4; ++i) check(p.txn.outputs[i] == outputs[i], "reinit holds cylinder outputs");
            p.until(60);
            check(p.result() == 2 && p.G["handoff_status"]["error_code"] == 5, "reinit finishes as interrupted");
        }
        {
            PlcFixture p; p.request(1); p.until(30); p.fault_axis = 1; p.fault_kind = 5;
            for (int i = 0; i < 4000 && p.G["gen_state"] == _handle; ++i) p.cycle();
            check(p.G["gen_state"] == _err && p.G["estop_hold_req"], "enable timeout retains original err transition");
        }
        std::cout << "PASS PLC ST transaction checks=" << checks << '\n';
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
