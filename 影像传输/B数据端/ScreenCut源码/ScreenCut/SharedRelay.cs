using Microsoft.Win32.SafeHandles;
using System;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using VI.Imaging;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text;
using System.Threading;

namespace ScreenCut
{
    // Each app holds one handle; Windows stops MediaMTX when the last handle closes.
    internal sealed class SharedRelay : IDisposable
    {
        private readonly SafeFileHandle _job;
        private readonly Mutex _gate;
        private readonly string _configPath, _config;
        private readonly HttpClient _http;
        private bool _disposed;
        public string WorkDirectory { get; }

        public SharedRelay(StreamingSettings settings)
        {
            using (var identity = WindowsIdentity.GetCurrent())
            {
                string key = @"Local\VI.Imaging." + identity.User.Value + "." + settings.ApiPort;
                WorkDirectory = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "VI.Imaging", "relay-" + settings.ApiPort);
                var acl = new DirectorySecurity();
                acl.SetAccessRuleProtection(true, false);
                acl.AddAccessRule(new FileSystemAccessRule(identity.User, FileSystemRights.FullControl,
                    InheritanceFlags.ContainerInherit | InheritanceFlags.ObjectInherit, PropagationFlags.None, AccessControlType.Allow));
                Directory.CreateDirectory(WorkDirectory, acl);
                _gate = new Mutex(false, key + ".start");
                try { _job = ProcessJob.CreateKillOnCloseJob(key + ".relay"); }
                catch { _gate.Dispose(); throw; }
            }
            _configPath = Path.Combine(WorkDirectory, "mediamtx.yml");
            _config = settings.MediaMtxConfig(new int[2], shared: true);
            _http = new HttpClient(new HttpClientHandler { UseProxy = false }) {
                BaseAddress = new Uri("http://127.0.0.1:" + settings.ApiPort), Timeout = TimeSpan.FromSeconds(2)
            };
        }

        public uint Ensure(CancellationToken token)
        {
            try { _gate.WaitOne(); } catch (AbandonedMutexException) { }
            try
            {
                token.ThrowIfCancellationRequested();
                var state = State();
                if (state.Active == 0)
                {
                    File.WriteAllText(_configPath, _config, new UTF8Encoding(false));
                    using (var media = Process.Start(new ProcessStartInfo(StreamingSettings.FindRuntime("mediamtx.exe"), "\"" + _configPath + "\"") {
                        UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = WorkDirectory
                    }))
                    {
                        ProcessJob.Attach(_job, media);
                    }
                    state = State();
                }
                else if (File.ReadAllText(_configPath) != _config)
                    throw new IOException("本机该服务端口已被不同设置使用。请检查本软件 streaming.config 的 IP 和端口，关闭占用程序后重启。");

                for (int i = 0; i < 20; i++)
                {
                    token.ThrowIfCancellationRequested();
                    if (State().Active == 0) break;
                    try
                    {
                        using (var result = _http.GetAsync("/v3/info", token).GetAwaiter().GetResult())
                            if (result.IsSuccessStatusCode && State().Active != 0) return state.Total;
                    }
                    catch (HttpRequestException) { }
                    catch (OperationCanceledException) when (!token.IsCancellationRequested) { }
                    token.WaitHandle.WaitOne(250);
                }
                throw new IOException("媒体服务未就绪。请确认已关闭旧版 ScreenCut，并检查本地 IP 和端口是否被占用。");
            }
            finally { _gate.ReleaseMutex(); }
        }

        public void SetCamera(int index, string source, CancellationToken token)
        {
            // The URI is already percent encoded; serialize even so the request always has valid JSON.
            string json = new System.Web.Script.Serialization.JavaScriptSerializer().Serialize(new {
                source = source ?? "publisher", rtspTransport = "tcp", sourceOnDemand = false
            });
            using (var body = new StringContent(json, Encoding.UTF8, "application/json"))
            using (var response = _http.PostAsync("/v3/config/paths/replace/stream_cam" + index, body, token).GetAwaiter().GetResult())
                response.EnsureSuccessStatusCode();
        }

        public bool CameraReady(int index, CancellationToken token)
        {
            using (var response = _http.GetAsync("/v3/paths/get/stream_cam" + index, token).GetAwaiter().GetResult())
            {
                if (response.StatusCode == System.Net.HttpStatusCode.NotFound) return false;
                response.EnsureSuccessStatusCode();
                var path = new System.Web.Script.Serialization.JavaScriptSerializer().Deserialize<System.Collections.Generic.Dictionary<string, object>>(
                    response.Content.ReadAsStringAsync().GetAwaiter().GetResult());
                return path.TryGetValue("ready", out var ready) && Equals(ready, true);
            }
        }

        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;
            _job.Dispose(); _gate.Dispose(); _http.Dispose();
        }

        private ProcessJob.Accounting State() => ProcessJob.State(_job);
    }
}
