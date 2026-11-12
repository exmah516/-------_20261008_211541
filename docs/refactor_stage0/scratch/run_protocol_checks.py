"""复用第一轮离线协议/归零链路编排，仅重定向日志和已构建的测试程序。"""
from pathlib import Path
import importlib.util
import sys

root = Path(__file__).resolve().parents[3]
original = root / 'docs/arch_survey_r1/scratch'
spec = importlib.util.spec_from_file_location('run_survey_checks', original / 'run_survey_checks.py')
common = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = common
spec.loader.exec_module(common)
common.out = root / 'docs/refactor_stage0/logs/protocol_final'
common.out.mkdir(parents=True, exist_ok=True)
common.results = {}
script = original / 'run_zenoh_and_home.py'
source = script.read_text(encoding='utf-8-sig')
source = source.replace("out / 'managed/bin/", "root / '介入机器人/A主端/工程/build/Stage0Release/bin/")
exec(compile(source, str(script), 'exec'), {'__file__': str(script), '__name__': '__main__'})
raise SystemExit(any(row['exit'] for row in common.results.values()))
