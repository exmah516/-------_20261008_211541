using System;
using System.Diagnostics;
using System.Collections.Concurrent;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using MasterConsole.Services;
using VI.Service.Net;

namespace MasterConsole.Protocol.Tests
{
    internal static partial class Program
    {
        static partial void NetworkSmoke(string[] args, byte[] key)
        {
            string simulator = Option(args, "--simulator");
            string router = Option(args, "--router");
            string endpoint = Option(args, "--endpoint", "tcp/127.0.0.1:17447");
            string tokenFile = Path.Combine(Path.GetTempPath(), "vi-zenoh-" + Guid.NewGuid().ToString("N") + ".token");
            byte[] token = Encoding.ASCII.GetBytes("zenoh-offline-integration-token-32");
            File.WriteAllBytes(tokenFile, token);
            Process process = null;
            var ready = new ManualResetEventSlim();
            int handleCount = 0, forceCount = 0, stateCount = 0, commandCount = 0;
            var commands = new ConcurrentQueue<CommandFrame>();
            try
            {
                process = StartSimulator(simulator, endpoint, tokenFile, router, ready);
                if (!ready.Wait(15000)) throw new TimeoutException("模拟数据端未就绪。");
                using (var observer = new ZenohSession(endpoint))
                using (var link = new RemoteRobotLink(new RemoteLinkSettings { Host = "127.0.0.1", Endpoint = endpoint, Token = token }, null))
                {
                    observer.Subscribe<HandleFrame>(ZenohProtocol.HandleTopic, f => Interlocked.Increment(ref handleCount));
                    observer.Subscribe<ForceFrame>(ZenohProtocol.ForceTopic, f => Interlocked.Increment(ref forceCount));
                    observer.Subscribe<StateFrame>(ZenohProtocol.StateTopic, f => Interlocked.Increment(ref stateCount));
                    observer.Subscribe<CommandFrame>(ZenohProtocol.CommandTopic, f => { Interlocked.Increment(ref commandCount); commands.Enqueue(f); });
                    var cachedPublisher = observer.Advertise<CommandFrame>(ZenohProtocol.CommandTopic);
                    cachedPublisher.Dispose(); cachedPublisher.Dispose();
                    Check("相同 Topic 复用与 Publisher 幂等释放", !cachedPublisher.IsAvailable);
                    link.LogReceived += (sender, log) => Console.WriteLine("[链路] " + log.Text);
                    process.StandardInput.WriteLine("pause"); process.StandardInput.Flush();
                    Thread.Sleep(700); // 等缓存状态过期，确保 ConnectAsync 后状态帧尚未到达。
                    var acquire = Task.Run(async () =>
                    {
                        await link.ConnectAsync();
                        return await link.AcquireControlAsync();
                    });
                    Thread.Sleep(100);
                    process.StandardInput.WriteLine("resume"); process.StandardInput.Flush();
                    var acquired = Result(acquire, 16000);
                    Check("连接后立即申请控制权会等待首个有效状态帧", acquired.Ok);
                    if (!acquired.Ok) return;
                    Check("原生 Zenoh-C 与真实主端认证", link.IsConnected);
                    Check("有控制权", Wait(() => link.HasControl, 3000));
                    Check("准备位置专用状态", Result(link.PreparePositionAsync(12, 125), 7000).Ok);
                    Check("开始控制专用状态", Result(link.StartControlAsync(), 7000).Ok);
                    Check("点动直接返回发送结果", Result(link.SetArmCartesianJogAsync(1, 1000), 1000).Ok);
                    link.KeepArmCartesianAlive(); link.SetArmAxisJog(1, 0);
                    Parallel.For(0, 64, i => link.SetArmAxisJog(1, 0));
                    Thread.Sleep(100);
                    ulong previous = 0; bool increasing = true;
                    foreach (var command in commands)
                    { if (command.Sequence <= previous) increasing = false; previous = command.Sequence; }
                    Check("并发发送命令的序号按发送顺序递增", increasing && commands.Count >= 64);
                    int startHandle = handleCount, startState = stateCount, startForce = forceCount;
                    var frequencyClock = Stopwatch.StartNew();
                    Thread.Sleep(3000);
                    double seconds = frequencyClock.Elapsed.TotalSeconds;
                    double handleHz = (handleCount - startHandle) / seconds;
                    double stateHz = (stateCount - startState) / seconds;
                    double forceHz = (forceCount - startForce) / seconds;
                    Console.WriteLine("频率：handle=" + handleHz.ToString("F1") + "Hz state=" + stateHz.ToString("F1") + "Hz force=" + forceHz.ToString("F1") + "Hz");
                    Check("手柄与力反馈接近 100Hz，状态接近 30Hz", handleHz >= 85 && handleHz <= 115 && forceHz >= 85 && forceHz <= 115 && stateHz >= 25 && stateHz <= 35);
                    Check("四个 Topic 均有流量", handleCount >= 100 && forceCount >= 100 && stateCount >= 20 && commandCount >= 5);
                    Check("单调 Echo RTT 与有效触觉帧", !double.IsNaN(link.GetStats().RttMs) && link.GetStats().HapticAgeMs < 500);
                    process.StandardInput.WriteLine("pause"); process.StandardInput.Flush();
                    Check("有效状态超过 500ms 断联", Wait(() => !link.IsConnected, 1500));
                    process.StandardInput.WriteLine("resume"); process.StandardInput.Flush();
                    Check("短暂中断自动恢复", Wait(() => link.IsConnected, 4000));
                    process.StandardInput.WriteLine("pause"); process.StandardInput.Flush();
                    Thread.Sleep(3600);
                    Check("状态停止后无假在线", !link.IsConnected);
                    process.StandardInput.WriteLine("resume"); process.StandardInput.Flush();
                    Check("长中断后重新认证恢复", Wait(() => link.IsConnected, 10000));
                    Check("重新认证后获取控制权", Result(link.AcquireControlAsync(), 7000).Ok);
                    Check("释放控制权", Result(link.ReleaseControlAsync(), 7000).Ok);
                    link.Disconnect();
                    Check("主动断开", !link.IsConnected);
                    link.ConnectAsync().GetAwaiter().GetResult();
                    Check("同一对象重新连接", Wait(() => link.IsConnected, 10000));
                    link.Disconnect();
                }
                StopSimulator(process); process.Dispose(); process = null;
                var unavailableClock = Stopwatch.StartNew();
                CheckThrows("自启路由器随数据端关闭", () => { using (var unavailable = new ZenohSession(endpoint)) { } });
                Check("无路由器时连接有界返回", unavailableClock.ElapsedMilliseconds < 5000);
                using (var external = Process.Start(new ProcessStartInfo(router, "--listen " + Quote(endpoint))
                    { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true }))
                {
                    external.OutputDataReceived += (sender, e) => { };
                    external.ErrorDataReceived += (sender, e) => { };
                    external.BeginOutputReadLine(); external.BeginErrorReadLine();
                    try
                    {
                        Thread.Sleep(300);
                        ready.Reset();
                        process = StartSimulator(simulator, endpoint, tokenFile, router, ready);
                        Check("数据端复用已有路由器", ready.Wait(15000));
                        StopSimulator(process); process.Dispose(); process = null;
                        Check("退出数据端保留外部路由器", !external.HasExited);
                    }
                    finally { if (!external.HasExited) { external.Kill(); external.WaitForExit(5000); } }
                }
            }
            finally
            {
                if (process != null)
                {
                    StopSimulator(process);
                    process.Dispose();
                }
                ready.Dispose();
                if (File.Exists(tokenFile)) File.Delete(tokenFile);
            }
        }

        private static CommandResult Result(Task<CommandResult> task, int milliseconds)
        { if (!task.Wait(milliseconds)) throw new TimeoutException("主端命令等待超时。"); return task.GetAwaiter().GetResult(); }
        private static bool Wait(Func<bool> condition, int milliseconds)
        {
            var clock = Stopwatch.StartNew();
            do { if (condition()) return true; Thread.Sleep(10); } while (clock.ElapsedMilliseconds < milliseconds);
            return condition();
        }
        private static string Option(string[] args, string name, string fallback = null)
        {
            int index = Array.IndexOf(args, name);
            if (index >= 0 && index + 1 < args.Length) return args[index + 1];
            if (fallback != null) return fallback;
            throw new ArgumentException("缺少参数 " + name);
        }
        private static string Quote(string value) => "\"" + value.Replace("\"", "") + "\"";
        private static Process StartSimulator(string simulator, string endpoint, string tokenFile, string router, ManualResetEventSlim ready)
        {
            var process = new Process
            {
                StartInfo = new ProcessStartInfo(simulator,
                    "--endpoint " + Quote(endpoint) + " --token " + Quote(tokenFile) + " --router " + Quote(router))
                {
                    UseShellExecute = false, CreateNoWindow = true, RedirectStandardInput = true,
                    RedirectStandardOutput = true, RedirectStandardError = true
                }
            };
            process.OutputDataReceived += (sender, e) => { if (e.Data != null) { Console.WriteLine("[模拟] " + e.Data); if (e.Data.Contains("SIM_READY")) ready.Set(); } };
            process.ErrorDataReceived += (sender, e) => { if (e.Data != null) Console.WriteLine("[模拟] " + e.Data); };
            process.Start(); process.BeginOutputReadLine(); process.BeginErrorReadLine();
            return process;
        }
        private static void StopSimulator(Process process)
        {
            if (process.HasExited) return;
            process.StandardInput.WriteLine("quit"); process.StandardInput.Flush();
            if (!process.WaitForExit(5000)) { process.Kill(); process.WaitForExit(5000); }
            process.WaitForExit(); // 等异步输出回调退出后再释放就绪事件。
        }
    }
}
