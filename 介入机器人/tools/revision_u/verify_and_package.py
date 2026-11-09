"""核对本轮范围并导出仅包含 U 源码、测试和复现脚本的补丁。"""
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
out = repo / "docs/revision_u_20261108"
start = json.loads((out / "start_manifest.json").read_text(encoding="utf-8"))
allowed = start["allowed"]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


outside = [name for name, sha in start["hashes"].items()
           if name not in allowed and digest(repo / name) != sha]
assert not outside, outside
gitroot = Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"], cwd=repo, text=True).strip())
prefixes = [(repo / name).relative_to(gitroot).as_posix() for name in allowed]


def omit_u(patch):
    return b"".join(b"diff --git " + chunk for chunk in patch.split(b"diff --git ")[1:]
                    if not any(chunk.startswith(("a/" + name + " b/" + name + "\n").encode("utf-8")) for name in prefixes))


initial_patch = (out / "start-of-u.patch").read_bytes()
current_patch = subprocess.check_output(["git", "diff", "--binary"], cwd=repo)
assert omit_u(initial_patch) == omit_u(current_patch), "本轮之外的既有 diff 发生变化"
module_cs = "A主端/工程/master/VI.Module.Intervention/InterventionPanel.xaml.cs"
assert digest(repo / module_cs) == start["hashes"][module_cs]

new_files = ["A主端/工程/master/VI.Module.Intervention/ReadoutPartConverter.cs",
             "A主端/工程/master/VI.Control.Tests/RevisionUChecks.cs",
             *[p.relative_to(repo).as_posix() for p in sorted((repo / "A主端/工程/master/VI.Control.Tests/Fixtures").glob("*.xml"))],
             "tools/revision_u/run_managed.py", "tools/revision_u/run_regressions.py", "tools/revision_u/verify_and_package.py"]
paths = [*allowed, *new_files]
index = gitroot / ".git/index"
index_before = digest(index)
with tempfile.TemporaryDirectory(prefix="vi-u-index-") as temporary:
    env = dict(os.environ, GIT_INDEX_FILE=str(Path(temporary) / "index"))
    subprocess.run(["git", "read-tree", "HEAD"], cwd=repo, env=env, check=True)
    subprocess.run(["git", "add", "--intent-to-add", "--", *new_files], cwd=repo, env=env, check=True)
    patch = subprocess.check_output(["git", "-c", "core.quotepath=false", "diff", "HEAD", "--binary", "--", *paths], cwd=repo, env=env)
    subprocess.run(["git", "diff", "HEAD", "--check", "--", *paths], cwd=repo, env=env, check=True)
assert digest(index) == index_before, "真实暂存区发生变化"
(out / "u-only.patch").write_bytes(patch)
subprocess.run(["git", "apply", "--reverse", "--check", str(out / "u-only.patch")], cwd=gitroot, check=True)

unchanged_arm = {}
for name in ("arm_cart_jog_round4_diagnostics", "arm_cart_jog_tests"):
    current = out / "arm/round4" / (name + ".log")
    previous = repo / "docs/revision_uhn_20261108/arm/round4" / (name + ".log")
    unchanged_arm[name] = {"same_output_as_before_u": current.read_bytes() == previous.read_bytes(),
                           "failed_assertions": len(re.findall(r"^FAIL ", current.read_text(encoding="utf-8"), re.M))}
    assert unchanged_arm[name]["same_output_as_before_u"]

builds = {}
for name in ("master_build", "wpf_build", "handlehost_build", "ads_build"):
    log = (out / (name + ".log")).read_text(encoding="utf-8-sig")
    builds[name] = "0 Warning(s)" in log and "0 Error(s)" in log and "Build succeeded." in log
    assert builds[name], name

screens = {}
for index, name in enumerate(("01-layout-1280x800.png", "02-layout-2560x1600.png", "03-preparation-expanded.png", "04-arm-debug-expanded.png", "05-disconnected.png")):
    path = out / "screenshots" / name
    width, height = struct.unpack(">II", path.read_bytes()[16:24])
    assert (width, height) == ((2560, 1600) if index == 1 else (1280, 800))
    screens[name] = {"width": width, "height": height, "sha256": digest(path)}

preview_cards = [(16, 12, 1248, 52), (16, 118, 852, 54), (16, 222, 421, 191),
                 (447, 222, 421, 191), (16, 423, 852, 204), (880, 118, 384, 668)]
cards = json.loads((out / "screenshots/card_geometry.json").read_text(encoding="utf-8"))
assert len(cards) == len(preview_cards)
geometry = []
for actual, reference in zip(cards, preview_cards):
    delta = {key: round(actual[key] - expected, 4) for key, expected in zip(("x", "y", "width", "height"), reference)}
    assert all(abs(value) < 0.25 for value in delta.values()), delta
    geometry.append({"actual": actual, "html_reference": dict(zip(("x", "y", "width", "height"), reference)), "delta_dip": delta})
key = "{http://schemas.microsoft.com/winfx/2006/xaml}Key"
old_keys = {element.get(key) for element in ET.parse(out / "baseline/A主端/工程/master/VI.Control/Themes/LightClinical.xaml").getroot() if element.get(key)}
new_keys = {element.get(key) for element in ET.parse(repo / "A主端/工程/master/VI.Control/Themes/LightClinical.xaml").getroot() if element.get(key)}
assert old_keys <= new_keys
result = {
    "tracked_files_checked": len(start["hashes"]), "changed_outside_u": outside,
    "preexisting_hn_diff_identical": True, "intervention_code_behind_unchanged": True,
    "real_git_index_unchanged": True, "reverse_patch_check": "passed", "patch_sha256": digest(out / "u-only.patch"),
    "u_source_files": {name: digest(repo / name) for name in paths},
    "old_named_style_keys_retained": len(old_keys), "new_named_style_keys": len(new_keys),
    "builds_zero_warnings_and_errors": builds, "known_arm_failures": unchanged_arm,
    "screenshots": screens, "card_geometry_comparison": geometry,
    "preview_sha256": digest(repo / "主端界面预览 (1).html")
}
(out / "scope_and_artifacts.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps({"source_files": len(paths), "outside_changes": outside, "known_arm_failures": unchanged_arm,
                  "patch_sha256": result["patch_sha256"]}, ensure_ascii=False, indent=2))
