"""将用户指定当前源码逐字节打包；MAIN ST保存原XML CDATA字节，不重排换行。"""
from pathlib import Path
import hashlib
import json
import re
import zipfile

OUT = Path(__file__).resolve().parent.parent
ROOT = OUT.parents[1]
D = ROOT / '介入机器人/B数据端/工程/ADS项目'
U = ROOT / '介入机器人/C从端/250902/Untitled2'
files = [D/name for name in (
    'main.cpp control_types.h control_types.cpp control_messages.h motion_sync.h motion_sync.cpp '
    'return_recovery.h delivery_tracking.h delivery_tracking.cpp guidewire_mode.h guidewire_mode.cpp '
    'startup_sequence.h startup_sequence.cpp handle_net_distance.h plc_io.h plc_io.cpp '
    'ads_communication.h ads_communication.cpp zenoh_service.h zenoh_service.cpp tools/run_offline_tests.cjs').split()]
files += sorted(p for p in (D/'手柄').rglob('*') if p.is_file())
files += [U/'POUs'/name for name in (
    'handle.TcPOU MAIN.TcPOU err.TcPOU clear_err.TcPOU init.TcPOU reset.TcPOU '
    'SelfCheck.TcPOU Axis2Sync.TcPOU InjectorManual.TcPOU').split()]
files += [U/'GVLs/G.TcGVL']
files += sorted(p for p in (U/'DUTs').rglob('*') if p.is_file())
manifest = []
with zipfile.ZipFile(OUT/'sources_r2.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    for p in files:
        content = p.read_bytes()
        name = p.relative_to(ROOT).as_posix()
        z.writestr(name,content)
        manifest.append(dict(archive_path=name,source=name,bytes=len(content),sha256=hashlib.sha256(content).hexdigest(),kind='original'))
    main = U/'POUs/MAIN.TcPOU'
    source = main.read_bytes()
    match = re.search(rb'<ST><!\[CDATA\[([\s\S]*?)\]\]></ST>',source)
    assert match
    content = match[1]
    name = '介入机器人/C从端/250902/Untitled2/POUs/MAIN.ST'
    z.writestr(name,content)
    manifest.append(dict(archive_path=name,source=main.relative_to(ROOT).as_posix(),
        bytes=len(content),sha256=hashlib.sha256(content).hexdigest(),kind='exact ST CDATA bytes',
        source_byte_start=match.start(1),source_byte_end_exclusive=match.end(1),
        source_start_line=source[:match.start(1)].count(b'\n')+1,source_end_line=source[:match.end(1)].count(b'\n')+1))
    data = dict(files=manifest,original_count=len(files),
        note='所有original条目保持磁盘原始字节；MAIN.TcPOU原文件随包附带，MAIN.ST为Implementation/ST的CDATA逐字节内容，未转换编码或换行。')
    z.writestr('MANIFEST.json',json.dumps(data,ensure_ascii=False,indent=2).encode('utf-8'))
(OUT/'scratch/source_manifest.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
with zipfile.ZipFile(OUT/'sources_r2.zip') as z:
    assert z.testzip() is None
    for item in manifest:
        assert hashlib.sha256(z.read(item['archive_path'])).hexdigest()==item['sha256']
        if item['kind']=='original':
            assert z.read(item['archive_path'])==(ROOT/item['source']).read_bytes()
print(json.dumps(dict(original_count=len(files),st_extracts=1,archive_entries=len(manifest)+1,
    bytes=(OUT/'sources_r2.zip').stat().st_size,sha256=hashlib.sha256((OUT/'sources_r2.zip').read_bytes()).hexdigest()),indent=2))
