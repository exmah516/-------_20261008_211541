from pathlib import Path
import os
import subprocess

repo=Path(__file__).resolve().parents[2]
out=repo.parent/'docs/refactor_stage1/logs/recovery_regression'
out.mkdir(parents=True,exist_ok=True)
vswhere=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
vs=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
script=out/'build.cmd'
script.write_text('@echo off\nchcp 65001 >nul\ncall "'+vs+'\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'
    'cl /nologo /std:c++17 /EHsc /utf-8 /W4 /DNOMINMAX /I"'+str(repo/'B数据端/工程/ADS项目')+'" "'+str(Path(__file__).with_name('recovery_tests.cpp'))+'" /Fe:recovery_tests.exe\n'
    'if errorlevel 1 exit /b 1\nrecovery_tests.exe\nexit /b %errorlevel%\n',encoding='utf-8')
r=subprocess.run(['cmd.exe','/d','/c',str(script)],cwd=out,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'results.log').write_bytes(r.stdout)
print(r.stdout.decode('utf-8',errors='replace'))
raise SystemExit(r.returncode)
