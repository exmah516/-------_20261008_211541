$ErrorActionPreference = 'Stop'
$package = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$fixture = Join-Path $env:TEMP ('robot-launch-check-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$global:robotLauncherCheck = @()
function Start-Process {
    param($FilePath, $ArgumentList, $WorkingDirectory, $WindowStyle)
    $global:robotLauncherCheck += [pscustomobject]@{ File = $FilePath; Arguments = $ArgumentList; Directory = $WorkingDirectory }
}
foreach ($side in @('A_主端电脑_地址修正版', 'B_从端电脑')) {
    $isMaster = $side.StartsWith('A_')
    $project = Join-Path $package "$side/工程"
    $copy = Join-Path $fixture $side
    New-Item -ItemType Directory -Path (Join-Path $copy 'config') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $project 'build-and-run.ps1') -Destination $copy
    $scriptPath = Join-Path $copy 'build-and-run.ps1'
    $token = Join-Path $copy 'config/remote.token'
    # 保留假旧程序，故意使编译失败，验证脚本不能落到启动分支。
    $oldExe = if ($isMaster) { 'master/MasterConsole/bin/Release/net472/MasterConsole.exe' } else { 'ADS项目/x64/Release/ADS.exe' }
    $oldPath = Join-Path $copy $oldExe
    New-Item -ItemType Directory -Path (Split-Path $oldPath -Parent) -Force | Out-Null
    Set-Content -LiteralPath $oldPath -Value 'old executable fixture'
    foreach ($mode in @('missing-token', 'short-token', 'build-failure')) {
        if ($mode -eq 'short-token') { Set-Content -LiteralPath $token -Value 'short' }
        if ($mode -eq 'build-failure') { Set-Content -LiteralPath $token -Value 'test-only-token-0000000000' }
        & $scriptPath
        if ($LASTEXITCODE -eq 0 -or $global:robotLauncherCheck.Count -ne 0) { throw "FAIL: $side $mode" }
        Write-Output "PASS: $side $mode prevented launch (old EXE present)"
    }
    # 真正执行增量构建，仅拦截进程启动，确认最终路径、参数和工作目录。
    & (Join-Path $project 'build-and-run.ps1')
    if ($LASTEXITCODE -ne 0 -or $global:robotLauncherCheck.Count -ne 1) { throw "FAIL: $side successful launch route" }
    $launch = $global:robotLauncherCheck[0]
    $expected = Join-Path $project $oldExe
    if ([IO.Path]::GetFullPath($launch.File) -ne [IO.Path]::GetFullPath($expected)) { throw "Wrong EXE: $($launch.File)" }
    if ([IO.Path]::GetFullPath($launch.Directory) -ne (Split-Path ([IO.Path]::GetFullPath($expected)) -Parent)) { throw 'Wrong working directory' }
    if ($isMaster -and ($launch.Arguments -notmatch '192\.168\.50\.2' -or $launch.Arguments -notmatch 'config[\\/]remote\.token')) { throw 'Wrong master arguments' }
    Write-Output "PASS: $side successful build routes to current engineering output"
    $global:robotLauncherCheck = @()
}
& (Join-Path $package 'B_从端电脑/工程/build-and-run.ps1') -LocalConsole
if ($LASTEXITCODE -ne 0 -or $global:robotLauncherCheck.Count -ne 1 -or $global:robotLauncherCheck[0].File -notlike '*AdsControlUI*bin*Release*AdsControlUI.exe') {
    throw 'FAIL: local console launch route'
}
Write-Output 'PASS: local console launch route; no real processes launched by this test'
Write-Output "Fixtures retained for inspection: $fixture"
