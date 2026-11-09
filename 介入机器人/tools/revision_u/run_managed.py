"""独立目录构建并执行既有主端测试，不终止运行中的界面。"""
from pathlib import Path
import html
import json
import os
import shutil
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
out = repo / "docs/revision_u_20261108"
out.mkdir(parents=True, exist_ok=True)
master = repo / "A主端/工程"
build = master / "build/URelease/bin"
props = Path(tempfile.mkdtemp(prefix="vi-u-build-")) / "isolated.props"
props.write_text('<Project><Import Project="' + html.escape(str(master / "Directory.Build.props"), quote=True)
    + '"/><PropertyGroup><OutputPath>' + html.escape(str(build), quote=True)
    + '/$(MSBuildProjectName)/</OutputPath></PropertyGroup></Project>', encoding="utf-8")
dotnet = subprocess.check_output(["powershell.exe", "-NoProfile", "-File",
    str(master / "tools/Get-ViDotnet.ps1")], text=True).strip()
results = {}

def run(name, args, env=None):
    with (out / (name + ".log")).open("wb") as log:
        result = subprocess.run([str(x) for x in args], cwd=repo, env=env, stdout=log, stderr=subprocess.STDOUT)
    results[name] = result.returncode
    (out / "managed_results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    print(name, result.returncode, flush=True)
    assert result.returncode == 0, str(out / (name + ".log"))

env = dict(os.environ, DOTNET_CLI_UI_LANGUAGE="en", PYTHONUTF8="1")
for name, project in [("master_build", "MasterConsole.sln"), ("wpf_build", "VI.Control.Tests.slnx"),
        ("protocol_build", "MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.csproj"),
        ("handlehost_build", "VI.HandleHost/VI.HandleHost.csproj")]:
    run(name, [dotnet, "build", master / "master" / project, "-c", "Release", "-p:Platform=x64",
        "-p:DirectoryBuildPropsPath=" + str(props), "-warnaserror", "-m:2", "-v:minimal"], env)
for name in ["VI.Control", "VI.Control.Tests", "MasterConsole.Protocol.Tests", "VI.Service.Robot"]:
    shutil.copytree(build / "VI.HandleHost", build / name / "HandleHost", dirs_exist_ok=True)
fixtures = repo.parent / "protocol/fixtures/zenoh-v1"
run("wpf_tests", [build / "VI.Control.Tests/VI.Control.Tests.exe", out / "screenshots"], env)
run("managed_tests", [build / "MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.exe", fixtures], env)
# 复用第五轮原生SDK替身构建入口，输出落入本轮目录，保留第五轮记录。
script = repo / "tools/reliability_round5/run_host_sdk.py"
source = script.read_text(encoding="utf-8").replace("docs/reliability_round5/host_sdk",
    "docs/revision_u_20261108/host_sdk").split("dotnet=")[0]
exec(compile(source, str(script), "exec"), {"__file__": str(script)})
sdk_env = dict(env, VI_TEST_FAKE_SDK=str(out / "host_sdk/FakeFLCatheter.dll"))
run("host_sdk_results", [build / "MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.exe", fixtures], sdk_env)
