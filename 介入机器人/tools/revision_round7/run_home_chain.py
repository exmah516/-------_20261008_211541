"""归零五场景：真实WPF、Zenoh、数据端服务/规划器与内存PLC替身。"""
from pathlib import Path
import html
import shutil
import tempfile

repo = Path(__file__).resolve().parents[2]
setup = (repo / "tools/revision_round7/run_dls.py").read_text(encoding="utf-8").split('run("translate"')[0]
setup = setup.replace('build = out / "dls_build"', 'build = out / "home_chain_build"')
exec(compile(setup, "run_dls_setup", "exec"))
ads = repo / "B数据端/工程/ADS项目"
sdk = repo.parent / "zenoh/sdk/zenoh-c-1.10.1-msvc"
flags = ["/nologo", "/O2", "/std:c++17", "/EHsc", "/utf-8", "/DNOMINMAX", "/MD", "/W3", "/bigobj",
    "/DMSGPACK_NO_BOOST", "/I" + str(kin / "include"), "/I" + str(kin / ".deps/Library/include/eigen3"),
    "/I" + str(tests), "/I" + str(ads), "/I" + str(ads / "ADS/Include"),
    "/I" + str(sdk / "include"), "/I" + str(repo.parent / "zenoh/sdk/msgpack-cxx-9.0.0/include")]
simulator = build / "home_chain_simulator.exe"
if run("home_chain_native_build", [cl, *flags, repo / "tools/revision_round7/home_chain_simulator.cpp",
    ads / "arm_manual_ads_service.cpp", ads / "zenoh_service.cpp", kin / "src/kinematics.cpp", kin / "src/program_zero.cpp",
    "/Fe:" + str(simulator), "/link", "/LIBPATH:" + str(sdk / "lib"), "zenohc.dll.lib", "bcrypt.lib", "ws2_32.lib"]):
    raise SystemExit(1)
shutil.copy2(sdk / "bin/zenohc.dll", build)
master = repo / "A主端/工程"
managed_build = master / "build/Round7Release/bin"
props = Path(tempfile.mkdtemp(prefix="vi-home-chain-")) / "isolated.props"
props.write_text('<Project><Import Project="' + html.escape(str(master / "Directory.Build.props"), quote=True)
    + '"/><PropertyGroup><OutputPath>' + html.escape(str(managed_build), quote=True)
    + '/$(MSBuildProjectName)/</OutputPath></PropertyGroup></Project>', encoding="utf-8")
dotnet = subprocess.check_output(["powershell.exe", "-NoProfile", "-File",
    str(master / "tools/Get-ViDotnet.ps1")], text=True).strip()
if run("home_chain_wpf_build", [dotnet, "build", master / "master/VI.Control.Tests.slnx", "-c", "Release",
    "-p:Platform=x64", "-p:DirectoryBuildPropsPath=" + str(props), "-warnaserror", "-m:2", "-v:minimal"]):
    raise SystemExit(1)
run("home_chain", [managed_build / "VI.Control.Tests/VI.Control.Tests.exe", "--home-chain", simulator,
    repo.parent / "zenoh/zenohd.exe"])
(out / "home_chain_results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
raise SystemExit(1 if any(results.values()) else 0)
