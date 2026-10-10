"""离线对照 TMC 中的符号、结构字段、数组边界；不代表在线 PLC 已下载此版本。"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

workspace = Path(__file__).resolve().parents[3]
project = workspace / '主从分机软件包_20261008_211541/B_从端电脑/工程/ADS项目'
source = '\n'.join((project / n).read_text(encoding='utf-8-sig') for n in
                   ('plc_io.cpp', 'plc_io.h', 'ads_communication.cpp', 'main.cpp', 'arm_manual_ads_service.cpp'))
symbols = set(re.findall(r'"((?:G\.|TwinCAT_SystemInfoVarList\.)[\w.\[\]]+)"', source))
symbols.update(f'G.axis[{i}].NcToPlc.ActPos' for i in range(1, 8))
symbols.discard('G.axis[')

def resolve(root, name):
    sy = {s.findtext('Name').lower(): s for s in root.findall('.//Symbol')}
    ty = {(t.find('Name').get('Namespace', ''), t.findtext('Name')): t
          for t in root.findall('./DataTypes/DataType')}
    guid_types = {t.find('Name').get('GUID'): t for t in root.findall('./DataTypes/DataType')
                  if t.find('Name').get('GUID')}
    candidates = [k for k in sy if name.lower() == k or name.lower().startswith(k + '.') or name.lower().startswith(k + '[')]
    if not candidates:
        return 'MISSING'
    key = max(candidates, key=len)
    node = sy[key]
    remaining = name[len(key):]
    typ = node.find('BaseType')
    bits = int(node.findtext('BitSize', '0'))
    while remaining:
        if remaining.startswith('['):
            m = re.match(r'\[(\d+)\]', remaining)
            a = node.find('ArrayInfo')
            if not m or a is None:
                return 'INVALID ARRAY'
            low, count = int(a.findtext('LBound')), int(a.findtext('Elements'))
            if not low <= int(m[1]) < low + count:
                return 'OUT OF BOUNDS'
            bits //= count
            remaining = remaining[m.end():]
        elif remaining.startswith('.'):
            field, _, tail = remaining[1:].partition('.')
            match = re.match(r'(\w+)(.*)', field)
            definition = ty.get((typ.get('Namespace', ''), typ.text))
            if definition is None:
                definition = guid_types.get(typ.get('GUID'))
            if definition is None:
                return 'TYPE UNRESOLVED ' + typ.text
            node = next((s for s in definition.findall('SubItem') if s.findtext('Name').lower() == match[1].lower()), None)
            if node is None:
                return 'MISSING FIELD'
            typ = node.find('Type')
            bits = int(node.findtext('BitSize', '0'))
            remaining = match[2] + ('.' + tail if tail else '')
        else:
            return 'INVALID PATH'
    return f'{typ.text} {bits // 8} bytes'

lines = ['# ADS 符号离线核查', '', '只核对本地 TMC，不等于核对在线 PLC。', '']
for rel in ('下位机工程程序/250902/Untitled2/Untitled2.tmc', '250902/250902/Untitled2/Untitled2.tmc'):
    root = ET.parse(workspace / '260316' / rel).getroot()
    lines += ['## ' + rel, '', '| 符号 | TMC 结果 |', '|---|---|']
    unresolved = []
    for name in sorted(symbols):
        result = resolve(root, name)
        lines.append(f'| `{name}` | {result} |')
        if any(result.startswith(x) for x in ('MISSING', 'INVALID', 'OUT', 'TYPE UNRESOLVED')):
            unresolved.append((name, result))
    print(rel, 'checked=', len(symbols), 'unresolved=', unresolved)
    lines.append('')
Path(__file__).with_name('符号核查.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
