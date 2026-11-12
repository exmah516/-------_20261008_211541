"""静态解析仓库其它 runner 的 between/index 标记，不执行 runner。"""
from pathlib import Path
import ast
import hashlib
import json

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'docs/arch_survey_r2/scratch'
D=ROOT/'介入机器人/B数据端/工程/ADS项目'
main=(D/'main.cpp').read_text(encoding='utf-8-sig')
data=json.loads((OUT/'extraction_manifest.json').read_text(encoding='utf-8'))
additional=[]

def add(script,line,start,end):
    lo=main.find(start); hi=main.find(end,lo)
    if lo<0 or hi<0:
        additional.append(dict(runner=str(script.relative_to(ROOT)).replace('\\','/'),runner_line=line,error='missing marker',start_marker=start,end_marker=end)); return
    s=main[lo:hi]
    additional.append(dict(runner=str(script.relative_to(ROOT)).replace('\\','/'),runner_line=line,start_line=main[:lo].count('\n')+1,end_line=main[:hi-1].count('\n')+1,start_marker=start,end_marker=end,sha256=hashlib.sha256(s.encode()).hexdigest()))

scripts=[D/'tools/run_dual_control_test.py',D/'tools/run_ads_diagnostics_test.py',ROOT/'介入机器人/tools/revision_uhn/run_handle.py',ROOT/'介入机器人/tools/revision_uhn/run_network.py']
for p in scripts:
    tree=ast.parse(p.read_text(encoding='utf-8-sig'))
    for node in ast.walk(tree):
        if not isinstance(node,ast.Call) or not isinstance(node.func,ast.Name) or node.func.id!='between': continue
        args=node.args
        # 只汇总 main/source 片段；其它来源的结构体/窗口不算 main 覆盖。
        if len(args)==3:
            if not isinstance(args[0],ast.Name) or args[0].id not in ('source','main'): continue
            args=args[1:]
        if len(args)!=2: continue
        for axis in ([1,6] if any(isinstance(x,ast.JoinedStr) for x in args) else [None]):
            try:
                vals=[eval(compile(ast.Expression(x),str(p),'eval'),{'__builtins__':{}},{'axis':axis,'observe_start':'\t\tconst bool ads_self_check_valid','active_recovery':'\t\telse if (ads_soft_hold_active && planned_return.active())'}) for x in args]
            except (NameError,TypeError): continue
            add(p,node.lineno,*vals)

p=D/'tools/run_ads_diagnostics_test.py'
for line,start,end in [(25,'\t\tauto handle_distance_follow_allowed','\t\t// 9) 根据当前顶层模式'),(26,'\t\tconst bool distance_follow_output','\t\tconst std::uint64_t output_generation')]: add(p,line,start,end)
p=D/'tools/check_axis4_window.py'
add(p,42,'auto planned_return_settle_wait_ms =','auto apply_planned_return_outputs =')
lo=main.index('cylinder3_cmd = (restore_clamps && planned_return.restore_stage'); hi=main.index(';',main.index('cylinder4_cmd =',lo))+1
add(p,8,main[lo:lo+80],main[hi:hi+25])

def union(rows):
    result=[]
    for a,b in sorted((r['start_line'],r['end_line']) for r in rows if 'start_line' in r):
        if result and a<=result[-1][1]+1: result[-1][1]=max(result[-1][1],b)
        else: result.append([a,b])
    return result
all_ranges=[r for r in data['ranges'] if r['source'].endswith('/main.cpp')]+additional
merged=union(all_ranges); missing=[]; cursor=1; count=len(main.splitlines())
for a,b in merged:
    if a>cursor: missing.append([cursor,a-1])
    cursor=b+1
if cursor<=count:missing.append([cursor,count])
result={'scope':'all main.cpp extraction markers in production tools runners; historical broken runners included conservatively','additional_ranges':additional,'main_line_count':count,'main_union':merged,'never_extracted_in_searched_runners':missing}
assert count==5424 and len(additional)>10
(OUT/'all_runner_extraction_manifest.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
rows=['| runner 行 | 片段 | main.cpp 区间 |','|---|---|---|']
for r in data['ranges']:
    if not r['source'].endswith('/main.cpp'):continue
    rows.append(f"| {r['runner_line']} | {', '.join(r['outputs']) or r['start_marker'].strip().replace('|','&#124;')[:55]} | {r['start_line']}–{r['end_line']} |")
(OUT/'extraction_tables.md').write_text('\n'.join(rows)+'\n\n全仓已检索 runner 的保守未抽取补集：'+', '.join(f'{a}–{b}' for a,b in missing)+'\n',encoding='utf-8')
print(json.dumps({'additional':len(additional),'missing_markers':sum('error'in r for r in additional),'line_count':count,'never_extracted':missing},ensure_ascii=False,indent=2))
