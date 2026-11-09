"""第七轮离线DLS验收；只运行数学模型，不连接PLC。"""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys

repo = Path(__file__).resolve().parents[2]
kin = repo / "B数据端/工程/PositioningArmKinematics"
tests = kin / "tests"
out = repo / "docs/revision_round7_20261109"
build = out / "dls_build"
build.mkdir(parents=True, exist_ok=True)
vs = Path("D:/Work_software/VS2022")
vcvars = vs / "VC/Auxiliary/Build/vcvarsall.bat"
environment = subprocess.check_output(
    f'cmd /d /s /c ""{vcvars}" x64 -vcvars_ver=14.29 >nul && chcp 65001 >nul && set"',
    text=True, encoding="utf-8")
env = dict(line.split("=", 1) for line in environment.splitlines() if "=" in line and not line.startswith("="))
env["PATH"] = env.pop("Path", env.get("PATH", ""))
env.update(VSLANG="1033", PYTHONUTF8="1", PYTHONIOENCODING="utf-8")
cl = shutil.which("cl.exe", path=env["PATH"])
results = {}


def run(name, command):
    process = subprocess.run([str(x) for x in command], cwd=build, env=env,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / (name + ".log")).write_bytes(process.stdout)
    print(name, "exit=" + str(process.returncode), flush=True)
    print(process.stdout.decode("utf-8", errors="replace"), end="", flush=True)
    results[name] = process.returncode
    return process.returncode


run("translate", [sys.executable, tests / "translate_cart_jog_st.py",
    out / "dls_candidate/ArmCartJogGen.TcPOU", build / "arm_cart_jog_st_generated.h"])
flags = ["/nologo", "/O2", "/std:c++17", "/EHsc", "/utf-8", "/DNOMINMAX", "/MD", "/W3", "/bigobj",
    "/I" + str(kin / "include"), "/I" + str(kin / ".deps/Library/include/eigen3"), "/I" + str(build)]
exe = build / "dls_acceptance.exe"
if run("dls_build", [cl, *flags, out / "dls_candidate/dls_acceptance.cpp", kin / "src/kinematics.cpp",
                    kin / "src/program_zero.cpp", "/Fe:" + str(exe)]) == 0:
    run("dls_acceptance", [exe, out / "dls_cases.csv"])
(out / "dls_results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
sys.exit(1 if any(results.values()) else 0)
