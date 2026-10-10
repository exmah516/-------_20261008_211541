from pathlib import Path
import re
import zipfile

package = Path(__file__).resolve().parents[2]
sides = [package / 'A_主端电脑_地址修正版', package / 'B_从端电脑']
tokens = [(side / '工程/config/remote.token').read_bytes() for side in sides]
assert tokens[0] == tokens[1], 'Master/slave tokens differ'

for side in sides:
    project = side / '工程'
    source = side / '运行'
    archive = side / '归档_旧运行_20261010.zip'
    def original(relative):
        if source.exists():
            return (source / relative).read_bytes()
        with zipfile.ZipFile(archive) as saved:
            return saved.read(relative)
    assert (project / 'config/remote.token').read_bytes() == original('remote.token')
    if side == sides[0]:
        for p in (project / 'config').glob('*.ini*'):
            assert p.read_bytes() == original('config/' + p.name), p
    files = (project / 'tools/runtime-files.txt').read_text(encoding='utf-8-sig').splitlines()
    for mode in ['Debug', 'Release']:
        output = (project / f'master/MasterConsole/bin/{mode}/net472' if side == sides[0]
                  else project / f'ADS项目/x64/{mode}')
        for relative in files:
            assert (output / relative).is_file(), (mode, relative)
        # 静态资源必须与迁移前一致，托管程序本体允许重新编译。
        for relative in files:
            if relative != 'MasterConsole.Protocol.dll':
                assert (output / relative).read_bytes() == original(relative), (mode, relative)
        assert not (output / 'remote.token').exists(), 'Stale token in output'
        assert not (output / 'config/remote.token').exists(), 'Stale token in output/config'
        assert not (output / 'config/hik_cameras.ini').exists(), 'Stale camera settings'
    print(f'PASS: {side.name}: config preserved; Debug/Release runtime resources complete and identical')

preset_path = Path('B_从端电脑/工程/ADS项目/control_types.h')
before = (Path(__file__).resolve().parent / '原文件' / preset_path).read_bytes()
after = (package / preset_path).read_bytes()
expected = before.replace(b'cyl2_clamp = 600;', b'cyl2_clamp = 1000;').replace(b'cyl4_clamp = 500;', b'cyl4_clamp = 1000;')
assert after == expected, 'Unexpected changes to cylinder parameters'
assert re.search(rb'cyl2_open = 5;', after) and re.search(rb'cyl4_open = 5;', after)
print('PASS: only two clamp presets changed; open values and all other parameters unchanged')
