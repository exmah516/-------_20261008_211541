param([string]$DataRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
$stage = Join-Path ([IO.Path]::GetTempPath()) ('VI.Imaging.SplitCheck-'+[guid]::NewGuid().ToString('N'))
$children = [Collections.Generic.List[Diagnostics.Process]]::new()
$http = [Net.Http.HttpClient]::new([Net.Http.HttpClientHandler]@{UseProxy=$false})
$http.Timeout = [TimeSpan]::FromSeconds(1)

function Set-Value($xml, [string]$key, [string]$value) {
    $entry = $xml.SelectSingleNode("//add[@key='$key']")
    if (!$entry) {
        $entry = $xml.CreateElement('add'); $entry.SetAttribute('key',$key)
        $null = $xml.SelectSingleNode('//appSettings').AppendChild($entry)
    }
    $entry.SetAttribute('value',$value)
}
function Api([int]$port, [string]$path='/v3/info') {
    $http.GetStringAsync("http://127.0.0.1:$port$path").GetAwaiter().GetResult() | ConvertFrom-Json
}
function Until([scriptblock]$test, [string]$message) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt 20) {
        if (& $test) { Write-Output "PASS $message"; return }
        Start-Sleep -Milliseconds 200
    }
    throw "Timeout: $message"
}
function Relay-Pid([int]$port) {
    (Get-NetTCPConnection -State Listen -LocalPort $port -ErrorAction SilentlyContinue | Select-Object -First 1).OwningProcess
}
function Start-App([string]$role, [string]$name) {
    $dir = Join-Path $stage $role
    $p = Start-Process -FilePath (Join-Path $dir $name) -WorkingDirectory $dir -WindowStyle Normal -PassThru
    $children.Add($p); $null = $p.WaitForInputIdle(10000)
    return $p
}
function Element([Diagnostics.Process]$p, [string]$property, [string]$value) {
    $p.Refresh()
    $window = [Windows.Automation.AutomationElement]::FromHandle($p.MainWindowHandle)
    $prop = if ($property -eq 'id') { [Windows.Automation.AutomationElement]::AutomationIdProperty } else { [Windows.Automation.AutomationElement]::NameProperty }
    $result = $window.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new($prop,$value))
    if (!$result) { throw "Missing control: $value" }
    return $result
}
function Click([Diagnostics.Process]$p, [string]$name) {
    $button = Element $p 'name' $name
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while (!$button.Current.IsEnabled -and $watch.Elapsed.TotalSeconds -lt 15) { Start-Sleep -Milliseconds 100 }
    ([Windows.Automation.InvokePattern]$button.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern)).Invoke()
}
function Set-Text([Diagnostics.Process]$p, [string]$id, [string]$value) {
    $control = Element $p 'id' $id
    ([Windows.Automation.ValuePattern]$control.GetCurrentPattern([Windows.Automation.ValuePattern]::Pattern)).SetValue($value)
}
try {
    Until { !(Relay-Pid 49997) -and !(Relay-Pid 49998) } 'independent test ports free'
    New-Item -ItemType Directory -Path $stage | Out-Null
    foreach ($role in @('截图端','监控端')) {
        Copy-Item -LiteralPath (Join-Path $DataRoot $role) -Destination $stage -Recurse
        $isCapture = $role -eq '截图端'
        $dir = Join-Path $stage $role
        $service = [xml][IO.File]::ReadAllText((Join-Path $dir 'streaming.config'))
        $values = if ($isCapture) {
            @(@('MasterIp','127.0.0.1'),@('RelayListenIp','127.0.0.1'),@('WebRtcPort','48889'),@('WebRtcMediaPort','48189'),@('LocalRtspPort','48554'),@('LocalApiPort','49997'))
        } else {
            @(@('MasterIp','127.0.0.1'),@('RelayListenIp','127.0.0.4'),@('WebRtcPort','48890'),@('WebRtcMediaPort','48190'),@('LocalRtspPort','48556'),@('LocalApiPort','49998'))
        }
        foreach ($pair in $values) { Set-Value $service $pair[0] $pair[1] }
        $service.Save((Join-Path $dir 'streaming.config'))
        $appName = if ($isCapture) { 'ScreenCut' } else { 'CameraMonitor' }
        $appPath = Join-Path $dir ($appName+'.exe.config')
        $config = [xml][IO.File]::ReadAllText($appPath)
        if ($isCapture) {
            foreach ($pair in @(@('CaptureLeft','0'),@('CaptureTop','0'),@('CaptureWidth','640'),@('CaptureHeight','480'))) {
                Set-Value $config $pair[0] $pair[1]
            }
        } else {
            Set-Value $config 'Camera1Enabled' 'false'; Set-Value $config 'Camera2Enabled' 'false'
        }
        $config.Save($appPath)
    }
    $screen = Start-App '截图端' 'ScreenCut.exe'
    Until { try { (Api 49997).version } catch { $false } } 'standalone screenshot package starts outside workspace'
    $cam = Start-App '监控端' 'CameraMonitor.exe'
    Until { try { (Api 49998).version } catch { $false } } 'standalone monitor package starts on a different IP with no FFmpeg'
    $screenRelay = Relay-Pid 49997; $cameraRelay = Relay-Pid 49998
    if ($screenRelay -eq $cameraRelay) { throw 'Independent services unexpectedly share a process' }
    Write-Output 'PASS two packages own distinct relay processes'

    Click $screen '预览区域'
    $combo = Element $screen 'id' 'qualityPreset'
    $expand = [Windows.Automation.ExpandCollapsePattern]$combo.GetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern)
    $expand.Expand()
    $preset = $combo.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.AndCondition]::new(
        [Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,'清晰 720p'),
        [Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::ListItem)))
    if (!$preset) { throw 'Preset list item not found' }
    ([Windows.Automation.SelectionItemPattern]$preset.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern)).Select()
    $expand.Collapse()
    Start-Sleep -Milliseconds 500
    Click $screen '开始传图 / 应用画质'
    Until { try { (Api 49997 '/v3/paths/get/stream_dsa').ready } catch { $false } } 'quality preset starts real desktop capture'
    $savedPath = Join-Path $stage '截图端/ScreenCut.exe.config'
    $saved = [xml][IO.File]::ReadAllText($savedPath)
    if ($saved.SelectSingleNode("//add[@key='VideoWidth']").value -ne '1280' -or
        $saved.SelectSingleNode("//add[@key='VideoHeight']").value -ne '720' -or
        $saved.SelectSingleNode("//add[@key='VideoBitrateKbps']").value -ne '2500') { throw 'Preset values were not saved correctly' }
    Write-Output 'PASS 720p preset saves resolution and bitrate'

    Set-Text $screen 'txtVideoWidth' '1920'; Set-Text $screen 'txtVideoHeight' '1080'
    Set-Text $screen 'txtVideoFps' '30'; Set-Text $screen 'txtBitrate' '10000'
    Click $screen '开始传图 / 应用画质'
    Until { (Get-Content -LiteralPath (Join-Path $stage '截图端/streaming.log') -Raw).Contains('30 fps，10000 kbps') } 'manual quality applies 1080p30 at 10000 kbps'
    $savedHash = (Get-FileHash -LiteralPath $savedPath).Hash
    Set-Text $screen 'txtBitrate' '20001'
    Click $screen '开始传图 / 应用画质'
    Until { (Get-Content -LiteralPath (Join-Path $stage '截图端/streaming.log') -Raw).Contains('128–20000') } 'out-of-range bitrate is rejected in the UI'
    if ((Get-FileHash -LiteralPath $savedPath).Hash -ne $savedHash) { throw 'Invalid quality overwrote saved config' }
    if ((Relay-Pid 49998) -ne $cameraRelay) { throw 'Screenshot quality changed monitor service' }
    Write-Output 'PASS invalid input preserves saved settings and independent monitor'

    $null = $screen.CloseMainWindow()
    if (!$screen.WaitForExit(10000)) { throw 'Screenshot did not close' }
    Until { try { !(Relay-Pid 49997) -and (Relay-Pid 49998) -eq $cameraRelay -and (Api 49998).version } catch { $false } } 'closing screenshot stops only its service'
    $screen = Start-App '截图端' 'ScreenCut.exe'
    Until { try { (Api 49997).version } catch { $false } } 'screenshot reopens while monitor remains running'
    Click $screen '开始传图 / 应用画质'
    Until { try { (Api 49997 '/v3/paths/get/stream_dsa').ready } catch { $false } } 'persisted quality resumes desktop capture'
    $cam.Kill(); $null = $cam.WaitForExit(10000)
    Until { try { !(Relay-Pid 49998) -and (Api 49997 '/v3/paths/get/stream_dsa').ready } catch { $false } } 'forced monitor exit stops its service while screenshot continues'
    $screenRelay = Relay-Pid 49997
    (Get-Process -Id $screenRelay).Kill()
    Until { try { (Relay-Pid 49997) -and (Relay-Pid 49997) -ne $screenRelay -and (Api 49997 '/v3/paths/get/stream_dsa').ready } catch { $false } } 'failed screenshot service restarts and resumes capture'
    $null = $screen.CloseMainWindow()
    if (!$screen.WaitForExit(10000)) { throw 'Screenshot did not close' }
    Until { !(Relay-Pid 49997) -and !(Relay-Pid 49998) } 'all independent services release ports on exit'
    Write-Output 'ALL INDEPENDENT PACKAGE CHECKS PASSED'
} finally {
    foreach ($p in $children) { if (!$p.HasExited) { $p.Kill(); $null = $p.WaitForExit(10000) } }
    $http.Dispose()
    $fullStage = [IO.Path]::GetFullPath($stage)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if (Test-Path -LiteralPath $fullStage) {
        if (!$fullStage.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -or
            (Split-Path $fullStage -Leaf) -notlike 'VI.Imaging.SplitCheck-*') { throw 'Unexpected test cleanup target' }
        Remove-Item -LiteralPath $fullStage -Recurse -Force
    }
}
