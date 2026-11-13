// 直接验证生产恢复对象；事务与急停逐拍路径由完整主循环+ST闭环覆盖。
#include "../return_recovery.h"
#include <cassert>
#include <iostream>

int main()
{
    int checks=0;
    auto check=[&](bool value) { ++checks; assert(value); };
    ReturnRecoveryConditions ready;
    ready.ads_running=ready.snapshot_valid=ready.handle_reinit_done=ready.handles_valid=ready.handoff_idle=true;
    for (std::uint8_t mask : {1,2,3})
    {
        ReturnRecovery recovery;
        recovery.original_trigger(mask);
        check(!recovery.can_restore(ready));
        recovery.interrupted("PLC已安全收尾");
        check(recovery.mask()==mask && recovery.reason()=="PLC已安全收尾");
        for (int gate=0;gate<9;++gate)
        {
            auto blocked=ready;
            switch(gate)
            {
            case 0: blocked.ads_running=false; break;
            case 1: blocked.snapshot_valid=false; break;
            case 2: blocked.handle_reinit_done=false; break;
            case 3: blocked.handles_valid=false; break;
            case 4: blocked.handoff_idle=false; break;
            case 5: blocked.estop=true; break;
            case 6: blocked.host_timeout=true; break;
            case 7: blocked.startup_active=true; break;
            case 8: blocked.exclusive_task=true; break;
            }
            check(!recovery.can_restore(blocked));
        }
        check(recovery.can_restore(ready));
        recovery.restored(); check(recovery.resume_pending());
        recovery.consume_resume(); check(recovery.resumes()==1 && !recovery.resume_pending());
        recovery.interrupted("续做再次失败"); check(recovery.can_restore(ready));
        recovery.restored(); check(!recovery.resume_pending());
        recovery.completed(); recovery.original_trigger(mask); recovery.interrupted("新一轮");
        recovery.restored(); check(recovery.resume_pending() && recovery.resumes()==0);
    }
    ReturnRecovery finished;
    finished.original_trigger(1); finished.interrupted("已完成",false); finished.restored();
    check(!finished.resume_pending());
    std::cout << "PASS recovery checks=" << checks << '\n';
}
