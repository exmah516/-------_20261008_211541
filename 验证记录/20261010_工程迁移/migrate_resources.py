from pathlib import Path
import hashlib
import shutil

PACKAGE = Path(__file__).resolve().parents[2]
SIDES = [PACKAGE / 'A_主端电脑_地址修正版', PACKAGE / 'B_从端电脑']
BACKUP = Path(__file__).resolve().parent / '原文件'
CRT = ['concrt140.dll', 'msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll',
       'msvcp140_atomic_wait.dll', 'msvcp140_codecvt_ids.dll', 'vccorlib140.dll',
       'vcruntime140.dll', 'vcruntime140_1.dll']


def replace_once(path, old, new):
    original = path.read_bytes()
    before, after = old.encode('utf-8'), new.encode('utf-8')
    assert original.count(before) == 1, (path, old)
    backup = BACKUP / path.relative_to(PACKAGE)
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        backup.write_bytes(original)
    path.write_bytes(original.replace(before, after))


tokens = [(side / '运行/remote.token').read_bytes() for side in SIDES]
assert tokens[0] == tokens[1], '主从密钥不一致，停止迁移'
assert len(tokens[0].decode('utf-8-sig').strip().encode('utf-8')) >= 16
for side in SIDES:
    project, runtime = side / '工程', side / '运行'
    config, crt, tools = project / 'config', project / 'Runtime/VC142', project / 'tools'
    for directory in (config, crt, tools):
        directory.mkdir(parents=True, exist_ok=True)
    shutil.copy2(runtime / 'remote.token', config / 'remote.token')
    if (runtime / 'config').exists():
        shutil.copytree(runtime / 'config', config, dirs_exist_ok=True)
    for name in CRT:
        shutil.copy2(runtime / name, crt / name)
    master = side == SIDES[0]
    names = ['check_vtk_motion.ps1', 'VtkMotionCheck.cs'] if master else ['send_vtk_motion.ps1']
    for name in names:
        shutil.copy2(runtime / name, tools / name)
    # 清单只记录运行依赖及模型，不包含密钥、旧程序或历史测试结果。
    required = sorted(p.relative_to(runtime).as_posix() for p in runtime.rglob('*')
                      if p.is_file() and (p.parent == runtime and p.suffix in ('.dll', '.manifest')
                                         or p.relative_to(runtime).parts[0] in ('HCNetSDKCom', 'URDF')))
    (tools / 'runtime-files.txt').write_text('\n'.join(required) + '\n', encoding='utf-8-sig')
    (project / '.gitignore').write_text('**/bin/\n**/obj/\n**/.vs/\nADS项目/x64/\nconfig/remote.token\nconfig/hik_cameras.ini\n', encoding='utf-8')
    for p in (runtime / 'config').glob('*'):
        if p.is_file():
            assert hashlib.sha256(p.read_bytes()).digest() == hashlib.sha256((config / p.name).read_bytes()).digest()
    assert (config / 'remote.token').read_bytes() == (runtime / 'remote.token').read_bytes()

master = SIDES[0] / '工程/master/MasterConsole/MasterConsole.csproj'
replace_once(master, '    <Reference Include="System.Windows.Forms" />',
             '    <!-- 两种构建配置均携带同一套 VC++ 运行库。 -->\r\n'
             '    <None Include="..\\..\\Runtime\\VC142\\*.dll" Link="%(Filename)%(Extension)" CopyToOutputDirectory="PreserveNewest" />\r\n'
             '    <Reference Include="System.Windows.Forms" />')
slave = SIDES[1] / '工程/ADS项目/ADS.vcxproj'
replace_once(slave, '    <Copy SourceFiles="$(ProjectDir)FLCatheter.dll;',
             '    <ItemGroup>\r\n'
             '      <LocalVCRuntime Include="$(ProjectDir)..\\Runtime\\VC142\\*.dll" />\r\n'
             '    </ItemGroup>\r\n'
             '    <Copy SourceFiles="@(LocalVCRuntime)" DestinationFolder="$(OutDir)" SkipUnchangedFiles="true" />\r\n'
             '    <Copy SourceFiles="$(ProjectDir)FLCatheter.dll;')
ui = SIDES[1] / '工程/ADS项目/AdsControlUI/AdsControlUI.csproj'
replace_once(ui, '    <PackageReference Include="ScottPlot.WPF" Version="4.1.74" />',
             '    <PackageReference Include="ScottPlot.WPF" Version="4.1.74" />\r\n'
             '    <None Include="..\\..\\Runtime\\VC142\\*.dll" Link="%(Filename)%(Extension)" CopyToOutputDirectory="PreserveNewest" />')
preset = SIDES[1] / '工程/ADS项目/control_types.h'
replace_once(preset, 'unsigned short cyl2_clamp = 600;', 'unsigned short cyl2_clamp = 1000;')
replace_once(preset, 'unsigned short cyl4_clamp = 500;', 'unsigned short cyl4_clamp = 1000;')
print('资源迁移完成；主从密钥一致，配置字节一致；仅修改两个夹紧预设。')
