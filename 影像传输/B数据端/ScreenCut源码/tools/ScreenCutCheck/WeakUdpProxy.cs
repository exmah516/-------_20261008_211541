using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

// 只用于回环验证：所有媒体连接和测试探针共用瓶颈，不修改系统网络。
internal sealed class SharedBottleneck
{
    private readonly object _gate = new object();
    private readonly Stopwatch _clock = Stopwatch.StartNew();
    private readonly int _bitsPerSecond;
    private double _tailMs;
    public long Overflow, AcceptedBytes;
    public SharedBottleneck(int bitsPerSecond) { _bitsPerSecond = bitsPerSecond; }
    public int Reserve(int payloadBytes)
    {
        lock (_gate)
        {
            double now = _clock.Elapsed.TotalMilliseconds;
            // 每包另计 80 字节，近似 IP/UDP 与隧道开销；双向共同计入 2 Mbps。
            double end = Math.Max(now, _tailMs) + (payloadBytes + 80) * 8000.0 / _bitsPerSecond;
            if (end - now > 250) { Overflow++; return -1; }
            _tailMs = end; AcceptedBytes += payloadBytes + 80;
            return (int)Math.Ceiling(end - now);
        }
    }
}

internal sealed class WeakUdpProxy : IDisposable
{
    private readonly UdpClient _front;
    private readonly IPEndPoint _server;
    private readonly ConcurrentDictionary<string, UdpClient> _back = new ConcurrentDictionary<string, UdpClient>();
    private readonly CancellationTokenSource _stop = new CancellationTokenSource();
    private readonly Random _random = new Random(42);
    private readonly SemaphoreSlim _pending = new SemaphoreSlim(4096);
    private readonly SharedBottleneck _bandwidth;
    private readonly Stopwatch _clock = Stopwatch.StartNew();
    public long Dropped, Forwarded;
    public volatile bool Paused;
    public WeakUdpProxy(int port, SharedBottleneck bandwidth = null)
    {
        _bandwidth = bandwidth;
        _server = new IPEndPoint(IPAddress.Loopback, port);
        _front = new UdpClient(new IPEndPoint(IPAddress.Parse("127.0.0.2"), port));
        _ = ReceiveAsync();
    }
    private async Task ReceiveAsync()
    {
        try
        {
            while (!_stop.IsCancellationRequested)
            {
                var packet = await _front.ReceiveAsync();
                string key = packet.RemoteEndPoint.ToString();
                if (!_back.TryGetValue(key, out var socket))
                {
                    socket = new UdpClient(new IPEndPoint(IPAddress.Loopback, 0)); socket.Connect(_server);
                    _back[key] = socket; _ = ReturnAsync(socket, packet.RemoteEndPoint);
                }
                _ = SendAsync(socket, packet.Buffer, null);
            }
        }
        catch (Exception) when (_stop.IsCancellationRequested) { }
    }
    private async Task ReturnAsync(UdpClient socket, IPEndPoint endpoint)
    {
        try
        {
            while (!_stop.IsCancellationRequested)
            {
                var packet = await socket.ReceiveAsync();
                _ = SendAsync(_front, packet.Buffer, endpoint);
            }
        }
        catch (Exception) when (_stop.IsCancellationRequested) { }
    }
    private async Task SendAsync(UdpClient socket, byte[] data, IPEndPoint destination)
    {
        if (Paused) { Interlocked.Increment(ref Dropped); return; }
        if (!await _pending.WaitAsync(0)) { Interlocked.Increment(ref Dropped); return; }
        try
        {
            int delay; bool drop;
            lock (_random) {
                // 限带宽场景模拟整条链路的时延波动，保留独立测试的逐包剧烈乱序。
                delay = _bandwidth == null ? _random.Next(50, 201) :
                    (int)(125 + 75 * Math.Sin(_clock.Elapsed.TotalMilliseconds * 2 * Math.PI / 5000));
                drop = _random.Next(100) == 0;
            }
            int queue = _bandwidth?.Reserve(data.Length) ?? 0;
            if (queue < 0) { Interlocked.Increment(ref Dropped); return; }
            if (drop) { Interlocked.Increment(ref Dropped); return; }
            await Task.Delay(delay + queue, _stop.Token);
            if (destination == null) await socket.SendAsync(data, data.Length);
            else await socket.SendAsync(data, data.Length, destination);
            Interlocked.Increment(ref Forwarded);
        }
        catch (Exception) when (_stop.IsCancellationRequested) { }
        finally { _pending.Release(); }
    }
    public void Dispose()
    {
        _stop.Cancel(); _front.Close();
        foreach (var socket in _back.Values) socket.Close();
    }
}
