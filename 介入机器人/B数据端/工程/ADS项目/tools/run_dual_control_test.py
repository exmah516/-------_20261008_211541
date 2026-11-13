"""沿用公共离线测试入口，避免两套生产代码抽取边界漂移。"""
from pathlib import Path
import subprocess
import sys

here = Path(__file__).resolve().parent
command = ['node', str(here / 'run_offline_tests.cjs'), 'dual']
if len(sys.argv) > 1:
    command.append(str(Path(sys.argv[1]).resolve()))
raise SystemExit(subprocess.call(command))
