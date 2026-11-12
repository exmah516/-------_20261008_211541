"""仅运行既有离线测试；重定向 runner 产物，不改测试源码或断言。"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import datetime, html, json, os, shutil, subprocess, sys

root = Path(__file__).resolve().parents[3]
repo = root / '介入机器人'
out = Path(__file__).resolve().parent / 'tests'
out.mkdir(exist_ok=True)
temp = out / 'temp'
temp.mkdir(exist_ok=True)
env = dict(os.environ, TMP=str(temp), TEMP=str(temp), PYTHONUTF8='1', PYTHONIOENCODING='utf-8', VSLANG='1033', DOTNET_CLI_UI_LANGUAGE='en', DOTNET_CLI_TELEMETRY_OPTOUT='1', DOTNET_CLI_HOME=str(out / 'dotnet_home'))
results = {}

def run(name, args, cwd=root, extra=None):
    start = datetime.datetime.now().isoformat()
    with (out / (name + '.log')).open('wb') as log:
        result = subprocess.run([str(v) for v in args], cwd=cwd, env=env | (extra or {}), stdout=log, stderr=subprocess.STDOUT)
    results[name] = dict(exit=result.returncode, started=start, finished=datetime.datetime.now().isoformat(), command=[str(v) for v in args], cwd=str(cwd))
    print(name, result.returncode, flush=True)
    (out / 'results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
    return result.returncode

def relocated(name, script, replacements, args=(), prefix=None):
    source = script.read_text(encoding='utf-8-sig')
    for old, new in replacements:
        assert old in source, (name, old)
        source = source.replace(old, new)
    if prefix:
        source = source.split(prefix)[0]
    code = 'import sys; sys.path.insert(0,' + repr(str(script.parent)) + '); sys.argv=' + repr([str(script), *args]) + '; exec(compile(' + repr(source) + ',' + repr(str(script)) + ",'exec'), {'__file__':" + repr(str(script)) + ", '__name__':'__main__'})"
    # runner 路径重定向写入独立文件，便于审核实际命令。
    command = out / (name + '_runner.py')
    command.write_text(code, encoding='utf-8')
    return run(name, [sys.executable, command])

ads = repo / 'B数据端/工程/ADS项目'
kin = repo / 'B数据端/工程/PositioningArmKinematics'
master = repo / 'A主端/工程'
vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
vs = Path(subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip())

def native_extra():
    run('net_distance', [sys.executable, ads / 'tools/run_ads_diagnostics_test.py', 'handle_net_distance_test'])
    for kind in ['recovery', 'st']:
        target = out / ('v5_' + kind)
        relocated('v5_' + kind, repo / ('tools/reliability_round5/run_' + kind + '.py'), [("repo / 'docs/reliability_round5/st'" if kind == 'st' else "repo/'docs/reliability_round5/recovery'", 'Path(' + repr(str(target)) + ')')])
    relocated('handle_replay', repo / 'tools/revision_uhn/run_handle.py', [('repo / "docs/revision_uhn_20261108/handle"', 'Path(' + repr(str(out / 'handle_replay')) + ')')])
    relocated('network_replay', repo / 'tools/revision_uhn/run_network.py', [('repo / "docs/revision_uhn_20261108/network"', 'Path(' + repr(str(out / 'network_replay')) + ')')])

def arm():
    relocated('arm', kin / 'tests/run_cart_jog_acceptance.py', [('build = kin / "build-cart-jog"', 'build = Path(' + repr(str(out / 'arm_build')) + ')'), ('logs = repo / "docs/acceptance_cart_jog"', 'logs = Path(' + repr(str(out / 'arm')) + ')')], ['--round4'])

def build_ads():
    run('ads_build', [vs / 'MSBuild/Current/Bin/MSBuild.exe', ads / 'ADS.sln', '/t:Rebuild', '/p:Configuration=Release', '/p:Platform=x64', '/warnaserror', '/p:TreatWarningAsError=true', '/p:OutDir=' + str(out / 'ads/bin') + '/', '/p:IntDir=' + str(out / 'ads/obj') + '/', '/nologo', '/verbosity:minimal', '/clp:Summary'])

def managed():
    dotnet = shutil.which('dotnet')
    run('dotnet_info', [dotnet, '--info'])
    build = out / 'managed/bin'
    props = out / 'isolated.props'
    props.write_text('<Project><Import Project="' + html.escape(str(master / 'Directory.Build.props'), quote=True) + '"/><PropertyGroup><SurveyProjectKey>$([System.IO.Path]::GetFileName($(MSBuildProjectDirectory)))</SurveyProjectKey><OutputPath>' + html.escape(str(build), quote=True) + '/$(SurveyProjectKey)/</OutputPath><BaseIntermediateOutputPath>' + html.escape(str(out / 'managed/obj'), quote=True) + '/$(SurveyProjectKey)/</BaseIntermediateOutputPath><MSBuildProjectExtensionsPath>$(BaseIntermediateOutputPath)</MSBuildProjectExtensionsPath><DefaultItemExcludes>$(DefaultItemExcludes);obj/**;bin/**</DefaultItemExcludes></PropertyGroup></Project>', encoding='utf-8')
    for name, project in [('master_build','MasterConsole.sln'), ('wpf_build','VI.Control.Tests.slnx'), ('protocol_build','MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.csproj'), ('handlehost_build','VI.HandleHost/VI.HandleHost.csproj')]:
        run(name, [dotnet, 'build', master / 'master' / project, '-c', 'Release', '-p:Platform=x64', '-p:DirectoryBuildPropsPath=' + str(props), '-warnaserror', '-m:2', '-v:minimal'])
    if (build / 'VI.HandleHost').exists():
        for name in ['VI.Control', 'VI.Control.Tests', 'MasterConsole.Protocol.Tests', 'VI.Service.Robot']:
            shutil.copytree(build / 'VI.HandleHost', build / name / 'HandleHost', dirs_exist_ok=True)
    if (build / 'VI.Control.Tests/VI.Control.Tests.exe').exists():
        run('wpf_tests', [build / 'VI.Control.Tests/VI.Control.Tests.exe', out / 'screenshots'])
    relocated('fake_sdk_build', repo / 'tools/reliability_round5/run_host_sdk.py', [("repo/'docs/reliability_round5/host_sdk'", 'Path(' + repr(str(out / 'fake_sdk')) + ')')], prefix='dotnet=')
    if (build / 'MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.exe').exists():
        run('protocol_managed_tests', [build / 'MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.exe', root / 'protocol/fixtures/zenoh-v1'], extra={'VI_TEST_FAKE_SDK': str(out / 'fake_sdk/FakeFLCatheter.dll')})

if __name__ == '__main__':
    assert (root / '.git').is_dir() and ads.is_dir()
    selected = sys.argv[1:] or ['native', 'arm', 'ads', 'managed']
    jobs = {'native':native_extra, 'arm':arm, 'ads':build_ads, 'managed':managed}
    with ThreadPoolExecutor(max_workers=4) as pool:
        for job in pool.map(lambda name: jobs[name](), selected):
            pass
    print(json.dumps({k:v['exit'] for k,v in results.items()}, ensure_ascii=False, indent=2), flush=True)
