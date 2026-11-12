from pathlib import Path
import html, json, os, subprocess, tempfile
root=Path.cwd()
repo=root/'介入机器人'
out=root/'docs/refactor_stage0/logs/build_final'
out.mkdir(parents=True,exist_ok=True)
master=repo/'A主端/工程'
build=master/'build/Stage0Release/bin'
props=Path(tempfile.mkdtemp(prefix='stage0-managed-'))/'isolated.props'
props.write_text('<Project><Import Project="'+html.escape(str(master/'Directory.Build.props'),quote=True)+'"/><PropertyGroup><OutputPath>'+html.escape(str(build),quote=True)+'/$(MSBuildProjectName)/</OutputPath></PropertyGroup></Project>',encoding='utf-8')
dotnet=subprocess.check_output(['powershell.exe','-NoProfile','-File',str(master/'tools/Get-ViDotnet.ps1')],text=True).strip()
result={}
env=dict(os.environ,DOTNET_CLI_UI_LANGUAGE='en',PYTHONUTF8='1')
for name,project in [('master','MasterConsole.sln'),('handlehost','VI.HandleHost/VI.HandleHost.csproj'),('wpf_tests_build','VI.Control.Tests.slnx'),('protocol_tests_build','MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.csproj')]:
 args=[dotnet,'build',str(master/'master'/project),'-c','Release','-p:Platform=x64','-p:DirectoryBuildPropsPath='+str(props),'-warnaserror','-m:2','-v:minimal']
 with (out/(name+'.log')).open('wb') as log:
  r=subprocess.run(args,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT)
 result[name]=dict(exit=r.returncode,command=args)
 (out/'managed_result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
 print(name,r.returncode,flush=True)
 if r.returncode: print((out/(name+'.log')).read_text(encoding='utf-8',errors='replace'));raise SystemExit(r.returncode)
