"""离线验证夹爪时序与取消保护；窗口/轴4分支由 run_dual_control_test.py 覆盖。"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'main.cpp').read_text(encoding='utf-8-sig')
clamp_start = source.index('cylinder3_cmd = (restore_clamps && planned_return.restore_stage')
clamp_end = source.index(';', source.index('cylinder4_cmd =', clamp_start)) + 1
clamps = source[clamp_start:clamp_end]
guard_start = source.index('if (planned_return.active() &&\n')
guard_end = source.index('\n\t\t}', guard_start) + len('\n\t\t}')
guard = source[guard_start:guard_end]
prefix = r'''
#include <algorithm>
#include <cassert>
#include <iostream>
using DWORD = unsigned long;
'''
wait_start = source.index('auto planned_return_settle_wait_ms =')
wait_end = source.index('auto apply_planned_return_outputs =', wait_start)
wait_code = source[wait_start:wait_end]
protection = r'''
bool cancelled=false;
bool cancel_active_return_motion(bool){cancelled=true;return true;}
void check_guards(){
 struct {int clamp_stage=0,restore_stage=0;bool active(){return true;}} planned_return;
 struct {int cyl3_open=450,cyl3_clamp=50,cyl4_open=5,cyl4_clamp=500;} cyl;
 int cylinder3_cmd=0,cylinder4_cmd=0;bool restore_clamps=false;
 auto output=[&](){
''' + clamps + r'''
 };
 output();assert(cylinder3_cmd==50 && cylinder4_cmd==500);
 planned_return.clamp_stage=1;output();assert(cylinder3_cmd==50 && cylinder4_cmd==5);
 restore_clamps=true;output();assert(cylinder3_cmd==50 && cylinder4_cmd==500);
 planned_return.restore_stage=1;output();assert(cylinder3_cmd==450 && cylinder4_cmd==500);
 bool control_active=true,connection_hold_active=false,estop_hold_active=false,return_ads_fault_hold=false,
 startup_sequence_active=false,axis6_soft_limit_hold=false;
 struct {bool requested=false;bool active(){return false;}} spacing_recovery;
 auto safety=[&](){cancelled=false;
''' + guard + r'''
 };
 safety();assert(!cancelled);
 bool* faults[]={&connection_hold_active,&estop_hold_active,&return_ads_fault_hold,&startup_sequence_active,&axis6_soft_limit_hold};
 for(auto f:faults){*f=true;safety();assert(cancelled);*f=false;}
 control_active=false;safety();assert(cancelled);
 std::cout<<"PASS: actual clamp outputs and stop/estop/soft-limit cancellation guard\n";
}
'''
timing = r'''
void check_waits() {
 struct { bool axis1=false,axis6=false;
  bool contains_axis(int axis) const { return axis==0 ? axis1 : axis6; }
 } planned_return;
 // 使用不同的值，避免两缸参数相同掩盖选择错误。
 struct {
  DWORD cyl1_release_wait_ms=11,cyl1_clamp_wait_ms=17;
  DWORD cyl2_release_wait_ms=23,cyl2_clamp_wait_ms=29;
  DWORD cyl3_release_wait_ms=31,cyl3_clamp_wait_ms=37;
  DWORD cyl4_release_wait_ms=41,cyl4_clamp_wait_ms=43;
 } cfg;
''' + wait_code + r'''
 DWORD axis1[]={17,23,29,11},axis6[]={37,41,43,31};
 for (int mask=0;mask<4;++mask) {
  planned_return.axis1=(mask&1)!=0; planned_return.axis6=(mask&2)!=0;
  for (int phase=0;phase<4;++phase) {
   DWORD expected=std::max(planned_return.axis1?axis1[phase]:0UL,
                           planned_return.axis6?axis6[phase]:0UL);
   assert(planned_return_settle_wait_ms(phase>=2,phase%2)==expected);
  }
 }
 std::cout<<"PASS: 8 cylinder wait parameters, single/combined axes and 4 handoff stages\n";
}
int main(){check_guards();check_waits();}
'''
# 状态机仍等待输出实际落地以及按电缸区分的夹紧/释放等待时间。
assert 'planned_return.clamp_output_applied &&\n\t\t\t\t\t\t(now_ms - planned_return.phase_t0_ms) >= clamp_wait_ms' in source
for name in (
    'cyl1_release_wait_ms', 'cyl1_clamp_wait_ms',
    'cyl2_release_wait_ms', 'cyl2_clamp_wait_ms',
    'cyl3_release_wait_ms', 'cyl3_clamp_wait_ms',
    'cyl4_release_wait_ms', 'cyl4_clamp_wait_ms',
):
    assert f'cfg.{name}' in source
vcvars = Path(r'C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat')
with tempfile.TemporaryDirectory(prefix='axis4-check-') as tmp:
    tmp = Path(tmp)
    (tmp / 'check.cpp').write_text(prefix + protection + timing, encoding='utf-8')
    command = f'@call "{vcvars}" >nul\n@cl /nologo /EHsc /utf-8 check.cpp /Fe:check.exe\n@if errorlevel 1 exit /b 1\n@check.exe\n'
    (tmp / 'check.cmd').write_text(command, encoding='ascii')
    subprocess.run(['cmd', '/d', '/c', 'check.cmd'], cwd=tmp, check=True)
