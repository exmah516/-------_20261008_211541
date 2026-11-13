"""验证完整生产主循环产生的事件；与原黄金输出分开，不生成或覆盖黄金。"""
from pathlib import Path
import csv
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / '介入机器人/B数据端/工程/ADS项目/experiment_events.h'


def verify(directory):
    directory = Path(directory)
    header = HEADER.read_text(encoding='utf-8-sig')
    code_block = re.search(r'enum class ExperimentEventCode[^{}]*\{(.*?)\};', header, re.S)[1]
    codes = {name: int(value) for name, value in re.findall(r'(\w+)\s*=\s*(\d+)', code_block)}
    streams = {}
    for path in directory.glob('*.csv.events.csv'):
        with path.open(encoding='utf-8') as file:
            streams[path.name.removesuffix('.csv.events.csv')] = [
                {key: int(value) for key, value in row.items()} for row in csv.DictReader(file)]
    checks = []

    def check(condition, label):
        if not condition:
            raise AssertionError(label)
        checks.append(label)

    def events(scenario, name):
        return [r for r in streams[scenario] if r['event_code'] == codes[name]]

    def ordered(scenario, names):
        index = 0
        for row in streams[scenario]:
            if index < len(names) and row['event_code'] == codes[names[index]]:
                index += 1
        check(index == len(names), scenario + ': ' + ' -> '.join(names))

    # 事件中的当前状态必须能还原黄金逐拍状态，包括同一拍多次变化。
    for scenario, rows in streams.items():
        check(all(r['ads_snapshot_sequence'] == r['tick'] for r in rows), scenario + ': snapshot sequence')
        check([r['tick'] for r in rows] == sorted(r['tick'] for r in rows), scenario + ': ordered ticks')
        active, hold = False, False
        for row in rows:
            if row['event_code'] == codes['ControlActiveChanged']:
                value = bool(row['value'] >> 16)
                check(value != active, scenario + ': control change is an edge')
                active = value
            elif row['event_code'] in (codes['ConnectionHoldEnter'], codes['ConnectionHoldExit']):
                value = row['event_code'] == codes['ConnectionHoldEnter']
                check(value != hold, scenario + ': hold change is an edge')
                hold = value
        with (directory / (scenario + '.csv')).open(encoding='utf-8') as file:
            trace = list(csv.DictReader(file))
        state = {'ControlActiveChanged': 0, 'ConnectionHoldEnter': 0,
                 'PlannedReturnPhase': 0, 'SpacingRecoveryPhase': 0, 'Axis1LeadArmed': 0}
        index = 0
        for tick in trace:
            while index < len(rows) and rows[index]['tick'] <= int(tick['tick']):
                row = rows[index]
                code, value = row['event_code'], row['value']
                if code == codes['ControlActiveChanged']:
                    state['ControlActiveChanged'] = value >> 16
                if code in (codes['ConnectionHoldEnter'], codes['ConnectionHoldExit']):
                    state['ConnectionHoldEnter'] = int(code == codes['ConnectionHoldEnter'])
                for name in ('PlannedReturnPhase', 'SpacingRecoveryPhase'):
                    if code == codes[name]:
                        state[name] = value & 65535
                if code in (codes['Axis1LeadArmed'], codes['Axis1LeadCompleted'], codes['Axis1LeadCleared']):
                    state['Axis1LeadArmed'] = int(code == codes['Axis1LeadArmed'])
                index += 1
            for name, field in [('ControlActiveChanged', 'state0'), ('ConnectionHoldEnter', 'state2'),
                                ('SpacingRecoveryPhase', 'state16'),
                                ('Axis1LeadArmed', 'state20')]:
                if state[name] != int(float.fromhex(tick[field])):
                    raise AssertionError(f'{scenario} tick {tick["tick"]}: {name} != {field}')
        check(True, scenario + ': events reconstruct four per-tick states')

    ordered('return_both', ['DualHandoffIntent','PlannedReturnEnter','HandoffRequest','HandoffAck','HandoffDone','PlannedReturnRebase','HandoffTakeover','Axis1LeadArmed'])
    check([r['value'] & 65535 for r in events('return_both','PlannedReturnPhase')]==[10,20,30,40,50,60,0], 'PLC complete phase sequence')
    check([r['value'] for r in events('return_both','PlannedReturnRebase')]==[50], 'one rebase on a newer snapshot')
    for scenario,mask in [('return_axis1',1),('return_axis6',2),('return_both',3)]:
        check(events(scenario,'PlannedReturnEnter')[0]['value']==mask,scenario+': legs')
        ordered(scenario,['HandoffRequest','HandoffAck','HandoffDone','HandoffTakeover'])
        for name in ['HandoffPhase10Ms','HandoffPhase20Ms','HandoffPhase30Ms','HandoffPhase40Ms','HandoffPhase50Ms','HandoffAxis1MotionMs','HandoffAxis6MotionMs','HandoffReservedMs']:
            check(len(events(scenario,name))==1,scenario+': '+name)
    sends=events('return_retry','HandoffRequest')
    check(len(sends)==2 and sends[0]['value']==sends[1]['value'] and sends[1]['tick']-sends[0]['tick']==30,'same id resend at 300ms')
    ordered('return_fault',['HandoffDone','HandoffTakeover','ReturnFaultEnter','ReturnFaultCleared','ReturnResume','HandoffDone','HandoffTakeover'])
    check([r['value'] for r in events('return_fault','PlannedReturnPlcError')]==[42],'NC error ID')
    check([r['value'] for r in events('return_fault','ReturnResume')]==[65537],'resume once with original leg mask')
    ordered('return_estop',['EstopEnter','EstopExit','HandoffDone','HandoffTakeover','ReturnFaultEnter','ReturnFaultCleared','ReturnResume'])
    ordered('spacing', ['SpacingRecoveryButtonRise', 'SpacingRecoveryPhase', 'SpacingRecoveryStart',
                        'SpacingRecoveryPhase', 'SpacingRecoveryButtonFall', 'SpacingRecoveryPhase', 'SpacingRecoveryPhase'])
    check([r['value'] for r in events('spacing', 'SpacingRecoveryPhase')][:4] == [1, 2, 3, 0],
          'ClampWait -> Active -> ExitSync -> Idle')
    check(all(r['value'] == 7 for r in events('spacing', 'SpacingRecoveryButtonRise')), 'raw rising buttons')
    check(all(r['value'] == 0 for r in events('spacing', 'SpacingRecoveryButtonFall')), 'raw falling buttons')
    ordered('spacing_hold', ['SpacingRecoveryButtonRise', 'SpacingRecoveryPhase', 'SpacingRecoveryHoldCleared',
                             'SpacingRecoveryButtonFall', 'SpacingRecoveryButtonRise', 'SpacingRecoveryPhase'])
    check([r['value'] for r in events('event_rejected_hold', 'SpacingRecoveryRejected')] == [(7 << 16) | 44],
          'rejected during ADS hold: reason and raw buttons')
    ordered('lead_hold', ['Axis1LeadArmed', 'ConnectionHoldEnter', 'Axis1LeadCleared', 'ConnectionHoldExit'])
    check([r['value'] for r in events('lead_hold', 'Axis1LeadCleared')] == [24], 'lead clear hold reason')
    check(len(events('return_both', 'Axis1LeadCompleted')) == 1, 'lead completion edge')
    check(any(r['value'] == 32 for r in events('ads_hold', 'ConnectionHoldEnter')), 'initial sync source')
    check(any(r['value'] & 1 for r in events('ads_hold', 'ConnectionHoldEnter')), 'ADS hold source')
    check(any(r['value'] & 2 for r in events('handle_disconnect', 'ConnectionHoldEnter')), '587 hold source')
    check(any(r['value'] & 4 for r in events('event_missing582', 'ConnectionHoldEnter')), '582 hold source')
    check(any(r['value'] & (8 | 16) for r in events('device_epoch', 'ConnectionHoldEnter')), 'resume settling source')
    ordered('restart_accept', ['PlcRestart', 'HomingReuseResult', 'HomingReuseResult'])
    check([r['value'] & 65535 for r in events('restart_accept', 'HomingReuseResult')] == [68, 60],
          'homing submitted and accepted')
    check([r['value'] & 65535 for r in events('restart_reject', 'HomingReuseResult')] == [64],
          'homing position rejection')
    observed = {r['event_code'] for rows in streams.values() for r in rows}
    check(set(codes.values()) <= observed, 'every defined event code has a production scenario: '+str(set(codes.values())-observed))
    result = dict(status='pass', scenarios=len(streams), checks=len(checks), checks_passed=checks,
                  events=sum(map(len, streams.values())), event_codes=codes)
    (directory / 'event_checks.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'PASS event timeline: {len(streams)} scenarios; {len(checks)} checks; all {len(codes)} codes observed')


if __name__ == '__main__':
    verify(sys.argv[1])
