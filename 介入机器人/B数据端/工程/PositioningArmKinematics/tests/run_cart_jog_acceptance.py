"""离线验收：仅编译和运行本地模型，不连接ADS或PLC；不会重写归零基线。"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys

kin = Path(__file__).resolve().parents[1]
repo = kin.parents[2]
ads = kin.parent / "ADS项目"
tests = kin / "tests"
build = kin / "build-cart-jog"
logs = repo / "docs/acceptance_cart_jog"
build.mkdir(exist_ok=True)
logs.mkdir(parents=True, exist_ok=True)
vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
vs = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
vcvars = Path(vs) / "VC/Auxiliary/Build/vcvarsall.bat"
environment = subprocess.check_output(
    f'cmd /d /s /c ""{vcvars}" x64 -vcvars_ver=14.29 >nul && chcp 65001 >nul && set"', text=True, encoding="utf-8")
env = dict(line.split("=", 1) for line in environment.splitlines() if "=" in line and not line.startswith("="))
env["PATH"] = env.pop("Path", env.get("PATH", ""))
env["VSLANG"] = "1033"
cl = shutil.which("cl.exe", path=env["PATH"])
flags = ["/nologo", "/O2", "/std:c++17", "/EHsc", "/utf-8", "/DNOMINMAX", "/MD", "/W3", "/bigobj",
    "/I" + str(kin / "include"), "/I" + str(kin / ".deps/Library/include/eigen3"), "/I" + str(build),
    "/I" + str(ads), "/I" + str(ads / "ADS/Include")]
sources = [kin / "src/kinematics.cpp", kin / "src/program_zero.cpp"]
results = {}


def run(name, command):
    result = subprocess.run([str(x) for x in command], cwd=build, env=env, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    output = result.stdout.decode("utf-8", errors="replace")
    (logs / (name + ".log")).write_text(output, encoding="utf-8")
    print(name, "exit=" + str(result.returncode), flush=True)
    print(output, end="", flush=True)
    results[name] = result.returncode
    return result.returncode


run("st_translation", [sys.executable, tests / "translate_cart_jog_st.py",
    repo / "C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU", build / "arm_cart_jog_st_generated.h"])
run("st_static_audit_round2", [sys.executable, tests / "audit_cart_jog_st.py"])
for name, files, arguments in [
    ("home_regression", [tests / "home_regression.cpp", *sources], ["--verify", tests / "home_baseline.txt"]),
    ("arm_cart_jog_tests", [tests / "arm_cart_jog_tests.cpp", *sources], []),
    ("arm_cart_jog_equivalence", [tests / "arm_cart_jog_equivalence.cpp"], []),
    ("arm_ads_tests", [tests / "arm_ads_test.cpp", ads / "arm_manual_ads_service.cpp", *sources], []),
    ("kinematics_tests", [tests / "kinematics_test.cpp", *sources], []),
]:
    executable = build / (name + ".exe")
    if run(name + "_build", [cl, *flags, *files, "/Fe:" + str(executable)]) == 0:
        run(name, [executable, *arguments])

results["home_baseline_sha256"] = hashlib.sha256((tests / "home_baseline.txt").read_bytes()).hexdigest()
(logs / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
sys.exit(1 if any(value != 0 for value in results.values() if isinstance(value, int)) else 0)
