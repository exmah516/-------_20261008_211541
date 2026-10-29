param([switch]$BandwidthOnly, [switch]$LoggingOnly, [switch]$EncodingOnly)
$ErrorActionPreference = 'Stop'
if (!$LoggingOnly) { & (Join-Path $PSScriptRoot '准备依赖.ps1') -Offline }
$project = Join-Path $PSScriptRoot 'B数据端/ScreenCut源码/tools/ScreenCutCheck/ScreenCutCheck.csproj'
& dotnet build $project -c Release --nologo --verbosity minimal -p:Platform=x64 -p:NuGetAudit=false -m:1
if ($LASTEXITCODE -ne 0) { throw '回环检查工程编译失败。' }
$env:IMAGING_TEST_OUTPUT = Join-Path $PSScriptRoot '验证记录'
New-Item -ItemType Directory -Path $env:IMAGING_TEST_OUTPUT -Force | Out-Null
$exe = Join-Path $PSScriptRoot 'B数据端/ScreenCut源码/tools/ScreenCutCheck/bin/x64/Release/net472/ScreenCutCheck.exe'
$testArgs = @()
$record = 'last-run.txt'
if ($BandwidthOnly) { $testArgs = @('--bandwidth'); $record = 'bandwidth-run.txt' }
if ($LoggingOnly) { $testArgs = @('--logging'); $record = 'logging-run.txt' }
if ($EncodingOnly) { $testArgs = @('--encoding'); $record = 'encoding-run.txt' }
& $exe @testArgs 2>&1 | Tee-Object -FilePath (Join-Path $env:IMAGING_TEST_OUTPUT $record)
if ($LASTEXITCODE -ne 0) { throw "回环检查失败，见 验证记录/$record。" }
