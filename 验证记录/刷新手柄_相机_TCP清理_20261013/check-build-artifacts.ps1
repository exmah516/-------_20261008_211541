$ErrorActionPreference = 'Stop'
$package = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$slave = Join-Path $package 'B_从端电脑\工程\ADS项目'
$master = Join-Path $package 'A_主端电脑_地址修正版\工程'
[xml]$cpp = Get-Content -LiteralPath (Join-Path $slave 'ADS.vcxproj') -Raw -Encoding UTF8
$members = @($cpp.Project.ItemGroup.ClCompile.Include) + @($cpp.Project.ItemGroup.ClInclude.Include)
foreach ($file in @('action4_camera.cpp','action4_camera.h','tcp_force_daq.cpp','tcp_force_daq.h')) {
    if ($members -contains $file) { throw "Excluded source still in project: $file" }
}
Write-Output 'PASS: Action 4 and TCP sensor source/header excluded from C++ project'
foreach ($configuration in @('Debug','Release')) {
    $linkLog = Join-Path $slave "x64\$configuration\ADS.tlog\link.command.1.tlog"
    if (Test-Path -LiteralPath $linkLog) {
        $linkText = Get-Content -LiteralPath $linkLog -Raw
        if ($linkText -match '(?i)(action4_camera|tcp_force_daq)\.obj') { throw 'Obsolete object still linked' }
    }
}
$ui = [Reflection.Assembly]::LoadFrom((Join-Path $slave 'AdsControlUI\bin\Release\net472\AdsControlUI.exe'))
if ($null -ne $ui.GetType('AdsControlUI.CameraPreviewWindow', $false)) { throw 'Camera preview still compiled' }
$wire = $ui.GetType('AdsControlUI.VisState', $true)
if ([Runtime.InteropServices.Marshal]::SizeOf([type]$wire) -ne 871) { throw 'VisState layout changed' }
$stream = $ui.GetManifestResourceStream('AdsControlUI.g.resources')
$reader = New-Object System.Resources.ResourceReader($stream)
foreach ($entry in $reader) {
    if ($entry.Key -match 'camerapreview') { throw 'Camera preview BAML still compiled' }
}
$reader.Close()
Write-Output 'PASS: WPF camera preview type/resource absent; VisState remains 871 bytes'
$app = [Reflection.Assembly]::LoadFrom((Join-Path $master 'master\MasterConsole\bin\Release\net472\MasterConsole.exe'))
if ($null -ne $app.GetType('MasterConsole.Services.IRobotLink', $true).GetMethod('RefreshHandlesAsync')) {
    throw 'Refresh API still compiled'
}
if ($null -ne $app.GetType('MasterConsole.Services.HandleService', $true).GetMethod('RefreshNow')) {
    throw 'Refresh implementation still compiled'
}
Write-Output 'PASS: compiled master link and handpiece service contain no refresh API'

