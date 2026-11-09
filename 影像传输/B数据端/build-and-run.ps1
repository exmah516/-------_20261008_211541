param([switch]$BuildOnly)
$ErrorActionPreference = 'Stop'
try {
    if (Get-Process -Name ScreenCut,CameraMonitor -ErrorAction SilentlyContinue) { throw '请先关闭截图、监控或旧版 ScreenCut。' }
    & (Join-Path $PSScriptRoot '../准备依赖.ps1') -Offline
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw '需要 VS2022 的 .NET 桌面开发工作负载和 net472 开发包。' }
    $msbuild = @(& $vswhere -latest -products '*' -version '[17.0,)' -requires Microsoft.Component.MSBuild -find 'MSBuild/Current/Bin/amd64/MSBuild.exe') | Select-Object -First 1
    if (!$msbuild) { throw '未找到 MSBuild。' }
    & $msbuild (Join-Path $PSScriptRoot 'ScreenCut源码/ScreenCut.sln') /restore /m:1 /nologo /verbosity:minimal /p:Configuration=Release /p:Platform=x64 /p:NuGetAudit=false
    if ($LASTEXITCODE -ne 0) { throw "编译失败 ($LASTEXITCODE)，未启动。" }

    $runtime = Join-Path $PSScriptRoot '运行'
    $legacy = Join-Path $PSScriptRoot 'ScreenCut'
    $legacyConfig = Join-Path $legacy 'ScreenCut.exe.config'
    $legacyEntries = @()
    if (Test-Path -LiteralPath $legacyConfig) {
        $legacyEntries = ([xml][IO.File]::ReadAllText($legacyConfig,[Text.Encoding]::UTF8)).SelectNodes('/configuration/appSettings/add')
    }
    $captureOutput = Join-Path $PSScriptRoot 'ScreenCut源码/ScreenCut/bin/x64/Release'
    $monitorOutput = Join-Path $PSScriptRoot 'ScreenCut源码/CameraMonitor/bin/x64/Release'
    $configDir = Join-Path $runtime 'config'
    $mediaDir = Join-Path $runtime 'runtime'
    New-Item -ItemType Directory -Path $runtime,$configDir,$mediaDir -Force | Out-Null

    function Initialize-Config([string]$Source, [string]$Pattern) {
        $target = Join-Path $runtime (Split-Path $Source -Leaf)
        if (Test-Path -LiteralPath $target) { return }
        $xml = [xml][IO.File]::ReadAllText($Source,[Text.Encoding]::UTF8)
        $settings = $xml.SelectSingleNode('/appSettings | /configuration/appSettings')
        foreach ($old in $legacyEntries) {
            if ($old.key -notmatch $Pattern) { continue }
            $entry = $settings.SelectNodes('add') | Where-Object { $_.key -eq $old.key } | Select-Object -First 1
            if ($entry) { $entry.SetAttribute('value', $old.value) }
            else { $null = $settings.AppendChild($xml.ImportNode($old,$true)) }
        }
        $xml.Save($target)
    }
    Initialize-Config (Join-Path $captureOutput 'streaming.config') '^(MasterIp|RelayListenIp|WebRtcPort|WebRtcMediaPort|LocalRtspPort|LocalApiPort)$'
    Initialize-Config (Join-Path $captureOutput 'ScreenCut.exe.config') '^(Capture(Left|Top|Width|Height)|Video(Width|Height|Fps|BitrateKbps))$'
    Initialize-Config (Join-Path $monitorOutput 'CameraMonitor.exe.config') '^Camera[12](Enabled|Ip|LocalIp|RtspPort|RtspChannel|Username|Password)$'
    $cameraIni = Join-Path $configDir 'hik_cameras.ini'
    if (!(Test-Path -LiteralPath $cameraIni)) {
        $oldIni = Join-Path $legacy 'config/hik_cameras.ini'
        if (Test-Path -LiteralPath $oldIni) { Copy-Item -LiteralPath $oldIni -Destination $cameraIni }
        else {
            $oldIni = Join-Path $PSScriptRoot '../A主端/工程/config/hik_cameras.ini'
            if (!(Test-Path -LiteralPath $oldIni)) { $oldIni = Join-Path $PSScriptRoot 'config/hik_cameras.ini.example' }
            $cameraText = [IO.File]::ReadAllText($oldIni,[Text.Encoding]::UTF8)
            $cameraText = [regex]::Replace($cameraText, '(?m)^(stream\s*=\s*)0\s*$', '${1}1')
            [IO.File]::WriteAllText($cameraIni,$cameraText,[Text.UTF8Encoding]::new($false))
        }
    }
    foreach ($name in @('ffmpeg.exe','mediamtx.exe','MediaMTX-LICENSE','FFmpeg-LICENSE.txt')) {
        Copy-Item -LiteralPath (Join-Path $captureOutput ('runtime/'+$name)) -Destination $mediaDir -Force
    }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../依赖/第三方组件说明.md') -Destination $runtime -Force
    foreach ($name in @('check-camera-connection.ps1','检查相机连接.cmd')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $runtime -Force
    }
    Copy-Item -LiteralPath (Join-Path $captureOutput 'ScreenCut.exe'),(Join-Path $monitorOutput 'CameraMonitor.exe') -Destination $runtime -Force
    # 每份包都有独立配置和运行库，复制到不同电脑后互不依赖。
    foreach ($app in @('ScreenCut','CameraMonitor')) {
        $role = if ($app -eq 'ScreenCut') { '截图端' } else { '监控端' }
        $package = Join-Path $PSScriptRoot $role
        $isCapture = $app -eq 'ScreenCut'
        $packageMedia = Join-Path $package 'runtime'
        New-Item -ItemType Directory -Path $package,$packageMedia -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $runtime ($app+'.exe')) -Destination $package -Force
        $appConfig = Join-Path $package ($app+'.exe.config')
        if (!(Test-Path -LiteralPath $appConfig)) {
            $xml = [xml][IO.File]::ReadAllText((Join-Path $runtime ($app+'.exe.config')),[Text.Encoding]::UTF8)
            if ($isCapture) {
                foreach ($pair in @(@('VideoWidth','1920'),@('VideoHeight','1080'),@('VideoFps','25'),@('VideoBitrateKbps','6000'))) {
                    $xml.SelectSingleNode("//add[@key='$($pair[0])']").SetAttribute('value',$pair[1])
                }
            }
            $xml.Save($appConfig)
        }
        $serviceConfig = Join-Path $package 'streaming.config'
        if (!(Test-Path -LiteralPath $serviceConfig)) {
            $roleOutput = if ($isCapture) { $captureOutput } else { $monitorOutput }
            Copy-Item -LiteralPath (Join-Path $roleOutput 'streaming.config') -Destination $serviceConfig
        }
        $mediaFiles = if ($isCapture) { @('ffmpeg.exe','mediamtx.exe','MediaMTX-LICENSE','FFmpeg-LICENSE.txt') } else { @('mediamtx.exe','MediaMTX-LICENSE') }
        foreach ($name in $mediaFiles) { Copy-Item -LiteralPath (Join-Path $mediaDir $name) -Destination $packageMedia -Force }
        Copy-Item -LiteralPath (Join-Path $runtime '第三方组件说明.md') -Destination $package -Force
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot '有线分机部署与画质设置.md') -Destination $package -Force
        if (!$isCapture) {
            $packageConfig = Join-Path $package 'config'
            New-Item -ItemType Directory -Path $packageConfig -Force | Out-Null
            $packageIni = Join-Path $packageConfig 'hik_cameras.ini'
            if (!(Test-Path -LiteralPath $packageIni)) { Copy-Item -LiteralPath $cameraIni -Destination $packageIni }
            foreach ($name in @('check-camera-connection.ps1','检查相机连接.cmd')) {
                Copy-Item -LiteralPath (Join-Path $runtime $name) -Destination $package -Force
            }
        }
        Write-Host "已生成独立运行包：$package"
    }
    if (!$BuildOnly) {
        foreach ($entry in @('截图端/ScreenCut.exe','监控端/CameraMonitor.exe')) {
            $exe = Join-Path $PSScriptRoot $entry
            Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -WindowStyle Normal
        }
    }
    exit 0
} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }
