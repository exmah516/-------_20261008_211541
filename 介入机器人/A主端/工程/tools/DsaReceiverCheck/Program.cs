using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Reflection;
using System.Windows.Threading;
using MasterConsole.Controls;
using MasterConsole.Services;

// 只在本机回环口发送合成 JPEG，不打开主窗口、机器人或截图设备。
class Program
{
    static readonly ConcurrentQueue<Tuple<int, byte[]>> Frames = new ConcurrentQueue<Tuple<int, byte[]>>();

    static void Check(bool value, string message)
    {
        if (!value) throw new Exception(message);
        Console.WriteLine("PASS: " + message);
    }

    static void Wait(Func<bool> ready, string message)
    {
        var clock = Stopwatch.StartNew();
        while (clock.ElapsedMilliseconds < 3000)
        {
            Dispatcher.CurrentDispatcher.Invoke(new Action(() => { }), DispatcherPriority.ApplicationIdle);
            if (ready()) return;
            Thread.Sleep(10);
        }
        throw new TimeoutException(message);
    }

    static int FreePort()
    {
        using (var socket = new UdpClient(new IPEndPoint(IPAddress.Loopback, 0)))
            return ((IPEndPoint)socket.Client.LocalEndPoint).Port;
    }

    static void Drain(UdpClient socket)
    {
        var endpoint = new IPEndPoint(IPAddress.Any, 0);
        while (socket.Available > 0) socket.Receive(ref endpoint);
    }

    static void Command(UdpClient socket, byte first, byte second, string message)
    {
        socket.Client.ReceiveTimeout = 1500;
        var endpoint = new IPEndPoint(IPAddress.Any, 0);
        var command = socket.Receive(ref endpoint);
        Check(command.SequenceEqual(new[] { first, second }), message);
    }

    static byte[] Jpeg(int width)
    {
        var pixels = new byte[width * 180 * 3];
        new Random(width).NextBytes(pixels);
        var bitmap = BitmapSource.Create(width, 180, 96, 96, PixelFormats.Bgr24, null, pixels, width * 3);
        var encoder = new JpegBitmapEncoder { QualityLevel = 80 };
        encoder.Frames.Add(BitmapFrame.Create(bitmap));
        using (var output = new MemoryStream()) { encoder.Save(output); return output.ToArray(); }
    }

    static byte[] Header(int wireStream, int length)
        => new[] { (byte)wireStream, (byte)(length >> 24), (byte)(length >> 16), (byte)(length >> 8), (byte)length };

    static void Send(UdpClient sender, int port, int wireStream, byte[] jpeg)
    {
        var destination = new IPEndPoint(IPAddress.Loopback, port);
        var header = Header(wireStream, jpeg.Length);
        sender.Send(header, header.Length, destination);
        for (int offset = 0; offset < jpeg.Length; offset += 8192)
        {
            var part = jpeg.Skip(offset).Take(8192).ToArray();
            sender.Send(part, part.Length, destination);
        }
    }

    static void Frame(UdpClient sender, int port, int wireStream, int expectedStream, DsaImageView view, int width)
    {
        var jpeg = Jpeg(width);
        Check(jpeg.Length > 8192, "JPEG exercises multiple legacy UDP chunks");
        Send(sender, port, wireStream, jpeg);
        Wait(() => !Frames.IsEmpty, "legacy stream not received: " + expectedStream);
        Frames.TryDequeue(out var received);
        Check(received.Item1 == expectedStream && received.Item2.SequenceEqual(jpeg), "legacy stream " + expectedStream + " routes with identical JPEG bytes");
        var image = (Image)view.FindName("Image");
        Wait(() => image.Source is BitmapSource bitmap && bitmap.PixelWidth == width, "JPEG not displayed for stream " + expectedStream);
        Check(((BitmapSource)image.Source).PixelHeight == 180, "stream " + expectedStream + " decoded and displayed by real DsaImageView");
    }

    [STAThread]
    static int Main()
    {
        try
        {
            var app = new Application();
            var views = new[] { new DsaImageView(), new DsaImageView(), new DsaImageView() };
            var settings = new DsaJpegReceiverSettings { ListenPort0 = FreePort(), ListenPort1 = FreePort() };
            while (settings.ListenPort1 == settings.ListenPort0) settings.ListenPort1 = FreePort();
            using (var commands0 = new UdpClient(new IPEndPoint(IPAddress.Loopback, 0)))
            using (var commands1 = new UdpClient(new IPEndPoint(IPAddress.Parse("127.0.0.2"), 0)))
            {
            settings.ScreenCutIp = "127.0.0.1";
            settings.EcgScreenCutIp = "127.0.0.2";
            settings.CommandPort0 = ((IPEndPoint)commands0.Client.LocalEndPoint).Port;
            settings.CommandPort1 = ((IPEndPoint)commands1.Client.LocalEndPoint).Port;
            using (var receiver = new DsaJpegReceiver(settings))
            using (var sender = new UdpClient())
            {
                receiver.FrameReceived += (stream, jpeg, time) =>
                {
                    Frames.Enqueue(Tuple.Create(stream, jpeg));
                    views[stream].PublishJpeg(jpeg, time);
                };
                receiver.Start();
                Frame(sender, settings.ListenPort0, 0, 0, views[0], 240);
                Frame(sender, settings.ListenPort0, 1, 1, views[1], 241);
                Frame(sender, settings.ListenPort1, 0, 2, views[2], 242);

                var destination = new IPEndPoint(IPAddress.Loopback, settings.ListenPort0);
                var bad = Header(0, settings.MaxJpegBytes + 1);
                sender.Send(bad, bad.Length, destination);
                bad = Header(0, 20000);
                sender.Send(bad, bad.Length, destination);
                sender.Send(new byte[8192], 8192, destination);
                Frame(sender, settings.ListenPort0, 1, 1, views[1], 243);
                Check(Frames.IsEmpty, "invalid length and incomplete JPEG are not published; next header resynchronizes");
                var fiveByteTail = Jpeg(244).ToList();
                int padding = (5 - fiveByteTail.Count % 8192 + 8192) % 8192;
                if (padding < 4) padding += 8192;
                var comment = new byte[padding];
                comment[0] = 0xFF; comment[1] = 0xFE;
                comment[2] = (byte)((padding - 2) >> 8); comment[3] = (byte)(padding - 2);
                fiveByteTail.InsertRange(fiveByteTail.Count - 2, comment);
                Send(sender, settings.ListenPort0, 0, fiveByteTail.ToArray());
                Wait(() => !Frames.IsEmpty, "five-byte JPEG tail lost");
                Frames.TryDequeue(out var tailFrame);
                Check(tailFrame.Item2.SequenceEqual(fiveByteTail), "five-byte tail is not mistaken for frame header");
                Wait(() => ((Image)views[0].FindName("Image")).Source is BitmapSource b && b.PixelWidth == 244, "JPEG with five-byte tail not decoded");

                Command(commands0, 1, 0, "default first DSA stream uses legacy [1,0]");
                Command(commands0, 1, 0, "fast command repeats without another button click");
                receiver.SendSpeedCommand(false);
                Drain(commands0); Drain(commands1);
                Thread.Sleep(700);
                Check(commands0.Available == 0 && commands1.Available == 0, "slow mode stops keepalive, allowing original ScreenCut timeout");
                Check(receiver.SendSpeedCommand(true, 1), "second DSA stream selected");
                Command(commands0, 0, 1, "second DSA stream uses [0,1] on first computer");
                receiver.SendSpeedCommand(false);
                Drain(commands0); Drain(commands1);
                Check(receiver.SendSpeedCommand(true, 2), "ECG stream selected");
                Command(commands1, 1, 0, "ECG uses [1,0] at separate configured IP and port");
                Command(commands1, 1, 0, "ECG keepalive repeats");
                Check(commands0.Available == 0, "inactive computer receives no [0,0] keepalive");
            }
            Drain(commands0); Drain(commands1);
            Thread.Sleep(500);
            Check(commands0.Available == 0 && commands1.Available == 0, "shutdown stops speed timer");
            }
            var lastDisplay = typeof(DsaImageView).GetField("_lastDisplayedUtc", BindingFlags.Instance | BindingFlags.NonPublic);
            lastDisplay.SetValue(views[1], DateTime.UtcNow.AddSeconds(-3));
            views[1].StaleAfterSeconds = 10;
            Thread.Sleep(70);
            Dispatcher.CurrentDispatcher.Invoke(new Action(() => { }), DispatcherPriority.ApplicationIdle);
            Check(((TextBlock)views[1].FindName("StatusText")).Text == "在线", "slow JPEG view tolerates interval longer than two seconds");
            views[1].StaleAfterSeconds = 2;
            Wait(() => ((TextBlock)views[1].FindName("StatusText")).Text == "过期", "fast view freshness threshold not applied");
            Check(true, "fast JPEG view retains two-second stale warning");

            // 只渲染这三个 JPEG 控件，截图不包含桌面或硬件主窗口。
            var panel = new StackPanel { Orientation = Orientation.Horizontal };
            for (int i = 0; i < views.Length; i++)
            {
                views[i].Caption = i == 2 ? "心电影像" : "DSA画面" + (i + 1);
                views[i].Width = 420; views[i].Height = 310;
                panel.Children.Add(views[i]);
            }
            panel.Measure(new Size(1260, 310)); panel.Arrange(new Rect(0, 0, 1260, 310)); panel.UpdateLayout();
            var render = new RenderTargetBitmap(1260, 310, 96, 96, PixelFormats.Pbgra32);
            render.Render(panel);
            var png = new PngBitmapEncoder(); png.Frames.Add(BitmapFrame.Create(render));
            using (var file = File.Create(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "dsa-check.png"))) png.Save(file);
            foreach (var view in views) view.Dispose();
            using (new UdpClient(settings.ListenPort0)) { }
            using (new UdpClient(settings.ListenPort1)) { }
            Check(true, "receiver releases both ports on shutdown");
            app.Shutdown();
            return 0;
        }
        catch (Exception ex) { Console.Error.WriteLine("FAIL: " + ex.Message); return 1; }
    }
}
