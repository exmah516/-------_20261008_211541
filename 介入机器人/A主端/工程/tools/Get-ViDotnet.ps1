$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$env:DOTNET_CLI_HOME = Join-Path $project '.dotnet-home'
$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
$localDotnet = Join-Path $project '.tools/dotnet-sdk-9.0.315/dotnet.exe'
if (Test-Path -LiteralPath $localDotnet -PathType Leaf) {
    $env:DOTNET_ROOT = Split-Path $localDotnet -Parent
    $env:DOTNET_ROOT_X64 = $env:DOTNET_ROOT
    $localDotnet
    return
}
$installed = Get-Command dotnet -ErrorAction SilentlyContinue
if ($installed) {
    Push-Location $project
    try {
        $version = & $installed.Source --version 2>$null
        if ($LASTEXITCODE -eq 0 -and $version -match '^9\.0\.3\d\d$') {
            $env:DOTNET_ROOT = Split-Path $installed.Source -Parent
            $env:DOTNET_ROOT_X64 = $env:DOTNET_ROOT
            $installed.Source
            return
        }
    }
    finally { Pop-Location }
}
throw '需要 .NET SDK 9.0.315（或同功能带的更新补丁）。请先运行 tools/Prepare-Sdk.ps1。'
