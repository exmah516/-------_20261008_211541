from pathlib import Path
import json,os,subprocess
root=Path.cwd()
out=root/'docs/refactor_stage0/logs/build_final'
out.mkdir(parents=True,exist_ok=True)
vswhere=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
vs=Path(subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip())
args=[str(vs/'MSBuild/Current/Bin/MSBuild.exe'),str(root/'介入机器人/B数据端/工程/ADS项目/ADS.vcxproj'),'/t:Rebuild','/p:Configuration=Release','/p:Platform=x64','/p:OutDir='+str(out/'native')+'\\','/p:IntDir='+str(out/'native_obj')+'\\','/m:2','/v:minimal','/nologo']
with (out/'data_build.log').open('wb') as log:
 r=subprocess.run(args,cwd=root,stdout=log,stderr=subprocess.STDOUT)
(out/'data_result.json').write_text(json.dumps(dict(exit=r.returncode,command=args),ensure_ascii=False,indent=2),encoding='utf-8')
print((out/'data_build.log').read_text(encoding='utf-8',errors='replace'))
raise SystemExit(r.returncode)
