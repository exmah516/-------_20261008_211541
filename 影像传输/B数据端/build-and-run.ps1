param([switch]$BuildOnly)
$ErrorActionPreference = 'Stop'
try {
    if (Get-Process -Name ScreenCut -ErrorAction SilentlyContinue) { throw '请先关闭 ScreenCut。' }
    & (Join-Path $PSScriptRoot '../准备依赖.ps1') -Offline
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw '需要 VS2022 的 .NET 桌面开发工作负载和 net472 开发包。' }
    $msbuild = @(& $vswhere -latest -products '*' -version '[17.0,)' -requires Microsoft.Component.MSBuild -find 'MSBuild/Current/Bin/amd64/MSBuild.exe') | Select-Object -First 1
    if (!$msbuild) { throw '未找到 MSBuild。' }
    & $msbuild (Join-Path $PSScriptRoot 'ScreenCut源码/ScreenCut.sln') /restore /m:1 /nologo /verbosity:minimal /p:Configuration=Release /p:Platform=x64 /p:NuGetAudit=false
    if ($LASTEXITCODE -ne 0) { throw "编译失败 ($LASTEXITCODE)，未启动。" }
    $runtime = Join-Path $PSScriptRoot 'ScreenCut'
    $output = Join-Path $PSScriptRoot 'ScreenCut源码/ScreenCut/bin/x64/Release'
    foreach ($name in @('ScreenCut.exe','ScreenCut.exe.config','runtime/ffmpeg.exe','runtime/mediamtx.exe')) {
        if (!(Test-Path -LiteralPath (Join-Path $output $name))) { throw "缺少运行文件：$name" }
    }
    New-Item -ItemType Directory -Path $runtime -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $output 'ScreenCut.exe') -Destination $runtime -Force
    $mediaRuntime = Join-Path $runtime 'runtime'
    New-Item -ItemType Directory -Path $mediaRuntime -Force | Out-Null
    foreach ($name in @('ffmpeg.exe','mediamtx.exe','MediaMTX-LICENSE','FFmpeg-LICENSE.txt')) {
        Copy-Item -LiteralPath (Join-Path $output ('runtime/'+$name)) -Destination $mediaRuntime -Force
    }
    $deployedConfig = Join-Path $runtime 'ScreenCut.exe.config'
    if (!(Test-Path -LiteralPath $deployedConfig)) { Copy-Item -LiteralPath (Join-Path $output 'ScreenCut.exe.config') -Destination $deployedConfig }
    $current = [xml][IO.File]::ReadAllText($deployedConfig,[Text.Encoding]::UTF8)
    $defaults = [xml][IO.File]::ReadAllText((Join-Path $output 'ScreenCut.exe.config'),[Text.Encoding]::UTF8)
    $appSettings = $current.SelectSingleNode('/configuration/appSettings')
    if (!$appSettings) { $appSettings = $current.DocumentElement.AppendChild($current.CreateElement('appSettings')) }
    foreach ($entry in $defaults.configuration.appSettings.add) {
        if (!($appSettings.SelectNodes('add') | Where-Object { $_.key -eq $entry.key })) { $null = $appSettings.AppendChild($current.ImportNode($entry,$true)) }
    }
    # 私有相机凭据沿用 INI，首次迁移才复制；后续编译保留现场数据端配置。
    $cameraConfigDir = Join-Path $runtime 'config'
    New-Item -ItemType Directory -Path $cameraConfigDir -Force | Out-Null
    $cameraIni = Join-Path $cameraConfigDir 'hik_cameras.ini'
    if (!(Test-Path -LiteralPath $cameraIni)) {
        $oldIni = Join-Path $PSScriptRoot '../A主端/工程/config/hik_cameras.ini'
        if (!(Test-Path -LiteralPath $oldIni)) { $oldIni = Join-Path $PSScriptRoot 'config/hik_cameras.ini.example' }
        Copy-Item -LiteralPath $oldIni -Destination $cameraIni
    }
    $settings = [Xml.XmlWriterSettings]::new(); $settings.Encoding = [Text.UTF8Encoding]::new($false); $settings.Indent = $true
    $writer = [Xml.XmlWriter]::Create($deployedConfig,$settings)
    try { $current.Save($writer) } finally { $writer.Dispose() }
    $oldRelay = Join-Path $runtime 'ImagingCameraRelay.dll'
    if (Test-Path -LiteralPath $oldRelay) { Remove-Item -LiteralPath $oldRelay -Force }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../依赖/第三方组件说明.md') -Destination $runtime -Force
    $exe = Join-Path $runtime 'ScreenCut.exe'
    Write-Host "已生成完整数据端运行包：$runtime"
    if (!$BuildOnly) { Start-Process -FilePath $exe -WorkingDirectory $runtime -WindowStyle Normal }
    exit 0
} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }
