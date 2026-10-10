$ErrorActionPreference = 'Stop'
$package = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$runtime = Join-Path $package 'A_主端电脑_地址修正版/工程/master/MasterConsole/bin/Release/net472'
$config = Join-Path $runtime 'config'
if (Test-Path -LiteralPath $config) { throw 'Output config exists; refusing to overwrite.' }
New-Item -ItemType Directory -Path $config | Out-Null
$camera = Join-Path $config 'hik_cameras.ini'
$process = $null
try {
    # 测试时把自动登录摄像头的目标限制在本机，不触碰工程中的现场配置。
    Set-Content -LiteralPath $camera -Encoding UTF8 -Value "[camera1]`nip=127.0.0.1`nport=1`nusername=test`npassword=`n[camera2]`nip=127.0.0.1`nport=1`nusername=test`npassword="
    $process = Start-Process -FilePath (Join-Path $runtime 'MasterConsole.exe') -ArgumentList '--sim' -WorkingDirectory $runtime -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 6
    $process.Refresh()
    if ($process.HasExited -or !$process.Responding -or $process.MainWindowHandle -eq 0) { throw 'Master simulated window failed to start.' }
    Write-Output ('PASS: --sim main window responds; executable = ' + $process.MainModule.FileName)
    if (!$process.CloseMainWindow()) { throw 'Could not close simulation window.' }
    if (!$process.WaitForExit(10000)) { throw 'Simulation did not close within 10 seconds.' }
    if ($process.ExitCode -ne 0) { throw "Simulation exited with code $($process.ExitCode)" }
    Write-Output 'PASS: simulation closes normally; cameras restricted to loopback during test'
}
finally {
    if ($process -and !$process.HasExited) { $process.Kill(); $process.WaitForExit() }
    Remove-Item -LiteralPath $camera -ErrorAction SilentlyContinue
    # 仅移除本测试创建的空配置目录。
    if ((Get-ChildItem -LiteralPath $config -Force).Count -eq 0) { Remove-Item -LiteralPath $config }
}
