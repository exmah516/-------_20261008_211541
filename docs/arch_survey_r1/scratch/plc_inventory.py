from pathlib import Path
import re, xml.etree.ElementTree as ET
from collections import defaultdict
ROOT=Path.cwd()
P=ROOT/'介入机器人/C从端/250902/Untitled2'
OUT=ROOT/'docs/arch_survey_r1/scratch'
ns={'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
tree=ET.parse(P/'Untitled2.plcproj')
compiled=[e.attrib['Include'].replace('\\\\','/') for e in tree.findall('.//m:Compile',ns)]
pous=[P/p for p in compiled if p.endswith('.TcPOU')]
assert len(pous)==13, len(pous)
gvl=P/'GVLs/G.TcGVL'
vars=[]
for line_no,line in enumerate(gvl.read_text(encoding='utf-8-sig').splitlines(),1):
    code=line.split('//',1)[0].strip()
    m=re.match(r'([A-Za-z_][\w]*(?:\s*,\s*[A-Za-z_]\w*)*)\s*(?:AT\s+%[IQ]\*)?\s*:\s*(.+?);',code)
    if not m: continue
    decl=m.group(2)
    for raw_name in m.group(1).split(','):
        name=raw_name.strip()
        vars.append((name,decl,line_no,line.split('//',1)[1].strip() if '//' in line else ''))
assert any(v[0]=='return_cmd' for v in vars)
refs=defaultdict(lambda:defaultdict(lambda:defaultdict(list)))
pattern=re.compile(r'\bG\.([A-Za-z_]\w*)((?:\[[^\]\n]+\]|\.[A-Za-z_]\w*)*)',re.I)
for f in pous:
    raw=f.read_text(encoding='utf-8-sig')
    clean=re.sub(r'\(\*.*?\*\)',lambda m:'\n'*m.group().count('\n'),raw,flags=re.S)
    for ln,line in enumerate(clean.splitlines(),1):
        if '<v n="Operand">' in line: continue
        line=line.split('//',1)[0]
        for m in pattern.finditer(line):
            name=m.group(1).lower()
            tail=line[m.end():]
            mode='W' if re.match(r'\s*:=',tail) else 'R'
            if re.match(r'\s*\(',tail): mode='RW/FB'
            if re.search(r'Axis\s*:=\s*$',line[:m.start()],re.I): mode='RW/Axis'
            refs[name][f.stem][mode].append(ln)
    try:
        xml=ET.fromstring(raw)
        for obj in xml.iter('o'):
            op=next((v for v in obj.findall('v') if v.attrib.get('n')=='Operand'),None)
            if op is None or not op.text: continue
            m=pattern.search(op.text)
            if not m: continue
            props={v.attrib.get('n'):v.text for v in obj.findall('v')}
            mode='W/FBD' if props.get('LValue')=='true' else ('RW/FB' if props.get('IsInstance')=='true' else 'R/FBD')
            exact='<v n="Operand">'+op.text+'</v>'
            at=raw.find(exact)
            ln=raw[:at].count('\n')+1 if at>=0 else 1
            refs[m.group(1).lower()][f.stem][mode].append(ln)
    except ET.ParseError:
        pass
def esc(x): return x.replace('|','\\|')
lines=[
'# GVL 全量声明与 PLC 字面读写索引（自动抽取）','',
'范围：介入机器人/C从端/250902/Untitled2/Untitled2.plcproj:21 编译项中的 13 个 POU。P/ 统一展开为 介入机器人/C从端/250902/Untitled2/；GVLs/ 也相对此路径。',
'确定程度：本表为已确认的声明与字面引用；W=赋值左值，R=其余字面读取，RW/FB=功能块实例调用，W/FBD=FBD XML 的 LValue。不是对第三方 MC 库内部行为的验证。',
'每个单元格给该 POU/角色的首个行号和引用数量；逐行索引见 plc_gvl_refs.tsv。','',
'|变量|类型/初值（原样）|说明注释|PLC 写/调用方|PLC 读取方|声明出处|','|---|---|---|---|---|---|']
flat=['variable\tmode\tfile\tline']
for name,decl,ln,comment in vars:
    ws=[]; rs=[]
    for pou,modes in sorted(refs[name.lower()].items()):
        for mode,nums in sorted(modes.items()):
            nums=sorted(set(nums))
            val=f'{pou} [{mode}] P/POUs/{pou}.TcPOU:{nums[0]}（{len(nums)}处）'
            (ws if mode.startswith('W') or mode.startswith('RW') else rs).append(val)
            for n in nums: flat.append(f'{name}\t{mode}\t介入机器人/C从端/250902/Untitled2/POUs/{pou}.TcPOU\t{n}')
    lines.append('|'+ '|'.join(map(esc,[f'G.{name}',f'{decl}',comment or '—','<br>'.join(ws) or '无字面写/调用','<br>'.join(rs) or '无字面读取',f'GVLs/G.TcGVL:{ln}']))+'|')
(OUT/'C_GVL全量索引.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
(OUT/'plc_gvl_refs.tsv').write_text('\n'.join(flat)+'\n',encoding='utf-8')
print('POUs',len(pous),'GVL declarations',len(vars),'refs',len(flat)-1)

