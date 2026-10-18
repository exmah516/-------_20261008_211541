param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$BuildOnly,
    [switch]$Sim
)
$ErrorActionPreference = 'Stop'
try {
    $runtime = Join-Path $PSScriptRoot "master/MasterConsole/bin/$Configuration/net472"
    $exe = Join-Path $runtime 'MasterConsole.exe'
    $token = Join-Path $PSScriptRoot 'config/remote.token'
    if (!$Sim) {
        if (!(Test-Path -LiteralPath $token -PathType Leaf)) { throw 'Missing config/remote.token.' }
        if ([Text.Encoding]::UTF8.GetByteCount([IO.File]::ReadAllText($token).Trim()) -lt 16) {
            throw 'Invalid config/remote.token: at least 16 bytes required.'
        }
    }
    # 正在运行的旧实例必须由用户关闭，避免构建后仍操作旧窗口。
    if (Get-Process -Name MasterConsole -ErrorAction SilentlyContinue) {
        throw 'Close MasterConsole before building.'
    }
    & dotnet build (Join-Path $PSScriptRoot 'master/MasterConsole.sln') --configuration $Configuration --nologo
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE). Nothing was launched." }
    foreach ($file in @('MasterConsole.exe', 'MasterConsole.exe.config') + @(Get-Content -Encoding UTF8 -LiteralPath (Join-Path $PSScriptRoot 'tools/runtime-files.txt'))) {
        if (!(Test-Path -LiteralPath (Join-Path $runtime $file) -PathType Leaf)) {
            throw "Missing runtime file: $file"
        }
    }
    Write-Host "Build verified: $exe"
    if (!$BuildOnly) {
        $arguments = if ($Sim) { '--sim' } else { '--host 192.168.50.2 --token "' + $token + '"' }
        Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $runtime -WindowStyle Normal
    }
    exit 0
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
