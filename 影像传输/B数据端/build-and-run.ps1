param([switch]$BuildOnly)
$ErrorActionPreference = 'Stop'
try {
    if (Get-Process -Name ScreenCut -ErrorAction SilentlyContinue) { throw 'Close ScreenCut before building.' }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio 2022 with C++ v142 and .NET Framework 4.7.2 is required.' }
    $msbuild = @(& $vswhere -latest -products '*' -version '[17.0,)' -requires Microsoft.Component.MSBuild -find 'MSBuild/Current/Bin/MSBuild.exe') | Select-Object -First 1
    if (!$msbuild) { throw 'MSBuild not found.' }
    & $msbuild (Join-Path $PSScriptRoot 'ScreenCut源码/ScreenCut.sln') /m /nologo /verbosity:minimal /p:Configuration=Release /p:Platform=x64
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE). Nothing was launched." }
    $runtime = Join-Path $PSScriptRoot 'ScreenCut'
    $output = Join-Path $PSScriptRoot 'ScreenCut源码/ScreenCut/bin/x64/Release'
    foreach ($name in @('ScreenCut.exe', 'ScreenCut.exe.config', 'ImagingCameraRelay.dll')) {
        if (!(Test-Path -LiteralPath (Join-Path $output $name))) { throw "Missing runtime file: $name" }
    }
    New-Item -ItemType Directory -Path $runtime -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $output 'ScreenCut.exe'), (Join-Path $output 'ImagingCameraRelay.dll') -Destination $runtime -Force
    $deployedConfig = Join-Path $runtime 'ScreenCut.exe.config'
    if (!(Test-Path -LiteralPath $deployedConfig)) {
        Copy-Item -LiteralPath (Join-Path $output 'ScreenCut.exe.config') -Destination $deployedConfig
    }
    else {
        # 旧版升级只补齐缺失的配置项，保留现场修改的地址。
        $current = [xml][IO.File]::ReadAllText($deployedConfig, [Text.Encoding]::UTF8)
        $defaults = [xml][IO.File]::ReadAllText((Join-Path $output 'ScreenCut.exe.config'), [Text.Encoding]::UTF8)
        $appSettings = $current.SelectSingleNode('/configuration/appSettings')
        if (!$appSettings) { $appSettings = $current.DocumentElement.AppendChild($current.CreateElement('appSettings')) }
        foreach ($entry in $defaults.configuration.appSettings.add) {
            if (!($appSettings.SelectNodes('add') | Where-Object { $_.key -eq $entry.key })) {
                $null = $appSettings.AppendChild($current.ImportNode($entry, $true))
            }
        }
        $settings = [Xml.XmlWriterSettings]::new()
        $settings.Encoding = [Text.UTF8Encoding]::new($false)
        $settings.Indent = $true
        $writer = [Xml.XmlWriter]::Create($deployedConfig, $settings)
        try { $current.Save($writer) } finally { $writer.Dispose() }
    }
    $exe = Join-Path $runtime 'ScreenCut.exe'
    Write-Host "Build verified: $exe"
    if (!$BuildOnly) { Start-Process -FilePath $exe -WorkingDirectory $runtime }
    exit 0
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
