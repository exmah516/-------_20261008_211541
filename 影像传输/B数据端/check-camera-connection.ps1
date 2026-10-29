param([string]$ConfigPath, [switch]$SelfTest)
chcp 65001 > $null
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$OutputEncoding = [Console]::OutputEncoding
$ErrorActionPreference = 'Stop'

function Test-CameraTcp([string]$TargetIp, [int]$Port, [string]$LocalIp = '', [int]$InterfaceIndex = 0) {
    $client = [Net.Sockets.TcpClient]::new([Net.Sockets.AddressFamily]::InterNetwork)
    try {
        if ($LocalIp) { $client.Client.Bind([Net.IPEndPoint]::new([Net.IPAddress]::Parse($LocalIp), 0)) }
        if ($InterfaceIndex -gt 0) {
            # 只限定本次探测的出网卡，不修改系统网卡和路由。
            $client.Client.SetSocketOption([Net.Sockets.SocketOptionLevel]::IP, [Net.Sockets.SocketOptionName]31,
                [int][Net.IPAddress]::HostToNetworkOrder([int]$InterfaceIndex))
        }
        $connect = $client.ConnectAsync($TargetIp, $Port)
        if (!$connect.Wait(3000)) { throw [TimeoutException]::new('TCP 建连超过 3 秒') }
        return [pscustomobject]@{ Success = $true; Detail = '成功，本地端点 ' + $client.Client.LocalEndPoint }
    } catch {
        $cause = $_.Exception.GetBaseException()
        $detail = $cause.Message
        if ($cause -is [Net.Sockets.SocketException]) { $detail += '；SocketError=' + $cause.SocketErrorCode }
        return [pscustomobject]@{ Success = $false; Detail = '失败，' + $detail }
    } finally { $client.Dispose() }
}

if ($SelfTest) {
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
    $listener.Start()
    $port = $listener.LocalEndpoint.Port
    try {
        $bound = Test-CameraTcp '127.0.0.1' $port '127.0.0.2'
        if (!$bound.Success -or !$bound.Detail.Contains('127.0.0.2:')) { throw '指定源地址探测自检失败' }
        $loopback = [Net.NetworkInformation.NetworkInterface]::GetAllNetworkInterfaces() | Where-Object { $_.NetworkInterfaceType -eq 'Loopback' } | Select-Object -First 1
        $pinned = Test-CameraTcp '127.0.0.1' $port '127.0.0.1' $loopback.GetIPProperties().GetIPv4Properties().Index
        if (!$pinned.Success) { throw ('限定网卡探测自检失败：' + $pinned.Detail) }
    } finally { $listener.Stop() }
    $closed = Test-CameraTcp '127.0.0.1' $port
    if ($closed.Success) { throw '关闭端口未被识别' }
    Write-Output 'PASS 指定源地址、限定网卡和关闭端口三项自检。'
    exit 0
}

if (!$ConfigPath) {
    $ConfigPath = Join-Path $PSScriptRoot 'ScreenCut.exe.config'
    if (!(Test-Path -LiteralPath $ConfigPath)) { $ConfigPath = Join-Path $PSScriptRoot 'ScreenCut/ScreenCut.exe.config' }
}
$settings = [xml][IO.File]::ReadAllText($ConfigPath, [Text.Encoding]::UTF8)
function Read-Setting([string]$Key, [string]$Fallback) {
    $entry = $settings.configuration.appSettings.add | Where-Object { $_.key -eq $Key } | Select-Object -First 1
    if ($entry) { return [string]$entry.value }
    return $Fallback
}
$records = [Collections.Generic.List[string]]::new()
function Write-Record([string]$Text) { $records.Add($Text); Write-Output $Text }
Write-Record ('相机连接检查 ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
Write-Record ('配置：' + [IO.Path]::GetFullPath($ConfigPath))
$interfaces = @([Net.NetworkInformation.NetworkInterface]::GetAllNetworkInterfaces())
foreach ($nic in $interfaces) {
    foreach ($address in $nic.GetIPProperties().UnicastAddresses) {
        if ($address.Address.AddressFamily -eq 'InterNetwork') {
            Write-Record ('网口：' + $nic.Name + '，状态=' + $nic.OperationalStatus + '，IP=' + $address.Address + '，掩码=' + $address.IPv4Mask)
        }
    }
}
for ($i = 1; $i -le 2; $i++) {
    $prefix = 'Camera' + $i
    if ((Read-Setting ($prefix + 'Enabled') 'true') -eq 'false') { continue }
    $cameraIp = Read-Setting ($prefix + 'Ip') (@('192.168.1.64','192.168.1.32')[$i-1])
    $localIp = Read-Setting ($prefix + 'LocalIp') (@('192.168.1.97','192.168.1.96')[$i-1])
    $rtspPort = [int](Read-Setting ($prefix + 'RtspPort') '554')
    Write-Record ('相机' + $i + '：' + $localIp + ' → ' + $cameraIp + ':' + $rtspPort)
    $bound = Test-CameraTcp $cameraIp $rtspPort $localIp
    Write-Record ('  按程序指定源地址：' + $bound.Detail)
    $automatic = Test-CameraTcp $cameraIp $rtspPort
    Write-Record ('  系统自动选路：' + $automatic.Detail)
    $available = @()
    $candidates = @((Read-Setting 'Camera1LocalIp' '192.168.1.97'), (Read-Setting 'Camera2LocalIp' '192.168.1.96')) | Select-Object -Unique
    foreach ($candidate in $candidates) {
        $nic = $interfaces | Where-Object { @($_.GetIPProperties().UnicastAddresses.Address.IPAddressToString) -contains $candidate } | Select-Object -First 1
        if (!$nic) { Write-Record ('  本地地址不存在：' + $candidate); continue }
        $pinned = Test-CameraTcp $cameraIp $rtspPort $candidate ($nic.GetIPProperties().GetIPv4Properties().Index)
        Write-Record ('  限定网口 ' + $candidate + '（' + $nic.Name + '）：' + $pinned.Detail)
        if ($pinned.Success) { $available += $candidate }
        if ($pinned.Success -and $candidate -eq $localIp -and !$bound.Success) { Write-Record '  结论：同一地址限定出网卡后恢复，存在多网卡选路差异。' }
    }
    if ($available.Count -eq 1) { Write-Record ('  可达映射：' + $prefix + 'LocalIp=' + $available[0]) }
    if (!$bound.Success -and $automatic.Success) { Write-Record '  系统选路可用，优先核对 LocalIp 与相机物理接线。' }
    if (!$bound.Success -and !$automatic.Success -and $available.Count -eq 0) {
        $sdk = Test-CameraTcp $cameraIp ([int](Read-Setting ($prefix + 'Port') '8000'))
        Write-Record ('  iVMS/SDK 端口对照：' + $sdk.Detail)
        if ($sdk.Success) { Write-Record '  SDK 可达而 RTSP 不可达，核对相机实际 RTSP 端口、RTSP 是否启用和端口放行。' }
    }
}
$report = Join-Path $PSScriptRoot 'camera-connectivity.log'
[IO.File]::WriteAllLines($report, $records, [Text.UTF8Encoding]::new($false))
Write-Output ('检查完成，结果：' + $report)
