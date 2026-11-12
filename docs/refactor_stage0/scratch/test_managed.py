from pathlib import Path
import json,os,shutil,subprocess
root=Path.cwd(); repo=root/'介入机器人'; out=root/'docs/refactor_stage0/logs/build_final'; build=repo/'A主端/工程/build/Stage0Release/bin'
for name in ['VI.Control','VI.Control.Tests','MasterConsole.Protocol.Tests','VI.Service.Robot']:
 shutil.copytree(build/'VI.HandleHost',build/name/'HandleHost',dirs_exist_ok=True)
sdk_script=repo/'tools/reliability_round5/run_host_sdk.py'
s=sdk_script.read_text(encoding='utf-8-sig').replace("repo/'docs/reliability_round5/host_sdk'",'Path('+repr(str(out/'fake_sdk'))+')').split('dotnet=')[0]
exec(compile(s,str(sdk_script),'exec'),{'__file__':str(sdk_script)})
env=dict(os.environ,VI_TEST_FAKE_SDK=str(out/'fake_sdk/FakeFLCatheter.dll'),DOTNET_CLI_UI_LANGUAGE='en')
results={}
for name,args in [('wpf_tests',[build/'VI.Control.Tests/VI.Control.Tests.exe',out/'screenshots']),('protocol_managed_tests',[build/'MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.exe',root/'protocol/fixtures/zenoh-v1'])]:
 with (out/(name+'.log')).open('wb') as log:
  r=subprocess.run(list(map(str,args)),cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT)
 results[name]=r.returncode
 (out/'managed_tests_result.json').write_text(json.dumps(results),encoding='utf-8')
 print(name,r.returncode,flush=True)
