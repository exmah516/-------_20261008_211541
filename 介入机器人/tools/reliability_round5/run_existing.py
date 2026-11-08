"""保存第五轮修改前/后的既有离线验收；不加载生产 ADS 程序。"""
from pathlib import Path
import concurrent.futures
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile

repo = Path(__file__).resolve().parents[2]
stage = sys.argv[1] if len(sys.argv) > 1 else "after"
assert stage in ("baseline", "after")
out = repo / "docs/reliability_round5" / stage
out.mkdir(parents=True, exist_ok=True)
ads = repo / "B数据端/工程/ADS项目"
master = repo / "A主端/工程"
env = dict(os.environ, PYTHONUTF8="1", PYTHONIOENCODING="utf-8", VSLANG="1033", DOTNET_CLI_UI_LANGUAGE="en")
ps = shutil.which("pwsh") or shutil.which("powershell.exe")
dotnet = subprocess.check_output([ps, "-NoProfile", "-File", str(master / "tools/Get-ViDotnet.ps1")], text=True).strip()
results = {}


def run(name, args, cwd=repo):
    result = subprocess.run([str(x) for x in args], cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / (name + ".log")).write_bytes(result.stdout)
    results[name] = result.returncode
    print(name, "exit=" + str(result.returncode), flush=True)
    if result.returncode:
        print(result.stdout.decode("utf-8", errors="replace")[-5000:], flush=True)
    return result.returncode


if stage == "baseline":
    archive = out / "source.zip"
    assert not archive.exists(), "基线已存在，禁止覆盖"
    files = [p for root in (ads, master / "master", repo / "C从端/250902/Untitled2")
             for p in root.rglob("*") if p.is_file() and p.suffix in
             (".cpp", ".h", ".cs", ".xaml", ".csproj", ".vcxproj", ".sln", ".slnx", ".TcPOU", ".TcGVL", ".plcproj", ".json", ".config")
             and not any(x in p.parts for x in ("bin", "obj", "build", "x64", ".vs", ".build"))]
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as z:
        for p in files:
            z.write(p, p.relative_to(repo))
    (out / "source_hashes.json").write_text(json.dumps({str(p.relative_to(repo)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in files}, ensure_ascii=False, indent=2), encoding="utf-8")
    (out / "git_head.txt").write_bytes(subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo))


def native():
    run("native_existing", ["node", ads / "tools/run_offline_tests.cjs", "all", out / "native"])
    run("net_distance", [sys.executable, ads / "tools/run_ads_diagnostics_test.py", "handle_net_distance_test"])
    kin_runner = repo / "B数据端/工程/PositioningArmKinematics/tests/run_cart_jog_acceptance.py"
    # 保持既有脚本及第四轮日志原样，只将本次输出重定向到独立目录。
    source = kin_runner.read_text(encoding="utf-8").replace('logs = repo / "docs/acceptance_cart_jog"',
        "logs = Path(" + repr(str(out / "arm")) + ")")
    command = "import sys; sys.argv = [" + repr(str(kin_runner)) + ", '--round4']; exec(compile(" + repr(source) + ", " + repr(str(kin_runner)) + ", 'exec'), {'__file__': " + repr(str(kin_runner)) + ", '__name__': '__main__'})"
    run("arm_existing", [sys.executable, "-c", command])


def managed():
    for name, solution in (("master_build", "MasterConsole.sln"), ("wpf_build", "VI.Control.Tests.slnx")):
        run(name, [dotnet, "build", master / "master" / solution, "-c", "Release", "-p:Platform=x64", "-warnaserror", "-m:2", "-v:minimal"])
    run("wpf_tests", [master / "build/Release/bin/VI.Control.Tests/VI.Control.Tests.exe", out / "ui"])
    run("zenoh_existing", [ps, "-NoProfile", "-File", ads / "tests/run-zenoh-tests.ps1", "-DotnetPath", dotnet])


with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    for future in (pool.submit(native), pool.submit(managed)):
        future.result()
run("ads_build", ["D:/Work_software/VS2022/MSBuild/Current/Bin/MSBuild.exe", ads / "ADS.sln", "/t:Rebuild", "/p:Configuration=Release", "/p:Platform=x64", "/nologo", "/verbosity:minimal"])
(out / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
print(json.dumps(results, indent=2), flush=True)
sys.exit(1 if any(v for k, v in results.items() if k != "arm_existing") else 0)
