from pathlib import Path
import json, re
ROOT=Path(__file__).resolve().parents[3]
D=ROOT/'介入机器人/B数据端/工程/ADS项目'
P=ROOT/'介入机器人/C从端/250902/Untitled2'
main=(D/'main.cpp').read_text(encoding='utf-8-sig').splitlines()
z=(D/'zenoh_service.cpp').read_text(encoding='utf-8-sig')
h=(D/'control_messages.h').read_text(encoding='utf-8-sig')
names=[]
cases=[(i+1,re.search(r'case VisCommandType::([A-Za-z0-9_]+)',line)[1]) for i,line in enumerate(main) if 'case VisCommandType::' in line]
command_table=[]
for j,(a,n) in enumerate(cases):
    b=cases[j+1][0]-1 if j+1<len(cases) else 5395
    public=any(('VisCommandType::'+n+c) in z for c in [',',';',')'])
    command_table.append(dict(name=n,start=a,end=b,lines=b-a+1,public=public))
counts={}
for f in ['POUs/move_edge.TcPOU','POUs/handle(备份).TcPOU','DUTs/state.TcDUT','DUTs/ST_AxisPlannedReturnCmd.TcDUT']:
    lines=(P/f).read_text(encoding='utf-8-sig').splitlines()
    counts[f]=len(lines)
calls=[]
for n in ['begin_direct_positioning','start_startup_sequence','direct_positioning = true','emergency_retract_active = true']:
    calls.append(dict(symbol=n,lines=[i+1 for i,l in enumerate(main) if n in l]))
tests=[]
for p in (ROOT/'介入机器人').rglob('*'):
    if not p.is_file() or p.suffix not in ('.cpp','.cjs','.py','.cs') or 'docs' in p.parts: continue
    if not ('test' in p.name.lower() or p.name.startswith('run')): continue
    s=p.read_text(encoding='utf-8-sig',errors='replace')
    hits=[dict(line=i+1,text=l.strip()) for i,l in enumerate(s.splitlines()) if any(n in l for n in ['ExecuteStartup','EmergencyRetract','SetAxis1PostReturnLead','SetTrackingCompensation','SetReverseMode','RequestModeSwitch','begin_direct_positioning','startup_sequence.cpp','startup_sequence.h','main_startup','start_startup_sequence','main_mode.inc','main_mapping','mapping_helpers','SetGravityCompensation','SetCleanForceMonitor','SetArmJogParameter','move_edge','handle(备份)'])]
    if hits: tests.append(dict(file=p.relative_to(ROOT).as_posix(),hits=hits))
out=dict(commands=command_table,counts=counts,calls=calls,tests=tests)
(Path(__file__).parent/'entry_gate_inventory.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(out,ensure_ascii=False,indent=2))
