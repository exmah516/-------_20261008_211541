"""Append read-only configuration/size inventories; only writes this survey folder."""
from pathlib import Path
import json
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'docs/arch_survey_r1'
ADS = ROOT / '介入机器人/B数据端/工程/ADS项目'
KIN = ROOT / '介入机器人/B数据端/工程/PositioningArmKinematics'
DOC = OUT / 'A_工程全貌.md'
MARKER = '\n## A.15 数据端配置字段逐项表'


def compact(ns):
    spans = []
    for n in sorted(set(ns)):
        if spans and n == spans[-1][1] + 1:
            spans[-1][1] = n
        else:
            spans.append([n, n])
    return ','.join(str(a) if a == b else f'{a}-{b}' for a,b in spans)


def escape(s):
    return s.replace('|', r'\|').replace('\n', ' ')


def fields(path, struct):
    lines = path.read_text(encoding='utf-8-sig').splitlines()
    start = next(i for i,s in enumerate(lines) if re.match(r'\s*struct\s+'+re.escape(struct)+r'\b', s))
    end = next(i for i in range(start+1, len(lines)) if lines[i].strip() == '};')
    found=[]
    for i in range(start+1, end):
        body, _, comment = lines[i].partition('//')
        m=re.match(r'\s*([\w:<> ]+?)\s+(\w+)\s*(?:=\s*(.+?))?\s*;\s*$',body)
        if m:
            default = m[3].strip() if m[3] else 'std::string{}（空；无显式赋值）'
            assert m[3] or m[1].strip() == 'std::string', (path, i+1, body)
            found.append((m[1].strip(), m[2], default, i+1, comment.strip()))
    assert found, (path, struct)
    return found


assert compact([3,2,1,7,7,9,10]) == '1-3,7,9-10'
assert escape('a|b') == 'a\\|b'

# Read only production project items; exclude the vendor SDK and build outputs.
sources = []
for elem in ET.parse(ADS/'ADS.vcxproj').getroot().iter():
    inc = elem.attrib.get('Include', '').replace('\\', '/')
    if elem.tag.split('}')[-1] not in ('ClCompile', 'ClInclude') or not inc:
        continue
    p = (ADS/inc).resolve()
    if p.is_file() and 'ADS/Include/' not in inc:
        sources.append(p)
sources += list((KIN/'include/positioning_arm').glob('*.h'))
sources = list(dict.fromkeys(sources))
contents = {p:p.read_text(encoding='utf-8-sig').splitlines() for p in sources}

sections = [MARKER, '', '下表是**已确认**的当前源码默认表达式与生产引用位置，逐项列出字段名/类型/默认值。默认表达式没有擅自换成现场配置；语义和单位可由名称、声明注释及使用处复核。引用索引按标识符词法匹配，包含读/写两种用法；不把这些位置都称为运行时入口。生产公开Zenoh枚举没有tracking/lead/spacing/内部启动参数命令，当前WPF也没有它们的编辑项；内部`VisCommand`分支仍保留。证据：`D/protocol/cpp/zenoh_protocol.h:14-24`、`D/control_messages.h:144-199`、`D/main.cpp:4870-5402`。', '']
stats={}
for path,struct in [(ADS/'control_types.h','ControlConfig'),(ADS/'control_types.h','CylinderPreset'),(ADS/'force_calibration.h','ForceCalibrationConfig'),(ADS/'delivery_tracking.h','DeliveryTrackingParameters'),(ADS/'zenoh_service.h','ZenohServiceConfig')]:
    fs=fields(path,struct)
    stats[struct]=len(fs)
    sections += [f'### A.15.{len(stats)} `{struct}`（{len(fs)}个配置字段）', '', '| 字段（类型） | 默认表达式 | 当前生产引用位置 |', '|---|---|---|']
    for typ,name,default,n,comment in fs:
        loc=[]
        for p,ls in contents.items():
            found=[i+1 for i,s in enumerate(ls) if re.search(r'\b'+re.escape(name)+r'\b',s.split('//')[0]) and not (p==path and i+1==n)]
            if found:
                base='D/'+p.relative_to(ADS).as_posix() if p.is_relative_to(ADS) else 'K/'+p.relative_to(KIN).as_posix()
                loc.append('`'+base+':'+compact(found)+'`')
        decl='D/'+path.relative_to(ADS).as_posix()
        text='；'.join(loc) if loc else '当前生产代码未查到声明外引用'
        if comment:
            text = comment + '；' + text
        sections.append(f'| `{struct}.{name}` (`{typ}`) | `{escape(default)}`；`{decl}:{n}` | {escape(text)} |')
    sections.append('')

assert stats['ControlConfig']==83
assert stats['CylinderPreset']==10
assert stats['ForceCalibrationConfig']==25
assert stats['DeliveryTrackingParameters']==4
assert stats['ZenohServiceConfig']==6
assert any(x[1]=='axis1_post_return_lead_mm' and x[2]=='2.0' for x in fields(ADS/'control_types.h','ControlConfig'))
assert any(x[1]=='router_path' and '空' in x[2] for x in fields(ADS/'zenoh_service.h','ZenohServiceConfig'))

sections += ['### A.15.6 力标定基础常量', '', '全部为**已确认**；运行上限和标定验证上限是两个不同量，源文件注释说明5N运行上限超过0–50g单向实验验证范围。', '', '| 常量 | 当前默认表达式和使用处 |', '|---|---|']
for n,line in enumerate(contents[ADS/'force_calibration.h'],1):
    if n>25: break
    m=re.search(r'constexpr double (\w+) = (.+);',line)
    if m:
        sections.append(f'| `{m[1]}` | `{escape(m[2])}`；`D/force_calibration.h:{n}`；换算/去零在同文件`D/force_calibration.h:20-24,100-160`。 |')

sections += ['', '### A.15.7 运动学与程序归零默认值', '', '**已确认**：库配置含标定、软限位和solver/home参数。`AxisCalibration`、速度/限位及HomeOptions加速度的NaN是未配置占位；生产调用在`plan_arm_curve`中用实际位置、PLC读回的速度和加速度构造，不存在`ArmKinematicConfig::Default()`入口。当前笛卡尔点动在PLC生成，数据端保留程序归零规划。', '', '| 组 | 参数/默认值 | 来源与生产使用 |', '|---|---|---|', '| AxisCalibration | zero=NaN、scale=NaN、direction=0；生产构造为zero=0、scale=轴1 .001/其余π÷180、direction=1 | `K/include/positioning_arm/kinematics.h:15-19`、`D/arm_cartesian_motion.h:36-47` |', '| 几何 | link_lengths=(.200,.180)m、lateral_offset=.4563m、tool_length=.5545m、base_offset_mm=(0,0,0) | `K/include/positioning_arm/kinematics.h:21-25`；生产默认构造 `D/arm_cartesian_motion.h:29` |', '| 软限位/最大速度 | lower/upper/max_velocity五维均NaN；生产设轴1–4为±∞，轴5±60°；max_velocity由PLC快照jog_velocity换算 | `K/include/positioning_arm/kinematics.h:26-30`、`D/arm_cartesian_motion.h:30-47`、`D/arm_manual_ads_service.cpp:375-376` |', '| SolverOptions | position_tolerance=1e-7、singularity_stop=1e-6、singularity_slow=.03、max_translation=.02、max_rotation=.1 | `K/include/positioning_arm/kinematics.h:33-39`；构造默认参数 `K/include/positioning_arm/kinematics.h:129-130` |', '| HomeOptions | max_acceleration=4维NaN、sample_period=.02s、minimum_duration=1s、max_samples=20000、optimization_iterations=30；生产加速度用amax.tail<4>() | `K/include/positioning_arm/kinematics.h:72-81`、`D/arm_cartesian_motion.h:55-57` |', '| 定位臂ADS方向/参数默认 | desired手动=false，enable/reset/jog均false；每轴velocity=5、acc/dec=50、jerk=500；speed_scale=1；Cartesian租约500ms/axis租约300ms | `D/arm_manual_ads_service.h:93-112`、`D/arm_manual_ads_service.cpp:13-17,601-642` |', '| PLC几何/profile | L1=200、L2=180、offset=456.3、tool=554.5mm；acc1s、dec.5s、branch1、bisection12、joint caps24/30/30/30/30 | `P/ArmCartJogGen.TcPOU:22-33` |', '']

sections += ['## A.16 补充规模：当前编译源文件', '', '**已确认**：下表从`ADS.vcxproj`的Compile项读取，只统计生产源、声明头和PLC当前Compile项；第三方SDK、build、历史镜像和离线测试不混入生产规模。main最长20项见A.10.1。', '', '| 文件 | 物理行数/证据 |', '|---|---|']
for p in sorted(sources,key=lambda x:x.as_posix().casefold()):
    base='D/'+p.relative_to(ADS).as_posix() if p.is_relative_to(ADS) else 'K/'+p.relative_to(KIN).as_posix()
    sections.append(f'| `{base}` | {len(contents[p])}；`{base}:1-{len(contents[p])}` |')
plc=ROOT/'介入机器人/C从端/250902/Untitled2'
for elem in ET.parse(plc/'Untitled2.plcproj').getroot().iter():
    inc=elem.attrib.get('Include','').replace('\\','/')
    if elem.tag.endswith('Compile') and (plc/inc).is_file():
        p=plc/inc
        n=len(p.read_text(encoding='utf-8-sig').splitlines())
        sections.append(f'| `介入机器人/C从端/250902/Untitled2/{inc}` | {n}；`介入机器人/C从端/250902/Untitled2/{inc}:1-{n}` |')

sections += ['', '## A.17 PLC构建和库的补充边界', '', '**已确认**：TwinCAT系统工程为3.1.4024.67，主PLC工程ProgramVersion=3.1.4024.0；引用Tc2_MC2、Tc2_NC、Tc2_Standard、Tc2_System、Tc3_MC2_AdvancedHoming、Tc3_Module。Boot中有Port851产物，但未由本轮TwinCAT重建。证据：`介入机器人/C从端/250902/250902.tsproj:2,3594-3598`、`介入机器人/C从端/250902/Untitled2/Untitled2.plcproj:2-18,79-107`。', '', '**已确认**：NC1..5→G.arm_axis[1..5]、NC6..12→G.axis[1..7]、NC13/14→G.inject_axis[1..2]、NC15→G.axis2_sync_axis。Box7力通道2/电缸3/4、Box8力通道1/电缸1/2/5。证据：`介入机器人/C从端/250902/250902.tsproj:7617-7628,7630-7688`。**未知**：真实接线和在线最大速度配置。', '', '**已确认**：C从端README称没有PLC源码，与当前POUs、GVL和plcproj实际存在相矛盾；目录中handle备份和move_edge也不在当前Compile项。证据：`介入机器人/C从端/README.md:3-5`、`介入机器人/C从端/250902/Untitled2/Untitled2.plcproj:20-70`。', '']
original=DOC.read_text(encoding='utf-8-sig').split(MARKER)[0]
DOC.write_text(original.rstrip()+'\n'+'\n'.join(sections)+'\n',encoding='utf-8')
(OUT/'scratch/A_inventory_counts.json').write_text(json.dumps(stats,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(stats,ensure_ascii=False))
