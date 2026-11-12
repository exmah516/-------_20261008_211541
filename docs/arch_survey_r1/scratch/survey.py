"""只读源码视图、盘点与基线；所有生成物仅写本调研目录。"""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'docs/arch_survey_r1'
ADS = ROOT / '介入机器人/B数据端/工程/ADS项目'

def view(name, start=1, end=None):
    p = Path(name)
    if not p.is_absolute():
        p = ADS / name if (ADS / name).exists() else ROOT / name
    ls = p.read_text(encoding='utf-8-sig').splitlines()
    print(p.relative_to(ROOT).as_posix())
    for i in range(int(start)-1, min(int(end or len(ls)), len(ls))):
        print(f'{i+1}: {ls[i]}')

def baseline():
    files = subprocess.check_output(['git','ls-files','-z'], cwd=ROOT).decode('utf-8').split('\0')
    status = subprocess.check_output(['git','status','--porcelain=v1','-z'], cwd=ROOT).decode('utf-8')
    data = {'head': subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),
            'status': status, 'tracked': {}}
    for n in files:
        p = ROOT/n
        if p.is_file() and not n.startswith('docs/arch_survey_r1/'):
            data['tracked'][n] = hashlib.sha256(p.read_bytes()).hexdigest()
    (OUT/'scratch/baseline.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    print('baseline', data['head'], len(data['tracked']))

def mask_cpp(s):
    # 保留偏移/换行，避免注释与字符串中的大括号影响跨度统计。
    return re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|R"([^ ()\\\t\r\n]{0,16})\([\s\S]*?\)\1"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda m: ''.join('\n' if c=='\n' else ' ' for c in m[0]), s)

def spans():
    s=(ADS/'main.cpp').read_text(encoding='utf-8-sig'); m=mask_cpp(s)
    results=[]
    pattern=r'\bauto\s+(\w+)\s*=\s*\[[^\]]*\]\s*\([^;{}]*\)\s*(?:->[^{}]+)?\s*\{|\b(?:int|bool|void|double)\s+(main|[A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{'
    for x in re.finditer(pattern,m):
        depth=1; j=x.end()
        while depth and j<len(m):
            depth += (m[j]=='{')-(m[j]=='}'); j+=1
        a=s.count('\n',0,x.start())+1; b=s.count('\n',0,j)+1
        results.append({'name': x[1] or x[2], 'start':a,'end':b,'lines':b-a+1})
    results.sort(key=lambda x:-x['lines'])
    (OUT/'scratch/main_spans.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(results,ensure_ascii=False,indent=2))

def verify():
    data=json.loads((OUT/'scratch/baseline.json').read_text(encoding='utf-8'))
    changed=[]
    for n,h in data['tracked'].items():
        p=ROOT/n
        if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=h: changed.append(n)
    (OUT/'scratch/readonly_verification.json').write_text(json.dumps({'changed_existing_files':changed},ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(changed,ensure_ascii=False)); assert not changed

assert mask_cpp('"{" // }\n{}').count('{')==1
if __name__=='__main__':
    if sys.argv[1]=='view': view(*sys.argv[2:])
    elif sys.argv[1]=='baseline': baseline()
    elif sys.argv[1]=='spans': spans()
    elif sys.argv[1]=='verify': verify()
