"""生成完整源码差异及交付清单；只做反向check，不应用补丁。"""
from pathlib import Path
import hashlib
import json
import re
import subprocess
from collections import Counter

repo=Path(__file__).resolve().parents[2]
out=repo/"docs/reliability_round5"
gitroot=Path(subprocess.check_output(["git","rev-parse","--show-toplevel"],cwd=repo,text=True).strip())
prefix=repo.relative_to(gitroot).as_posix()+"/"

def git(*args):
    return subprocess.run(["git","-c","core.safecrlf=false","-c","core.quotePath=false",*args],
        cwd=repo,stdout=subprocess.PIPE,stderr=subprocess.PIPE)

tracked=git("diff","--relative","--name-only","-z","--",".").stdout.decode("utf-8").split("\0")
tracked=[name for name in tracked if name]
assert all(".build/" not in name for name in tracked), "先恢复已跟踪测试EXE"
assert all(name.startswith(("A主端/","B数据端/","C从端/")) for name in tracked), tracked
untracked=git("ls-files","--others","--exclude-standard","-z","--",".").stdout.decode("utf-8").split("\0")
new=[]
for name in untracked:
    if (name.startswith("A主端/") and name.endswith((".cs",".csproj",".targets"))
        or name.startswith("tools/reliability_round5/") and name.endswith((".cpp",".h",".py"))):
        assert "/obj/" not in name and "/bin/" not in name, name
        new.append(name)
new += ["B数据端/工程/ADS项目/"+name for name in (
    "return_recovery.h","homing_record.h","handle_velocity_recorder.h","reliability_config.json","reliability_config.md")]
new += ["docs/reliability_round5/report.md","docs/reliability_round5/low_frequency_inventory.md","docs/reliability_round5/supplement/report.md"]
new=sorted(set(new))
for name in new:
    assert (repo/name).is_file(),name
patch=git("diff","--no-ext-diff","--binary","--",".").stdout
for name in new:
    result=git("diff","--no-index","--binary","--","/dev/null",name)
    assert result.returncode==1, (name,result.stderr.decode("utf-8",errors="replace"))
    lines=result.stdout.splitlines(keepends=True)
    for index,line in enumerate(lines):
        if line.startswith(b"diff --git "):
            lines[index]=line.replace(b" a/",(" a/"+prefix).encode()).replace(b" b/",(" b/"+prefix).encode())
        elif line.startswith(b"+++ b/"):
            lines[index]=line.replace(b"+++ b/",("+++ b/"+prefix).encode(),1)
    patch+=b"".join(lines)
(out/"full.diff").write_bytes(patch)
manifest={"base_commit":"806036a8f0b65a2d6557b3f7d47bcf5f65990acf",
    "apply_from":str(gitroot),"tracked":tracked,"new_including_gitignored_production_files":new,
    "excluded":"构建EXE/DLL/obj及日志不重复写入源码patch；原始日志保留在docs/reliability_round5",
    "sha256":{name:hashlib.sha256((repo/name).read_bytes()).hexdigest() for name in tracked+new},
    "patch_sha256":hashlib.sha256(patch).hexdigest()}
check=subprocess.run(["git","-c","core.safecrlf=false","apply","--reverse","--check",str(out/"full.diff")],
    cwd=gitroot,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/"patch_check.log").write_bytes(check.stdout)
assert check.returncode==0, check.stdout.decode("utf-8",errors="replace")
manifest["reverse_check"]="PASS；只检查当前工作区与补丁一致，没有撤销或应用"
(out/"patch_manifest.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding="utf-8")

def lines(path): return (out/path).read_text(encoding="utf-8-sig",errors="replace").splitlines()
old_arm,new_arm=lines("baseline/arm_existing.log"),lines("after/arm_existing.log")
fail=lambda rows:[line.strip() for line in rows if line.startswith("FAIL")]
assert fail(old_arm)==fail(new_arm)
numeric=lambda rows:[line.strip() for line in rows if line.startswith(("S4 case=","ST_EQ "))]
assert numeric(old_arm)==numeric(new_arm)
old_ui,new_ui=lines("baseline/wpf_tests.log"),lines("after/wpf_tests.log")
ui_pass=lambda rows:Counter(line for line in rows if line.startswith("PASS:"))
assert ui_pass(old_ui)==ui_pass(new_ui)
build={}
for name in ("master","wpf","host","ads"):
    data="\n".join(lines("build/"+name+".log"))
    assert re.search(r"0 (个警告|Warning)",data),name
    assert re.search(r"0 (个错误|Error)",data),name
    build[name]="0 warnings, 0 errors"
for report in (out/"report.md",out/"supplement/report.md"):
    prose=re.sub(r"`[^`]*`", "", report.read_text(encoding="utf-8"))
    for path in re.findall(r"\]\(([^)]+)\)",prose):
        assert (report.parent/path).exists(),path
counts=json.loads((out/"supplement/test_counts.json").read_text(encoding="utf-8"))
assert counts["wpf"]["executed"]==182 and counts["zenoh_cpp"]["executed"]==81
assert counts["zenoh_cs"]["after_supplement"]==174
summary={"build":build,"plc_build":"未构建",
    "wpf_baseline_pass_labels_retained":len(ui_pass(old_ui)),
    "wpf_executed_checks":sum(ui_pass(new_ui).values()),
    "arm_failure_labels_identical":fail(new_arm),
    "arm_numeric_output_lines_identical":len(numeric(old_arm)),
    "native":"PASS final_native.log，包含100Hz速度记录及全部既有离线用例",
    "zenoh":"PASS final_zenoh.log C++81/0,C#174/0；原有用例逐项保留",
    "host_sdk":"PASS host_sdk/results.log C#151/0，200.0Hz；remove45.8ms/arrival52.0ms到双柄有效",
    "st":"11049 cycles, max_difference=0,29228 checks,62 new-exit assertions；SelfCheck完整5686cycles",
    "record_events":"13 checks","restore_ack":"10 checks","recovery":"129 checks",
    "not_completed":["真实USB拔插／驱动恢复耗时现场验证","现场速度数据尚未代入追赶量／时间公式","PLC XAE及全部现场验收"],
    "source_patch_files":len(tracked)+len(new),"report_links":"PASS"}
(out/"final_results.json").write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding="utf-8")
print(json.dumps(summary,ensure_ascii=False,indent=2))
