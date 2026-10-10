from pathlib import Path
import hashlib
import json
import zipfile

package = Path(__file__).resolve().parents[2]
for name in ('A_主端电脑_地址修正版', 'B_从端电脑'):
    side = package / name
    runtime = side / '运行'
    archive = side / '归档_旧运行_20261010.zip'
    assert runtime.is_dir() and not archive.exists(), 'Unexpected archive state'
    files = sorted(p for p in runtime.rglob('*') if p.is_file())
    manifest = {p.relative_to(runtime).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as saved:
        for path in files:
            saved.write(path, path.relative_to(runtime).as_posix())
    with zipfile.ZipFile(archive) as saved:
        assert saved.testzip() is None, 'Archive CRC check failed'
        assert set(saved.namelist()) == set(manifest), 'Archive file list mismatch'
        for relative, sha in manifest.items():
            assert hashlib.sha256(saved.read(relative)).hexdigest() == sha, relative
    (Path(__file__).resolve().parent / (name + '-archive-manifest.json')).write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'PASS: {name}: {len(files)} original runtime files archived and SHA-256 verified')
