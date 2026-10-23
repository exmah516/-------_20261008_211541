param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$BuildOnly,
    [switch]$Sim
)
$ErrorActionPreference = 'Stop'
try {
    $dotnet = & (Join-Path $PSScriptRoot 'tools/Get-ViDotnet.ps1')
    $runtime = Join-Path $PSScriptRoot "build/$Configuration/bin/VI.Control"
    $exe = Join-Path $runtime 'VI.Control.exe'
    $token = Join-Path $PSScriptRoot 'config/remote.token'
    if (!$Sim -and !$BuildOnly) {
        if (!(Test-Path -LiteralPath $token -PathType Leaf)) { throw 'Missing config/remote.token.' }
        if ([Text.Encoding]::UTF8.GetByteCount([IO.File]::ReadAllText($token).Trim()) -lt 16) {
            throw 'Invalid config/remote.token: at least 16 bytes required.'
        }
    }
    # 正在运行的旧实例必须由用户关闭，避免构建后仍操作旧窗口。
    if (Get-Process -Name VI.Control, MasterConsole -ErrorAction SilentlyContinue) {
        throw 'Close VI.Control / MasterConsole before building.'
    }
    & $dotnet build (Join-Path $PSScriptRoot 'master/VI.Control.slnx') --configuration $Configuration -p:Platform=x64 -m:1 --nologo
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE). Nothing was launched." }
    foreach ($file in Get-Content -Encoding UTF8 -LiteralPath (Join-Path $PSScriptRoot 'tools/runtime-files.txt')) {
        if (!(Test-Path -LiteralPath (Join-Path $runtime $file) -PathType Leaf)) {
            throw "Missing runtime file: $file"
        }
    }
    Write-Host "Build verified: $exe"
    if (!$BuildOnly) {
        $arguments = if ($Sim) { '--sim' } else { '--host 192.168.60.1 --token "' + $token + '"' }
        Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $runtime -WindowStyle Normal
    }
    exit 0
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
