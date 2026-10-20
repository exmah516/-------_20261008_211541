[CmdletBinding()]
param([switch]$SkipNetwork)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$project = Split-Path $PSScriptRoot -Parent
$package = [System.IO.Path]::GetFullPath((Join-Path $project '..\..\..\..'))
$sdk = Join-Path $package 'zenoh\sdk\zenoh-c-1.10.1-msvc'
$msgpack = Join-Path $package 'zenoh\sdk\msgpack-cxx-9.0.0\include'
$fixtures = Join-Path $package 'protocol\fixtures\zenoh-v1'
$output = Join-Path $PSScriptRoot 'zenoh\.build'
$source = Join-Path $PSScriptRoot 'zenoh'
$vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$visualStudio) { throw '找不到 Visual C++ 编译器。' }
$developerShell = Join-Path $visualStudio 'Common7\Tools\VsDevCmd.bat'
New-Item -ItemType Directory -Force -Path $output | Out-Null

function Build-Cpp([string]$Name, [string[]]$Sources, [switch]$Zenoh) {
    $quotedSources = ($Sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $arguments = '/nologo /std:c++17 /EHsc /utf-8 /DNOMINMAX /DMSGPACK_NO_BOOST ' +
        '/I"' + $msgpack + '" /I"' + (Join-Path $sdk 'include') + '" /I"' + $project + '" ' + $quotedSources +
        ' /Fe:"' + (Join-Path $output ($Name + '.exe')) + '" /Fo:"' + $output + '\\"'
    if ($Zenoh) { $arguments += ' /link /LIBPATH:"' + (Join-Path $sdk 'lib') + '" zenohc.dll.lib bcrypt.lib ws2_32.lib' }
    & cmd /d /c ('"' + $developerShell + '" -arch=x64 -host_arch=x64 && cl ' + $arguments)
    if ($LASTEXITCODE -ne 0) { throw "$Name 编译失败：$LASTEXITCODE" }
}

Build-Cpp 'protocol_tests' @((Join-Path $source 'protocol_tests.cpp'))
& (Join-Path $output 'protocol_tests.exe') $fixtures
if ($LASTEXITCODE -ne 0) { throw 'C++ 黄金样本测试失败。' }

# 使用真实 Zenoh 服务和内存快照；测试程序没有链接 ADS/PLC 库。
$service = Join-Path $project 'zenoh_service.cpp'
Build-Cpp 'service_tests' @((Join-Path $source 'service_tests.cpp'), $service) -Zenoh
Build-Cpp 'simulated_ads' @((Join-Path $source 'simulated_ads.cpp'), $service) -Zenoh
Copy-Item -LiteralPath (Join-Path $sdk 'bin\zenohc.dll') -Destination $output -Force
& (Join-Path $output 'service_tests.exe')
if ($LASTEXITCODE -ne 0) { throw 'C++ 服务边界测试失败。' }

$master = Join-Path $package '介入机器人\A主端\工程\master'
$tests = Join-Path $master 'MasterConsole.Protocol.Tests\MasterConsole.Protocol.Tests.csproj'
& dotnet build $tests -v:minimal
if ($LASTEXITCODE -ne 0) { throw 'C# 测试工程编译失败。' }
$testExe = Join-Path $master 'MasterConsole.Protocol.Tests\bin\Debug\net472\MasterConsole.Protocol.Tests.exe'
if ($SkipNetwork) {
    & $testExe $fixtures
} else {
    & $testExe $fixtures --network --simulator (Join-Path $output 'simulated_ads.exe') --router (Join-Path $package 'zenoh\zenohd.exe')
}
if ($LASTEXITCODE -ne 0) { throw 'C# 协议或原生跨语言通信测试失败。' }
Write-Host 'Zenoh 离线验证全部通过。'

