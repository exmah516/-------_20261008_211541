[CmdletBinding()]
param([string]$CacheDirectory = '')

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$project = Split-Path $PSScriptRoot -Parent
$dotnet = & (Join-Path $PSScriptRoot 'Get-ViDotnet.ps1')
$solution = Join-Path $project 'master/VI.Control.Tests.slnx'
$feed = [IO.Path]::GetFullPath((Join-Path $project 'Packages'))
if (!$CacheDirectory) { $CacheDirectory = Join-Path $project ('.nuget-clean/' + [Guid]::NewGuid().ToString('N')) }
$CacheDirectory = [IO.Path]::GetFullPath($CacheDirectory)
if ((Test-Path -LiteralPath $CacheDirectory) -and (Get-ChildItem -LiteralPath $CacheDirectory -Force | Select-Object -First 1)) {
    throw '隔离缓存必须为空，确保包源映射实际参与还原。'
}
New-Item -ItemType Directory -Force -Path $CacheDirectory | Out-Null

function Assert-Check([bool]$Passed, [string]$Message) {
    if (!$Passed) { throw $Message }
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
foreach ($name in @('Robotics.HostFramework', 'Robotics.HostFramework.Contracts')) {
    $archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $feed "$name.0.1.0-dev.nupkg"))
    try {
        $libraries = @($archive.Entries | Where-Object { $_.FullName -match '^lib/net8\.0-windows[^/]*/[^/]+\.dll$' })
        Assert-Check ($libraries.Count -eq 1 -and $libraries[0].Name -eq "$name.dll") "$name 包必须且只能包含自身的 net8.0-windows DLL。"
        foreach ($entry in $archive.Entries) {
            Assert-Check ($entry.FullName -match '^(lib/net8\.0-windows[^/]*/[^/]+\.dll|[^/]+\.nuspec|_rels/\.rels|\[Content_Types\]\.xml|package/services/metadata/core-properties/[^/]+\.psmdcp)$') "不允许的包内容：$($entry.FullName)"
        }
        $manifest = $archive.Entries | Where-Object { $_.Name -eq "$name.nuspec" }
        Assert-Check ($null -ne $manifest) "$name 缺少 NuGet 元数据。"
        $reader = [IO.StreamReader]::new($manifest.Open())
        try { $manifestText = $reader.ReadToEnd() } finally { $reader.Dispose() }
        [xml]$nuspec = $manifestText
        Assert-Check ($nuspec.package.metadata.version -eq '0.1.0-dev') "$name 包版本错误。"
        Assert-Check ($manifestText -notmatch '<repository\b|[A-Za-z]:\\|github\.com|gitlab\.') "$name 不得包含私有仓库或本机路径。"
        $dependencies = @($nuspec.package.metadata.dependencies.group.dependency)
        foreach ($dependency in $dependencies | Where-Object { $_.id -like 'Microsoft.Extensions.*' }) {
            Assert-Check ($dependency.version -eq '8.0.1') "$name 的 $($dependency.id) 版本必须为 8.0.1。"
        }
    }
    finally { $archive.Dispose() }
}

& $dotnet restore $solution -p:Platform=x64 -m:1 --configfile (Join-Path $project 'NuGet.Config') --packages $CacheDirectory --nologo
if ($LASTEXITCODE -ne 0) { throw '隔离缓存还原失败。' }
foreach ($name in @('robotics.hostframework', 'robotics.hostframework.contracts')) {
    $metadata = Get-Content -Encoding UTF8 -Raw -LiteralPath (Join-Path $CacheDirectory "$name/0.1.0-dev/.nupkg.metadata") | ConvertFrom-Json
    Assert-Check ([IO.Path]::GetFullPath($metadata.source).TrimEnd('\', '/') -eq $feed.TrimEnd('\', '/')) "$name 未从工程 Packages 源还原。"
}

function Assert-X64([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    Assert-Check ($bytes.Length -gt 64) "无效 PE 文件：$Path"
    $peOffset = [BitConverter]::ToInt32($bytes, 60)
    Assert-Check ($peOffset -ge 0 -and $peOffset + 6 -lt $bytes.Length) "无效 PE 头：$Path"
    Assert-Check ([BitConverter]::ToUInt32($bytes, $peOffset) -eq 0x4550 -and [BitConverter]::ToUInt16($bytes, $peOffset + 4) -eq 0x8664) "运行库不是 x64：$Path"
}

$packageRoot = [IO.Path]::GetFullPath((Join-Path $project '../../..'))
$fixtures = Join-Path $packageRoot 'protocol/fixtures/zenoh-v1'
foreach ($configuration in @('Debug', 'Release')) {
    & $dotnet build $solution -c $configuration -p:Platform=x64 -m:1 "-p:RestorePackagesPath=$CacheDirectory" --no-restore --nologo
    if ($LASTEXITCODE -ne 0) { throw "$configuration x64 构建失败。" }
    $bin = Join-Path $project "build/$configuration/bin"
    $runtime = Join-Path $bin 'VI.Control'
    foreach ($file in Get-Content -Encoding UTF8 -LiteralPath (Join-Path $PSScriptRoot 'runtime-files.txt')) {
        Assert-Check (Test-Path -LiteralPath (Join-Path $runtime $file) -PathType Leaf) "$configuration 缺少运行文件：$file"
    }
    foreach ($file in @('FLCatheter.dll', 'zenohc.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
        Assert-X64 (Join-Path $runtime $file)
    }
    $runtimeConfig = Get-Content -Encoding UTF8 -Raw -LiteralPath (Join-Path $runtime 'VI.Control.runtimeconfig.json') | ConvertFrom-Json
    Assert-Check ($runtimeConfig.runtimeOptions.tfm -eq 'net8.0') 'VI.Control runtimeconfig 必须使用 .NET 8。'
    Assert-Check (@($runtimeConfig.runtimeOptions.frameworks | Where-Object { $_.name -eq 'Microsoft.WindowsDesktop.App' -and $_.version -like '8.*' }).Count -eq 1) 'VI.Control 缺少 .NET 8 WindowsDesktop 运行框架。'
    foreach ($module in Get-ChildItem -LiteralPath (Join-Path $runtime 'Modules') -Filter '*.dll' -Recurse) {
        Assert-Check ($module.Name -in @('VI.Module.Intervention.dll', 'VI.Module.PositioningArm.dll') -or $module.Name -like '*.resources.dll') "模块目录含共享依赖：$($module.FullName)"
    }
    & $dotnet (Join-Path $bin 'MasterConsole.Protocol.Tests/MasterConsole.Protocol.Tests.dll') $fixtures
    if ($LASTEXITCODE -ne 0) { throw "$configuration 协议或业务保护测试失败。" }
    & $dotnet (Join-Path $bin 'VI.Control.Tests/VI.Control.Tests.dll') (Join-Path $project "build/verification/$configuration/ui")
    if ($LASTEXITCODE -ne 0) { throw "$configuration 界面检查失败。" }
}
Write-Host "VI.Control 隔离缓存、Debug/Release x64、包与运行文件、协议及界面检查通过。缓存：$CacheDirectory"
