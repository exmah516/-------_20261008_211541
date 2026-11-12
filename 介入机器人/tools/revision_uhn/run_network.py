"""复用现有ADS替身和Zenoh服务边界harness，真实等待六种10/30秒中断。"""
from pathlib import Path
import os
import re
import subprocess
import sys

repo = Path(__file__).resolve().parents[2]
ads = repo / "B数据端/工程/ADS项目"
package = repo.parent
out = repo / "docs/revision_uhn_20261108/network"
out.mkdir(parents=True, exist_ok=True)
build = out / ("baseline" if "--baseline" in sys.argv else "fixed")
build.mkdir(exist_ok=True)
vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
vs = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
source = (ads / "main.cpp").read_text(encoding="utf-8-sig")
if "--baseline" in sys.argv:
    source = subprocess.check_output(["git", "show", "HEAD:介入机器人/B数据端/工程/ADS项目/main.cpp"],
        cwd=repo, encoding="utf-8-sig").replace("\r\n", "\n")

def between(text, start, end):
    begin = text.index(start)
    return text[begin:text.index(end, begin)]

rebase = between(source, "\t\telse if (ads_soft_hold_active && planned_return.active())",
    "\t\tconst bool connection_hold_active").replace("else if", "if", 1)
(build / "main_rebase.inc").write_text(rebase, encoding="utf-8")
connection = between(source, "\t\tconst bool connection_hold_active =", "\t\tif (!initial_sync_done)")
(build / "main_connection.inc").write_text(connection, encoding="utf-8")
(build / "main_observe.inc").write_text(between(source, "\t\tconst bool ads_self_check_valid",
    "\t\tif (ads_events.selfcheck_status"), encoding="utf-8")
(build / "main_auto_resume.inc").write_text(between(source, "\t\tif (!control_active && !return_ads_fault_hold",
    "\t\tunsigned short cylinder1_cmd"), encoding="utf-8")
(build / "main_start.inc").write_text(between(source, "\t\t\t\tcase VisCommandType::SelectDirectControl:",
    "\t\t\t\tdefault:").split("\n", 1)[1], encoding="utf-8")
fixture = (ads / "tools/main_recovery_test.cpp").read_text(encoding="utf-8-sig")
fixture = fixture[:fixture.index("\nint main()")]
fixture = re.sub(r"^#include <.*>\n", "", fixture, flags=re.M)
fixture = re.sub(r"^enum class (AdsConnectionState|StartupPhase|GuidewireMode).*\n", "", fixture, flags=re.M)
fixture = fixture.replace("void reset() {} } spacing_recovery;",
    "SpacingRecoveryPhase phase = SpacingRecoveryPhase::Idle; void reset() {} } spacing_recovery;")
fixture = fixture.replace("    void recover() {", """
    double plc_act_pos[7] = {}, axis2_hold_rel = 0, axis7_hold_rel = 0, independent_axis2_hold_rel = 0;
    struct { void disable_compensation() {} } tracking_controller;
    void clear_force_output() {}
    void edge() {
#include "main_connection.inc"
    }
    void recover() {""")
(build / "main_fixture.inc").write_text(fixture, encoding="utf-8")
mock = (ads / "tools/ads_diagnostics_test.cpp").read_text(encoding="utf-8-sig")
mock = mock[:mock.index("\nint main()")]
mock = mock.replace('#include "../ads_communication.h"', '#include "ads_communication.h"')
mock = mock.replace('#include "../ads_communication.cpp"', '#include "ads_communication.cpp"')
(build / "ads_mock.inc").write_text(mock, encoding="utf-8")
entries = []
for row in (ads / "tools/ads_symbol_sizes.tsv").read_text(encoding="utf-8").splitlines():
    if row and not row.startswith("#"):
        name, size = row.split("\t")
        entries.append('{"' + name + '", ' + str(int(size)) + '},')
(build / "ads_symbol_sizes.inc").write_text("\n".join(entries), encoding="utf-8")
sdk = package / "zenoh/sdk/zenoh-c-1.10.1-msvc"
msgpack = package / "zenoh/sdk/msgpack-cxx-9.0.0/include"
includes = [build, ads, ads / "ADS/Include", ads / "手柄", sdk / "include", msgpack]
files = [Path(__file__).with_name("network_tests.cpp"), ads / "plc_io.cpp",
    ads / "zenoh_service.cpp", ads / "手柄/手柄.cpp"]
command = ('@echo off\nchcp 65001 >nul\ncall "' + vs + '\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
    'if errorlevel 1 exit /b 1\ncl /nologo /std:c++17 /EHsc /utf-8 /DNOMINMAX /DMSGPACK_NO_BOOST /Gy /MD '
    + ('/DBASELINE ' if "--baseline" in sys.argv else '')
    + " ".join('/I"' + str(p) + '"' for p in includes) + " "
    + " ".join('"' + str(p) + '"' for p in files)
    + ' /Fe:network_tests.exe /link /OPT:REF /LIBPATH:"' + str(sdk / "lib")
    + '" zenohc.dll.lib bcrypt.lib ws2_32.lib\nexit /b %errorlevel%\n')
(build / "build.cmd").write_text(command, encoding="utf-8")
result = subprocess.run(["cmd.exe", "/d", "/c", str(build / "build.cmd")], cwd=build,
    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(build / "build.log").write_bytes(result.stdout)
print(result.stdout.decode("utf-8", errors="replace"), flush=True)
if result.returncode:
    raise SystemExit(result.returncode)
import shutil
shutil.copyfile(sdk / "bin/zenohc.dll", build / "zenohc.dll")
scenarios = [("zenoh", 10)] if "--baseline" in sys.argv else [
    (scenario, duration) for scenario in ("zenoh", "ads", "both") for duration in (10, 30)]
for scenario, duration in scenarios:
    result = subprocess.run([str(build / "network_tests.exe"), scenario, str(duration)], cwd=build,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (build / f"{scenario}_{duration}.log").write_bytes(result.stdout)
    print(result.stdout.decode("utf-8", errors="replace"), flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)
