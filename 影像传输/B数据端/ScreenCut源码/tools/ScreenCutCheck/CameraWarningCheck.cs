using System;
using System.Net;
using System.Net.Sockets;
using System.Threading.Tasks;
using ScreenCut;

internal static partial class Program
{
    private static async Task CameraWarningCheckAsync()
    {
        var backend = new TcpListener(IPAddress.Loopback, 0);
        backend.Start(32);
        try
        {
            var settings = TestSettings();
            foreach (var camera in settings.Cameras)
                camera.RtspPort = ((IPEndPoint)backend.LocalEndpoint).Port;
            using (var healthy = new StreamingHost(settings))
            {
                bool warned = false;
                var checkedBoth = new TaskCompletionSource<bool>();
                healthy.CameraWarning += message => warned = true;
                healthy.Status += message => { if (message.StartsWith("监控 2 网络检查通过")) checkedBoth.TrySetResult(true); };
                await healthy.StartAsync();
                if (await Task.WhenAny(checkedBoth.Task, Task.Delay(8000)) != checkedBoth.Task)
                    throw new TimeoutException("正常相机连接检查未完成。");
                Check(!warned, "reachable camera source addresses do not raise a wiring warning");
            }
            // 指定不存在的源地址，复现配置网口不可达、另一路网口可达。
            settings.Cameras[0].LocalIp = "192.0.2.250";
            using (var faulty = new StreamingHost(settings))
            {
                var warning = new TaskCompletionSource<string>();
                faulty.CameraWarning += message => warning.TrySetResult(message);
                await faulty.StartAsync();
                if (await Task.WhenAny(warning.Task, Task.Delay(8000)) != warning.Task)
                    throw new TimeoutException("网口错误未给出独立提示。");
                string text = await warning.Task;
                Check(text.Contains("网口可能接反") &&
                    (text.Contains("Camera1LocalIp=127.0.0.2") || text.Contains("Camera1LocalIp=127.0.0.3")),
                    "wrong camera source raises a persistent warning with the reachable source address");
                Check(settings.Cameras[0].LocalIp == "192.0.2.250", "camera diagnostic does not silently change the configured source address");
            }
        }
        finally { backend.Stop(); }
    }
}
