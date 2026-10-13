"""离线编译真实记录器，验证移除相机后两次会话的 CSV 与 JSON。"""
from pathlib import Path
import csv
import json
import os
import subprocess
import tempfile

package = Path(__file__).resolve().parents[2]
project = package / "B_从端电脑/工程/ADS项目"
build = Path(tempfile.mkdtemp(prefix="recorder-without-camera-"))
source = build / "recorder_check.cpp"
source.write_text(r'''
#include "experiment_recorder.h"
#include <filesystem>
#include <iostream>
#include <cstdlib>
static void check(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << "\n"; std::exit(1); }
    std::cout << "PASS: " << message << "\n";
}
int main() {
    ExperimentRecorder recorder;
    AdsCommunicationStats stats{};
    stats.state = AdsConnectionState::Running;
    stats.actual_hz = 100;
    recorder.update_ads_communication_stats(stats);
    for (int session = 0; session < 2; ++session) {
        check(recorder.start("offline_recording", {}), "recording starts without camera");
        check(!recorder.start("duplicate_start", {}), "active recording rejects duplicate start");
        for (const auto& entry : std::filesystem::recursive_directory_iterator("records")) {
            if (entry.path().filename() == "session.json") {
                const auto copy = entry.path().parent_path() / "startup.json";
                if (!std::filesystem::exists(copy)) std::filesystem::copy_file(entry.path(), copy);
            }
        }
        ForceCsvRow force{};
        force.ads_snapshot_sequence = 1;
        force.source_qpc_ticks = recorder.clock().now_qpc();
        force.sample_valid = true;
        force.fn_1_raw_v = 1.25;
        force.ft_1_raw_v = 2.5;
        force.fn_2_raw_v = 3.25;
        force.ft_2_raw_v = 4.5;
        check(recorder.enqueue_force(force), "valid force sample accepted");
        check(!recorder.enqueue_force(force), "duplicate ADS sample rejected");
        force.ads_snapshot_sequence = 2;
        force.source_qpc_ticks = recorder.clock().now_qpc();
        force.sample_valid = false;
        check(recorder.enqueue_force(force), "invalid force sample retains time slot");
        MotionCsvRow motion{};
        motion.ads_snapshot_sequence = 2;
        motion.source_qpc_ticks = recorder.clock().now_qpc();
        motion.position_valid = true;
        motion.axis_from_left_mm[0] = 12.5;
        check(recorder.enqueue_motion(motion), "motion sample accepted");
        recorder.poll_health();
        recorder.stop_and_wait("offline_stop");
        check(recorder.snapshot().state == ExperimentRecordingState::Idle &&
              recorder.snapshot().error == ExperimentRecordingError::None,
              "stop flushes recording and returns idle");
    }
}
''', encoding="utf-8")
vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
vs = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
cmd = build / "build.cmd"
cmd.write_text('@echo off\nchcp 65001 >nul\n'
    f'call "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
    'if errorlevel 1 exit /b 1\n'
    f'cl /nologo /std:c++17 /EHsc /utf-8 /MD /I"{project}" /I"{project / "ADS/Include"}" '
    f'/I"{project / "手柄"}" /I"{project / "手柄/include"}" '
    f'"{source}" "{project / "experiment_recorder.cpp"}" /Fe:recorder_check.exe\n'
    'if errorlevel 1 exit /b 1\nrecorder_check.exe\nexit /b %errorlevel%\n', encoding="utf-8")
subprocess.run(["cmd.exe", "/d", "/c", str(cmd)], cwd=build, check=True, timeout=60)
sessions = list((build / "records").rglob("session.json"))
assert len(sessions) == 2
for session in sessions:
    initial = json.loads(session.with_name("startup.json").read_text(encoding="utf-8"))
    final = json.loads(session.read_text(encoding="utf-8"))
    assert initial["utc_end"] is None and initial["stop_reason"] is None
    assert final["utc_end"] and final["stop_reason"] == "offline_stop"
    assert final["schema_version"] == 4 and "camera" not in final
    counts = final["ads"]["recorder"]
    assert counts["force_accepted"] == 2 and counts["force_invalid"] == 1
    assert counts["force_duplicate_rejected"] == 1
    with session.with_name("force.csv").open(newline="", encoding="utf-8-sig") as f:
        force = list(csv.DictReader(f))
    with session.with_name("motion.csv").open(newline="", encoding="utf-8-sig") as f:
        motion = list(csv.DictReader(f))
    assert len(force) == 2 and len(motion) == 1
    assert force[0]["sample_valid"] == "1" and float(force[0]["fn_1_raw_v"]) == 1.25
    assert force[1]["sample_valid"] == "0" and force[1]["fn_1_raw_v"] == "NaN"
    assert float(motion[0]["axis1_from_left_mm"]) == 12.5
    assert not session.with_name("video.mp4").exists()
    assert not session.with_name("video_frames.csv").exists()
    print("PASS: startup/final JSON, ADS counts, valid/invalid force CSV, motion CSV, no video files")
print("Offline recorder artifacts:", build)

