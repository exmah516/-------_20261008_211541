using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Threading;
using MasterConsole.Protocol;

namespace MasterConsole.Services
{
    /// <summary>监管独占 SDK 的宿主；网络和界面只读取主端缓存。</summary>
    public sealed class HandleService : IHandleService
    {
        public const uint SerialA = 582, SerialB = 587;
        private readonly object _lock = new object();
        private readonly object _lifecycleLock = new object();
        private readonly Func<string, Process> _launch;
        private readonly bool _deviceNotifications;
        private Thread _thread;
        private volatile bool _stop = true, _sdkMissing, _restartRequested;
        private bool _disposed;
        private HostSamples _samples;
        private HostForces _forces;
        private long _sentSequenceA, _sentSequenceB;
        private string _status = "";
        internal int RestartCount { get; private set; }

        public HandleService() : this(Launch, true) { }
        internal HandleService(Func<string, Process> launch, bool deviceNotifications)
        { _launch = launch; _deviceNotifications = deviceNotifications; }
        private static Process Launch(string name)
        {
            string executable = Path.Combine(AppContext.BaseDirectory, "HandleHost", "VI.HandleHost.exe");
            var info = new ProcessStartInfo(executable) { UseShellExecute = false, CreateNoWindow = true };
            info.ArgumentList.Add(name);
            return Process.Start(info);
        }
        public bool SdkMissing => _sdkMissing;
        public string RecoveryText { get { lock (_lock) return _status; } }
        internal bool IsRunning => _thread != null && _thread.IsAlive;
        public void Start()
        {
            lock (_lifecycleLock)
            {
                if (_disposed) throw new ObjectDisposedException(nameof(HandleService));
                if (IsRunning) return;
                _stop = false;
                _thread = new Thread(Run) { IsBackground = true, Name = "HandleHost-Supervisor" };
                _thread.Start();
            }
        }
        public void Stop()
        {
            lock (_lifecycleLock)
            {
                _stop = true;
                ClearHaptic();
                _thread?.Join();
                _thread = null;
                lock (_lock) _samples = default;
            }
        }
        public bool IsOnline(int slot)
        { lock (_lock) return !_stop && (slot == 0 ? _samples.A.Valid : _samples.B.Valid) != 0; }
        public HandleSample GetSample(int slot)
        {
            lock (_lock)
            {
                HostSample value = slot == 0 ? _samples.A : _samples.B;
                var result = value.Sample;
                if (_stop || value.Tick == 0) result.Valid = false;
                long sent = slot == 0 ? _sentSequenceA : _sentSequenceB;
                if (value.Sequence == sent) result.Valid = false;
                if (slot == 0) _sentSequenceA = value.Sequence; else _sentSequenceB = value.Sequence;
                return result;
            }
        }
        public void SetHaptic(int slot, HapticOut output)
        {
            lock (_lock)
            {
                if (_stop || _restartRequested) return;
                var force = new HostForce { Enable = output.Enable ? 1 : 0, Axis = Math.Clamp((int)output.Axis, 0, 2),
                    Force = Finite(output.ForceN), Torque = Finite(output.TorqueNm), Tick = Environment.TickCount64 };
                if (slot == 0) _forces.A = force; else _forces.B = force;
            }
        }
        public void ClearHaptic() { lock (_lock) _forces = default; }
        internal static bool IsHapticFresh(long now, long receivedMs)
            => receivedMs >= 0 && now >= receivedMs && now - receivedMs < ProtocolConstants.HapticTimeoutMs;
        internal void DeviceChanged(bool arrived)
        {
            lock (_lock)
            {
                _samples = default;
                _forces = default;
                _status = arrived ? "手柄宿主重启中：USB 重新插入" : "手柄宿主重启中：USB 拔出，两柄暂停";
                _restartRequested = true;
            }
        }
        private void Run()
        {
            using var notifications = _deviceNotifications ? new HandleDeviceNotifications(DeviceChanged) : null;
            Process process = null;
            HandleHostMemory memory = null;
            long lastStart = -1000;
            try
            {
                while (!_stop)
                {
                    long now = Environment.TickCount64;
                    if (memory != null && memory.Error != 0)
                    {
                        _sdkMissing = true;
                        lock (_lock) _status = "手柄宿主重启中：SDK 缺失或入口缺失（" + memory.Error + "）";
                    }
                    if (process != null && (process.HasExited || _restartRequested || now - Math.Max(lastStart, memory.Heartbeat) > 1000))
                    {
                        lock (_lock)
                        {
                            _samples = default; _forces = default;
                            if (!_sdkMissing && !_restartRequested) _status = "手柄宿主重启中：" + (process.HasExited ? "进程退出" : "心跳中断");
                            memory.Write(HandleHostMemory.ForceOffset, _forces);
                        }
                        if (!process.HasExited && _restartRequested)
                        {
                            // 拔插先让宿主沿既有退出路径清零两路原生力；阻塞时仍按既有1s退出规则结束进程。
                            memory.Stop = true;
                            if (!process.WaitForExit(1000)) process.Kill(true);
                        }
                        else if (!process.HasExited) process.Kill(true);
                        process.WaitForExit(); process.Dispose(); process = null;
                        memory.Dispose(); memory = null;
                    }
                    if (process == null && now - lastStart >= 1000)
                    {
                        _restartRequested = false;
                        string name = "Local\\VI.HandleHost." + Guid.NewGuid().ToString("N");
                        memory = new HandleHostMemory(name, true);
                        lastStart = Environment.TickCount64;
                        try { process = _launch(name); ++RestartCount; }
                        catch (Win32Exception ex)
                        {
                            lock (_lock) _status = "手柄宿主重启中：" + ex.Message;
                            memory.Dispose(); memory = null;
                        }
                    }
                    if (memory != null)
                    {
                        lock (_lock)
                        {
                            memory.Write(HandleHostMemory.ForceOffset, _forces);
                            if (!_restartRequested && memory.Read(HandleHostMemory.SampleOffset, out HostSamples samples))
                            {
                                _samples = samples;
                                if (samples.A.Valid != 0 || samples.B.Valid != 0) { _status = ""; _sdkMissing = false; }
                            }
                        }
                    }
                    Thread.Sleep(5);
                }
            }
            finally
            {
                if (process != null)
                {
                    memory.Stop = true;
                    if (!process.WaitForExit(1000)) { process.Kill(true); process.WaitForExit(); }
                    process.Dispose();
                }
                memory?.Dispose();
            }
        }
        public void Dispose() { lock (_lifecycleLock) { if (_disposed) return; Stop(); _disposed = true; } }
        private static double Finite(double value) => double.IsFinite(value) ? value : 0;
    }
}
