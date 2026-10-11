param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$runtime = Join-Path $project "master/MasterConsole/bin/$Configuration/net472"
if (!$OutputDirectory) { $OutputDirectory = Join-Path $project '../../验证记录/主端界面与距离_20261012' }
$framework = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319'
$references = @(
    "$runtime/MasterConsole.exe", "$runtime/MasterConsole.Protocol.dll",
    "$framework/WPF/PresentationFramework.dll", "$framework/WPF/PresentationCore.dll",
    "$framework/WPF/WindowsBase.dll", "$framework/System.Xaml.dll", "$framework/System.Xml.Linq.dll"
)
$exe = Join-Path $runtime 'MasterUiCheck.exe'
$compileArgs = @('/nologo', '/platform:x64', "/out:$exe")
$compileArgs += $references | ForEach-Object { "/r:$_" }
$compileArgs += Join-Path $PSScriptRoot 'MasterUiCheck.cs'
& "$framework/csc.exe" @compileArgs
if ($LASTEXITCODE -ne 0) { throw 'Offline UI test compilation failed.' }
& $exe $project $OutputDirectory
if ($LASTEXITCODE -ne 0) { throw 'Offline UI verification failed.' }
