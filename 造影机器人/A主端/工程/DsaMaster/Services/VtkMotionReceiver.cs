using System;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using MasterConsole.Controls;

namespace MasterConsole.Services
{
    /// <summary>接收从端的显示测试帧，并交给主窗口已有的 RobotViewport。</summary>
    internal sealed class VtkMotionReceiver : IDisposable
    {
        private const int Port = 32110;
        private const int FrameBytes = 4 + 11 * 8;
        private const int Magic = 0x314B5456;
        private const int ReadTimeoutMs = 10000;
        private readonly IPAddress _peerAddress;
        private readonly TcpListener _listener;
        private readonly CancellationTokenSource _stop = new CancellationTokenSource();
        private readonly object _sync = new object();
        private Task _loop;
        private bool _started;
        private bool _disposed;

        internal event Func<DsaArmDisplayState, bool> StateReceived;
        internal event Action<string> StatusChanged;

        internal VtkMotionReceiver(string peerAddress = "192.168.60.1")
        {
            _peerAddress = IPAddress.Parse(peerAddress);
            _listener = new TcpListener(IPAddress.Any, Port);
        }

        internal void Start()
        {
            lock (_sync)
            {
                if (_started || _disposed) return;
                _started = true;
                try
                {
                    _listener.Start(1);
                    StatusChanged?.Invoke("VTK模型测试监听已启动：TCP 32110，等待从端 " + _peerAddress);
                    _loop = Task.Run(ReceiveLoop);
                }
                catch (Exception ex)
                {
                    StatusChanged?.Invoke("VTK模型测试监听启动失败：" + ex.GetType().Name);
                }
            }
        }

        private async Task ReceiveLoop()
        {
            while (!_stop.IsCancellationRequested)
            {
                TcpClient client = null;
                try
                {
                    client = await _listener.AcceptTcpClientAsync().ConfigureAwait(false);
                    if (_stop.IsCancellationRequested) break;
                    var remote = ((IPEndPoint)client.Client.RemoteEndPoint).Address;
                    if (!remote.Equals(_peerAddress) && !IPAddress.IsLoopback(remote))
                    {
                        StatusChanged?.Invoke("拒绝 VTK 测试连接：来源 " + remote);
                        client.Close();
                        continue;
                    }

                    client.NoDelay = true;
                    StatusChanged?.Invoke("VTK模型测试已连接：" + remote);
                    await HandleClient(client).ConfigureAwait(false);
                    StatusChanged?.Invoke("VTK模型测试连接已断开");
                }
                catch (ObjectDisposedException) { break; }
                catch (SocketException) { if (!_stop.IsCancellationRequested) StatusChanged?.Invoke("VTK模型测试监听异常"); }
                catch (Exception ex) { if (!_stop.IsCancellationRequested) StatusChanged?.Invoke("VTK模型测试接收失败：" + ex.GetType().Name); }
                finally
                {
                    try { client?.Close(); } catch { }
                }
            }
        }

        private async Task HandleClient(TcpClient client)
        {
            var stream = client.GetStream();
            var frame = new byte[FrameBytes];
            var movedPositive = new bool[9];
            var movedNegative = new bool[9];
            while (!_stop.IsCancellationRequested)
            {
                if (!await ReadFrame(stream, frame).ConfigureAwait(false)) return;
                if (!TryDecode(frame, out DsaArmDisplayState state, out double[] values, out int active))
                {
                    stream.WriteByte(0);
                    continue;
                }

                if (active >= 0)
                {
                    if (values[active] > 1e-9) movedPositive[active] = true;
                    if (values[active] < -1e-9) movedNegative[active] = true;
                }
                bool accepted = StateReceived?.Invoke(state) == true;
                if (!accepted)
                {
                    stream.WriteByte(0);
                    continue;
                }

                bool zero = active < 0;
                bool complete = zero;
                for (int i = 0; i < 9; i++) complete &= movedPositive[i] && movedNegative[i];
                stream.WriteByte(complete ? (byte)2 : (byte)1);
                if (complete) return;
            }
        }

        private async Task<bool> ReadFrame(NetworkStream stream, byte[] frame)
        {
            int offset = 0;
            while (offset < frame.Length && !_stop.IsCancellationRequested)
            {
                Task<int> read = stream.ReadAsync(frame, offset, frame.Length - offset);
                Task timeout = Task.Delay(ReadTimeoutMs, _stop.Token);
                Task completed = await Task.WhenAny(read, timeout).ConfigureAwait(false);
                if (completed != read) return false;
                int count = await read.ConfigureAwait(false);
                if (count == 0) return false;
                offset += count;
            }
            return offset == frame.Length;
        }

        private static bool TryDecode(byte[] frame, out DsaArmDisplayState state, out double[] values, out int active)
        {
            state = null;
            values = new double[11];
            active = -1;
            if (BitConverter.ToInt32(frame, 0) != Magic) return false;
            for (int i = 0; i < values.Length; i++)
            {
                values[i] = BitConverter.ToDouble(frame, 4 + i * 8);
                double limit = i == 0 || i == 1 || i == 5 ? 0.1 : 0.5;
                if (double.IsNaN(values[i]) || double.IsInfinity(values[i]) || Math.Abs(values[i]) > limit) return false;
                if (Math.Abs(values[i]) > 1e-9)
                {
                    if (active >= 0) return false;
                    active = i;
                }
            }
            var sj = new double[7];
            var xj = new double[2];
            Array.Copy(values, sj, 7);
            Array.Copy(values, 7, xj, 0, 2);
            state = new DsaArmDisplayState(sj, xj, values[9], values[10]);
            return true;
        }

        public void Dispose()
        {
            lock (_sync)
            {
                if (_disposed) return;
                _disposed = true;
                _stop.Cancel();
                try { _listener.Stop(); } catch { }
                StatusChanged?.Invoke("VTK模型测试监听已停止");
            }
        }
    }
}
