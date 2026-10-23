$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$sdk = Join-Path $project '.tools/dotnet-sdk-9.0.315'
$archive = Join-Path $project '.tools/dotnet-sdk-9.0.315-win-x64.zip'
$expected = '5799c71270a56686e08b0f219b77a000f33790d92e0be0778c4c0923945dc551bf37e55e920dd8a527d32efd7554751b1073e02dc318d9f2274961ebd2c189fa'
if (!(Test-Path -LiteralPath (Join-Path $sdk 'dotnet.exe'))) {
    New-Item -ItemType Directory -Force -Path (Split-Path $archive -Parent) | Out-Null
    & curl.exe --fail --location --retry 4 --retry-all-errors --continue-at - --output $archive 'https://builds.dotnet.microsoft.com/dotnet/Sdk/9.0.315/dotnet-sdk-9.0.315-win-x64.zip'
    if ($LASTEXITCODE -ne 0) { throw "SDK 下载失败：$LASTEXITCODE" }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA512).Hash.ToLowerInvariant() -ne $expected) {
        throw 'SDK SHA-512 校验失败，请移除 .tools 中未通过校验的 ZIP 后重试。'
    }
    New-Item -ItemType Directory -Force -Path $sdk | Out-Null
    Expand-Archive -LiteralPath $archive -DestinationPath $sdk -Force
}
$dotnet = & (Join-Path $PSScriptRoot 'Get-ViDotnet.ps1')
& $dotnet --version
if ($LASTEXITCODE -ne 0) { throw 'SDK 不能运行。' }
