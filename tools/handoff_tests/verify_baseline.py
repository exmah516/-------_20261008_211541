"""原黄金保持不变：无换手全帧比较，有换手严格比较同一触发点之前的每一帧。"""
import csv
def rows(path):
    with path.open(encoding='utf-8-sig') as f:
        return [{k:float.fromhex(v) if '0x' in v else float(v) for k,v in row.items()} for row in csv.DictReader(f)]
def compare_prefix(actual,golden):
    a,b=rows(actual),rows(golden)
    assert len(a)==len(b),(actual.name,'帧数不同')
    starts=[next((i for i,r in enumerate(data) if r['state9']!=0),len(data)) for data in (a,b)]
    assert starts[0]==starts[1],(actual.name,'换手触发拍改变',starts)
    for i in range(starts[0]):
        assert a[i]==b[i],(actual.name,'非换手输出/状态改变',i+1,[k for k in a[i] if a[i][k]!=b[i][k]])
    return dict(scope='full' if starts[0]==len(a) else 'before_first_handoff',equal_ticks=starts[0],trigger_tick=None if starts[0]==len(a) else starts[0]+1)
