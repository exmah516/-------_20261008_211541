using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Windows;
using System.Windows.Controls;
using MasterConsole.Protocol;
using MasterConsole.Services;
using MasterConsole.ViewModels;

internal static partial class Program
{
    private sealed class HomeChainHandles : IHandleService
    {
        public bool SdkMissing => false;
        public bool IsOnline(int slot) => true;
        public HandleSample GetSample(int slot) => new() { Valid = true };
        public void SetHaptic(int slot, HapticOut output) { }
        public void ClearHaptic() { }
        public void Start() { }
        public void Stop() { }
        public void Dispose() { }
    }

    private static void CheckHomeChain(string simulator, string router)
    {
        var application = new Application { ShutdownMode = ShutdownMode.OnExplicitShutdown };
        application.Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri("pack://application:,,,/VI.Control;component/Themes/LightClinical.xaml", UriKind.Absolute)
        });
        string tokenFile = Path.Combine(Path.GetTempPath(), "vi-home-chain-" + Guid.NewGuid().ToString("N") + ".token");
        byte[] token = System.Text.Encoding.ASCII.GetBytes("round7-home-chain-offline-token");
        File.WriteAllBytes(tokenFile, token);
        using var ready = new ManualResetEventSlim();
        int readyCase = -1;
        using var process = new Process
        {
            StartInfo = new ProcessStartInfo(simulator)
            {
                UseShellExecute = false, CreateNoWindow = true,
                RedirectStandardInput = true, RedirectStandardOutput = true, RedirectStandardError = true
            }
        };
        process.StartInfo.ArgumentList.Add(tokenFile);
        process.StartInfo.ArgumentList.Add(router);
        process.OutputDataReceived += (_, e) =>
        {
            if (e.Data is null) return;
            Console.WriteLine("[home mock] " + e.Data);
            if (e.Data == "HOME_SIM_READY") ready.Set();
            if (e.Data.StartsWith("CASE_READY ", StringComparison.Ordinal))
                Volatile.Write(ref readyCase, int.Parse(e.Data[11..], System.Globalization.CultureInfo.InvariantCulture));
        };
        process.ErrorDataReceived += (_, e) => { if (e.Data is not null) Console.WriteLine(e.Data); };
        process.Start(); process.BeginOutputReadLine(); process.BeginErrorReadLine();
        try
        {
            Check(ready.Wait(15000), "home chain simulator starts on loopback without hardware");
            using var handles = new HomeChainHandles();
            using var link = new RemoteRobotLink(new RemoteLinkSettings
            {
                Host = "127.0.0.1", Endpoint = "tcp/127.0.0.1:17448", Token = token
            }, handles);
            using var vm = new MainViewModel(link);
            var module = Assembly.LoadFrom(Path.Combine(AppContext.BaseDirectory, "Modules/Features/VI.Module.PositioningArm.dll"));
            var panel = (UserControl)Activator.CreateInstance(module.GetType("VI.Module.PositioningArm.PositioningArmPanel", true)!, vm)!;
            var home = (Button)panel.FindName("ProgramHome")!;
            var text = (TextBlock)panel.FindName("ArmProgramZeroStatus")!;
            var alive = Stopwatch.StartNew();
            void Until(Func<bool> condition)
            {
                var deadline = Stopwatch.StartNew();
                while (!condition())
                {
                    CheckDeadline(deadline.ElapsedMilliseconds < 20000);
                    Pump();
                    if (alive.ElapsedMilliseconds >= 100)
                    {
                        Invoke(panel, "KeepaliveTimer_Tick", null!, EventArgs.Empty);
                        alive.Restart();
                    }
                    Thread.Sleep(5);
                }
                Pump();
            }
            var connected = link.ConnectAsync(); Until(() => connected.IsCompleted); connected.GetAwaiter().GetResult();
            var acquired = link.AcquireControlAsync(); Until(() => acquired.IsCompleted);
            Check(acquired.GetAwaiter().GetResult().Ok, "home chain authenticates and acquires control");
            Until(() => vm.CanOperate);
            var seen = new HashSet<string>();
            vm.PropertyChanged += (_, e) => { if (e.PropertyName == nameof(MainViewModel.ArmProgramZeroStatusText)) seen.Add(vm.ArmProgramZeroStatusText); };
            foreach (var (scenario, expected) in new[]
            {
                (0, "归零已完成"), (1, "已在零位"),
                (2, "归零失败：1002（当前关节位置超出允许范围）"),
                (3, "归零失败：2003（PLC 驱动、使能、心跳或急停条件未满足）"),
                (4, "归零失败：1008（末端点动占用，请松开点动并等待停稳）")
            })
            {
                Invoke(panel, "ArmStop_Click", home, new RoutedEventArgs());
                process.StandardInput.WriteLine(scenario); process.StandardInput.Flush();
                Until(() => Volatile.Read(ref readyCase) == scenario);
                seen.Clear();
                Check(home.IsEnabled, $"home chain case {scenario}: button enabled by real telemetry");
                home.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
                Until(() => text.Text == expected);
                Check(vm.ArmProgramZeroStatusText == expected, $"E10 chain case {scenario}: {text.Text}");
                if (scenario == 0) Check(seen.Contains("归零进行中"), "normal home exposes running then completion through the entire chain");
            }
            Invoke(panel, "ArmStop_Click", home, new RoutedEventArgs());
            Console.WriteLine("PASS E10: WPF click -> MainViewModel -> RemoteRobotLink -> Zenoh -> request_program_zero -> update_cartesian -> plan_arm_curve(mode0) -> mock PLC -> StateFrame -> WPF; five scenarios.");
        }
        finally
        {
            if (!process.HasExited)
            {
                process.StandardInput.WriteLine("quit"); process.StandardInput.Flush();
                process.WaitForExit(10000);
            }
            File.Delete(tokenFile);
        }
    }

    private static void CheckDeadline(bool condition)
    {
        if (!condition) throw new TimeoutException("归零全链路 mock 等待超时。");
    }
}
