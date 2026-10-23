param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$dotnet = & (Join-Path $PSScriptRoot 'Get-ViDotnet.ps1')
if (!$OutputDirectory) { $OutputDirectory = Join-Path $project "build/verification/$Configuration/ui" }
& $dotnet build (Join-Path $project 'master/VI.Control.Tests/VI.Control.Tests.csproj') --configuration $Configuration -p:Platform=x64 -m:1 --nologo
if ($LASTEXITCODE -ne 0) { throw 'Offline UI build failed.' }
& $dotnet (Join-Path $project "build/$Configuration/bin/VI.Control.Tests/VI.Control.Tests.dll") $OutputDirectory
if ($LASTEXITCODE -ne 0) { throw 'Offline UI verification failed.' }
