using System;
using System.Diagnostics;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Threading;
using MasterConsole.Services;
using MasterConsole.Protocol;

namespace MasterConsole.Protocol.Tests
{
    internal static partial class Program
    {
        private static int HandleHostStub(string[] args)
        {
            using var memory = new HandleHostMemory(args[1], false);
            var started = Stopwatch.StartNew();
            long sequence = 0;
            while (!memory.Stop)
            {
                if (args[2] == "crash" && started.ElapsedMilliseconds > 250) return 42;
                if (args[2] == "stall" && started.ElapsedMilliseconds > 250) Thread.Sleep(1500);
                var sample = new HostSample { Valid = 1, Tick = Environment.TickCount64,
                    Sequence = ++sequence, Encoder0 = 582 };
                memory.Write(HandleHostMemory.SampleOffset, new HostSamples { A = sample, B = sample });
                memory.Heartbeat = Environment.TickCount64;
                Thread.Sleep(5);
            }
            return 0;
        }
        private static void HandleHostTests()
        {
            foreach (string fault in new[] { "crash", "stall", "usb", "remove" })
            {
                int launches = 0;
                long firstStart = 0, secondStart = 0;
                using var service = new HandleService(name =>
                {
                    if (++launches == 1) firstStart = Environment.TickCount64; else secondStart = Environment.TickCount64;
                    var info = new ProcessStartInfo(Environment.ProcessPath) { UseShellExecute = false, CreateNoWindow = true };
                    if (Path.GetFileNameWithoutExtension(Environment.ProcessPath).Equals("dotnet", StringComparison.OrdinalIgnoreCase))
                        info.ArgumentList.Add(typeof(Program).Assembly.Location);
                    info.ArgumentList.Add("--handle-host-stub"); info.ArgumentList.Add(name);
                    info.ArgumentList.Add(launches == 1 ? fault : "normal");
                    return Process.Start(info);
                }, false);
                service.Start();
                Check("宿主替身启动采样 " + fault, HostWait(() => service.GetSample(0).Valid, 3000));
                service.SetHaptic(0, new HapticOut { Enable = true, ForceN = 2, Axis = 1 });
                if (fault == "usb" || fault == "remove")
                {
                    service.DeviceChanged(fault == "usb");
                    Check("拔插通知立即使两柄invalid " + fault, !service.GetSample(0).Valid && !service.GetSample(1).Valid);
                }
                Check("宿主自动重启并恢复采样 " + fault, HostWait(() => launches >= 2 && service.IsOnline(0) && service.IsOnline(1), 5000));
                Check("宿主重启间隔至少1s " + fault, secondStart - firstStart >= 1000);
                Console.WriteLine("V5替身 " + fault + " launch到双柄有效=" + (Environment.TickCount64 - secondStart) + "ms");
                var watch = Stopwatch.StartNew();
                for (int n = 0; n < 10000; ++n) { service.GetSample(0); service.ClearHaptic(); }
                Check("网络/UI缓存操作不等待宿主 " + fault, watch.ElapsedMilliseconds < 500);
            }
            string sdk = Environment.GetEnvironmentVariable("VI_TEST_FAKE_SDK");
            if (!string.IsNullOrEmpty(sdk)) NativeHostTests(sdk);
        }
        private static void NativeHostTests(string sdk)
        {
            string directory = Path.Combine(Path.GetTempPath(), "vi-host-sdk-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            foreach (string file in Directory.GetFiles(Path.Combine(AppContext.BaseDirectory, "HandleHost")))
                if (!Path.GetFileName(file).Equals("FLCatheter.dll", StringComparison.OrdinalIgnoreCase)) File.Copy(file, Path.Combine(directory, Path.GetFileName(file)));
            string metricName = "Local\\VI.FakeSDK." + Guid.NewGuid().ToString("N");
            using var metrics = MemoryMappedFile.CreateNew(metricName, 128);
            using var values = metrics.CreateViewAccessor();
            long lastLaunch = 0;
            Process Launch(string name)
            {
                var info = new ProcessStartInfo(Path.Combine(directory, "VI.HandleHost.exe")) { UseShellExecute = false, CreateNoWindow = true };
                info.ArgumentList.Add(name); info.Environment["VI_FAKE_SDK_MAP"] = metricName;
                lastLaunch = Stopwatch.GetTimestamp();
                return Process.Start(info);
            }
            File.Copy(sdk, Path.Combine(directory, "FLCatheter.dll"));
            using (var service = new HandleService(Launch, false))
            {
                service.Start();
                Check("V5真实宿主连接模拟SDK", HostWait(() => service.IsOnline(0) && service.IsOnline(1), 3000));
                int count = values.ReadInt32(8); var watch = Stopwatch.StartNew(); Thread.Sleep(1000);
                double hz = (values.ReadInt32(8) - count) * 1000.0 / watch.ElapsedMilliseconds;
                Console.WriteLine("V5真实宿主采样频率=" + hz.ToString("F1") + "Hz");
                Check("V5真实宿主200Hz采样", hz >= 199);
                service.SetHaptic(0, new HapticOut { Enable = true, Axis = 1, ForceN = 2, TorqueNm = .25f });
                Check("V5力目标到达模拟SDK", HostWait(() => values.ReadDouble(24) == 2 && values.ReadDouble(40) == .25, 1000));
                values.Write(0, -1);
                Check("V5单柄SDK离线立即invalid并清力", HostWait(() => !service.IsOnline(0) && service.IsOnline(1) && values.ReadDouble(24) == 0, 1000));
                values.Write(0, 0);
                Check("V5SDK状态恢复采样", HostWait(() => service.IsOnline(0), 1000));
                service.SetHaptic(0, new HapticOut { Enable = true, Axis = 1, ForceN = 2 });
                Check("V5力新鲜时输出", HostWait(() => values.ReadDouble(24) == 2, 1000));
                Thread.Sleep(3050);
                Check("V5超过3s力目标自动清零", HostWait(() => values.ReadDouble(24) == 0, 500));
                service.SetHaptic(0, new HapticOut { Enable = true, Axis = 1, ForceN = 2 });
                HostWait(() => values.ReadDouble(24) == 2, 1000);
                service.SetHaptic(1, new HapticOut { Enable = true, Axis = 1, ForceN = 3, TorqueNm = .5f });
                Check("V5拔出前双路非零力", HostWait(() => values.ReadDouble(24) == 2 && values.ReadDouble(56) == 3, 1000));
                int beforeRestart = service.RestartCount;
                var removedClock = Stopwatch.StartNew();
                service.DeviceChanged(false);
                Check("V5拔出通知同步撤销两柄采样", !service.GetSample(0).Valid && !service.GetSample(1).Valid);
                service.SetHaptic(0, new HapticOut { Enable = true, Axis = 1, ForceN = 9 });
                Check("V5拔出后两路力和扭矩清零", HostWait(() => values.ReadDouble(24) == 0 && values.ReadDouble(40) == 0 && values.ReadDouble(56) == 0 && values.ReadDouble(72) == 0, 1000));
                Console.WriteLine("V5拔出通知到双路清力=" + removedClock.Elapsed.TotalMilliseconds.ToString("F1") + "ms");
                Check("V5真实宿主拔出通知重启恢复双柄", HostWait(() => service.RestartCount > beforeRestart && service.IsOnline(0) && service.IsOnline(1), 3000));
                Console.WriteLine("V5真实宿主 remove launch到双柄有效=" + ((Stopwatch.GetTimestamp() - lastLaunch) * 1000.0 / Stopwatch.Frequency).ToString("F1") + "ms");
                Check("V5重启不补发拔出前或重启期间力", values.ReadDouble(24) == 0 && values.ReadDouble(56) == 0);
                beforeRestart = service.RestartCount;
                service.DeviceChanged(true);
                Check("V5真实宿主插入后重启恢复", HostWait(() => service.RestartCount > beforeRestart && service.IsOnline(0) && service.IsOnline(1), 3000));
                Console.WriteLine("V5真实宿主 arrival launch到双柄有效=" + ((Stopwatch.GetTimestamp() - lastLaunch) * 1000.0 / Stopwatch.Frequency).ToString("F1") + "ms");
            }
            foreach (string failure in new[] { "missing", "entry" })
            {
                string dll = Path.Combine(directory, "FLCatheter.dll");
                File.Delete(dll);
                if (failure == "entry") File.Copy(Path.Combine(Path.GetDirectoryName(sdk), "MissingEntry.dll"), dll);
                using var service = new HandleService(Launch, false); service.Start();
                Check("V5 " + failure + " 只退出宿主并提示", HostWait(() => service.SdkMissing && service.RestartCount >= 2, 4000));
                Check("V5 " + failure + " 主端监管继续运行", service.IsRunning && !service.GetSample(0).Valid);
            }
            Directory.Delete(directory, true);
        }
        private static bool HostWait(Func<bool> condition, int timeout)
        {
            var clock = Stopwatch.StartNew();
            while (clock.ElapsedMilliseconds < timeout) { if (condition()) return true; Thread.Sleep(5); }
            return condition();
        }
    }
}
