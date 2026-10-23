using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows;
using MasterConsole.Services;
using Microsoft.Web.WebView2.Wpf;
using ScreenCut;

internal static class Program
{
    private static int _checks, _exitCode;
    private static readonly JavaScriptSerializer Json = new JavaScriptSerializer();
    private static readonly string Work = Path.Combine(Path.GetTempPath(), "VI.Imaging.Check-" + Guid.NewGuid().ToString("N"));
    private static readonly List<Process> Children = new List<Process>();
    private static void Check(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
        Console.WriteLine("PASS " + (++_checks) + " " + message);
    }
    private static void Reject(Action action, string message)
    {
        try { action(); } catch (Exception) { Check(true, message); return; }
        throw new Exception("Not rejected: " + message);
    }
    [STAThread]
    public static int Main()
    {
        Directory.CreateDirectory(Work);
        var app = new Application();
        var view = new WebView2();
        var window = new Window { Title = "影像 WebRTC 回环验证（自动关闭）", Width = 900, Height = 620, Content = view };
        window.Loaded += async (s, e) => {
            try { await RunAsync(view); Console.WriteLine("ALL " + _checks + " CHECKS PASSED"); }
            catch (Exception ex) { _exitCode = 1; Console.Error.WriteLine("FAIL " + ex); }
            finally {
                foreach (var p in Children) Stop(p);
                view.Dispose(); window.Close();
            }
        };
        app.Run(window);
        return _exitCode;
    }
    private static StreamingSettings TestSettings()
    {
        var values = new NameValueCollection {
            ["MasterIp"] = "127.0.0.1", ["RelayListenIp"] = "127.0.0.1",
            ["WebRtcPort"] = "38889", ["WebRtcMediaPort"] = "38189", ["LocalRtspPort"] = "38554", ["LocalApiPort"] = "39997"
        };
        for (int i = 1; i <= 2; i++) {
            values["Camera" + i + "Ip"] = "127.0.0.1";
            values["Camera" + i + "LocalIp"] = "127.0.0." + (i + 1);
            values["Camera" + i + "RtspPort"] = "38555";
            values["Camera" + i + "RtspChannel"] = i == 1 ? "101" : "201";
            values["Camera" + i + "Username"] = "camera";
            values["Camera" + i + "Password"] = "test@p=ass#42";
        }
        return StreamingSettings.Load(values);
    }
    private static StreamEndpoint[] Endpoints() => new[] {
        new StreamEndpoint { key = "dsa", caption = "DSA", url = "http://127.0.0.1:38889/stream_dsa/whep" },
        new StreamEndpoint { key = "camera1", caption = "监控 1", url = "http://127.0.0.1:38889/stream_cam1/whep" },
        new StreamEndpoint { key = "camera2", caption = "监控 2", url = "http://127.0.0.1:38889/stream_cam2/whep" }
    };
    private static Process Spawn(string name, string arguments)
    {
        var p = new Process { StartInfo = new ProcessStartInfo(StreamingSettings.FindRuntime(name), arguments) {
            UseShellExecute = false, CreateNoWindow = true, RedirectStandardError = true, RedirectStandardOutput = true, WorkingDirectory = Work
        } };
        p.ErrorDataReceived += (s, e) => { if (e.Data != null && e.Data.Contains("ERR")) Console.WriteLine(StreamingHost.Redact(e.Data)); };
        p.OutputDataReceived += (s, e) => { if (e.Data != null && e.Data.Contains("ERR")) Console.WriteLine(StreamingHost.Redact(e.Data)); };
        p.Start(); p.BeginErrorReadLine(); p.BeginOutputReadLine(); Children.Add(p); return p;
    }
    private static void Stop(Process p)
    {
        try { if (!p.HasExited) { p.Kill(); p.WaitForExit(2000); } } catch (InvalidOperationException) { }
    }
    private static async Task UntilAsync(Func<Task<bool>> test, string message, int seconds = 30)
    {
        var watch = Stopwatch.StartNew();
        while (watch.Elapsed.TotalSeconds < seconds) { if (await test()) { Check(true, message); return; } await Task.Delay(500); }
        throw new TimeoutException(message);
    }
    private static async Task<Dictionary<string, object>> Snapshot(WebRtcPlayer player) => Json.Deserialize<Dictionary<string, object>>(await player.SnapshotAsync());
    private static Dictionary<string, object>[] Streams(Dictionary<string, object> snapshot) =>
        ((System.Collections.ArrayList)snapshot["streams"]).Cast<Dictionary<string, object>>().ToArray();
    private static int N(Dictionary<string, object> row, string key) => row.ContainsKey(key) ? Convert.ToInt32(row[key]) : 0;
    private static async Task RunAsync(WebView2 view)
    {
        var settings = TestSettings();
        Reject(() => StreamingSettings.Load(new NameValueCollection { ["MasterIp"] = "0.0.0.0" }), "invalid IP rejected");
        Reject(() => StreamingSettings.Load(new NameValueCollection { ["VideoBitrateKbps"] = "9999" }), "bitrate outside 2–3Mbps rejected");
        Reject(() => StreamingSettings.Load(new NameValueCollection { ["WebRtcMediaPort"] = "8889" }), "ICE TCP and signaling port conflict rejected");
        Reject(() => StreamingSettings.ValidateRegion(int.MaxValue, 0, int.MaxValue, 10, new Rectangle(0, 0, 1920, 1080)), "overflowing crop rejected");
        Check(StreamingSettings.ValidateRegion(-100, 0, 100, 100, new Rectangle(-100, 0, 2020, 1080)).X == -100, "negative multi-monitor coordinates retained");
        Check(settings.FfmpegArguments(new Rectangle(0,0,640,480)).Contains("-bf 0") && settings.FfmpegArguments(new Rectangle()).Contains("-g 30"), "H.264 zero B-frame/GOP30 settings");
        Check(!StreamingHost.Redact("rtsp://camera:p%40ss@host/test").Contains("p%40ss"), "RTSP credentials redacted");
        string privateIni = Path.Combine(Work, "camera-source.ini");
        File.WriteAllText(privateIni, "[camera1]\nusername=camera\npassword=a=b@c#d\nstream=1\nchannel=1\n", Encoding.UTF8);
        var withIni = StreamingSettings.Load(new NameValueCollection(), privateIni);
        Check(withIni.Cameras[0].Password == "a=b@c#d" && withIni.Cameras[0].Channel == 102, "private INI retains password characters and maps channel/stream");
        Check(withIni.Cameras[0].SourceUrl(1234).Contains("a%3Db%40c%23d@127.0.0.1"), "RTSP credentials safely URI-encoded");
        File.AppendAllText(privateIni, "channel=0\n", Encoding.UTF8);
        Reject(() => StreamingSettings.Load(new NameValueCollection(), privateIni), "invalid private camera channel rejected");
        File.WriteAllText(Path.Combine(Work, "dsa_screencut.ini"), "screenCutIp=127.0.0.1\nlistenPort0=36661\nwebrtcPort=38889\n", Encoding.UTF8);
        File.WriteAllText(Path.Combine(Work, "hik_cameras.ini"), "[camera1]\nip=127.0.0.1\nport=18000\nusername=ignored\npassword=a=b\n[camera2]\nip=127.0.0.1\n", Encoding.UTF8);
        Check(StreamSettings.Load(Work)[1].url == "http://127.0.0.1:38889/stream_cam1/whep", "legacy INI IP mapping retained; old SDK port ignored");
        File.AppendAllText(Path.Combine(Work, "dsa_screencut.ini"), "streamPath=../invalid\n", Encoding.UTF8);
        Reject(() => StreamSettings.Load(Work), "unsafe stream path rejected");
        await BoundSocketCheckAsync();

        string cameraConfig = Path.Combine(Work, "fake-cameras.yml");
        File.WriteAllText(cameraConfig, @"logLevel: warn
rtspAddress: 127.0.0.1:38555
rtspTransports: [tcp]
rtspAuthMethods: [digest]
rtmp: false
hls: false
webrtc: false
srt: false
moq: false
authInternalUsers:
- user: any
  ips: [127.0.0.1]
  permissions:
  - action: publish
- user: camera
  pass: ""test@p=ass#42""
  ips: [127.0.0.2, 127.0.0.3]
  permissions:
  - action: read
paths:
  Streaming/Channels/101:
  Streaming/Channels/201:
", new UTF8Encoding(false));
        var cameras = Spawn("mediamtx.exe", "\"" + cameraConfig + "\"");
        await Task.Delay(1000);
        Check(!cameras.HasExited, "RTSP camera simulator started");
        var cam1 = Spawn("ffmpeg.exe", settings.FfmpegArguments(new Rectangle(), true).Replace("127.0.0.1:38554/stream_dsa", "127.0.0.1:38555/Streaming/Channels/101"));
        var cam2 = Spawn("ffmpeg.exe", settings.FfmpegArguments(new Rectangle(), true).Replace("127.0.0.1:38554/stream_dsa", "127.0.0.1:38555/Streaming/Channels/201"));
        using (var host = new StreamingHost(settings))
        using (var player = new WebRtcPlayer(view))
        {
            host.Status += line => { if (line.Contains("ERR")) Console.WriteLine(line); };
            await host.StartAsync(); host.StartDsa(new Rectangle(), true);
            await player.InitializeAsync(Endpoints(), Path.Combine(Work, "WebView2"));
            await UntilAsync(async () => Streams(await Snapshot(player)).All(s => N(s, "framesDecoded") > 20 && !(bool)s["stale"]), "three H.264 WebRTC streams decoded (camera RTSP authentication + source binding)");
            var before = Streams(await Snapshot(player));
            Check(before.All(s => N(s, "width") == 1280 && N(s, "height") == 720 && (string)s["objectFit"] == "contain"), "all video preserves aspect ratio");
            Check(before.All(s => (string)s["codec"] == "video/H264"), "all negotiated video codecs H.264");
            await player.SelectAsync("camera1"); await Task.Delay(1200);
            await player.SelectAsync("camera2"); await Task.Delay(1200);
            var after = Streams(await Snapshot(player));
            Check(before.Zip(after, (a,b) => N(a,"generation") == N(b,"generation") && N(b,"presented") > N(a,"presented")).All(x=>x), "layout switching keeps all peer connections and decoding alive");
            var presentationBefore = Streams(await Snapshot(player))[0];
            await view.CoreWebView2.ExecuteScriptAsync("document.querySelector('video').pause()");
            await Task.Delay(9500);
            var presentationAfter = Streams(await Snapshot(player))[0];
            Console.WriteLine("PRESENTATION_PAUSED before=" + Json.Serialize(presentationBefore) + " after=" + Json.Serialize(presentationAfter));
            Check(N(presentationAfter,"generation") == N(presentationBefore,"generation") && N(presentationAfter,"framesDecoded") > N(presentationBefore,"framesDecoded"), "paused presentation with healthy H.264 decoding does not rebuild the connection");
            await view.CoreWebView2.ExecuteScriptAsync("void document.querySelector('video').play()");
            await UntilAsync(async () => N(Streams(await Snapshot(player))[0],"presented") > N(presentationAfter,"presented"), "video presentation resumes without reconnecting");
            string output = Environment.GetEnvironmentVariable("IMAGING_TEST_OUTPUT");
            if (!string.IsNullOrEmpty(output)) {
                Directory.CreateDirectory(output);
                using (var file = File.Create(Path.Combine(output,"loopback.png")))
                    await view.CoreWebView2.CapturePreviewAsync(Microsoft.Web.WebView2.Core.CoreWebView2CapturePreviewImageFormat.Png,file);
                File.WriteAllText(Path.Combine(output,"loopback-stats.txt"), await player.SnapshotAsync(), Encoding.UTF8);
            }
            var availableBefore = Streams(await Snapshot(player));
            Stop(cam2);
            await UntilAsync(async () => (bool)Streams(await Snapshot(player))[2]["stale"], "missing camera is marked unavailable independently");
            await Task.Delay(9000);
            var availableAfter = Streams(await Snapshot(player));
            Check(availableBefore.Take(2).Zip(availableAfter.Take(2), (a,b) => N(a,"generation") == N(b,"generation") && N(b,"presented") > N(a,"presented") && !(bool)b["stale"]).All(x=>x), "missing camera does not disconnect DSA or the connected camera");
            host.StopDsa();
            await UntilAsync(async () => (bool)Streams(await Snapshot(player))[0]["stale"], "stopped source is marked unavailable; old picture hidden", 12);
            var bounds = System.Windows.Forms.Screen.PrimaryScreen.Bounds;
            host.StartDsa(new Rectangle(bounds.Left,bounds.Top,Math.Min(640,bounds.Width),Math.Min(480,bounds.Height)));
            await UntilAsync(async () => !(bool)Streams(await Snapshot(player))[0]["stale"], "real GDI desktop H.264 capture recovers without restarting master");
            await ProbeAsync(settings);
            // 结束正常管线，再用同一配置生成器做真实 WebRTC UDP 弱网回环。
            host.Dispose(); Stop(cam1); Stop(cam2); Stop(cameras);
            settings.Cameras[0].Enabled = settings.Cameras[1].Enabled = false;
            string weakConfig = Path.Combine(Work, "weak-network.yml");
            File.WriteAllText(weakConfig, settings.MediaMtxConfig(new int[2])
                .Replace("webrtcLocalTCPAddress: 127.0.0.1:38189", "webrtcLocalTCPAddress: ''")
                .Replace("webrtcAdditionalHosts: [127.0.0.1]", "webrtcAdditionalHosts: [127.0.0.2]"),new UTF8Encoding(false));
            using (var proxy = new WeakUdpProxy(settings.MediaPort))
            {
                var weakMedia = Spawn("mediamtx.exe", "\"" + weakConfig + "\""); await Task.Delay(1000);
                var encoder = Spawn("ffmpeg.exe", settings.FfmpegArguments(new Rectangle(),true));
                await view.CoreWebView2.ExecuteScriptAsync("window.player.start(" + Json.Serialize(new[] { Endpoints()[0] }) + ")");
                await UntilAsync(async () => N(Streams(await Snapshot(player))[0],"presented") > 10, "WebRTC decodes across 50–200ms UDP jitter and 1% packet loss", 45);
                var weakBefore = Streams(await Snapshot(player))[0]; await Task.Delay(8000);
                var weakAfter = Streams(await Snapshot(player))[0];
                Check(N(weakAfter,"presented") > N(weakBefore,"presented"), "weak-network playback continues rather than accumulating a dead stream");
                Check(proxy.Dropped > 0 && proxy.Forwarded > 100, "loss/jitter actually injected into media path");
                Check((string)weakAfter["protocol"] == "udp", "weak-network test cannot bypass packet injection using TCP");
                Console.WriteLine("WEAK_NETWORK " + Json.Serialize(weakAfter) + " dropped=" + proxy.Dropped + " forwarded=" + proxy.Forwarded);
                proxy.Paused = true;
                await UntilAsync(async () => (bool)Streams(await Snapshot(player))[0]["stale"], "4.5-second media interruption hides old video", 10);
                var interrupted = Streams(await Snapshot(player))[0];
                Check(N(interrupted,"generation") == N(weakAfter,"generation"), "4.5-second media interruption hides old video without destroying the connection");
                proxy.Paused = false;
                await UntilAsync(async () => {
                    var recovered = Streams(await Snapshot(player))[0];
                    return !(bool)recovered["stale"] && N(recovered,"framesDecoded") > N(interrupted,"framesDecoded");
                }, "media recovers after a short UDP interruption");
                Check(N(Streams(await Snapshot(player))[0],"generation") == N(weakAfter,"generation"), "short UDP interruption recovers on the same peer connection");
                proxy.Paused = true;
                await UntilAsync(async () => N(Streams(await Snapshot(player))[0],"generation") > N(weakAfter,"generation"), "sustained UDP interruption eventually rebuilds the stalled session", 15);
                proxy.Paused = false;
                try {
                    await UntilAsync(async () => {
                        var recovered = Streams(await Snapshot(player))[0];
                        return !(bool)recovered["stale"] && N(recovered,"framesDecoded") > 10;
                    }, "playback recovers after sustained media loss and session replacement", 30);
                } catch (TimeoutException ex) {
                    throw new TimeoutException(ex.Message + ": " + await player.SnapshotAsync(), ex);
                }
                Stop(encoder); Stop(weakMedia);
            }
            string tcpConfig = Path.Combine(Work, "tcp-only.yml");
            File.WriteAllText(tcpConfig, settings.MediaMtxConfig(new int[2])
                .Replace("webrtcLocalUDPAddress: 127.0.0.1:38189", "webrtcLocalUDPAddress: ''"), new UTF8Encoding(false));
            var tcpMedia = Spawn("mediamtx.exe", "\"" + tcpConfig + "\""); await Task.Delay(1000);
            var tcpEncoder = Spawn("ffmpeg.exe", settings.FfmpegArguments(new Rectangle(), true));
            await view.CoreWebView2.ExecuteScriptAsync("window.player.start(" + Json.Serialize(new[] { Endpoints()[0] }) + ")");
            await UntilAsync(async () => N(Streams(await Snapshot(player))[0],"framesDecoded") > 20, "H.264 stream decodes using TCP-only ICE", 45);
            var tcpStats = Streams(await Snapshot(player))[0];
            Check((string)tcpStats["protocol"] == "tcp", "selected WebRTC ICE candidate uses TCP when UDP is disabled");
            Stop(tcpEncoder); Stop(tcpMedia);
        }
        Check(!Children.Any(p => !p.HasExited), "owned test processes stopped");
    }
    private static async Task BoundSocketCheckAsync()
    {
        var backend = new TcpListener(IPAddress.Loopback,0); backend.Start();
        var camera = new CameraSettings { Ip="127.0.0.1",LocalIp="127.0.0.2",RtspPort=((IPEndPoint)backend.LocalEndpoint).Port };
        using (var source = new BoundRtspSource(camera,Console.WriteLine))
        using (var local = new TcpClient())
        {
            await local.ConnectAsync(IPAddress.Loopback,source.Port);
            using (var upstream = await backend.AcceptTcpClientAsync())
            {
                Check(((IPEndPoint)upstream.Client.RemoteEndPoint).Address.ToString()=="127.0.0.2", "camera connection binds configured local interface");
                byte[] packet = Encoding.ASCII.GetBytes("RTSP/1.0 200 OK\r\nCSeq: 1\r\n\r\n$\0\0\x03" + "NAL");
                await upstream.GetStream().WriteAsync(packet,0,packet.Length);
                var received = new byte[packet.Length];int n=0;
                while(n<received.Length) { int read=await local.GetStream().ReadAsync(received,n,received.Length-n); if(read==0)break; n+=read; }
                Check(packet.SequenceEqual(received), "local RTSP and interleaved compressed bytes preserved");
            }
        }
        backend.Stop();
    }
    private static async Task ProbeAsync(StreamingSettings settings)
    {
        using (var probe = new Process { StartInfo = new ProcessStartInfo(StreamingSettings.FindRuntime("ffprobe.exe"),
            "-v error -rtsp_transport tcp -read_intervals %1 -show_entries stream=codec_name,width,height,has_b_frames,r_frame_rate -of default=noprint_wrappers=1 rtsp://127.0.0.1:"+settings.RtspPort+"/stream_dsa") {
                UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true
            } })
        {
            probe.Start();var text=probe.StandardOutput.ReadToEndAsync();var error=probe.StandardError.ReadToEndAsync();
            var completion = Task.WhenAll(text,error);
            if(await Task.WhenAny(completion,Task.Delay(12000))!=completion && !probe.HasExited) {
                // WaitForExit 由后台执行，UI 消息循环保持运行。
                if(!await Task.Run(()=>probe.WaitForExit(1000))) { probe.Kill();throw new TimeoutException("ffprobe timeout"); }
            }
            string data=await text;
            Check(data.Contains("codec_name=h264") && data.Contains("has_b_frames=0") && data.Contains("width=1280") && data.Contains("height=720") && data.Contains("r_frame_rate=30/1"), "ffprobe validates real desktop H.264, 720p30, no B-frames");
        }
    }
}
