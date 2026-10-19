using System;
using System.Configuration;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using MasterConsole.Controls;
using MasterConsole.Services;

// 隐藏窗口及两路假摄像头只走本机回环，不连接机器人或真实摄像头。
internal static class Program
{
    private static void Check(bool ok, string message)
    {
        if (!ok) throw new Exception(message);
        Console.WriteLine("PASS: " + message);
    }

    private static void Invoke(object window, string name, params object[] args)
        => window.GetType().GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic)
            .Invoke(window, args);

    private static object Field(object target, string name)
        => target.GetType().GetField(name, BindingFlags.Instance | BindingFlags.NonPublic).GetValue(target);

    private static TcpListener Listen()
    {
        var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start();
        return listener;
    }

    private static void CheckRelay(int relayPort, TcpListener camera)
    {
        using (var master = new TcpClient())
        {
            master.ReceiveTimeout = 3000;
            master.SendTimeout = 3000;
            master.Connect(IPAddress.Loopback, relayPort);
            var pending = camera.AcceptTcpClientAsync();
            Check(pending.Wait(3000), "native relay connects to loopback camera");
            using (var device = pending.Result)
            {
                device.ReceiveTimeout = 3000;
                master.GetStream().WriteByte(42);
                Check(device.GetStream().ReadByte() == 42, "master request reaches camera through ScreenCut DLL");
                device.GetStream().WriteByte(87);
                Check(master.GetStream().ReadByte() == 87, "camera response returns to master through ScreenCut DLL");
            }
        }
    }

    private static void RenderPreview(ScreenCut.MainWindow sender)
    {
        // 只保存合成预览，不把实际桌面内容写到磁盘。
        ((Image)sender.FindName("pictureBox")).Source = BitmapSource.Create(320, 240, 96, 96,
            PixelFormats.Bgr32, null, new byte[320 * 240 * 4], 320 * 4);
        var content = (FrameworkElement)sender.Content;
        ((Grid)content).Background = Brushes.White;
        content.Measure(new Size(824, 360));
        content.Arrange(new Rect(0, 0, 824, 360));
        content.UpdateLayout();
        var bitmap = new RenderTargetBitmap(824, 360, 96, 96, PixelFormats.Pbgra32);
        bitmap.Render(content);
        var png = new PngBitmapEncoder();
        png.Frames.Add(BitmapFrame.Create(bitmap));
        string path = Path.Combine(Path.GetTempPath(), "ScreenCut-layout-check.png");
        using (var file = File.Create(path)) png.Save(file);
        Console.WriteLine("UI preview: " + path);
    }

    [STAThread]
    private static int Main()
    {
        ScreenCut.MainWindow sender = null;
        DsaImageView view = null;
        var camera1 = Listen();
        var camera2 = Listen();
        var relay1 = Listen();
        var relay2 = Listen();
        int port1 = ((IPEndPoint)relay1.LocalEndpoint).Port;
        int port2 = ((IPEndPoint)relay2.LocalEndpoint).Port;
        relay1.Stop();
        relay2.Stop();
        try
        {
            var config = ConfigurationManager.OpenExeConfiguration(ConfigurationUserLevel.None);
            foreach (var pair in new[] { "RelayListenIp=127.0.0.1", "Camera1Ip=127.0.0.1", "Camera2Ip=127.0.0.1",
                "Camera1LocalIp=127.0.0.1", "Camera2LocalIp=127.0.0.1", "Camera1RelayPort=" + port1,
                "Camera2RelayPort=" + port2, "Camera1Port=" + ((IPEndPoint)camera1.LocalEndpoint).Port,
                "Camera2Port=" + ((IPEndPoint)camera2.LocalEndpoint).Port })
            {
                var parts = pair.Split('=');
                config.AppSettings.Settings.Remove(parts[0]);
                config.AppSettings.Settings.Add(parts[0], parts[1]);
            }
            config.Save(ConfigurationSaveMode.Modified);
            ConfigurationManager.RefreshSection("appSettings");
            var app = new Application();
            view = new DsaImageView();
            using (var receiver = new DsaJpegReceiver(new DsaJpegReceiverSettings()))
            {
                string error = null;
                receiver.StatusChanged += (stream, status, detail) => { if (status == "失败") error = detail; };
                receiver.FrameReceived += (stream, jpeg, time) => { if (stream == 0) view.PublishJpeg(jpeg, time); };
                receiver.Start();
                Check(error == null, "screenshot receiver owns loopback test ports");
                sender = new ScreenCut.MainWindow();
                ((TextBox)sender.FindName("txtIP")).Text = "127.0.0.1";
                Invoke(sender, "StartCameras");
                CheckRelay(port1, camera1);
                CheckRelay(port2, camera2);
                foreach (var pair in new[] { "txtLeft=0", "txtTop=0", "txtWidth=64", "txtHeight=64",
                    "txtLeft1=0", "txtTop1=0", "txtWidth1=64", "txtHeight1=64" })
                {
                    var parts = pair.Split('=');
                    ((TextBox)sender.FindName(parts[0])).Text = parts[1];
                }
                Invoke(sender, "Button_Click_1", null, null);
                var image = (Image)view.FindName("Image");
                for (int i = 0; i < 100 && image.Source == null; ++i)
                {
                    Thread.Sleep(10);
                    Invoke(view, "OnTick", null, null);
                }
                Check(image.Source is BitmapSource bitmap && bitmap.PixelWidth == 64 && bitmap.PixelHeight == 64,
                    "first send tick reaches real receiver and displays JPEG without fast command");
                ((TextBox)sender.FindName("txtWidth")).Text = "-1";
                Invoke(sender, "TimerImage_Tick", null, null);
                Check(((TextBlock)sender.FindName("sendStatus")).Text.Contains("失败"), "invalid capture region is visible to user");
                ((TextBox)sender.FindName("txtWidth")).Text = "64";
                Invoke(sender, "Button_Stop", null, null);
                Check(Field(sender, "_commandReceiver") == null && Field(sender, "_imageSender") == null,
                    "stop releases screenshot sockets");
                CheckRelay(port1, camera1);
                Invoke(sender, "Button_Click_1", null, null);
                Check(Field(sender, "_commandReceiver") != null, "screenshot sending restarts without port conflict");
                sender.GetType().GetField("_lastFastCommandUtc", BindingFlags.Instance | BindingFlags.NonPublic)
                    .SetValue(sender, DateTime.UtcNow.AddSeconds(-4));
                Invoke(sender, "TimerCmd_Tick", null, null);
                Check(!((bool[])Field(sender, "_fastSend"))[0], "fast mode expires after three seconds without command");
                using (var command = new UdpClient(new IPEndPoint(IPAddress.Loopback, 0)))
                    command.Send(new byte[] { 1, 0 }, 2, new IPEndPoint(IPAddress.Loopback, 36662));
                Invoke(sender, "TimerCmd_Tick", null, null);
                Check(((bool[])Field(sender, "_fastSend"))[0], "valid main command restores fast screenshot sending");
                RenderPreview(sender);
            }
            sender.Close();
            relay1 = new TcpListener(IPAddress.Loopback, port1);
            relay2 = new TcpListener(IPAddress.Loopback, port2);
            relay1.Start();
            relay2.Start();
            Check(true, "closing ScreenCut releases both camera listeners");
            view.Dispose();
            app.Shutdown();
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine("FAIL: " + (ex.InnerException ?? ex).Message);
            return 1;
        }
        finally
        {
            sender?.Close();
            view?.Dispose();
            camera1.Stop();
            camera2.Stop();
            relay1.Stop();
            relay2.Stop();
        }
    }
}
