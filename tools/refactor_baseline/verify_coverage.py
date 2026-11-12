"""检查黄金输入确实命中保留行为，避免空跑场景也得到相等结果。"""
import csv

def verify(folder):
    def rows(name):
        with (folder/(name+'.csv')).open(encoding='utf-8') as f:
            return [{k: float.fromhex(v) if '0x' in v else float(v)
                     for k,v in r.items()} for r in csv.DictReader(f)]
    def hit(name, predicate):
        assert any(predicate(r) for r in rows(name)), name
    for name in ['return_axis1','return_axis6','return_both']:
        for phase in [1,2,3,4,6,8,9]:
            hit(name,lambda r,p=phase:r['state9']==p)
        hit(name,lambda r:r['clear_mask']>0)
    for suffix in ['', '_axis6', '_both']:
        hit('return_retry'+suffix,lambda r:r['state13']==1 and r['state6']==0)
        assert all(r['state6']==0 for r in rows('return_retry'+suffix))
        for name in ['return_possible','return_fault']:
            hit(name+suffix,lambda r:r['state6']==1)
            hit(name+suffix,lambda r:r['commit_count']==2 and r['state9']==9)
    hit('handoff_merge',lambda r:r['state33']==r['state34']==1)
    hit('handoff_axis1',lambda r:r['state14']==1)
    hit('handoff_active',lambda r:r['state43']==1)
    hit('handoff_passive',lambda r:r['state44']==1 and r['state43']==0)
    hit('lead_hold',lambda r:r['state21']>=4)
    hit('lead_hold',lambda r:r['tick']>100 and r['state2']==1 and r['state20']==0)
    hit('lead_axis6',lambda r:r['state34']==1 and r['state33']==0 and r['state21']>=4)
    hit('handoff_merge',lambda r:abs(r['state21']-10)<1e-9 and abs(r['state22']-2)<1e-9)
    hit('axis4',lambda r:r['axis4r']==1 and r['refer6']<123)
    for name in ['spacing','spacing_hold']:
        hit(name,lambda r:r['state16']==2 and r['state19']>0)
        hit(name,lambda r:r['state16']==3)
    hit('spacing_hold',lambda r:r['tick']>=70 and r['state2']==1 and r['state16']==0)
    hit('restart_accept',lambda r:r['state45']==1)
    hit('return_estop',lambda r:r['state5']==1)
    hit('feedback',lambda r:r['state47']==1)
    hit('feedback',lambda r:r['state48']==1)
    hit('manual',lambda r:r['cyl3']==780)
    hit('manual',lambda r:r['yvalve']==1)
    hit('manual',lambda r:r['inject1p']==1)
    print('PASS behavior coverage checks')
