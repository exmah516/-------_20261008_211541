"""确定时钟回放生产 Handle、桥接、滤波、主循环恢复和四轴映射；不连接设备。"""
from pathlib import Path
import os
import subprocess
import sys

repo = Path(__file__).resolve().parents[2]
ads = repo / "B数据端/工程/ADS项目"
baseline = "--baseline" in sys.argv
out = repo / "docs/revision_uhn_20261108/handle" / ("baseline" if baseline else "fixed")
out.mkdir(parents=True, exist_ok=True)


def read(name):
    if baseline:
        return subprocess.check_output(["git", "show", "HEAD:介入机器人/B数据端/工程/ADS项目/" + name],
            cwd=repo).decode("utf-8-sig").replace("\r\n", "\n")
    return (ads / name).read_text(encoding="utf-8-sig")


def between(text, start, end):
    begin = text.index(start)
    return text[begin:text.index(end, begin)]


main = read("main.cpp")
for name in ("Handle.h", "remote_handle_bridge.h", "手柄.cpp"):
    (out / name).write_text(read("手柄/" + name), encoding="utf-8")
(out / "main_rebase.inc").write_text(between(main,
    "\t\telse if (ads_soft_hold_active && planned_return.active())",
    "\t\tconst bool connection_hold_active").replace("else if", "if", 1), encoding="utf-8")
(out / "main_filter.inc").write_text(between(main, "\t\tif (!handle_soft_hold_active)\n\t\t{\n\t\t\taxis1_handle_filter.update",
    "\t\tLARGE_INTEGER handle_record_qpc"), encoding="utf-8")
(out / "main_soft_hold.inc").write_text(between(main, "\t\thandle_soft_hold_active = single_handle_mode",
    "\t\tconst bool handle1_rebase_requested"), encoding="utf-8")
(out / "main_force.inc").write_text(between(main, "\t\tprocess_force_feedback(",
    "\n\t\t// 无论本拍是否进入控制分支"), encoding="utf-8")
increments = between(main, "\t\t\tconst double axis1_linear_filtered =", "\t\t\tconst double axis1_abs =")
for axis in (1, 6):
    increments += between(main, f"\t\t\tconst double axis{axis}_linear_increment_raw_mm =",
        f"\t\t\tconst double axis{axis}_directional_increment_mm =")
increments += "probe[0] = axis1_linear_increment_mm; probe[2] = axis6_linear_increment_mm;\n"
increments += between(main, "\t\t\t\tpos[1] = axis1_crawl.rot_base_rel +",
    "\t\t\t\taxis2_hold_rel = pos[1];")
increments += between(main, "\t\t\t\t\taxis6_crawl.rot_base_rel +", ";") + ";\n"
# 旋转表达式保持原样，仅把返回值接到测试探针。
increments = increments.replace("\t\t\t\t\taxis6_crawl.rot_base_rel +", "probe[3] = axis6_crawl.rot_base_rel +")
increments += "probe[1] = pos[1];\n"
(out / "main_mapping.inc").write_text(increments, encoding="utf-8")
mock = (ads / "tools/ads_diagnostics_test.cpp").read_text(encoding="utf-8-sig")
mock = mock[:mock.index("\nint main()")]
mock = mock.replace('#include "../ads_communication.h"', '#include "ads_communication.h"')
mock = mock.replace('#include "../ads_communication.cpp"', '#include "ads_communication.cpp"')
mock = mock.replace('void copy_positions(const double* source, double* destination, int count) {\n'
    '    std::copy(source, source + count, destination);\n}', '')
(out / "ads_mock.cpp").write_text(mock, encoding="utf-8")
entries = []
for row in (ads / "tools/ads_symbol_sizes.tsv").read_text(encoding="utf-8").splitlines():
    if row and not row.startswith("#"):
        name, size = row.split("\t")
        entries.append('{"' + name + '", ' + str(int(size)) + '},')
(out / "ads_symbol_sizes.inc").write_text("\n".join(entries), encoding="utf-8")
vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
vs = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
includes = [out, ads, ads / "ADS/Include", ads / "手柄"]
sources = [Path(__file__).with_name("handle_tests.cpp"), ads / "motion_sync.cpp",
    ads / "control_types.cpp", ads / "plc_io.cpp", ads / "force_feedback.cpp", out / "ads_mock.cpp"]
command = ('@echo off\nchcp 65001 >nul\ncall "' + vs + '\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
    'if errorlevel 1 exit /b 1\ncl /nologo /std:c++17 /EHsc /utf-8 /DNOMINMAX /Gy /MD '
    + ('/DBASELINE ' if baseline else '')
    + " ".join('/I"' + str(p) + '"' for p in includes) + " "
    + " ".join('"' + str(p) + '"' for p in sources)
    + ' /Fe:handle_tests.exe /link /OPT:REF\nexit /b %errorlevel%\n')
(out / "build.cmd").write_text(command, encoding="utf-8")
result = subprocess.run(["cmd.exe", "/d", "/c", str(out / "build.cmd")], cwd=out,
    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(out / "build.log").write_bytes(result.stdout)
print(result.stdout.decode("utf-8", errors="replace"), flush=True)
if result.returncode:
    raise SystemExit(result.returncode)
result = subprocess.run([str(out / "handle_tests.exe")], cwd=out,
    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(out / "results.log").write_bytes(result.stdout)
print(result.stdout.decode("utf-8", errors="replace"), flush=True)
raise SystemExit(result.returncode)
