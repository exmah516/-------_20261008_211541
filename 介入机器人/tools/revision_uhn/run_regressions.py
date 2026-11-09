"""复用既有原生、V5、定位臂与跨语言测试，结果独立保存且不忽略失败。"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import json
import os
import subprocess
import sys

repo = Path(__file__).resolve().parents[2]
ads = repo / "B数据端/工程/ADS项目"
out = repo / "docs/revision_uhn_20261108"
env = dict(os.environ, PYTHONUTF8="1", PYTHONIOENCODING="utf-8", VSLANG="1033", DOTNET_CLI_UI_LANGUAGE="en")
results = {}


def run(name, args):
    with (out / (name + ".log")).open("wb") as log:
        result = subprocess.run([str(x) for x in args], cwd=repo, env=env, stdout=log, stderr=subprocess.STDOUT)
    results[name] = result.returncode
    print(name, "exit=" + str(result.returncode), flush=True)


def run_source(name, script, source, args=()):
    code = "import sys; sys.path.insert(0, " + repr(str(script.parent)) + "); sys.argv = " + repr([str(script), *args])
    code += "; exec(compile(" + repr(source) + ", " + repr(str(script)) + ", 'exec'), {'__file__': " + repr(str(script)) + "})"
    run(name, [sys.executable, "-c", code])


def native():
    run("native", ["node", ads / "tools/run_offline_tests.cjs", "all", out / "native"])
    for test in ("handle_net_distance_test",):
        run(test, [sys.executable, ads / "tools/run_ads_diagnostics_test.py", test])
    for name in ("recovery", "st"):
        script = repo / ("tools/reliability_round5/run_" + name + ".py")
        source = script.read_text(encoding="utf-8").replace("docs/reliability_round5/" + name,
            "docs/revision_uhn_20261108/v5_" + name)
        run_source("v5_" + name, script, source)


def arm():
    script = repo / "B数据端/工程/PositioningArmKinematics/tests/run_cart_jog_acceptance.py"
    source = script.read_text(encoding="utf-8").replace('logs = repo / "docs/acceptance_cart_jog"',
        "logs = Path(" + repr(str(out / "arm")) + ")")
    source = source.replace('build = kin / "build-cart-jog"', 'build = Path(' + repr(str(out / "arm_build")) + ')')
    run_source("arm", script, source, ["--round4"])


def zenoh():
    dotnet = subprocess.check_output(["powershell.exe", "-NoProfile", "-File",
        str(repo / "A主端/工程/tools/Get-ViDotnet.ps1")], text=True).strip()
    run("zenoh", ["powershell.exe", "-NoProfile", "-File", ads / "tests/run-zenoh-tests.ps1", "-DotnetPath", dotnet])


def ads_build():
    vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    run("ads_build", [Path(vs) / "MSBuild/Current/Bin/MSBuild.exe", ads / "ADS.sln", "/t:Rebuild",
        "/p:Configuration=Release", "/p:Platform=x64", "/warnaserror", "/p:TreatWarningAsError=true",
        "/p:OutDir=" + str(out / "network/build_ads/bin") + "/",
        "/p:IntDir=" + str(out / "network/build_ads/obj") + "/", "/nologo", "/verbosity:minimal"])


with ThreadPoolExecutor(max_workers=4) as pool:
    futures = [pool.submit(fn) for fn in (native, arm, zenoh, ads_build)]
    for future in futures:
        future.result()
(out / "regression_results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
raise SystemExit(1 if any(results.values()) else 0)
