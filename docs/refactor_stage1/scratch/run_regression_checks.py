"""复用已审核的离线净距离和定位臂验收，不连接设备。"""
from pathlib import Path
import importlib.util
import sys
root=Path(__file__).resolve().parents[3]
script=root/'docs/arch_survey_r1/scratch/run_survey_checks.py'
spec=importlib.util.spec_from_file_location('stage1_regression',script)
common=importlib.util.module_from_spec(spec)
sys.modules[spec.name]=common
spec.loader.exec_module(common)
common.out=root/'docs/refactor_stage1/logs/regression'
common.out.mkdir(parents=True,exist_ok=True)
common.temp=common.out/'temp'
common.temp.mkdir(exist_ok=True)
common.env.update(TMP=str(common.temp), TEMP=str(common.temp), DOTNET_CLI_HOME=str(common.out/'dotnet_home'))
common.results={}
if 'net' in sys.argv:
    common.run('net_distance',[sys.executable,common.ads/'tools/run_ads_diagnostics_test.py','handle_net_distance_test'])
else:
    common.arm()
raise SystemExit(any(row['exit'] for row in common.results.values()))
