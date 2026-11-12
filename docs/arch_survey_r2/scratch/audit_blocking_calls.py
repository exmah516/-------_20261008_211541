from pathlib import Path
import csv
import json
import re

ROOT=Path(__file__).resolve().parents[3]
SRC=ROOT/'介入机器人/B数据端/工程/ADS项目'
OUT=ROOT/'docs/arch_survey_r2'
csv_path=OUT/'blocking_calls.csv'
with csv_path.open(encoding='utf-8-sig',newline='') as f:
    reader=csv.DictReader(f)
    assert reader.fieldnames==['位置','调用','最坏阻塞时长的依据']
    rows=list(reader)
    assert all(None not in r for r in rows)
covered={}
for r in rows:
    path,line=r['位置'].rsplit(':',1)
    covered.setdefault(str(Path(path).resolve()),set()).add(int(line))

source=(SRC/'main.cpp').read_text(encoding='utf-8-sig').splitlines()
main_covered=covered[str((SRC/'main.cpp').resolve())]
api=re.compile(r'(?:(?:ads_communication|arm_manual_ads|zenoh_service|experiment_recorder|handle_velocity_recorder)\.(?:start|stop|stop_async|stop_and_wait|read|write|read_sum|write_sum|refresh_coordinates|wait_for_snapshot|latest_snapshot|drain_snapshots|stats|event_state|publish_output|snapshot|update_ads_communication_stats|poll_health|publish_state|poll_command|command_processed|set_manual_enable|set_cartesian_jog|request_program_zero|stop_cartesian|keep_cartesian_alive|set_axis_enable|request_reset|set_jog_direction|set_jog_parameter)\s*\(|(?:handle_axis[16]\.|active_handle->)(?:init|poll|close|setforce_axis)\s*\(|homing::(?:enabled|load|save)\s*\(|std::(?:cout|cerr|ofstream)\b)')
candidates=[{'line':i,'source':t.strip()} for i,t in enumerate(source,1) if api.search(t)]
missing=[x for x in candidates if x['line'] not in main_covered]
assert not missing,missing

# 排除不等待网络/条件变量的接口：逐位置列出，防止以为CSV漏掉了它们。
nonblocking=re.compile(r'ads_communication\.(?:submit_planned_return_command|planned_return_command_result|applied_output_generation|applied_motion_output_generation|coordinate_refresh_generation|request_coordinate_refresh|request_watchdog_recovery)\s*\(')
excluded=[{'line':i,'source':t.strip(),'reason':'原子请求/代次/固定环队列；无同步ADS请求或条件变量等待。planned_return的Clear CAS可因竞争重试，源码未给重试次数上限，不是线程阻塞等待。'} for i,t in enumerate(source,1) if nonblocking.search(t)]

report={
    'scope':'数据端main线程的显式潜在等待点；含内联lambda及同步辅助函数，底层原语另列，不能相加计算预算',
    'csv_rows':len(rows),
    'columns':['位置','调用','最坏阻塞时长的依据'],
    'main_regex_candidate_lines':len(candidates),
    'main_regex_covered_lines':len(candidates)-len(missing),
    'main_regex_uncovered':missing,
    'main_regex_candidates':candidates,
    'main_atomic_or_queue_calls_excluded':excluded,
    'console_main_lines':sum(bool(re.search(r'\bstd::(?:cout|cerr)\b',t)) for t in source),
    'all_rows_have_certainty_marker':all(any(x in r['最坏阻塞时长的依据'] for x in ['已确认','推断','未知']) for r in rows),
    'caveat':'正则候选覆盖是机械校验，不是全程序AST调用图证明；同步模块经人工阅读调用链核验。排除纯数学、原子load/store、无I/O的内存复制，以及本身没有显式等待的常规分配/时钟查询。'
}
(OUT/'scratch/blocking_calls_audit.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k not in ['main_regex_candidates','main_atomic_or_queue_calls_excluded']},ensure_ascii=False,indent=2))
