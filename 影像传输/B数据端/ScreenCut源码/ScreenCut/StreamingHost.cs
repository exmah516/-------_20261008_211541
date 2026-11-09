using Microsoft.Win32.SafeHandles;
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Collections.Generic;
using System.Security.Principal;
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
        private readonly bool[] _registered = new bool[2];
        private readonly bool _monitor;
        private readonly Mutex _instance;
        private readonly SharedRelay _relay;
        private SafeFileHandle _encoderJob;
        private Process _encoder;
        private uint _relayGeneration;
        private string _lastCameraState;
        private Rectangle? _region;
        private bool _testPattern, _started, _disposed;
        private readonly string[] _cameraWarnings = new string[2];
        public event Action<string> Status;
        public event Action<string> CameraWarning;
        public event Action<string> CameraState;
        public string LogPath => StreamingLog.LogPath;

        public StreamingHost(StreamingSettings settings, bool monitor = true)
        {
            _settings = settings;
            _monitor = monitor;
            bool created;
            using (var identity = WindowsIdentity.GetCurrent())
                _instance = new Mutex(false, @"Local\VI.Imaging." + identity.User.Value + "." + settings.ApiPort +
                    (monitor ? ".monitor" : ".capture"), out created);
            if (!created)
            {
                _instance.Dispose();
                throw new InvalidOperationException(monitor ? "监控软件已启动。" : "截图软件已启动。");
            }
            try { _relay = new SharedRelay(settings); }
            catch { _instance.Dispose(); throw; }
        }

        public async Task StartAsync()
        {
            try
            {
                await Task.Run(() => {
                    lock (_gate)
                    {
                        if (_disposed) throw new ObjectDisposedException(nameof(StreamingHost));
                        if (_started) return;
                        if (_monitor)
                            for (int i = 0; i < _sources.Length; i++)
                            {
                                var camera = _settings.Cameras[i];
                                if (!camera.Enabled) continue;
                                Log("监控 " + (i + 1) + " 输入：" + camera.LocalIp + " → " + camera.Ip + ":" + camera.RtspPort + "，RTSP 通道 " + camera.Channel);
                                _sources[i] = new BoundRtspSource(camera, Log);
                            }
                        EnsureRelay();
                        _started = true;
                        Log("媒体服务已启动；主端 " + _settings.MasterIp + "，信令 TCP " + _settings.WebRtcPort + "，视频 UDP/TCP " + _settings.MediaPort);
                        if (_monitor) _ = CheckCameraConnectivityAsync();
                        _ = WatchAsync();
                    }
                }).ConfigureAwait(false);
            }
            catch { Dispose(); throw; }
        }

        private void EnsureRelay()
        {
            uint generation = _relay.Ensure(_stop.Token);
            if (generation == _relayGeneration) return;
            if (_monitor)
                for (int i = 0; i < _sources.Length; i++)
                {
                    _relay.SetCamera(i + 1, _sources[i] == null ? null : _settings.Cameras[i].SourceUrl(_sources[i].Port), _stop.Token);
                    _registered[i] = _sources[i] != null;
                }
            if (_relayGeneration != 0)
            {
                StopChild(ref _encoder);
                Log("媒体服务已重新启动。");
            }
            _relayGeneration = generation;
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
                            " 连接失败，改用 " + alternate + " 可达。请在 CameraMonitor.exe.config 中设置 Camera" + (i + 1) + "LocalIp=" + alternate + "，然后重启。";
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

        public void StartDsa(Rectangle region, bool testPattern = false, StreamingSettings video = null)
        {
            lock (_gate)
            {
                if (!_started || _disposed) throw new InvalidOperationException("媒体服务尚未启动。");
                if (video != null) _settings.SetVideo(video.VideoWidth, video.VideoHeight, video.VideoFps, video.BitrateKbps);
                StopChild(ref _encoder);
                _region = null;
                _encoder = StartEncoder(region, testPattern);
                _region = region; _testPattern = testPattern;
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
                    lock (_gate)
                    {
                        if (_disposed) return;
                        EnsureRelay();
                        if (_monitor && CameraState != null)
                        {
                            string State(int i) => "监控 " + (i + 1) + "：" + (!_settings.Cameras[i].Enabled ? "已禁用" :
                                _relay.CameraReady(i + 1, _stop.Token) ? "正在转发" : "等待画面，请检查 RTSP、凭据和码流设置");
                            string text = State(0) + Environment.NewLine + State(1);
                            if (text != _lastCameraState) { _lastCameraState = text; CameraState.Invoke(StreamingLog.Write(text)); }
                        }
                        if (_region.HasValue && (_encoder == null || _encoder.HasExited))
                        {
                            StopChild(ref _encoder);
                            _encoder = StartEncoder(_region.Value, _testPattern);
                            Log("DSA 编码器重新连接中。");
                        }
                    }
                }
                catch (OperationCanceledException) when (_stop.IsCancellationRequested) { return; }
                catch (Exception ex) { Log("媒体管线：" + ex.Message); }
            }
        }

        private Process StartEncoder(Rectangle region, bool testPattern)
        {
            _encoderJob = _encoderJob ?? SharedRelay.CreateKillOnCloseJob();
            const string filename = "ffmpeg.exe";
            var p = new Process { StartInfo = new ProcessStartInfo(StreamingSettings.FindRuntime(filename), _settings.FfmpegArguments(region, testPattern)) {
                UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = _relay.WorkDirectory,
                RedirectStandardError = true, RedirectStandardOutput = true
            } };
            p.ErrorDataReceived += (s, e) => { if (e.Data != null) Log("[" + filename + "] " + e.Data); };
            p.OutputDataReceived += (s, e) => { if (e.Data != null) Log("[" + filename + "] " + e.Data); };
            try
            {
                if (!p.Start()) throw new IOException("无法启动 " + filename);
                SharedRelay.Attach(_encoderJob, p);
                p.BeginErrorReadLine(); p.BeginOutputReadLine();
                return p;
            }
            catch { StopChild(ref p); throw; }
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
            _stop.Cancel();
            lock (_gate)
            {
                if (_disposed) return;
                _disposed = true; _region = null;
                StopChild(ref _encoder);
                _encoderJob?.Dispose();
                foreach (var source in _sources) source?.Dispose();
                for (int i = 0; i < _registered.Length; i++)
                    if (_registered[i])
                        try { _relay.SetCamera(i + 1, null, CancellationToken.None); }
                        catch (Exception ex) { Log("关闭监控 " + (i + 1) + "：" + ex.Message); }
                _relay.Dispose();
                _instance.Dispose();
            }
        }
    }
}
