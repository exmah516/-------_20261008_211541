param([string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
$work = Join-Path $Root ('验证记录/master-process-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
$package = Join-Path $work 'master'
New-Item -ItemType Directory -Path $work -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $Root 'A主端/运行') -Destination $package -Recurse
$runtime = Join-Path $Root '依赖/runtime'
$children = [Collections.Generic.List[Diagnostics.Process]]::new()
$http = [Net.Http.HttpClient]::new([Net.Http.HttpClientHandler]@{UseProxy=$false})
function Assert($ok,[string]$message) { if (!$ok) { throw $message }; Write-Host "PASS $message" }
function Until([scriptblock]$test,[string]$message) {
    $timer=[Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt 40) {
        if (& $test) { Assert $true $message; return }
        Start-Sleep -Milliseconds 250
    }
    throw "Timeout: $message; last recording status: $script:lastRecordingStatus"
}
function Start-Child([string]$exe,[string]$arguments) {
    $p=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $work -WindowStyle Hidden -PassThru
    $children.Add($p)
    return $p
}
function Api([int]$port,[string]$path='/v3/paths/list') {
    $http.GetStringAsync("http://127.0.0.1:$port$path").GetAwaiter().GetResult() | ConvertFrom-Json
}
function Files([string]$session) { @(Get-ChildItem -LiteralPath $session -Recurse -Filter '*.mp4') }
function Probe([string]$session) {
    $files=Files $session
    Assert ($files.Count -eq 3) 'actual master produces one recording per continuous stream'
    foreach ($file in $files) {
        $json=& (Join-Path $runtime 'ffprobe.exe') -v error -show_entries stream=codec_name -show_entries format=duration -of json $file.FullName | ConvertFrom-Json
        Assert ($LASTEXITCODE -eq 0 -and $json.streams[0].codec_name -eq 'h264' -and [double]$json.format.duration -gt 4) ('valid H.264 fMP4 '+$file.FullName)
        & (Join-Path $runtime 'ffmpeg.exe') -v error -i $file.FullName -frames:v 3 -f null -
        Assert ($LASTEXITCODE -eq 0) 'written recording frames decode'
    }
}
function Start-Master {
    $exe=Join-Path $package 'ImagingMaster.exe'
    $p=Start-Process -FilePath $exe -WorkingDirectory $package -WindowStyle Normal -PassThru
    $children.Add($p)
    Until { try { (Api 59999).items.Where({$_.ready}).Count -eq 3 } catch { $false } } 'actual master relay pulls all three upstreams'
    $p.Refresh()
    $window=[Windows.Automation.AutomationElement]::FromHandle($p.MainWindowHandle)
    $condition=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,'RecordingStatus')
    Until {
        $text=$window.FindFirst([Windows.Automation.TreeScope]::Descendants,$condition)
        $script:lastRecordingStatus=if($text){$text.Current.Name}else{'RecordingStatus control missing'}
        $text -and $text.Current.Name.StartsWith('录像中：')
    } 'actual master status line reports recording'
    return $p
}
try {
    foreach ($i in @(0,1)) {
        $httpPort=58889+$i; $mediaPort=58189+$i; $rtspPort=58554+$i; $apiPort=59997+$i
        $config=@"
logLevel: info
logDestinations: [file]
logFile: sender-$i.log
api: true
apiAddress: 127.0.0.1:$apiPort
rtsp: true
rtspAddress: 127.0.0.1:$rtspPort
rtspTransports: [tcp]
rtmp: false
hls: false
srt: false
moq: false
webrtc: true
webrtcAddress: 127.0.0.1:$httpPort
webrtcLocalUDPAddress: 127.0.0.1:$mediaPort
webrtcIPsFromInterfaces: false
webrtcAdditionalHosts: [127.0.0.1]
webrtcICEServers2: []
authInternalUsers:
- user: any
  ips: [127.0.0.1]
  permissions:
  - action: publish
  - action: read
  - action: api
paths:
  stream_dsa:
  stream_cam1:
  stream_cam2:
"@
        $configPath=Join-Path $work "sender-$i.yml"
        [IO.File]::WriteAllText($configPath,$config,[Text.UTF8Encoding]::new($false))
        $null=Start-Child (Join-Path $runtime 'mediamtx.exe') ('"'+$configPath+'"')
    }
    Until { try { (Api 59997 '/v3/info').version -and (Api 59998 '/v3/info').version } catch { $false } } 'two independent sender simulators ready'
    foreach ($stream in @('stream_dsa','stream_cam1','stream_cam2')) {
        $rtspPort=if($stream -eq 'stream_dsa'){58554}else{58555}
        $encodeArgs='-hide_banner -loglevel error -re -f lavfi -i testsrc2=size=640x360:rate=25 -an -c:v libx264 -preset veryfast -tune zerolatency -profile:v baseline -threads 2 -bf 0 -g 25 -b:v 512k -rtsp_transport tcp -f rtsp rtsp://127.0.0.1:'+ $rtspPort+'/'+$stream
        $null=Start-Child (Join-Path $runtime 'ffmpeg.exe') $encodeArgs
    }
    $dsa="screenCutIp=127.0.0.1`nwebrtcPort=58889`n[relay]`nwebrtcPort=58891`nmediaPort=58191`napiPort=59999`nrecordingRoot=recordings"
    $cam="[camera1]`nip=127.0.0.1`nwebrtcPort=58890`n[camera2]`nip=127.0.0.1`nwebrtcPort=58890"
    [IO.File]::WriteAllText((Join-Path $package 'config/dsa_screencut.ini'),$dsa)
    [IO.File]::WriteAllText((Join-Path $package 'config/hik_cameras.ini'),$cam)
    $master=Start-Master
    $relayId=(Get-NetTCPConnection -State Listen -LocalPort 59999).OwningProcess
    $tcp=@(Get-NetTCPConnection -OwningProcess $relayId -State Listen)
    $udp=@(Get-NetUDPEndpoint -OwningProcess $relayId)
    $tcp | Select-Object LocalAddress,LocalPort,OwningProcess | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'listeners-tcp.json')
    $udp | Select-Object LocalAddress,LocalPort,OwningProcess | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'sockets-udp.json')
    Assert ($tcp.Count -eq 3 -and !($tcp | Where-Object LocalAddress -ne '127.0.0.1')) 'all master TCP listeners bind only 127.0.0.1'
    Assert (@($udp | Where-Object LocalPort -eq 58191).Count -eq 1 -and ($udp | Where-Object LocalPort -eq 58191).LocalAddress -eq '127.0.0.1') 'master WebRTC server UDP port binds only 127.0.0.1'
    Start-Sleep -Seconds 10
    $first=(Get-ChildItem -LiteralPath (Join-Path $package 'recordings') -Directory | Sort-Object Name | Select-Object -Last 1).FullName
    Assert ($master.CloseMainWindow()) 'request normal master window close'
    Assert ($master.WaitForExit(15000)) 'master waits for MediaMTX and exits'
    Assert ((Get-Content -Raw -LiteralPath (Join-Path $first 'mediamtx.log')).Contains('shutting down gracefully')) 'actual master sends graceful MediaMTX shutdown'
    Probe $first
    $master=Start-Master
    Start-Sleep -Seconds 10
    $last=(Get-ChildItem -LiteralPath (Join-Path $package 'recordings') -Directory | Sort-Object Name | Select-Object -Last 1).FullName
    Assert ($last -ne $first) 'each master launch creates a distinct session directory'
    $master.Kill(); $master.WaitForExit()
    Until { !(Get-NetTCPConnection -State Listen -LocalPort 59999 -ErrorAction SilentlyContinue) } 'forced master termination also removes its relay process'
    Probe $last
    Write-Output "MASTER_PROCESS_OUTPUT $work"
} finally {
    foreach ($p in $children) { if (!$p.HasExited) { $p.Kill(); $p.WaitForExit() } }
    $http.Dispose()
}
