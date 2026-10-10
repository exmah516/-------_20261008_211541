from pathlib import Path

package = Path(__file__).resolve().parents[2]
master = package / 'A_主端电脑_地址修正版'
slave = package / 'B_从端电脑'


def cmd(root, name, script, args=''):
    text = ('@echo off\nchcp 65001 >nul\n'
            f'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0{script}" {args}\n'
            'set "result=%errorlevel%"\nif not "%result%"=="0" pause\nexit /b %result%\n')
    (root / name).write_bytes(text.replace('\n', '\r\n').encode('utf-8'))


cmd(master, '编译并启动主端.cmd', '工程\\build-and-run.ps1')
cmd(master, '模拟运行主端.cmd', '工程\\build-and-run.ps1', '-Sim')
cmd(slave, '编译并启动从端.cmd', '工程\\build-and-run.ps1')
cmd(slave, '编译并启动本地调试台.cmd', '工程\\build-and-run.ps1', '-LocalConsole')
cmd(master, '验证旧VTK模型运动.cmd', '工程\\tools\\check_vtk_motion.ps1')
cmd(master, '接收从端VTK模型.cmd', '工程\\tools\\check_vtk_motion.ps1',
    '-ListenAddress 192.168.50.1 -PeerAddress 192.168.50.2')
cmd(slave, '发送VTK模型测试.cmd', '工程\\tools\\send_vtk_motion.ps1', '-MasterAddress 192.168.50.1')

for root in (master, slave):
    # Windows PowerShell 5.1 通过 BOM 识别包含中文路径的 UTF-8 脚本。
    for path in [root / '工程/build-and-run.ps1', *list((root / '工程/tools').glob('*.ps1'))]:
        text = path.read_text(encoding='utf-8-sig')
        path.write_bytes(b'\xef\xbb\xbf' + text.replace('\n', '\r\n').encode('utf-8'))
    docs = root / '工程/docs'
    docs.mkdir(exist_ok=True)
    original = root / '运行/VTK分机测试与摄像头连接说明.md'
    text = original.read_text(encoding='utf-8-sig')
    first, rest = text.split('## 第二步', 1)
    first = ('# 从端发送、主端显示：旧 VTK 模型验证\n\n'
             '## 第一步：放好文件\n\n'
             '电脑 A 使用“A_主端电脑_地址修正版”，电脑 B 使用“B_从端电脑”。完整复制各自目录。'
             'A 的测试入口会先编译工程，需安装 .NET SDK 及 .NET Framework 4.7.2 开发包；'
             'B 的发送脚本仅需 Windows PowerShell。测试源码和脚本在“工程/tools”，DLL 和模型由工程自动输出。\n\n')
    text = first + '## 第二步' + rest
    text = text.replace('在 A 的“运行”目录', '在 A 的根目录').replace('在 B 的“运行”目录', '在 B 的根目录')
    text = text.replace('A 的“运行/VTK测试结果”', 'A 的“工程/master/MasterConsole/bin/Release/net472/VTK测试结果”')
    (docs / original.name).write_text(text, encoding='utf-8')

print('Created launchers, migrated VTK documentation, normalized script encodings.')
