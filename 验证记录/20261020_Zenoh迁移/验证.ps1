[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$taskPackage = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$taskLog = Join-Path $PSScriptRoot ('验证_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '.log')
Start-Transcript -Path $taskLog | Out-Null
try {
    # 仅构建两端和运行内存模拟测试；不启动 ADS.exe 或连接 PLC。
    foreach ($taskSide in @('A主端', 'B数据端')) {
        $taskBuild = Join-Path $taskPackage ("介入机器人\$taskSide\工程\build-and-run.ps1")
        foreach ($taskConfiguration in @('Release', 'Debug')) {
            & powershell -NoProfile -ExecutionPolicy Bypass -File $taskBuild -Configuration $taskConfiguration -BuildOnly | Out-Host
            if ($LASTEXITCODE -ne 0) { throw "$taskSide $taskConfiguration 构建或运行依赖检查失败。" }
        }
    }
    $taskTests = Join-Path $taskPackage '介入机器人\B数据端\工程\ADS项目\tests\run-zenoh-tests.ps1'
    & powershell -NoProfile -ExecutionPolicy Bypass -File $taskTests | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Zenoh 协议、服务或跨语言回环测试失败。' }
    Write-Host '全部构建、依赖与离线验证通过。未运行真实 ADS/PLC。'
}
finally {
    Stop-Transcript | Out-Null
    Write-Host "日志：$taskLog"
}
