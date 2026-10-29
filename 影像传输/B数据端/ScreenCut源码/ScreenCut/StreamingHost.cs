using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Collections.Generic;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace ScreenCut
{
    public sealed class StreamingHost : IDisposable
    {
        private readonly StreamingSettings _settings;
        private readonly object _gate = new object();
        private readonly CancellationTokenSource _stop = new CancellationTokenSource();
        private readonly BoundRtspSource[] _sources = new BoundRtspSource[2];
        private readonly string _work;
        private Process _media, _encoder;
        private Rectangle? _region;
        private bool _testPattern, _started, _disposed;
        private readonly string[] _cameraWarnings = new string[2];
        public event Action<string> Status;
        public event Action<string> CameraWarning;
        public string LogPath => StreamingLog.LogPath;

        public StreamingHost(StreamingSettings settings)
        {
            _settings = settings;
            string root = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "VI.Imaging");
            _work = Path.Combine(root, "run-" + Guid.NewGuid().ToString("N"));
            var acl = new DirectorySecurity();
            acl.SetAccessRuleProtection(true, false);
            acl.AddAccessRule(new FileSystemAccessRule(WindowsIdentity.GetCurrent().User, FileSystemRights.FullControl,
                InheritanceFlags.ContainerInherit | InheritanceFlags.ObjectInherit, PropagationFlags.None, AccessControlType.Allow));
            Directory.CreateDirectory(_work, acl);
        }

        public async Task StartAsync()
        {
            lock (_gate)
            {
                if (_disposed) throw new ObjectDisposedException(nameof(StreamingHost));
                if (_started) return;
                _started = true;
                var ports = new int[2];
                for (int i = 0; i < 2; i++)
                    if (_settings.Cameras[i].Enabled)
                    {
                        var camera = _settings.Cameras[i];
                        Log("监控 " + (i + 1) + " 输入：" + camera.LocalIp + " → " + camera.Ip + ":" + camera.RtspPort + "，RTSP 通道 " + camera.Channel);
                        _sources[i] = new BoundRtspSource(camera, Log);
                        ports[i] = _sources[i].Port;
                    }
                File.WriteAllText(Path.Combine(_work, "mediamtx.yml"), _settings.MediaMtxConfig(ports), new UTF8Encoding(false));
                _media = StartChild("mediamtx.exe", "\"" + Path.Combine(_work, "mediamtx.yml") + "\"");
            }
            try
            {
                await WaitReadyAsync().ConfigureAwait(false);
                Log("媒体服务已启动；主端 " + _settings.MasterIp + "，信令 TCP " + _settings.WebRtcPort + "，视频 UDP/TCP " + _settings.MediaPort);
                _ = CheckCameraConnectivityAsync();
                _ = WatchAsync();
            }
            catch { Dispose(); throw; }
        }

        private async Task CheckCameraConnectivityAsync()
        {
            try
            {
                var candidates = new List<string>();
                foreach (var camera in _settings.Cameras)
                    if (camera.Enabled && !candidates.Contains(camera.LocalIp)) candidates.Add(camera.LocalIp);

                for (int i = 0; i < _settings.Cameras.Length; i++)
                {
                    if (_stop.IsCancellationRequested) return;
                    var camera = _settings.Cameras[i];
                    if (!camera.Enabled) continue;
                    bool configured = await ProbeCameraAsync(camera.Ip, camera.RtspPort, camera.LocalIp).ConfigureAwait(false);
                    if (_stop.IsCancellationRequested) return;
                    if (configured)
                    {
                        Log("监控 " + (i + 1) + " 网络检查通过：" + camera.LocalIp + " → " + camera.Ip + ":" + camera.RtspPort);
                        continue;
                    }

                    string alternate = null;
                    foreach (string candidate in candidates)
                        if (!candidate.Equals(camera.LocalIp, StringComparison.OrdinalIgnoreCase) &&
                            await ProbeCameraAsync(camera.Ip, camera.RtspPort, candidate).ConfigureAwait(false))
                        {
                            alternate = candidate;
                            break;
                        }
                    if (alternate != null)
                        _cameraWarnings[i] = "监控 " + (i + 1) + " 网口可能接反：" + camera.Ip + " 从 " + camera.LocalIp +
                            " 连接失败，改用 " + alternate + " 可达。请在 ScreenCut.exe.config 中设置 Camera" + (i + 1) + "LocalIp=" + alternate + "，然后重启。";
                    else
                        _cameraWarnings[i] = "监控 " + (i + 1) + " 连接失败：" + camera.LocalIp + " → " + camera.Ip + ":" + camera.RtspPort +
                            "。请检查相机 RTSP、网口和端口设置。";
                    if (_stop.IsCancellationRequested) return;
                    Log("警告：" + _cameraWarnings[i]);
                    CameraWarning?.Invoke(string.Join(Environment.NewLine, Array.FindAll(_cameraWarnings, warning => warning != null)));
                }
            }
            catch (Exception ex)
            {
                if (!_stop.IsCancellationRequested) Log("相机连通性检查失败：" + ex.Message);
            }
        }

        private static async Task<bool> ProbeCameraAsync(string cameraIp, int port, string localIp)
        {
            using (var client = new TcpClient(AddressFamily.InterNetwork))
            {
                try
                {
                    client.Client.Bind(new IPEndPoint(IPAddress.Parse(localIp), 0));
                    var connect = client.ConnectAsync(cameraIp, port);
                    if (await Task.WhenAny(connect, Task.Delay(3000)).ConfigureAwait(false) != connect) return false;
                    await connect.ConfigureAwait(false);
                    return true;
                }
                catch (SocketException) { return false; }
                catch (TimeoutException) { return false; }
                catch (ArgumentException) { return false; }
            }
        }

        private async Task WaitReadyAsync()
        {
            using (var http = new HttpClient(new HttpClientHandler { UseProxy = false }) { Timeout = TimeSpan.FromSeconds(1) })
            {
                for (int i = 0; i < 40; i++)
                {
                    _stop.Token.ThrowIfCancellationRequested();
                    lock (_gate) if (_media == null || _media.HasExited) throw new IOException("MediaMTX 启动失败，请查看 " + LogPath);
                    try
                    {
                        using (var reply = await http.GetAsync("http://127.0.0.1:" + _settings.ApiPort + "/v3/paths/list", _stop.Token).ConfigureAwait(false))
                            if (reply.IsSuccessStatusCode) return;
                    }
                    catch (HttpRequestException) { }
                    catch (TaskCanceledException) when (!_stop.IsCancellationRequested) { }
                    await Task.Delay(250, _stop.Token).ConfigureAwait(false);
                }
            }
            throw new TimeoutException("媒体服务未就绪；检查端口占用和数据端本地 IP。日志：" + LogPath);
        }

        public void StartDsa(Rectangle region, bool testPattern = false)
        {
            lock (_gate)
            {
                if (!_started || _disposed) throw new InvalidOperationException("媒体服务尚未启动。");
                StopChild(ref _encoder);
                _region = region; _testPattern = testPattern;
                _encoder = StartChild("ffmpeg.exe", _settings.FfmpegArguments(region, testPattern));
            }
            Log("DSA H.264 已启动：" + _settings.VideoWidth + "×" + _settings.VideoHeight + "，" + _settings.VideoFps + "fps，" + _settings.BitrateKbps + "kbps");
        }
        public void StopDsa() { lock (_gate) { _region = null; StopChild(ref _encoder); } }

        private async Task WatchAsync()
        {
            while (!_stop.IsCancellationRequested)
            {
                try
                {
                    await Task.Delay(3000, _stop.Token).ConfigureAwait(false);
                    bool restarted = false;
                    lock (_gate)
                    {
                        if (_disposed) return;
                        if (_media == null || _media.HasExited)
                        {
                            StopChild(ref _encoder); StopChild(ref _media);
                            _media = StartChild("mediamtx.exe", "\"" + Path.Combine(_work, "mediamtx.yml") + "\"");
                            restarted = true;
                        }
                    }
                    if (restarted) { await WaitReadyAsync().ConfigureAwait(false); Log("媒体服务已重新启动。"); }
                    lock (_gate)
                    {
                        if (_disposed) return;
                        if (_region.HasValue && (_encoder == null || _encoder.HasExited))
                        {
                            StopChild(ref _encoder);
                            _encoder = StartChild("ffmpeg.exe", _settings.FfmpegArguments(_region.Value, _testPattern));
                            Log("DSA 编码器重新连接中。");
                        }
                    }
                }
                catch (OperationCanceledException) { return; }
                catch (Exception ex) { Log("媒体管线：" + ex.Message); }
            }
        }

        private Process StartChild(string filename, string arguments)
        {
            var p = new Process { StartInfo = new ProcessStartInfo(StreamingSettings.FindRuntime(filename), arguments) {
                UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = _work,
                RedirectStandardError = true, RedirectStandardOutput = true
            } };
            p.ErrorDataReceived += (s, e) => { if (e.Data != null) Log("[" + filename + "] " + e.Data); };
            p.OutputDataReceived += (s, e) => { if (e.Data != null) Log("[" + filename + "] " + e.Data); };
            if (!p.Start()) throw new IOException("无法启动 " + filename);
            p.BeginErrorReadLine(); p.BeginOutputReadLine();
            return p;
        }
        public static string Redact(string line) => Regex.Replace(line, @"rtsp://[^\s/]*@", "rtsp://***@", RegexOptions.IgnoreCase);
        private void Log(string text)
        {
            text = StreamingLog.Write(text);
            Status?.Invoke(text);
        }
        private static void StopChild(ref Process process)
        {
            var p = process; process = null;
            if (p == null) return;
            try { if (!p.HasExited) { p.Kill(); p.WaitForExit(2000); } }
            catch (InvalidOperationException) { }
            finally { p.Dispose(); }
        }
        public void Dispose()
        {
            lock (_gate)
            {
                if (_disposed) return;
                _disposed = true; _stop.Cancel(); _region = null;
                StopChild(ref _encoder); StopChild(ref _media);
                foreach (var source in _sources) source?.Dispose();
                try { Directory.Delete(_work, true); } catch (IOException) { }
            }
        }
    }
}
