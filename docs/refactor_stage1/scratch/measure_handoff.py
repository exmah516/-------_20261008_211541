"""以明确的块边界和声明口径比较阶段0与当前代码；不按删行数冒充状态数。"""
from pathlib import Path
import json
import re
import subprocess

root = Path(__file__).resolve().parents[3]
out = root / 'docs/refactor_stage1'
prefix = '介入机器人/B数据端/工程/ADS项目/'
baseline = 'd2bb5fc4de8e53aad750b2a048008b52fa488453'
namespace = {'__file__': str(out/'scratch/inventory.py')}
inventory = (out/'scratch/inventory.py').read_text(encoding='utf-8')
exec(inventory[:inventory.index('results = {}')], namespace)
clean = namespace['clean']


def source(name, old):
    if old:
        return subprocess.check_output(['git','show',baseline+':'+prefix+name],cwd=root).decode('utf-8-sig')
    return (root/prefix/name).read_text(encoding='utf-8-sig')


def block(text, start):
    code = clean(text)
    begin = code.index('{', start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (code[end] == '{') - (code[end] == '}')
        end += 1
    return text.count('\n', 0, start)+1, text.count('\n', 0, end)+1, text[begin+1:end-1]


def fields(text, name):
    start = re.search(r'\b(?:struct|class)\s+'+name+r'\b', text).start()
    _, _, body = block(text, start)
    depth = 0
    result = []
    for line in clean(body).splitlines():
        if depth == 0:
            match = re.match(r'\s*(?:std::\w+(?:<[^>]+>)?|[\w]+)\s+(\w.*)', line)
            if match and not re.match(r'\w+\s*\(', match[1]):
                nesting = 0
                pieces = []
                start = 0
                rest = match[1]
                for i, char in enumerate(rest):
                    if char in '([{': nesting += 1
                    elif char in ')]}': nesting -= 1
                    elif char == ',' and nesting == 0:
                        pieces.append(rest[start:i]); start = i+1
                pieces.append(rest[start:])
                result += [re.match(r'\s*(\w+)', value)[1] for value in pieces]
        depth += line.count('{')-line.count('}')
    return result


report = {}
for old in [True, False]:
    main = source('main.cpp', old)
    types = source('control_types.h', old)
    ranges = []
    for match in re.finditer(r'\bauto\s+(\w+)\s*=\s*\[', main):
        name = match[1]
        if any(word in name for word in ['planned_return','axis_return_request','axis1_group_return_requests','record_return_command','cancel_active_return_motion']):
            lo, hi, _ = block(main, match.start())
            ranges.append(dict(name=name, first=lo, last=hi))
    if old:
        lo, hi, _ = block(main, main.index('struct PlannedReturnEventView'))
        ranges.append(dict(name='PlannedReturnEventView',first=lo,last=hi))
        type_names = ['PlannedReturnLeg','PlannedReturnCoordinator']
    else:
        for marker in ['if (has_new_ads_snapshot && ads_snapshot.position_valid && !planned_return.active()',
                       'if (planned_return.active() && handoff_ready)',
                       'if (planned_return.phase == HandoffPhase::Requested && !planned_return.request_timed_out',
                       'if (planned_return.phase == HandoffPhase::Takeover && planned_return.rebased && handoff_ready)']:
            start = main.index(marker)
            lo, hi, _ = block(main, start)
            ranges.append(dict(name=marker,first=lo,last=hi))
        start = main.index('if (planned_return.active() && handoff_ready)',main.index('if (planned_return.active() && handoff_ready)')+1)
        lo, hi, _ = block(main,start)
        ranges.append(dict(name='事务期间保持输出',first=lo,last=hi))
        type_names = ['HandoffLeg','HandoffCoordinator']
    covered = {n for item in ranges for n in range(item['first'],item['last']+1)}
    report['before' if old else 'after'] = dict(
        main_lines=len(main.splitlines()),
        scheduler_block_lines=len(covered), blocks=ranges,
        lexical_reference_lines=sum(bool(re.search(r'planned_return|handoff|return_recovery|return_ads_fault',line,re.I)) for line in main.splitlines()),
        fields={name:fields(types,name) for name in type_names})
report['baseline']=baseline
payload=source('handoff_types.h',False)
report['after']['payload_fields']={name:fields(payload,name) for name in ['HandoffCommand','HandoffStatus']}
report['before']['expanded_fields']=len(report['before']['fields']['PlannedReturnCoordinator'])-1+2*len(report['before']['fields']['PlannedReturnLeg'])
report['after']['expanded_fields']=len(report['after']['fields']['HandoffCoordinator'])-3+2*len(report['after']['fields']['HandoffLeg'])+sum(len(value) for value in report['after']['payload_fields'].values())
report['definition']='调度块计完整lambda/事件视图或新协调器推进/保持/请求/接管块的并集，包含目标校验和规划，排除双柄触发运动树及分散门控；结构体按字段声明计，数组不展开，方法不计。'
(out/'handoff_size_comparison.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
for key in ['before','after']:
    print(key, {k:v for k,v in report[key].items() if k not in ['blocks','fields']}, {name:len(value) for name,value in report[key]['fields'].items()})
