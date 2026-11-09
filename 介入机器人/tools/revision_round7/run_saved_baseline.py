"""补跑开工时已构建并保留的基线二进制，使用其55字段协议夹具。"""
from pathlib import Path
import json
import os
import subprocess

repo = Path(__file__).resolve().parents[2]
out = repo / "docs/revision_round7_20261109"
exe = repo / "A主端/工程/build/UV3Release/bin/MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.exe"
fixtures = out / "baseline/protocol-fixtures"
results = {}
for name, extra in [("managed_tests", {}), ("host_sdk_results", {
    "VI_TEST_FAKE_SDK": str(out / "final_results/host_sdk/FakeFLCatheter.dll")})]:
    with (out / "baseline_results" / (name + ".log")).open("wb") as log:
        process = subprocess.run([str(exe), str(fixtures)], env=dict(os.environ, **extra), stdout=log, stderr=subprocess.STDOUT)
    results[name] = process.returncode
    print(name, process.returncode, flush=True)
(out / "baseline_saved_results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
raise SystemExit(1 if any(results.values()) else 0)
