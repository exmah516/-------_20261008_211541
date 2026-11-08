from pathlib import Path
import os
import shutil
import subprocess

repo=Path(__file__).resolve().parents[2]
out=repo/'docs/reliability_round5/host_sdk'
out.mkdir(parents=True,exist_ok=True)
vswhere=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
vs=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
source=Path(__file__).with_name('fake_handle_sdk.cpp')
cmd=out/'build.cmd'
cmd.write_text('@echo off\nchcp 65001 >nul\ncall "'+vs+'\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
    'cl /nologo /std:c++17 /EHsc /utf-8 /W4 /LD "'+str(source)+'" /Fe:FakeFLCatheter.dll\nif errorlevel 1 exit /b 1\n'
    'cl /nologo /std:c++17 /EHsc /utf-8 /W4 /LD /DVI_NO_JOINTS "'+str(source)+'" /Fe:MissingEntry.dll\nexit /b %errorlevel%\n',encoding='utf-8')
r=subprocess.run(['cmd.exe','/d','/c',str(cmd)],cwd=out,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'build.log').write_bytes(r.stdout)
print(r.stdout.decode('utf-8',errors='replace'))
if r.returncode: raise SystemExit(r.returncode)
dotnet=subprocess.check_output(['powershell.exe','-NoProfile','-File',str(repo/'A主端/工程/tools/Get-ViDotnet.ps1')],text=True).strip()
project=repo/'A主端/工程/master/MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.csproj'
r=subprocess.run([dotnet,'build',str(project),'-c','Release','-p:Platform=x64','-warnaserror','-v:minimal'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'managed_build.log').write_bytes(r.stdout)
print(r.stdout.decode('utf-8',errors='replace'))
if r.returncode: raise SystemExit(r.returncode)
env=dict(os.environ,VI_TEST_FAKE_SDK=str(out/'FakeFLCatheter.dll'))
test=repo/'A主端/工程/build/Release/bin/MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.exe'
r=subprocess.run([str(test)],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'results.log').write_bytes(r.stdout)
print(r.stdout.decode('utf-8',errors='replace'))
raise SystemExit(r.returncode)
