"""抽取完整生产主循环；仅虚拟时钟、ADS/网络/文件旁路，控制模块原样编译。"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, sys, tempfile
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ADS = ROOT / '介入机器人/B数据端/工程/ADS项目'
p = argparse.ArgumentParser()
p.add_argument('--record', action='store_true')
p.add_argument('--events', action='store_true', help='另运行事件专用输入并验证完整事件序列')
p.add_argument('--terminal', action='store_true', help='首次换手后静止手柄，另存接管状态供新旧终态比较')
p.add_argument('--scenarios', nargs='+')
p.add_argument('--golden', type=Path, default=HERE/'golden')
p.add_argument('--output', type=Path, default=ROOT/'docs/refactor_stage1/logs/baseline')
args = p.parse_args()
OUT = args.output.resolve()
OUT.mkdir(parents=True, exist_ok=True)
BUILD = Path(tempfile.mkdtemp(prefix='refactor-baseline-'))
manifest = {}
def read(name):
    b = (ADS/name).read_bytes()
    manifest[name] = hashlib.sha256(b).hexdigest()
    return b.decode('utf-8-sig').replace('\r\n','\n')
def write(name, text):
    dest = BUILD/name
    dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_text(text,encoding='utf-8')
# 只重定向 I/O 和时钟；其余代码按当前磁盘版本原样复制。
for f in ADS.glob('*.h'):
    write(f.name,read(f.name))
for f in (ADS/'手柄').glob('*.h'):
    write('手柄/'+f.name,read('手柄/'+f.name))
for f in ['control_types.cpp','motion_sync.cpp','plc_io.cpp','force_feedback.cpp',
          'delivery_tracking.cpp','guidewire_mode.cpp','startup_sequence.cpp','手柄/手柄.cpp']:
    if (ADS/f).exists(): write(f,read(f))
# 完整 main：改名避免设备进程入口；异常结束虚拟输入，退出清理不参与测试。
source = read('main.cpp').replace('int main()', 'int control_main()',1)
source = source.replace('#include "handle_velocity_recorder.h"','#include "baseline_velocity.h"')
source = re.sub(r'std::chrono::duration_cast<std::chrono::milliseconds>\(\s*std::chrono::steady_clock::now\(\).time_since_epoch\(\)\).count\(\)', 'baseline::now_ms', source)
# 在每拍所有输出、基准更新和命令处理之后观察，包含本拍公开状态和主要内部状态。
end = source.rfind('\n\t}\n')
trace = """
        baseline_capture(ads_output, {
            double(control_active), double(initial_sync_done), double(connection_hold_active),
            double(ads_soft_hold_active), double(handle_soft_hold_active), double(estop_hold_active),
            double(return_ads_fault_hold), double(startup.completed), double(startup.phase == StartupPhase::Done),
            double(planned_return.phase), 0.0, 0.0,
            double(planned_return.leg_count), 0.0,
            double(dual_handoff.active()), double(dual_reverse_active), double(spacing_recovery.phase),
            double(spacing_recovery.requested), spacing_recovery.pending_delta_mm, spacing_recovery.moved_mm,
            double(axis1_delivery_mapping_active), axis1_delivery_mapping_progress_mm, axis1_delivery_mapping_applied_extra_mm,
            axis1_follow_cmd_abs, axis6_follow_cmd_abs, axis3_base_rel, axis5_base_rel,
            ff.force_582_f, ff.force_582_n, ff.force_587_f, ff.force_587_n,
            ff.force_582_filtered_f, ff.force_587_filtered_f,
            double(planned_return.contains_axis(0)), double(planned_return.contains_axis(5)),
            axis1_net_distance.net_mm(), axis6_net_distance.net_mm(),
            0.0, 0.0,
            0.0, 0.0,
            double(dual_handoff.axis1), double(dual_handoff.axis6), double(dual_handoff.wire_caused),
            double(dual_handoff.gap_boundary), double(homing_restore_applied), double(homing_restore_submitted),
            double(ff.hold_587.active), double(ff.hold_587.blending)
        });
"""
source=source[:end]+trace+source[end:]
source=source.replace(trace,trace+(ROOT/'tools/handoff_tests/endpoint_trace.inc').read_text(encoding='utf-8'))
write('main.cpp','#include "baseline_support.h"\n'+source)
# 使用生产结构；替身不包含设备调用或设备 DLL。
ads = read('ads_communication.h')
write('ads_communication.h', ads[:ads.index('class AdsCommunicationService\n')]+(HERE/'mock_ads.h').read_text(encoding='utf-8-sig'))
rec = read('experiment_recorder.h')
write('experiment_recorder.h',rec[:rec.index('class ExperimentRecorder\n')]+(HERE/'mock_recorder.h').read_text(encoding='utf-8-sig'))
arm = read('arm_manual_ads_service.h')
arm=arm[:arm.index('// 定位臂只通过 ADS')].replace('#include "arm_cartesian_motion.h"','')
write('arm_manual_ads_service.h',arm+(HERE/'mock_arm.h').read_text(encoding='utf-8-sig'))
write('zenoh_service.h',(HERE/'mock_zenoh.h').read_text(encoding='utf-8-sig'))
write('ADSComm1.h',(HERE/'mock_cads.h').read_text(encoding='utf-8-sig'))
for name in ['baseline_support.h','virtual_clock.h','baseline_velocity.h','memory_io.cpp']:
    write(name,(HERE/name).read_text(encoding='utf-8-sig'))
memory=(BUILD/'memory_io.cpp').read_text(encoding='utf-8')
support=(ROOT/'tools/handoff_tests/endpoint_support.cpp').read_text(encoding='utf-8')
memory=memory.replace('void advance() {',support+'\nvoid advance() {')
memory=memory.replace('const bool pushing=tick>=12 && tick<260 && !(scenario.find("txn_")==0 && (commits>0 || scenario=="txn_start_wait"));', 'const bool pushing=tick>=12 && tick<260 && !(scenario.find("txn_")==0 && (commits>0 || scenario=="txn_start_wait")) && !(std::getenv("BASELINE_ENDPOINT") && endpoint_started);')
memory=memory.replace('void baseline_record_event(ExperimentEventCode code,int value,std::uint64_t sequence) {','void baseline_endpoint(std::initializer_list<double> values) { endpoint_capture(values); }\nvoid baseline_record_event(ExperimentEventCode code,int value,std::uint64_t sequence) {\n    if(int(code)==19) endpoint_started=true;')
write('memory_io.cpp',memory)
# 翻译当前PLC执行器与handle全文，闭环不使用旧Req内存模型。
SUPPORT=ROOT/'介入机器人/tools/reliability_round5'
sys.path.insert(0,str(SUPPORT))
from translate_st import translate, declaration
import xml.etree.ElementTree as ET
PLC=ROOT/'介入机器人/C从端/250902/Untitled2'
generated=['#pragma once']
for name in ['Handoff','handle']:
    generated.append(translate((PLC/('POUs/'+name+'.TcPOU')).read_bytes(),'PLC'+name))
_,init=declaration(ET.parse(PLC/'GVLs/G.TcGVL').find('.//Declaration').text,'G')
generated+=['inline void initPLC(Node& G) {']+init+['}']
write('plc_generated.h','\n'.join(generated))
write('st_runtime.h',(SUPPORT/'st_runtime.h').read_text(encoding='utf-8-sig'))
write('st_plc_fixture.h',(ROOT/'tools/handoff_tests/st_plc_fixture.h').read_text(encoding='utf-8-sig'))
# Handle.h 的历史位置在手柄目录。
vswhere=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
vs=Path(subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip())
sources=['main.cpp','control_types.cpp','motion_sync.cpp','plc_io.cpp','force_feedback.cpp','手柄/手柄.cpp','memory_io.cpp']
sources += [f for f in ['delivery_tracking.cpp','guidewire_mode.cpp','startup_sequence.cpp'] if (BUILD/f).exists()]
cmd='@echo off\r\nchcp 65001 >nul\r\ncall "'+str(vs/'VC/Auxiliary/Build/vcvars64.bat')+'" >nul\r\nif errorlevel 1 exit /b 1\r\n'
cmd+='cl /nologo /std:c++17 /EHsc /utf-8 /W3 /WX /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /FIvirtual_clock.h /I. /I手柄 '+' '.join('"'+s+'"' for s in sources)+' /Fe:baseline.exe\r\nexit /b %errorlevel%\r\n'
(BUILD/'build.cmd').write_text(cmd,encoding='utf-8')
r=subprocess.run(['cmd','/d','/c','build.cmd'],cwd=BUILD,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(OUT/'build.log').write_bytes(r.stdout)
if r.returncode: print(r.stdout.decode('utf-8',errors='replace'));sys.exit(r.returncode)
scenarios=['follow_delivery','follow_retraction','lead_hold','lead_axis6','axis4','handoff_axis1','handoff_active','handoff_passive','handoff_merge','return_axis1','return_axis6','return_both','return_retry','return_possible','return_fault','return_estop','spacing','spacing_hold','ads_hold','handle_gap_short','handle_gap_rebase','handle_disconnect','device_epoch','restart','restart_accept','restart_reject','manual','feedback']
scenarios += [kind+'_'+legs for kind in ['return_retry','return_possible','return_fault'] for legs in ['axis6','both']]
if args.events: scenarios += ['event_rejected_hold','event_missing582','txn_no_ack']
if args.scenarios: scenarios=args.scenarios
results=[]
for scenario in scenarios:
    env=dict(os.environ, BASELINE_SCENARIO=scenario,BASELINE_OUTPUT=str(OUT/(scenario+'.csv')))
    if args.terminal: env['BASELINE_ENDPOINT']='1'
    for f in ['homing_record.json', 'reliability_config.json']:
        if (BUILD/f).exists(): (BUILD/f).unlink()
    if scenario.startswith('restart'):
        (BUILD/'reliability_config.json').write_text('{"homing_reuse_enabled":true}',encoding='utf-8')
        (BUILD/'homing_record.json').write_text(json.dumps(dict(leftlimit=[0]*7,init_pos=[0]*7,completed_time_unix_ms=1,last_abs=[18,12,100,0,110,123,34],last_abs_time_unix_ms=1,application='MemoryPLC')),encoding='utf-8')
    r=subprocess.run([str(BUILD/'baseline.exe')],cwd=BUILD,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (OUT/(scenario+'.log')).write_bytes(r.stdout)
    if r.returncode: print(scenario,r.stdout.decode('utf-8',errors='replace'));sys.exit(r.returncode)
    actual=OUT/(scenario+'.csv')
    golden=args.golden.resolve()/(scenario+'.csv')
    if args.record:
        if golden.exists(): raise RuntimeError('拒绝覆盖已有黄金文件: '+str(golden))
        golden.parent.mkdir(exist_ok=True);shutil.copy2(actual,golden)
    # 场景是否发生换手由逐拍状态判定，不能按场景名字豁免差异。
    sys.path.insert(0,str(ROOT/'tools/handoff_tests'))
    from verify_baseline import compare_prefix
    comparison = compare_prefix(actual,golden) if not args.terminal and golden.exists() else {}
    # Git在Windows检出时可转换行尾；控制值仍逐列精确比较。
    equal=actual.read_bytes().replace(b'\r\n',b'\n')==golden.read_bytes().replace(b'\r\n',b'\n') if golden.exists() else False
    results.append(dict(scenario=scenario,ticks=len(actual.read_text().splitlines())-1,sha256=hashlib.sha256(actual.read_bytes()).hexdigest(),equal=equal,comparison=comparison))
(OUT/'result.json').write_text(json.dumps(dict(build=str(BUILD),sources=manifest,scenarios=results),ensure_ascii=False,indent=2),encoding='utf-8')
print('闭环构建:',BUILD)
print('列文本相同（忽略Git行尾）:',[x['scenario'] for x in results if x['equal']])
print('有差异:',[x['scenario'] for x in results if not x['equal']])
if not args.scenarios and not args.terminal:
    from verify_coverage import verify
    verify(OUT)
if args.events and not args.scenarios:
    from verify_events import verify
    verify(OUT)
