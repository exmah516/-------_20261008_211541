param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$BuildOnly,
    [switch]$LocalConsole
)
$ErrorActionPreference = 'Stop'
try {
    $token = Join-Path $PSScriptRoot 'config/remote.token'
    if (!(Test-Path -LiteralPath $token -PathType Leaf)) { throw 'Missing config/remote.token.' }
    if ([Text.Encoding]::UTF8.GetByteCount([IO.File]::ReadAllText($token).Trim()) -lt 16) {
        throw 'Invalid config/remote.token: at least 16 bytes required.'
    }
    $processNames = if ($LocalConsole) { @('AdsControlUI') } else { @('ADS', 'AdsControlUI') }
    if (Get-Process -Name $processNames -ErrorAction SilentlyContinue) {
        throw ('Close before building: ' + ($processNames -join ', '))
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio 2022 Build Tools required (MSBuild, v142, Windows SDK).' }
    $msbuild = @(& $vswhere -latest -products '*' -version '[17.0,)' -requires Microsoft.Component.MSBuild -find 'MSBuild/Current/Bin/MSBuild.exe') | Select-Object -First 1
    if (!$msbuild) { throw 'MSBuild not found. Install Visual Studio 2022 with the C++ v142 toolset.' }
    if ($LocalConsole) {
        & $msbuild (Join-Path $PSScriptRoot 'ADS项目/AdsControlUI/AdsControlUI.csproj') /restore /m /nologo /verbosity:minimal "/p:Configuration=$Configuration"
    }
    else {
        & $msbuild (Join-Path $PSScriptRoot 'ADS项目/ADS.sln') /restore /m /nologo /verbosity:minimal "/p:Configuration=$Configuration" /p:Platform=x64
    }
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE). Nothing was launched." }
    $runtime = Join-Path $PSScriptRoot "ADS项目/x64/$Configuration"
    if (!$LocalConsole) {
        foreach ($file in @('ADS.exe') + @(Get-Content -Encoding UTF8 -LiteralPath (Join-Path $PSScriptRoot 'tools/runtime-files.txt'))) {
            if (!(Test-Path -LiteralPath (Join-Path $runtime $file) -PathType Leaf)) {
                throw "Missing runtime file: $file"
            }
        }
    }
    $uiRuntime = Join-Path $PSScriptRoot "ADS项目/AdsControlUI/bin/$Configuration/net472"
    foreach ($file in @('AdsControlUI.exe', 'AdsControlUI.exe.config', 'ScottPlot.dll', 'ScottPlot.WPF.dll', 'System.Drawing.Common.dll', 'System.Numerics.Vectors.dll')) {
        if (!(Test-Path -LiteralPath (Join-Path $uiRuntime $file) -PathType Leaf)) {
            throw "Missing local console file: $file"
        }
    }
    $exe = Join-Path $runtime 'ADS.exe'
    if ($LocalConsole) {
        $runtime = $uiRuntime
        $exe = Join-Path $runtime 'AdsControlUI.exe'
    }
    Write-Host "Build verified: $exe"
    if (!$BuildOnly) {
        # ADS 是用户需要查看输出并操作的交互式控制台。
        Start-Process -FilePath $exe -WorkingDirectory $runtime -WindowStyle Normal
    }
    exit 0
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
