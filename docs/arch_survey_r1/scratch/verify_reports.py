"""Validate local evidence references, table shape and excerpt lengths; no source writes."""
from pathlib import Path
import json
import re

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'docs/arch_survey_r1'
ADS=ROOT/'介入机器人/B数据端/工程/ADS项目'
MASTER=ROOT/'介入机器人/A主端/工程/master'
PLC=ROOT/'介入机器人/C从端/250902/Untitled2'
KIN=ROOT/'介入机器人/B数据端/工程/PositioningArmKinematics'
pattern=re.compile(r'(?P<path>[A-Za-z0-9_\u4e00-\u9fff./\\()+-]+\.(?:cpp|cjs|cs|h|TcPOU|TcTTO|TcGVL|TcDUT|tsproj|plcproj|vcxproj|csproj|slnx|ps1|cmd|py|md|log|txt|json|csv|xaml|props|targets)):(?P<nums>\d+(?:[-–]\d+)?(?:[,、]\d+(?:[-–]\d+)?)*)|(?P<alias>\b(?:G|T|ZState)):(?P<anums>\d+(?:[-–]\d+)?(?:[,、]\d+(?:[-–]\d+)?)*)')
service_names={'HandleService.cs','HandleHostMemory.cs','HandleDeviceNotifications.cs','RemoteRobotLink.cs','FlCatheterNative.cs','IRobotLink.cs'}
protocol_names={'ZenohFrames.cs','ZenohWire.cs','ProtocolConstants.cs','FrameAuth.cs','Frames.cs'}
control_names={'App.xaml.cs','MainWindow.xaml','MainWindow.xaml.cs','StartupSelection.cs'}
arm_names={'PositioningArmPanel.xaml','PositioningArmPanel.xaml.cs'}
inter_names={'InterventionModule.cs','InterventionPanel.xaml','InterventionPanel.xaml.cs'}

def resolve(name, doc):
    if name=='G': return PLC/'GVLs/G.TcGVL'
    if name=='T': return PLC.parent/'250902.tsproj'
    if name=='ZState': return ADS/'zenoh_service.cpp'
    p=ROOT/name
    if p.is_file(): return p
    if (OUT/name).is_file(): return OUT/name
    prefixes={'D/':ADS,'M/':MASTER,'K/':KIN,'P/':PLC/'POUs','U/':PLC,
        'master/':MASTER,'工程/':MASTER.parent,'A主端/':ROOT/'介入机器人/A主端',
        'B数据端/':ROOT/'介入机器人/B数据端','C从端/':ROOT/'介入机器人/C从端',
        'Services/':MASTER/'VI.Service.Robot/Services','ADS项目/':ADS,
        'PositioningArmKinematics/':KIN,'Untitled2/':PLC,'POUs/':PLC/'POUs',
        'GVLs/':PLC/'GVLs','DUTs/':PLC/'DUTs'}
    for key,base in prefixes.items():
        if name.startswith(key):
            return base/name[len(key):]
    if name.startswith('scratch/'): return OUT/name
    if name=='VI.HandleHost.csproj': return MASTER/'VI.HandleHost'/name
    if name=='launchSettings.json': return MASTER/'VI.Control/Properties'/name
    if name.startswith(('VI.','MasterConsole.')): return MASTER/name
    if name in service_names: return MASTER/'VI.Service.Robot/Services'/name
    if name in protocol_names: return MASTER/'MasterConsole.Protocol'/name
    if name in control_names: return MASTER/'VI.Control'/name
    if name in arm_names: return MASTER/'VI.Module.PositioningArm'/name
    if name in inter_names: return MASTER/'VI.Module.Intervention'/name
    if name=='MainViewModel.cs': return MASTER/'VI.Service.Robot/ViewModels'/name
    if name=='ZenohSession.cs': return MASTER/'VI.Service.Net'/name
    if name=='Directory.Build.targets': return MASTER.parent/name
    if name=='BuildAndTest.ps1': return KIN/name
    if name=='PlcTask.TcTTO': return PLC/name
    if name=='250902.tsproj': return PLC.parent/name
    if name.endswith('.TcPOU'): return PLC/'POUs'/name
    if name.endswith('.TcDUT'): return PLC/'DUTs'/name
    if (ADS/name).is_file(): return ADS/name
    return p

assert resolve('D/main.cpp','A').is_file()
assert resolve('P/handle.TcPOU','C').is_file()
assert resolve('ZenohFrames.cs','A').is_file()
line_counts={}
issues=[]
summary={}
for doc in sorted(OUT.glob('*.md')):
    lines=doc.read_text(encoding='utf-8-sig').splitlines()
    fenced=False
    fence_begin=0
    expected=None
    checked=0
    tables=0
    for n,line in enumerate(lines,1):
        if line.lstrip().startswith('```'):
            if fenced and n-fence_begin-1>20:
                issues.append({'doc':doc.name,'line':fence_begin,'issue':'excerpt_over_20_lines','lines':n-fence_begin-1})
            fenced=not fenced
            fence_begin=n
            expected=None
            continue
        if not fenced and line.startswith('|'):
            columns=len(re.findall(r'(?<!\\)\|',line))-1
            if expected is None:
                expected=columns
                tables+=1
            elif columns != expected:
                issues.append({'doc':doc.name,'line':n,'issue':'table_shape','expected':expected,'actual':columns})
        else:
            expected=None
        if fenced: continue
        for m in pattern.finditer(line):
            name=(m['path'] or m['alias']).replace('\\','/')
            nums=m['nums'] or m['anums']
            p=resolve(name,doc.name)
            if not p.is_file():
                issues.append({'doc':doc.name,'line':n,'issue':'file_missing','ref':m[0],'resolved':str(p)})
                continue
            if p not in line_counts:
                line_counts[p]=len(p.read_text(encoding='utf-8-sig').splitlines())
            numbers=[int(i) for i in re.findall(r'\d+',nums)]
            if min(numbers)<1 or max(numbers)>line_counts[p]:
                issues.append({'doc':doc.name,'line':n,'issue':'line_out_of_range','ref':m[0],'lines':line_counts[p]})
            checked+=1
        if re.search(r'\.md:[A-F]\.',line):
            issues.append({'doc':doc.name,'line':n,'issue':'section_instead_of_line'})
    if fenced: issues.append({'doc':doc.name,'issue':'unclosed_fence'})
    summary[doc.name]={'lines':len(lines),'checked_reference_groups':checked,'tables':tables}
assert summary['00_总览.md']['lines']<=600
result={'reports':summary,'issues':issues,'note':'Mechanical reference checks only; semantic claims reviewed separately.'}
(OUT/'scratch/report_validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'reports':summary,'issues_count':len(issues),'issues':issues[:100]},ensure_ascii=False,indent=2))
