param(
    [string]$RuntimeDir = '',
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [switch]$AutoClose,
    [string]$ListenAddress = '',
    [string]$PeerAddress = '192.168.50.2',
    [ValidateRange(1024,65535)][int]$Port = 32110
)
$ErrorActionPreference = 'Stop'
if (!$RuntimeDir) {
    & (Join-Path $PSScriptRoot '../build-and-run.ps1') -Configuration $Configuration -BuildOnly -Sim
    if ($LASTEXITCODE -ne 0) { throw 'Master build failed; VTK test was not started.' }
    $RuntimeDir = Join-Path $PSScriptRoot "../master/MasterConsole/bin/$Configuration/net472"
}
$RuntimeDir = (Resolve-Path -LiteralPath $RuntimeDir).Path
$framework = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319'
$references = @(
    "$RuntimeDir/MasterConsole.exe", "$RuntimeDir/Kitware.VTK.dll", "$RuntimeDir/Kitware.mummy.Runtime.dll",
    "$framework/WPF/PresentationFramework.dll", "$framework/WPF/PresentationCore.dll",
    "$framework/WPF/WindowsBase.dll", "$framework/System.Xaml.dll",
    "$framework/System.Windows.Forms.dll", "$framework/System.Drawing.dll"
)
foreach ($file in $references) { if (!(Test-Path -LiteralPath $file)) { throw "Missing dependency: $file" } }
$exe = Join-Path $RuntimeDir 'VtkMotionCheck.exe'
$source = Join-Path $PSScriptRoot 'VtkMotionCheck.cs'
$compileArgs = @('/nologo', '/platform:x64', "/out:$exe")
$compileArgs += $references | ForEach-Object { "/r:$($_)" }
$compileArgs += $source
& "$framework/csc.exe" @compileArgs
if ($LASTEXITCODE -ne 0) { throw 'VTK test compilation failed' }
$runArgs = @()
if ($AutoClose) { $runArgs += '--auto-close' }
if ($ListenAddress) { $runArgs += @('--listen', $ListenAddress, '--peer', $PeerAddress, '--port', "$Port") }
& $exe @runArgs
exit $LASTEXITCODE
