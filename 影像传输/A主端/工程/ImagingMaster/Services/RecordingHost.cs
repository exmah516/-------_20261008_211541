using Microsoft.Win32.SafeHandles;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using VI.Imaging;

namespace MasterConsole.Services
{
    public sealed class RecordingHost
    {
        private readonly RelaySettings _settings;
        private readonly CancellationTokenSource _stop = new CancellationTokenSource();
        private readonly HttpClient _http;
        private readonly SafeFileHandle _job;
        private readonly long[] _sizes = new long[3];
        private readonly string _configPath;
        private Process _media;
        private Task _watch = Task.CompletedTask;
        public string SessionDirectory { get; }
        public event Action<string> Status;

        public RecordingHost(RelaySettings settings)
        {
            _settings = settings;
            SessionDirectory = Path.Combine(settings.RecordingRoot, DateTime.Now.ToString("yyyy-MM-dd_HH-mm-ss-fff", CultureInfo.InvariantCulture));
            Directory.CreateDirectory(SessionDirectory);
            _configPath = Path.Combine(SessionDirectory, "mediamtx.yml");
            File.WriteAllText(_configPath, settings.MediaMtxConfig(SessionDirectory), new UTF8Encoding(false));
            _http = new HttpClient(new HttpClientHandler { UseProxy = false }) {
                BaseAddress = new Uri("http://127.0.0.1:" + settings.ApiPort), Timeout = TimeSpan.FromSeconds(2)
            };
            _job = ProcessJob.CreateKillOnCloseJob();
        }

        public void Start()
        {
            EnsureRelay();
            Status?.Invoke("未录像：等待发送端画面；会话目录：" + SessionDirectory);
            _watch = WatchAsync();
        }

        private void StartRelay()
        {
            _media?.Dispose();
            string exe = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "runtime", "mediamtx.exe");
            _media = ConsoleRelay.Start(exe, _configPath, _job);
        }

        private void EnsureRelay()
        {
            _stop.Token.ThrowIfCancellationRequested();
            if (_media == null || _media.HasExited)
                StartRelay();
            // 沿用发送端的 API 就绪检查和三秒进程巡检。
            for (int i = 0; i < 20; i++)
            {
                _stop.Token.ThrowIfCancellationRequested();
                if (_media.HasExited) break;
                try
                {
                    using (var result = _http.GetAsync("/v3/info", _stop.Token).GetAwaiter().GetResult())
                        if (result.IsSuccessStatusCode && !_media.HasExited) return;
                }
                catch (HttpRequestException) { }
                catch (OperationCanceledException) when (!_stop.IsCancellationRequested) { }
                _stop.Token.WaitHandle.WaitOne(250);
            }
            throw new IOException("主端媒体服务未就绪，请检查本机端口和 " + Path.Combine(SessionDirectory, "mediamtx.log"));
        }

        private async Task WatchAsync()
        {
            while (!_stop.IsCancellationRequested)
            {
                try
                {
                    await Task.Delay(3000, _stop.Token).ConfigureAwait(false);
                    EnsureRelay();
                    UpdateStatus();
                }
                catch (OperationCanceledException) when (_stop.IsCancellationRequested) { return; }
                catch (Exception ex) { Status?.Invoke("未录像：" + ex.Message); }
            }
        }

        private void UpdateStatus()
        {
            var waiting = new List<string>();
            int active = 0;
            for (int i = 0; i < StreamSettings.Paths.Length; i++)
            {
                string path = Path.Combine(SessionDirectory, StreamSettings.Paths[i]);
                long size = Directory.Exists(path) ? Directory.GetFiles(path, "*.mp4").Sum(f => {
                    using (var file = new FileStream(f, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                        return file.Length;
                }) : 0;
                if (size > _sizes[i]) active++;
                else waiting.Add(_settings.Sources[i].caption);
                _sizes[i] = size;
            }
            string text = active > 0 ? "录像中：" + SessionDirectory : "未录像：三路均无新视频写入";
            if (waiting.Count > 0)
            {
                string logPath = Path.Combine(SessionDirectory, "mediamtx.log");
                string error = null;
                if (File.Exists(logPath))
                    using (var reader = new StreamReader(new FileStream(logPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete)))
                    {
                        string line;
                        while ((line = reader.ReadLine()) != null)
                            if (line.Contains(" ERR ")) error = line;
                    }
                text += "（" + string.Join("、", waiting) + " 等待视频；" + (error ?? "发送端未供图或写入暂停，详情见 mediamtx.log") + "）";
            }
            Status?.Invoke(text);
        }

        public async Task StopAsync()
        {
            _stop.Cancel();
            await _watch.ConfigureAwait(false);
            try
            {
                if (_media != null && !_media.HasExited)
                    await Task.Run(() => ConsoleRelay.Stop(_media)).ConfigureAwait(false);
            }
            finally
            {
                _media?.Dispose();
                _job.Dispose();
                _http.Dispose();
                _stop.Dispose();
            }
        }
    }

    internal static class ConsoleRelay
    {
        internal static Process Start(string exe, string config, SafeFileHandle job)
        {
            var startup = new StartupInfo { Size = Marshal.SizeOf<StartupInfo>(), Flags = 1, ShowWindow = 0 };
            // 独立隐藏控制台接收 Ctrl+C；先挂作业再运行，主端被强制结束也不会留下中继。
            if (!CreateProcess(exe, new StringBuilder("\"" + exe + "\" \"" + config + "\""), IntPtr.Zero, IntPtr.Zero,
                false, 0x10 | 0x4, IntPtr.Zero, Path.GetDirectoryName(config), ref startup, out var info))
                throw new Win32Exception();
            var process = Process.GetProcessById(info.ProcessId);
            try
            {
                ProcessJob.Attach(job, process);
                if (ResumeThread(info.Thread) == uint.MaxValue) throw new Win32Exception();
                return process;
            }
            catch
            {
                if (!process.HasExited) process.Kill();
                process.Dispose();
                throw;
            }
            finally { CloseHandle(info.Thread); CloseHandle(info.Process); }
        }

        internal static void Stop(Process process)
        {
            FreeConsole();
            if (!AttachConsole((uint)process.Id)) throw new Win32Exception();
            try
            {
                if (!SetConsoleCtrlHandler(IntPtr.Zero, true)) throw new Win32Exception();
                if (!GenerateConsoleCtrlEvent(0, 0)) throw new Win32Exception();
                process.WaitForExit();
            }
            finally { SetConsoleCtrlHandler(IntPtr.Zero, false); FreeConsole(); }
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct StartupInfo
        {
            public int Size;
            public string Reserved, Desktop, Title;
            public int X, Y, XSize, YSize, XCountChars, YCountChars, FillAttribute, Flags;
            public short ShowWindow, ReservedSize;
            public IntPtr ReservedBytes, StdInput, StdOutput, StdError;
        }
        [StructLayout(LayoutKind.Sequential)]
        private struct ProcessInfo
        {
            public IntPtr Process, Thread;
            public int ProcessId, ThreadId;
        }
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool CreateProcess(string application, StringBuilder command, IntPtr processAttributes, IntPtr threadAttributes,
            bool inheritHandles, uint flags, IntPtr environment, string directory, ref StartupInfo startup, out ProcessInfo info);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern uint ResumeThread(IntPtr thread);
        [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr handle);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool AttachConsole(uint processId);
        [DllImport("kernel32.dll")] private static extern bool FreeConsole();
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool GenerateConsoleCtrlEvent(uint kind, uint group);
    }
}
