"""运行完整生产主循环与当前ST的事务鲁棒性验收，不访问设备。"""
from pathlib import Path
import subprocess
import sys
from verify_robust import verify

root = Path(__file__).resolve().parents[2]
out = root / 'docs/refactor_stage1/logs/robust_final'
scenarios = ['txn_pause', 'txn_ads_hold', 'txn_handle_hold', 'txn_restart', 'txn_start_wait',
             'txn_no_ack', 'txn_always_fail', 'txn_version', 'return_retry', 'return_estop']
subprocess.run([sys.executable, '-X', 'utf8', str(root/'tools/refactor_baseline/run.py'), '--output', str(out), '--scenarios', *scenarios], check=True)
verify(out)
