param([switch]$BuildOnly)
$ErrorActionPreference = 'Stop'
try {
    if (Get-Process -Name ImagingMaster -ErrorAction SilentlyContinue) { throw '请先关闭 ImagingMaster。' }
    $project = Join-Path $PSScriptRoot '工程/ImagingMaster/ImagingMaster.csproj'
    $stage = Join-Path $PSScriptRoot ('工程/ImagingMaster/bin/publish-'+[Guid]::NewGuid().ToString('N'))
    & dotnet publish $project -c Release --nologo --verbosity minimal -p:NuGetAudit=false --output $stage
    if ($LASTEXITCODE -ne 0) { throw "编译失败 ($LASTEXITCODE)，未启动。" }
    $runtime = Join-Path $PSScriptRoot '运行'
    New-Item -ItemType Directory -Path $runtime -Force | Out-Null
    Get-ChildItem -LiteralPath $stage -File | Copy-Item -Destination $runtime -Force
    foreach ($folder in @('Player','runtimes')) {
        $source = Join-Path $stage $folder
        if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $runtime -Recurse -Force }
    }
    $configDir = Join-Path $runtime 'config'
    New-Item -ItemType Directory -Path $configDir -Force | Out-Null
    foreach ($name in @('dsa_screencut.ini','hik_cameras.ini')) {
        $destination = Join-Path $configDir $name
        if (!(Test-Path -LiteralPath $destination)) {
            $source = Join-Path $PSScriptRoot ('工程/config/'+$name)
            if (!(Test-Path -LiteralPath $source)) { $source += '.example' }
            Copy-Item -LiteralPath $source -Destination $destination
        }
    }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../依赖/第三方组件说明.md') -Destination $runtime -Force
    Write-Host "已生成完整主端运行包：$runtime"
    if (!$BuildOnly) { Start-Process -FilePath (Join-Path $runtime 'ImagingMaster.exe') -WorkingDirectory $runtime -WindowStyle Normal }
    exit 0
} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }
