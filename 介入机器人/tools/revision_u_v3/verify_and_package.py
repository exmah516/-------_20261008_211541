"""从 v3 开始时的源码导出本补充差异，核对控制代码未改动与验收产物。"""
from pathlib import Path
import hashlib
import json
import os
import re
import struct
import subprocess
import tempfile
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[2]
out = repo / "docs/revision_u_v3_20261109"
start = json.loads((out / "start_manifest.json").read_text(encoding="utf-8"))
gitroot = Path(start["git_root"])
allowed = start["allowed"]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


allowed_git = {(repo / name).relative_to(gitroot).as_posix() for name in allowed}
outside = [name for name, sha in start["tracked"].items()
           if name not in allowed_git and (not (gitroot / name).is_file() or digest(gitroot / name) != sha)]
assert not outside, outside
for name, sha in start["preserved"].items():
    assert digest(repo / name) == sha, name

new_files = [
    "A主端/工程/master/VI.Control/Themes/SizesSmall.xaml",
    "A主端/工程/master/VI.Control/Themes/SizesMedium.xaml",
    "A主端/工程/master/VI.Control/Themes/SizesLarge.xaml",
    "A主端/工程/master/VI.Control.Tests/ResponsiveLayoutChecks.cs",
    *[path.relative_to(repo).as_posix() for path in sorted((repo / "A主端/工程/master/VI.Control.Tests/Fixtures").glob("V3Baseline.*.xml"))],
    "tools/revision_u_v3/run_managed.py", "tools/revision_u_v3/run_regressions.py",
    "tools/revision_u_v3/verify_and_package.py"
]
paths = [*allowed, *new_files]
index_path = Path(subprocess.check_output(["git", "rev-parse", "--git-path", "index"], cwd=repo, text=True).strip())
if not index_path.is_absolute():
    index_path = repo / index_path
index_before = digest(index_path)
with tempfile.TemporaryDirectory(prefix="vi-v3-index-") as directory:
    env = dict(os.environ, GIT_INDEX_FILE=str(Path(directory) / "index"))

    def git(*args, data=None):
        return subprocess.check_output(["git", "-c", "core.quotepath=false", *args], cwd=gitroot, env=env, input=data)

    git("read-tree", "--empty")
    for name in allowed:
        relative = (repo / name).relative_to(gitroot).as_posix()
        sha = git("hash-object", "-w", "--stdin", "--path=" + relative,
                  data=(out / "baseline" / name).read_bytes()).decode().strip()
        git("update-index", "--add", "--cacheinfo", "100644", sha, relative)
    baseline = git("write-tree").decode().strip()
    git("add", "--", *[(repo / name).relative_to(gitroot).as_posix() for name in paths])
    git("diff", "--cached", baseline, "--check")
    patch = git("diff", "--cached", baseline, "--binary")
assert digest(index_path) == index_before, "真实 Git 暂存区发生变化"
(out / "v3-only.patch").write_bytes(patch)
subprocess.run(["git", "apply", "--reverse", "--check", str(out / "v3-only.patch")], cwd=gitroot, check=True)

builds = {}
for name in ["master_build", "wpf_build", "protocol_build", "handlehost_build", "ads_build"]:
    log = (out / (name + ".log")).read_text(encoding="utf-8-sig")
    builds[name] = "Build succeeded." in log and "0 Warning(s)" in log and "0 Error(s)" in log
    assert builds[name], name
wpf = (out / "wpf_tests.log").read_text(encoding="utf-8-sig")
checks = int(re.search(r"All VI.Control checks passed \((\d+) assertions\)", wpf)[1])
managed = json.loads((out / "managed_results.json").read_text(encoding="utf-8"))
assert all(code == 0 for code in managed.values())
regression = json.loads((out / "regression_results.json").read_text(encoding="utf-8"))
assert {name for name, code in regression.items() if code != 0} == {"arm"}
arm = {}
for name in ["arm_cart_jog_round4_diagnostics", "arm_cart_jog_tests"]:
    current = out / "arm/round4" / (name + ".log")
    previous = repo / "docs/revision_u_20261108/arm/round4" / (name + ".log")
    arm[name] = {"same_as_before_v3": current.read_bytes() == previous.read_bytes(),
                 "failed_assertions": len(re.findall(r"^FAIL ", current.read_text(encoding="utf-8"), re.M))}
    assert arm[name]["same_as_before_v3"]

geometry = json.loads((out / "screenshots/responsive_geometry.json").read_text(encoding="utf-8"))
screens = {}
expected_sizes = [(701, 415), None, (560, 600), (944, 509), (806, 470), (944, 1010),
                  (1280, 760), (1920, 1010), (944, 509), (944, 509), (944, 509)]
files = sorted((out / "screenshots").glob("[0-9][0-9]-*.png"))
assert len(files) == 11
for file, expected in zip(files, expected_sizes):
    size = struct.unpack(">II", file.read_bytes()[16:24])
    assert size == expected if expected else size[0] == 701 and size[1] > 415
    screens[file.name] = {"width": size[0], "height": size[1], "sha256": digest(file)}
for row in geometry:
    assert row["horizontalScrollbar"] != "Visible" and row["scrollableWidth"] < 0.1
    assert abs(row["width"] - row["viewport"]["X"] - row["viewport"]["Width"] - row["pad"]) < 0.1
    assert abs(row["height"] - row["viewport"]["Y"] - row["viewport"]["Height"] - row["pad"]) < 0.1
    if row["file"] == "04-layout-944x509.png":
        assert row["verticalScrollbar"] == "Collapsed" and row["scrollableHeight"] < 0.1
key = "{http://schemas.microsoft.com/winfx/2006/xaml}Key"
theme = "A主端/工程/master/VI.Control/Themes/LightClinical.xaml"
old_keys = {element.get(key) for element in ET.parse(out / "baseline" / theme).getroot() if element.get(key)}
new_keys = {element.get(key) for element in ET.parse(repo / theme).getroot() if element.get(key)}
assert old_keys <= new_keys
result = {
    "tracked_files_checked": len(start["tracked"]), "changed_outside_v3": outside,
    "preserved_files": start["preserved"], "real_git_index_unchanged": True,
    "reverse_patch_check": "passed", "patch_sha256": digest(out / "v3-only.patch"),
    "source_files": {name: digest(repo / name) for name in paths},
    "old_named_resource_keys_retained": len(old_keys), "new_named_resource_keys": len(new_keys),
    "builds_zero_warnings_and_errors": builds, "wpf_assertions": checks,
    "known_preexisting_arm_failures": arm, "screenshots": screens,
    "preview_sha256": digest(out / "preview.html")
}
(out / "scope_and_artifacts.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps({"files": len(paths), "wpf_assertions": checks, "outside_changes": outside,
                  "known_arm_failures": arm, "patch_sha256": result["patch_sha256"]}, ensure_ascii=False, indent=2))
