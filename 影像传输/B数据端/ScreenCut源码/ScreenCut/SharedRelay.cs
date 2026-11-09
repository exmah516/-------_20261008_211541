using Microsoft.Win32.SafeHandles;
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Runtime.InteropServices;
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
                try { _job = CreateKillOnCloseJob(key + ".relay"); }
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
                        Attach(_job, media);
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

        private Accounting State()
        {
            if (!QueryInformationJobObject(_job, 1, out var state, Marshal.SizeOf<Accounting>(), IntPtr.Zero)) throw new Win32Exception();
            return state;
        }

        internal static SafeFileHandle CreateKillOnCloseJob(string name = null)
        {
            var job = CreateJobObject(IntPtr.Zero, name);
            try
            {
                if (job.IsInvalid) throw new Win32Exception();
                var limits = new ExtendedLimits { Basic = new BasicLimits { Flags = 0x2000 } };
                if (!SetInformationJobObject(job, 9, ref limits, Marshal.SizeOf<ExtendedLimits>())) throw new Win32Exception();
                return job;
            }
            catch { job.Dispose(); throw; }
        }

        internal static void Attach(SafeFileHandle job, Process process)
        {
            if (AssignProcessToJobObject(job, process.Handle)) return;
            var error = new Win32Exception();
            if (!process.HasExited) process.Kill();
            throw error;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct BasicLimits
        {
            public long ProcessTime, JobTime;
            public uint Flags;
            public UIntPtr MinWorkingSet, MaxWorkingSet;
            public uint ActiveProcessLimit;
            public UIntPtr Affinity;
            public uint PriorityClass, SchedulingClass;
        }
        [StructLayout(LayoutKind.Sequential)]
        private struct ExtendedLimits
        {
            public BasicLimits Basic;
            public ulong ReadOps, WriteOps, OtherOps, ReadBytes, WriteBytes, OtherBytes;
            public UIntPtr ProcessMemory, JobMemory, PeakProcessMemory, PeakJobMemory;
        }
        [StructLayout(LayoutKind.Sequential)]
        private struct Accounting
        {
            public long UserTime, KernelTime, PeriodUserTime, PeriodKernelTime;
            public uint PageFaults, Total, Active, Terminated;
        }
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern SafeFileHandle CreateJobObject(IntPtr attributes, string name);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool SetInformationJobObject(SafeFileHandle job, int kind, ref ExtendedLimits limits, int size);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool QueryInformationJobObject(SafeFileHandle job, int kind, out Accounting state, int size, IntPtr returnLength);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool AssignProcessToJobObject(SafeFileHandle job, IntPtr process);
    }
}
