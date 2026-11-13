"""检查完整生产主循环与ST闭环的故障序列、最终状态及自动续做次数。"""
from pathlib import Path
import csv
import json
import re
import sys


def verify(folder):
    folder = Path(folder)
    checks = []

    def check(ok, label):
        if not ok:
            raise AssertionError(label)
        checks.append(label)

    def load(name):
        with (folder / (name + '.csv')).open(encoding='utf-8') as f:
            ticks = [{k: float.fromhex(v) if '0x' in v else float(v) for k, v in r.items()} for r in csv.DictReader(f)]
        with (folder / (name + '.csv.events.csv')).open(encoding='utf-8') as f:
            events = [{k: int(v) for k, v in r.items()} for r in csv.DictReader(f)]
        coverage = (folder / (name + '.log')).read_text(encoding='utf-8').split('COVERAGE ')[-1]
        counts = {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)(?![\d,])', coverage)}
        return ticks, events, counts

    def select(events, code):
        return [r for r in events if r['event_code'] == code]

    def ordered(events, codes, name):
        index = 0
        for row in events:
            if index < len(codes) and row['event_code'] == codes[index]:
                index += 1
        check(index == len(codes), name + ': ordered ' + str(codes))

    for name in ['txn_pause', 'txn_ads_hold', 'txn_handle_hold', 'txn_restart', 'txn_start_wait']:
        ticks, events, counts = load(name)
        last = ticks[-1]
        check(counts['starts'] == 2 and counts['complete'] == 1, name + ': each leg starts once; one takeover')
        check(counts['commits'] == (0 if name == 'txn_start_wait' else 1), name + ': no duplicate request')
        check(counts['plc_phase'] == 0 and counts['plc_result'] == 1, name + ': PLC released successfully')
        check(last['state9'] == last['state2'] == last['state6'] == 0 and last['state0'] == 1, name + ': Idle, controlled, no hold')
        check([last['cyl'+str(i)] for i in range(1, 5)] == [450, 1000, 550, 1000], name + ': normal grip')
        check(len(select(events, 44)) == len(select(events, 45)) == len(select(events, 28)) == 1, name + ': done/rebase/takeover exactly once')
        ordered(events, [43, 44, 28, 45], name)
        done, rebase, takeover = [select(events, code)[0] for code in [44, 28, 45]]
        check(rebase['ads_snapshot_sequence'] > done['ads_snapshot_sequence'] and takeover['tick'] > rebase['tick'], name + ': newer snapshot then output confirmation')
        check(done['value'] == 1, name + ': success result')
        expected = (25, 125) if name == 'txn_start_wait' else (30, 116)
        check(abs(last['refer1']-expected[0]) < .01 and abs(last['refer6']-expected[1]) < .01, name + ': retained PLC return endpoints')
        if name == 'txn_pause':
            check(counts['pause'] == 1, name + ': 2000 PLC scans run without a main iteration; internal completion assertion passed')
        if name in ['txn_ads_hold', 'txn_handle_hold']:
            source = 1 if name == 'txn_ads_hold' else 2
            check(any(r['value'] & source for r in select(events, 10)), name + ': injected hold source observed')
        if name in ['txn_restart', 'txn_start_wait']:
            starts = [r for r in select(events, 12) if r['value'] & 65535]
            check(starts[-1]['tick'] > takeover['tick'], name + ': StartControl follows takeover')
        if name == 'txn_restart':
            check(counts['restart'] == 1 and len(select(events, 43)) == 2, name + ': process locals reset and existing id adopted')

    ticks, events, counts = load('return_retry')
    sends = select(events, 42)
    check(len(sends) == 2 and sends[0]['value'] == sends[1]['value'] and sends[1]['tick'] - sends[0]['tick'] == 30, 'write failure: same id resend at 300ms')
    check(counts['transactions'] == counts['starts'] == counts['complete'] == 1, 'write failure: one accepted transaction and one motion')
    ordered(events, [42, 42, 43, 44, 45], 'write failure')

    for name, interrupted in [('return_estop', True), ('txn_always_fail', False)]:
        ticks, events, counts = load(name)
        check(counts['commits'] == counts['complete'] == 2, name + ': original and only one automatic resumption')
        check(len(select(events, 17)) == 1, name + ': one resume event')
        ordered(events, [44, 45, 15, 16, 17, 44, 45], name)
        check(ticks[-1]['state9'] == ticks[-1]['state6'] == 0 and ticks[-1]['state0'] == 1, name + ': recovers at current position')
        first_done = select(events, 44)[0]['value']
        check(first_done & 65535 == 2 and first_done >> 16 == (5 if interrupted else 3), name + ': classified failure result')
        if interrupted:
            ordered(events, [13, 14, 44, 45], name)
            check(select(events, 44)[1]['value'] == 1, name + ': resumed transaction succeeds')
        else:
            check(len(select(events, 15)) == 2 and select(events, 44)[1]['value'] & 65535 == 2, name + ': second failure never resumes again')

    ticks, events, counts = load('txn_no_ack')
    check(counts['starts'] == counts['transactions'] == 0, 'no ack: never accepted or moved')
    check(len(select(events, 47)) == 2 and len(select(events, 17)) == 1, 'no ack: timeout plus one resumption')
    sends = select(events, 42)
    check([r['value'] for r in sends] == [1]*4 + [2]*4, 'no ack: bounded resend counts per id')
    check(select(events, 47)[0]['tick'] - sends[0]['tick'] == 100, 'no ack: 1000ms acknowledgement deadline')

    ticks, events, counts = load('txn_version')
    check(all(r['state0'] == r['motion'] == 0 for r in ticks), 'version mismatch: never controls or emits motion')
    check(counts['starts'] == counts['commits'] == 0, 'version mismatch: no transaction')
    result = dict(status='pass', scenarios=10, checks=len(checks), checks_passed=checks)
    (folder / 'robust_checks.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print('PASS robustness:', len(checks), 'checks in 10 scenarios')


if __name__ == '__main__':
    verify(sys.argv[1])
