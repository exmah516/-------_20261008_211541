"""只在R2规范报告引用/表格并核对物理行号、片段长度。"""
from pathlib import Path
import json
import re

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'docs/arch_survey_r2'
ADS=ROOT/'介入机器人/B数据端/工程/ADS项目'
MASTER=ROOT/'介入机器人/A主端/工程/master'
PLC=ROOT/'介入机器人/C从端/250902/Untitled2'
ALIASES={
 'D/main':'D/main.cpp','D/force':'D/force_feedback.cpp',
 'D/zenoh':'D/zenoh_service.cpp','D/bridge':'D/手柄/remote_handle_bridge.h',
 'D/handle':'D/手柄/手柄.cpp','D/plc_io':'D/plc_io.cpp',
 'D/control_types':'D/control_types.h','D/return_recovery':'D/return_recovery.h',
 'M/link':'M/VI.Service.Robot/Services/RemoteRobotLink.cs',
 'M/handles':'M/VI.Service.Robot/Services/HandleService.cs',
 'M/vm':'M/VI.Service.Robot/ViewModels/MainViewModel.cs',
 'M/host':'M/VI.HandleHost/Program.cs',
 'M/proto':'M/MasterConsole.Protocol/ProtocolConstants.cs',
 'M/window':'M/VI.Control/MainWindow.xaml',
 'M/intervention':'M/VI.Module.Intervention/InterventionPanel.xaml'}
EXT=r'(?:cpp|cjs|cs|h|TcPOU|TcTTO|TcGVL|TcDUT|tsproj|plcproj|vcxproj|csproj|slnx|ps1|cmd|py|md|log|txt|json|csv|xaml|props|targets)'
NUM=r'\d+(?:[-–]\d+)?(?:[,、]\d+(?:[-–]\d+)?)*'
PATH=r'[A-Za-z0-9_\u4e00-\u9fff./\\()+-]+\.'+EXT
REF=re.compile(r'(?P<path>'+PATH+r'):(?P<nums>'+NUM+r')|(?P<alias>\bG):(?P<anums>'+NUM+r')')
TOKEN=re.compile(r'(?P<path>'+PATH+r'):(?P<nums>'+NUM+r')|(?<=`):(?P<short>'+NUM+r')')

def normalise(doc):
    s=doc.read_text(encoding='utf-8-sig')
    if doc.name=='R2-4_控制门控与可见性.md':
        for short,long in ALIASES.items():
            s=re.sub(re.escape(short)+r'(?=[:\s|])',lambda _:long,s)
    s=re.sub(r'(?<![/\w])zenoh_protocol\.h:', 'protocol/cpp/zenoh_protocol.h:', s)
    last=None
    def expand(m):
        nonlocal last
        if m['path']:
            last=m['path']; return m[0]
        assert last, (doc.name,m[0])
        return last+':'+m['short']
    s=TOKEN.sub(expand,s)
    lines=[]
    fenced=False
    for line in s.splitlines():
        if line.lstrip().startswith('```'): fenced=not fenced
        if not fenced and line.startswith('|'):
            line=re.sub(r'`[^`]+`',lambda m:re.sub(r'(?<!\\)\|',r'\|',m[0]),line)
        lines.append(line)
    doc.write_text('\n'.join(lines)+'\n',encoding='utf-8')

def resolve(name):
    name=name.replace('\\','/')
    if name=='G': return PLC/'GVLs/G.TcGVL'
    for base in (ROOT,OUT,OUT/'scratch'):
        if (base/name).is_file(): return base/name
    prefixes={'D/':ADS,'M/':MASTER,'P/':PLC/'POUs','U/':PLC,'S/':OUT/'scratch',
      'POUs/':PLC/'POUs','GVLs/':PLC/'GVLs','DUTs/':PLC/'DUTs'}
    for key,base in prefixes.items():
        if name.startswith(key): return base/name[len(key):]
    if name.startswith(('VI.','MasterConsole.')): return MASTER/name
    if name=='Untitled2.plcproj': return PLC/name
    if (ADS/name).is_file(): return ADS/name
    if '/' not in name and (ADS/'tools'/name).is_file(): return ADS/'tools'/name
    matches=list(MASTER.rglob(name)) if '/' not in name else []
    if len(matches)==1: return matches[0]
    return ROOT/name

for doc in OUT.glob('*.md'): normalise(doc)
issues=[]; counts={}; summary={}
for doc in sorted(OUT.glob('*.md')):
    lines=doc.read_text(encoding='utf-8').splitlines()
    fenced=False; start=0; expected=None; checked=0; tables=0
    for n,line in enumerate(lines,1):
        if line.lstrip().startswith('```'):
            if fenced and n-start-1>20: issues.append(dict(doc=doc.name,line=start,issue='excerpt_over_20',length=n-start-1))
            fenced=not fenced; start=n; expected=None; continue
        if fenced: continue
        if line.startswith('|'):
            cols=len(re.findall(r'(?<!\\)\|',line))-1
            if expected is None: expected=cols; tables+=1
            elif cols!=expected: issues.append(dict(doc=doc.name,line=n,issue='table_shape',expected=expected,actual=cols))
        else: expected=None
        for m in REF.finditer(line):
            name=m['path'] or m['alias']; nums=m['nums'] or m['anums']; p=resolve(name)
            if not p.is_file():
                issues.append(dict(doc=doc.name,line=n,issue='file_missing',ref=m[0],resolved=str(p))); continue
            if p not in counts: counts[p]=len(p.read_text(encoding='utf-8-sig').splitlines())
            ns=[int(x) for x in re.findall(r'\d+',nums)]
            if min(ns)<1 or max(ns)>counts[p]:
                issues.append(dict(doc=doc.name,line=n,issue='line_out_of_range',ref=m[0],actual=counts[p]))
            checked+=1
    if fenced: issues.append(dict(doc=doc.name,issue='unclosed_fence'))
    summary[doc.name]=dict(lines=len(lines),reference_groups=checked,tables=tables)
assert summary['00_总览.md']['lines']<=400
result=dict(reports=summary,issues=issues,note='Mechanical checks; semantic review separate.')
(OUT/'scratch/report_validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result,ensure_ascii=False,indent=2))
assert not issues
