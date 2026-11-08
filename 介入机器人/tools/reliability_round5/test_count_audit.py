"""按每次实际执行（不去重）核对四/五轮日志，并保留逐标签计数清单。"""
from pathlib import Path
from collections import Counter
import csv
import hashlib
import json
import zipfile

repo = Path(__file__).resolve().parents[2]
out = repo / "docs/reliability_round5/supplement"

def read(name):
    return (repo / name).read_text(encoding="utf-8-sig", errors="replace").splitlines()

def passes(name, prefix):
    return [line for line in read(name) if line.startswith(prefix)]

wpf_old = passes("docs/acceptance_cart_jog/round4/wpf_tests.log", "PASS:")
wpf_base = passes("docs/reliability_round5/baseline/wpf_tests.log", "PASS:")
wpf_previous = passes("docs/reliability_round5/after/wpf_tests.log", "PASS:")
wpf_new = passes("docs/reliability_round5/supplement/wpf.log", "PASS:")
assert wpf_old == wpf_base == wpf_previous == wpf_new
base_log = "docs/reliability_round5/baseline/zenoh_existing.log"
before_log = "docs/reliability_round5/supplement/zenoh_before.log"
now_log = "docs/reliability_round5/supplement/zenoh.log"
cpp_old, cpp_new = passes(base_log, "[PASS]"), passes(now_log, "[PASS]")
assert cpp_old == cpp_new and len(cpp_old) == 81
cs_base, cs_before, cs_now = [passes(name, "[通过]") for name in (base_log, before_log, now_log)]
assert len(cs_base) == 149 and len(cs_before) == 168
assert not (Counter(cs_base) - Counter(cs_now))
assert not (Counter(cs_before) - Counter(cs_now))
assert cs_base[115] == "[通过] 静止15分钟但状态持续接收不会断联或丢控制权"
assert cs_base[116] == "[通过] 相同 Topic 复用与 Publisher 幂等释放"
core = cs_base[:116]
original_network = cs_base[116:]
added_before = Counter(cs_before) - Counter(cs_base)
added_now = Counter(cs_now) - Counter(cs_before)
assert len(cs_now) == 174 and sum(added_now.values()) == 6
host_log = passes("docs/reliability_round5/host_sdk/results.log", "[通过]")
assert len(host_log) == 151
assert not (Counter(core) - Counter(host_log))

unchanged = []
with zipfile.ZipFile(repo / "docs/reliability_round5/baseline/source.zip") as baseline:
    for name in baseline.namelist():
        if (name.startswith("A主端/工程/master/VI.Control.Tests/") or name.endswith((
            "MasterConsole.Protocol.Tests/RobotServiceTests.cs", "tests/zenoh/protocol_tests.cpp", "tests/zenoh/service_tests.cpp"))):
            assert (repo / name).read_bytes() == baseline.read(name), name
            unchanged.append(name)

with (out / "test_inventory.csv").open("w", encoding="utf-8-sig", newline="") as fp:
    writer = csv.writer(fp)
    writer.writerow(["suite", "assertion_label", "fourth_or_actual_baseline", "fifth_before_supplement", "after_supplement", "result"])
    for suite, old, prior, now in (("WPF", wpf_old, wpf_previous, wpf_new),
                                  ("Zenoh C++ golden", cpp_old, cpp_old, cpp_new),
                                  ("Zenoh C# full", cs_base, cs_before, cs_now)):
        a, b, c = Counter(old), Counter(prior), Counter(now)
        for name in sorted(a.keys() | b.keys() | c.keys()):
            writer.writerow([suite, name, a[name], b[name], c[name], "retained" if a[name] else "added"])

summary = {
    "wpf": {"executed": len(wpf_new), "unique_labels": len(set(wpf_new)),
            "duplicate_label_executions": len(wpf_new) - len(set(wpf_new)),
            "ordered_output_identical_to_round4": True},
    "zenoh_cpp": {"executed": len(cpp_new), "ordered_output_identical_to_baseline": True,
                  "service_boundary_program": "also executed and exited 0; no numeric assertion counter"},
    "zenoh_cs": {"original_core": len(core), "original_network": len(original_network),
                 "actual_round5_baseline_with_network": len(cs_base), "before_supplement": len(cs_before),
                 "after_supplement": len(cs_now), "before_added": dict(added_before), "supplement_added": dict(added_now),
                 "sdk_substitute_separate_run": len(host_log), "removed_or_skipped_original": []},
    "unchanged_original_test_files": unchanged,
    "original_test_file_sha256": {name: hashlib.sha256((repo / name).read_bytes()).hexdigest() for name in unchanged},
    "verdict": "PASS: no original test removed or skipped; compare executed multiplicities, not distinct labels",
}
(out / "test_counts.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(summary, ensure_ascii=False, indent=2))
