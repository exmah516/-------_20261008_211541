// 直接验证生产手柄与桥接内存，不加载手柄 DLL、不连接设备。
#include "../手柄/Handle.h"
#include "../手柄/remote_handle_bridge.h"
#include <cstdlib>
#include <iostream>

static void check(bool ok, const char* label)
{
    std::cout << (ok ? "PASS: " : "FAIL: ") << label << std::endl;
    if (!ok) std::exit(1);
}

static bool output_cleared(int slot)
{
    const auto out = remote_handle_bridge::get_output(slot);
    return !out.enable && out.axis == 0 && out.force == 0.0 && out.torque == 0.0;
}

int main()
{
    using namespace remote_handle_bridge;
    clear_samples();
    clear_outputs();
    Handle a(582), b(587), unknown(999);
    check(slot_of(582) == 0 && slot_of(587) == 1 && slot_of(999) == -1,
          "physical serial numbers map to distinct slots");
    check(!a.init() && !a.is_open() && !a.poll() && !unknown.init(),
          "missing samples and unknown serial numbers cannot open a handle");
    a.setforce_axis(4.0, 1, 2.0);
    check(output_cleared(0), "an unopened handle cannot send force");

    Sample sa;
    sa.valid = true;
    sa.buttons = 5;
    sa.encoders[0] = 12;
    sa.encoders[1] = -34;
    sa.joints[0] = 0.25f;
    sa.joints[1] = -0.5f;
    sa.vels[0] = 1.5f;
    sa.vels[1] = -2.5f;
    Sample sb = sa;
    sb.buttons = 2;
    sb.encoders[0] = 56;
    sb.joints[0] = 0.75f;
    put_sample(0, sa);
    put_sample(1, sb);
    check(a.init() && b.init() && a.poll() && b.poll(),
          "fresh samples open both remote handles");
    check(a.serial() == 582 && b.serial() == 587 && a.buttons2 == 5 && b.buttons2 == 2 &&
          a.encoders2[0] == 12 && a.encoders2[1] == -34 && b.encoders2[0] == 56 &&
          a.fJoints2[0] == 0.25 && a.fJoints2[1] == -0.5 && b.fJoints2[0] == 0.75 &&
          a.fVels2[0] == 1.5 && a.fVels2[1] == -2.5 &&
          a.objData.buttons2 == a.buttons2 && a.objData.encoders2[1] == -34,
          "sampling keeps buttons, encoders, joints and velocities isolated by serial number");
    check(!unknown.init(), "unknown serial number stays rejected while valid slots exist");

    a.setforce_axis(4.0, 1, 2.0);
    auto out = get_output(0);
    check(out.enable && out.axis == 1 && out.force == 4.0 && out.torque == 2.0 && output_cleared(1),
          "force and torque are returned through the matching bridge slot");
    b.setforce_axis(-3.0, 9, -1.0);
    out = get_output(1);
    check(out.enable && out.axis == 2 && out.force == -3.0 && out.torque == -1.0 && get_output(0).force == 4.0,
          "second handle force is isolated and its axis is clamped");
    a.setforce_axis(1.0, -1, 0.0);
    check(get_output(0).axis == 0, "negative force axis is clamped to zero");
    a.setforce(2.0, 0.5);
    out = get_output(0);
    check(out.axis == 0 && out.force == 2.0 && out.torque == 0.5,
          "default force uses axis zero");

    // 直接调整已有内存中的接收时刻，避免真实等待造成测试时序波动。
    {
        auto& st = detail::state();
        std::lock_guard<std::mutex> lock(st.m);
        st.samples[0].rx_ms = GetTickCount64() - 500;
    }
    check(a.poll() && b.poll(), "a 500 ms network gap does not disconnect the remote handle");
    check(a.fVels2[0] == 0.0 && a.fVels2[1] == 0.0, "stale sample velocity is not repeated during the gap");
    sa.joints[0] = 8.0f;
    put_sample(0, sa);
    check(a.poll() && a.fJoints2[0] == 8.0 && a.take_rebase_request() && !a.take_rebase_request(),
          "first fresh sample after a gap requests one current-position rebase");
    {
        auto& st = detail::state();
        std::lock_guard<std::mutex> lock(st.m);
        st.samples[0].rx_ms = GetTickCount64() - 3001;
    }
    check(!a.poll() && b.poll(), "a sample older than 3 seconds is still rejected per handle");
    a.close();
    check(!a.is_open() && !a.poll() && a.buttons2 == 0 && a.encoders2[0] == 0 &&
          a.encoders2[1] == 0 && a.fJoints2[0] == 0.0 && a.fJoints2[1] == 0.0 &&
          a.fVels2[0] == 0.0 && a.fVels2[1] == 0.0 && a.objData.buttons2 == 0 &&
          a.objData.encoders2[0] == 0 && a.objData.encoders2[1] == 0 && output_cleared(0) && get_output(1).enable,
          "close clears the handle cache and force without touching the other handle");
    check(!a.init(), "an expired sample cannot reopen the handle");
    put_sample(0, Sample());
    check(!a.init(), "an explicitly invalid sample cannot reopen the handle");
    put_sample(0, sa);
    check(a.init() && a.poll(), "a fresh sample allows reconnection");
    put_sample(0, Sample());
    check(!a.poll(), "an invalid sample interrupts an opened handle");
    a.close();
    a.close();
    a.setforce(8.0, 3.0);
    check(output_cleared(0), "repeated close keeps output cleared and closed handles cannot reenable force");
    b.close();
    unknown.close();
    check(output_cleared(0) && output_cleared(1), "shutdown clears both output slots");
    return 0;
}
