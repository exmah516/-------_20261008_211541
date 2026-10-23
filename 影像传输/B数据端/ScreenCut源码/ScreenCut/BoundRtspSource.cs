using System;
using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

namespace ScreenCut
{
    // 相机 RTSP/TCP 只在本机转接，以保留双网卡源地址绑定；跨机视频由 MediaMTX 以 SRTP 发送。
    public sealed class BoundRtspSource : IDisposable
    {
        private readonly TcpListener _listener = new TcpListener(IPAddress.Loopback, 0);
        private readonly CancellationTokenSource _stop = new CancellationTokenSource();
        private readonly ConcurrentDictionary<TcpClient, byte> _clients = new ConcurrentDictionary<TcpClient, byte>();
        private readonly SemaphoreSlim _slots = new SemaphoreSlim(2);
        private readonly CameraSettings _camera;
        private readonly Action<string> _log;
        public int Port => ((IPEndPoint)_listener.LocalEndpoint).Port;

        public BoundRtspSource(CameraSettings camera, Action<string> log)
        {
            _camera = camera; _log = log;
            _listener.Start(2);
            _ = AcceptAsync();
        }
        private async Task AcceptAsync()
        {
            while (!_stop.IsCancellationRequested)
            {
                TcpClient client;
                try { client = await _listener.AcceptTcpClientAsync().ConfigureAwait(false); }
                catch (Exception ex) when (ex is SocketException || ex is ObjectDisposedException) { break; }
                if (!await _slots.WaitAsync(0).ConfigureAwait(false)) { client.Close(); continue; }
                _clients.TryAdd(client, 0);
                _ = ForwardAsync(client);
            }
        }
        private async Task ForwardAsync(TcpClient downstream)
        {
            using (var upstream = new TcpClient(AddressFamily.InterNetwork))
            {
                _clients.TryAdd(upstream, 0);
                try
                {
                    upstream.Client.Bind(new IPEndPoint(IPAddress.Parse(_camera.LocalIp), 0));
                    upstream.NoDelay = downstream.NoDelay = true;
                    var connect = upstream.ConnectAsync(_camera.Ip, _camera.RtspPort);
                    if (await Task.WhenAny(connect, Task.Delay(3000, _stop.Token)).ConfigureAwait(false) != connect)
                        throw new TimeoutException("连接相机 RTSP 超时");
                    await connect.ConfigureAwait(false);
                    var a = downstream.GetStream().CopyToAsync(upstream.GetStream(), 16384, _stop.Token);
                    var b = upstream.GetStream().CopyToAsync(downstream.GetStream(), 16384, _stop.Token);
                    await Task.WhenAny(a, b).ConfigureAwait(false);
                    upstream.Close(); downstream.Close();
                    try { await Task.WhenAll(a, b).ConfigureAwait(false); } catch { }
                }
                catch (Exception ex) { if (!_stop.IsCancellationRequested) _log("相机 " + _camera.Ip + "：" + ex.Message); }
                finally
                {
                    downstream.Close();
                    _clients.TryRemove(downstream, out _); _clients.TryRemove(upstream, out _);
                    _slots.Release();
                }
            }
        }
        public void Dispose()
        {
            _stop.Cancel(); _listener.Stop();
            foreach (var client in _clients.Keys) client.Close();
        }
    }
}
