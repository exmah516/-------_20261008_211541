using System;
using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

// 只用于回环验证：给 SRTP/ICE UDP 加入 50–200ms 单程抖动和 1% 丢包，不修改系统网络。
internal sealed class WeakUdpProxy : IDisposable
{
    private readonly UdpClient _front;
    private readonly IPEndPoint _server;
    private readonly ConcurrentDictionary<string, UdpClient> _back = new ConcurrentDictionary<string, UdpClient>();
    private readonly CancellationTokenSource _stop = new CancellationTokenSource();
    private readonly Random _random = new Random(42);
    private readonly SemaphoreSlim _pending = new SemaphoreSlim(4096);
    public long Dropped, Forwarded;
    public volatile bool Paused;
    public WeakUdpProxy(int port)
    {
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
            lock (_random) { delay = _random.Next(50, 201); drop = _random.Next(100) == 0; }
            if (drop) { Interlocked.Increment(ref Dropped); return; }
            await Task.Delay(delay, _stop.Token);
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
