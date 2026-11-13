"""从固定阶段0提交重建旧内存基线，与当前完整主循环+ST比较接管终态。"""
from pathlib import Path
import hashlib,io,json,os,subprocess,sys,tempfile,zipfile
from verify_baseline import rows
ROOT=Path(__file__).resolve().parents[2]
REF='d2bb5fc4de8e53aad750b2a048008b52fa488453'
OUT=ROOT/'docs/refactor_stage1/logs/terminal'
OUT.mkdir(parents=True,exist_ok=True)
SCENARIOS=['follow_delivery','follow_retraction','handoff_axis1','handoff_active','handoff_passive','handoff_merge','return_axis1','return_axis6','return_both','return_retry','return_retry_axis6','return_retry_both','return_possible','return_possible_axis6','return_possible_both','return_fault','return_fault_axis6','return_fault_both','return_estop','lead_hold','lead_axis6','ads_hold','handle_gap_short','device_epoch','feedback']
legacy=Path(tempfile.mkdtemp(prefix='handoff-stage0-'))
prefix='介入机器人/B数据端/工程/ADS项目/'
names=subprocess.check_output(['git','ls-tree','-rz','--name-only',REF,'--',prefix,'tools/refactor_baseline'],cwd=ROOT).decode('utf-8').strip('\0').split('\0')
names=[p for p in names if Path(p).suffix in ['.h','.cpp','.py'] and ('/tools/' not in p or p.startswith('tools/refactor_baseline/'))]
raw=subprocess.check_output(['git','archive','--format=zip',REF,'--',*names],cwd=ROOT)
zipfile.ZipFile(io.BytesIO(raw)).extractall(legacy)
# 阶段0有被ADS项目忽略规则漏收的头文件；以当时清单的SHA256核对补齐。
support_dir=ROOT/'tools/handoff_tests/stage0_support'
manifest=json.loads((support_dir/'manifest.json').read_text(encoding='utf-8'))
for name,expected in manifest['sources'].items():
    if not name.endswith('.h') or (legacy/prefix/name).exists():continue
    value=(support_dir/name).read_bytes()
    variants=[value,value.replace(b'\r\n',b'\n'),value.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')]
    recovered=next((b for b in variants if hashlib.sha256(b).hexdigest()==expected),None)
    assert recovered is not None,('缺少阶段0已校验头文件',name,expected)
    destination=legacy/prefix/name;destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(recovered)
here=legacy/'tools/refactor_baseline'
trace=(ROOT/'tools/handoff_tests/endpoint_trace.inc').read_text(encoding='utf-8')
support=(ROOT/'tools/handoff_tests/endpoint_support.cpp').read_text(encoding='utf-8')
p=here/'baseline_support.h';p.write_text(p.read_text(encoding='utf-8-sig')+'\nvoid baseline_endpoint(std::initializer_list<double>);\n',encoding='utf-8')
p=here/'memory_io.cpp';s=p.read_text(encoding='utf-8-sig')
s=s.replace('void advance() {',support+'\nvoid advance() {')
s=s.replace('const bool pushing=tick>=12 && tick<260;','const bool pushing=tick>=12 && tick<260 && !endpoint_started;')
s=s.replace('void baseline_record_event(ExperimentEventCode code,int value,std::uint64_t sequence) {','void baseline_endpoint(std::initializer_list<double> values) { endpoint_capture(values); }\nvoid baseline_record_event(ExperimentEventCode code,int value,std::uint64_t sequence) {\n    if(int(code)==19) endpoint_started=true;')
p.write_text(s,encoding='utf-8')
p=here/'run.py';s=p.read_text(encoding='utf-8-sig')
s=s.replace('source=source[:end]+trace+source[end:]','trace+='+repr(trace)+'\nsource=source[:end]+trace+source[end:]')
lo=s.index('scenarios=[');hi=s.index('results=[]',lo);s=s[:lo]+'scenarios='+repr(SCENARIOS)+'\n'+s[hi:]
s=s.split('import verify_coverage')[0]
p.write_text(s,encoding='utf-8')
env=dict(os.environ,BASELINE_ENDPOINT='1',PYTHONIOENCODING='utf-8')
commands=[('old',[sys.executable,str(p),'--record','--golden',str(legacy/'golden'),'--output',str(OUT/'old')]),('new',[sys.executable,str(ROOT/'tools/refactor_baseline/run.py'),'--terminal','--scenarios',*SCENARIOS,'--output',str(OUT/'new')])]
for kind,cmd in commands:
    with (OUT/(kind+'.log')).open('wb') as log:r=subprocess.run(cmd,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT)
    print(kind,r.returncode,flush=True)
    if r.returncode: print((OUT/(kind+'.log')).read_text(encoding='utf-8',errors='replace'));raise SystemExit(r.returncode)
result=[]
for name in SCENARIOS:
    values=[];ticks=[]
    for kind,last_phase in [('old',9),('new',3)]:
        control=rows(OUT/kind/(name+'.csv'))
        completed=[int(control[i]['tick']) for i in range(1,len(control)) if control[i-1]['state9']==last_phase and control[i]['state9']==0]
        assert completed,(name,kind,'没有完成接管')
        tick=completed[-1]; ticks.append(tick)
        detail={int(r['tick']):r for r in rows(OUT/kind/(name+'.csv.endpoint.csv'))}
        values.append(detail[tick])
    a,b=values;differences={}
    for key in a:
        if key=='tick':continue
        index=int(key[1:]);tol=.01/750 if index in [14,15,18,19] else .01 if index<26 or index in [28,29] else 0
        delta=abs(a[key]-b[key]);assert delta<=tol,(name,key,a[key],b[key],tol)
        differences[key]=delta
    result.append(dict(scenario=name,old_tick=ticks[0],new_tick=ticks[1],max_delta=max(differences.values()),fields=40,equal_within_tolerance=True))
(OUT/'result.json').write_text(json.dumps(dict(baseline_commit=REF,legacy_workspace=str(legacy),input_change='首次事务触发后保持手柄关节量不变，其他输入规则不变；原34场景黄金未修改',scenarios=result),ensure_ascii=False,indent=2),encoding='utf-8')
print('PASS terminal comparisons',len(result),'scenarios x 40 fields')
