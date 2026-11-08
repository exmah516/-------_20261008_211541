"""仅枚举系统设备与检查SDK文件；不加载SDK、不打开设备。"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import uuid

repo=Path(__file__).resolve().parents[2]
out=repo/"docs/reliability_round5"
dll=repo/"A主端/工程/FLCatheter.dll"
data=dll.read_bytes()
interface=uuid.UUID("ae18aa60-7f6a-11d4-97dd-00010229b959")
offset=data.index(interface.bytes_le)
vswhere=Path(os.environ["ProgramFiles(x86)"])/"Microsoft Visual Studio/Installer/vswhere.exe"
vs=Path(subprocess.check_output([str(vswhere),"-latest","-products","*","-requires","Microsoft.VisualStudio.Component.VC.Tools.x86.x64","-property","installationPath"],text=True).strip())
dumpbin=sorted(vs.glob("VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"))[-1]
exports=subprocess.check_output([str(dumpbin),"/exports",str(dll)])
(out/"sdk_exports.txt").write_bytes(exports)
command="$devices=@(Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match 'Cypress|Catheter|FlCatheter' -or $_.PNPDeviceID -match 'VID_04B4' } | Select-Object Name,PNPDeviceID,ClassGuid); ConvertTo-Json -InputObject $devices"
enumeration=subprocess.check_output(["powershell.exe","-NoProfile","-Command",command])
(out/"sdk_devices.json").write_bytes(enumeration)
summary={"sdk":str(dll.relative_to(repo)),"sha256":hashlib.sha256(data).hexdigest(),
    "device_interface_class":str(interface),"interface_guid_file_offset":hex(offset),
    "device_status_export_rva":"0x6150","serial_number_export_rva":"0x6120",
    "present_matching_devices":json.loads(enumeration.decode("utf-8-sig")),
    "serial_to_windows_interface_mapping":"未证实；不得将类级移除通知映射为单柄移除",
    "vid_pid":"本机无匹配设备；未核实"}
(out/"sdk_probe.json").write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding="utf-8")
print(json.dumps(summary,ensure_ascii=False,indent=2))
