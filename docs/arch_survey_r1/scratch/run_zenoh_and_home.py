"""既有 C++ 协议/服务程序及回环网络/归零链路测试，全部输出到 scratch。"""
from pathlib import Path
import json, shutil
import run_survey_checks as common

root, repo, ads, kin, out = common.root, common.repo, common.ads, common.kin, common.out
build = out / 'zenoh'
build.mkdir(exist_ok=True)
sdk = root / 'zenoh/sdk/zenoh-c-1.10.1-msvc'
msgpack = root / 'zenoh/sdk/msgpack-cxx-9.0.0/include'
source = ads / 'tests/zenoh'

def compile_native(name, sources, extra='', zenoh=False):
    flags = '/nologo /std:c++17 /EHsc /utf-8 /DNOMINMAX /DMSGPACK_NO_BOOST /I"' + str(msgpack) + '" /I"' + str(sdk / 'include') + '" /I"' + str(ads) + '" ' + extra
    link = ' /link /LIBPATH:"' + str(sdk / 'lib') + '" zenohc.dll.lib bcrypt.lib ws2_32.lib' if zenoh else ''
    command = '@echo off\nchcp 65001 >nul\ncall "' + str(common.vs / 'VC/Auxiliary/Build/vcvars64.bat') + '" >nul\nif errorlevel 1 exit /b 1\ncl ' + flags + ' ' + ' '.join('"' + str(p) + '"' for p in sources) + ' /Fe:' + name + '.exe' + link + '\nexit /b %errorlevel%\n'
    cmd = build / (name + '_build.cmd')
    cmd.write_text(command, encoding='utf-8')
    return common.run(name + '_build', ['cmd.exe', '/d', '/c', cmd], cwd=build)

shutil.copy2(sdk / 'bin/zenohc.dll', build / 'zenohc.dll')
if not compile_native('protocol_tests', [source / 'protocol_tests.cpp']):
    common.run('zenoh_cpp_protocol', [build / 'protocol_tests.exe', root / 'protocol/fixtures/zenoh-v1'], cwd=build)
if not compile_native('service_tests', [source / 'service_tests.cpp', ads / 'zenoh_service.cpp'], zenoh=True):
    common.run('zenoh_cpp_service', [build / 'service_tests.exe'], cwd=build)
if not compile_native('simulated_ads', [source / 'simulated_ads.cpp', ads / 'zenoh_service.cpp', ads / '手柄/手柄.cpp'], zenoh=True):
    common.run('zenoh_cross_language', [out / 'managed/bin/MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.exe', root / 'protocol/fixtures/zenoh-v1', '--network', '--simulator', build / 'simulated_ads.exe', '--router', root / 'zenoh/zenohd.exe'], cwd=build)
extra = '/O2 /MD /W3 /bigobj /I"' + str(kin / 'include') + '" /I"' + str(kin / '.deps/Library/include/eigen3') + '" /I"' + str(kin / 'tests') + '" /I"' + str(ads / 'ADS/Include') + '"'
if not compile_native('home_chain_simulator', [repo / 'tools/revision_round7/home_chain_simulator.cpp', ads / 'arm_manual_ads_service.cpp', ads / 'zenoh_service.cpp', kin / 'src/kinematics.cpp', kin / 'src/program_zero.cpp'], extra, True):
    common.run('home_chain', [out / 'managed/bin/VI.Control.Tests/VI.Control.Tests.exe', '--home-chain', build / 'home_chain_simulator.exe', root / 'zenoh/zenohd.exe'], cwd=build)
(out / 'zenoh_home_results.json').write_text(json.dumps(common.results, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({k:v['exit'] for k,v in common.results.items()}, indent=2))
