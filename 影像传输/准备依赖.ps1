param([switch]$Offline)
$ErrorActionPreference = 'Stop'
$dep = Join-Path $PSScriptRoot '依赖'
$runtime = Join-Path $dep 'runtime'
$feed = Join-Path $dep 'nuget'
New-Item -ItemType Directory -Path $dep,$runtime,$feed -Force | Out-Null
function Match-Hash([string]$Path,[string]$Hash) {
    return (Test-Path -LiteralPath $Path) -and ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -eq $Hash)
}
function Get-Pinned([string]$Name,[string]$Url,[string]$Hash) {
    $target = Join-Path $dep $Name
    if (Match-Hash $target $Hash) { return $target }
    if ($Offline) { throw "缺少或校验失败：$Name。联网运行 准备依赖.ps1 后再编译。" }
    $partial = $target + '.download'
    & curl.exe --fail --location --silent --show-error --retry 2 --continue-at - --connect-timeout 20 --max-time 1800 --output $partial $Url
    if ($LASTEXITCODE -ne 0) { throw "下载失败：$Name；重新运行可断点续传。" }
    if (!(Match-Hash $partial $Hash)) { throw "SHA256 不匹配：$Name；未使用下载文件。" }
    Move-Item -LiteralPath $partial -Destination $target -Force
    return $target
}
function Extract-One([string]$Archive,[string]$Leaf,[string]$Destination) {
    $zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        $entry = @($zip.Entries | Where-Object { $_.Name -eq $Leaf })
        if ($entry.Count -ne 1) { throw "压缩包中缺少唯一的 $Leaf" }
        $source = $entry[0].Open()
        $target = [IO.File]::Create($Destination)
        try { $source.CopyTo($target) } finally { $source.Dispose(); $target.Dispose() }
    } finally { $zip.Dispose() }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$mediaHash = '31bc3cc425a0278c7a5696c7ba309257235060157eb338eed20bb88dbe0faf87'
$ffHash = '3256173f3f8bffd7df12227c68adf68025edb1832273a9530688a7bb1ed8edec'
$probeHash = 'f0d36ecbbdd3bcfac3efa078c96c7271c2e68b3810595552ac3b7f17e9a65c52'
$webHash = '56f7f4b8bf9aee4b8efefbbdd4f67d5f74ebd1b100ed0806da71bf76af481aa9'
if (!(Match-Hash (Join-Path $runtime 'mediamtx.exe') $mediaHash) -or !(Test-Path -LiteralPath (Join-Path $runtime 'MediaMTX-LICENSE'))) {
    $zip = Get-Pinned 'mediamtx_v1.21.2_windows_amd64.zip' 'https://github.com/bluenviron/mediamtx/releases/download/v1.21.2/mediamtx_v1.21.2_windows_amd64.zip' 'a84224f5a490855684e62ba6fb71e80ea2a5e16aed61883bb58380290e93e467'
    Extract-One $zip 'mediamtx.exe' (Join-Path $runtime 'mediamtx.exe')
    Extract-One $zip 'LICENSE' (Join-Path $runtime 'MediaMTX-LICENSE')
}
if (!(Match-Hash (Join-Path $runtime 'ffmpeg.exe') $ffHash) -or !(Match-Hash (Join-Path $runtime 'ffprobe.exe') $probeHash) -or !(Test-Path -LiteralPath (Join-Path $runtime 'FFmpeg-LICENSE.txt'))) {
    $zip = Get-Pinned 'ffmpeg-release-essentials.zip' 'https://www.gyan.dev/ffmpeg/builds/packages/ffmpeg-9.0.2-essentials_build.zip' '60f467265b1e312373dbcd92200c2618a74850f98d3d078e94296bb3fa2047ba'
    Extract-One $zip 'ffmpeg.exe' (Join-Path $runtime 'ffmpeg.exe')
    Extract-One $zip 'ffprobe.exe' (Join-Path $runtime 'ffprobe.exe')
    Extract-One $zip 'LICENSE' (Join-Path $runtime 'FFmpeg-LICENSE.txt')
}
$webName = 'microsoft.web.webview2.1.0.4258.31.nupkg'
if (!(Match-Hash (Join-Path $feed $webName) $webHash)) {
    $package = Get-Pinned $webName 'https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.4258.31/microsoft.web.webview2.1.0.4258.31.nupkg' $webHash
    Copy-Item -LiteralPath $package -Destination (Join-Path $feed $webName) -Force
}
if (!(Match-Hash (Join-Path $runtime 'mediamtx.exe') $mediaHash) -or !(Match-Hash (Join-Path $runtime 'ffmpeg.exe') $ffHash) -or !(Match-Hash (Join-Path $runtime 'ffprobe.exe') $probeHash)) { throw '媒体运行库 SHA256 校验失败。' }
Write-Host '媒体运行库与 WebView2 SDK 均已就绪，SHA256 校验通过。'
