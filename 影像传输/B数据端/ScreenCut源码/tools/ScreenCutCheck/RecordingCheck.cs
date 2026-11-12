using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows;
using MasterConsole.Services;
using Microsoft.Web.WebView2.Wpf;
using ScreenCut;

internal static partial class Program
{
    private static async Task<Dictionary<string, object>> MediaApi(int port, string path = "/v3/paths/list")
    {
        using (var http = new HttpClient(new HttpClientHandler { UseProxy = false }))
            return Json.Deserialize<Dictionary<string, object>>(await http.GetStringAsync("http://127.0.0.1:" + port + path));
    }
    private static string[] RecordFiles(RecordingHost host, int i) =>
        Directory.Exists(Path.Combine(host.SessionDirectory, StreamSettings.Paths[i])) ?
        Directory.GetFiles(Path.Combine(host.SessionDirectory, StreamSettings.Paths[i]), "*.mp4").OrderBy(f=>f).ToArray() : new string[0];
    private static long RecordBytes(RecordingHost host, int i) => RecordFiles(host, i).Sum(f => new FileInfo(f).Length);
    private static Process RelayProcess(RecordingHost host) => (Process)typeof(RecordingHost).GetField("_media", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(host);

    private static async Task<string> RunMediaTool(string name, string arguments)
    {
        using (var p = new Process { StartInfo = new ProcessStartInfo(StreamingSettings.FindRuntime(name), arguments) {
            UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true
        } })
        {
            p.Start();
            var output = p.StandardOutput.ReadToEndAsync();
            var error = p.StandardError.ReadToEndAsync();
            await Task.Run(() => { if (!p.WaitForExit(20000)) { p.Kill(); throw new TimeoutException(name); } });
            string text = await output, err = await error;
            if (p.ExitCode != 0) throw new Exception(name + ": " + err);
            return text;
        }
    }
    private static async Task<double> CheckFile(string path)
    {
        var data = Json.Deserialize<Dictionary<string, object>>(await RunMediaTool("ffprobe.exe",
            "-v error -show_entries stream=codec_name,width,height -show_entries format=duration -of json \"" + path + "\""));
        var streams = ((System.Collections.ArrayList)data["streams"]).Cast<Dictionary<string,object>>().ToArray();
        Check(streams.Length == 1 && (string)streams[0]["codec_name"] == "h264", "recording contains original H.264: " + Path.GetFileName(path));
        double duration = double.Parse((string)((Dictionary<string,object>)data["format"])["duration"], CultureInfo.InvariantCulture);
        await RunMediaTool("ffmpeg.exe", "-hide_banner -v error -i \"" + path + "\" -frames:v 3 -f null -");
        Check(duration > 0, "recording decodes: " + duration.ToString("F2", CultureInfo.InvariantCulture) + " s");
        return duration;
    }
    private static async Task RecordingAsync(WebView2 view)
    {
        string output = Environment.GetEnvironmentVariable("IMAGING_TEST_OUTPUT") ?? Work;
        string recordRoot = Path.Combine(output, "recording-check-" + DateTime.Now.ToString("yyyyMMdd-HHmmss"));
        Directory.CreateDirectory(recordRoot);
        File.WriteAllText(Path.Combine(output, "recording-work.txt"), Work + Environment.NewLine + recordRoot);
        File.WriteAllText(Path.Combine(Work,"dsa_screencut.ini"), "screenCutIp=127.0.0.1\nwebrtcPort=38889\nstreamPath=stream_dsa\n[relay]\nwebrtcPort=38891\nmediaPort=38191\napiPort=39999\nrecordingRoot=" + recordRoot);
        File.WriteAllText(Path.Combine(Work,"hik_cameras.ini"), "[camera1]\nip=127.0.0.1\nwebrtcPort=38890\n[camera2]\nip=127.0.0.1\nwebrtcPort=38890\n");
        var relaySettings = StreamSettings.LoadRelay(Work);
        Check(relaySettings.Sources[0].url == "whep://127.0.0.1:38889/stream_dsa/whep" &&
            relaySettings.Sources[2].url == "whep://127.0.0.1:38890/stream_cam2/whep", "INI drives upstream WHEP sources");
        Check(StreamSettings.Load(Work).All(s => s.url.StartsWith("http://127.0.0.1:38891/")), "all three playback URLs use the local relay");
        var capture = TestSettings();
        capture.SetVideo(1920,1080,25,6000);
        var monitor = MonitorSettings();
        monitor.ListenIp = "127.0.0.1";
        monitor.SetVideo(1920,1080,25,4096);
        string screenConfig = Path.Combine(Work, "record-screen.yml"), monitorConfig = Path.Combine(Work, "record-monitor.yml");
        File.WriteAllText(screenConfig, capture.MediaMtxConfig(new int[2], shared:true)
            .Replace("webrtcLocalTCPAddress: 127.0.0.1:38189", "webrtcLocalTCPAddress: ''")
            .Replace("webrtcAdditionalHosts: [127.0.0.1]", "webrtcAdditionalHosts: [127.0.0.2]"));
        File.WriteAllText(monitorConfig, monitor.MediaMtxConfig(new int[2], shared:true)
            .Replace("    path: stream_dsa\n", "")
            .Replace("webrtcLocalTCPAddress: 127.0.0.1:38190", "webrtcLocalTCPAddress: ''")
            .Replace("webrtcAdditionalHosts: [127.0.0.1]", "webrtcAdditionalHosts: [127.0.0.2]"));
        Process screen = null, cameras = null, dsa = null, cam1 = null, cam2 = null;
        Process StartScreen() => Spawn("mediamtx.exe", "\"" + screenConfig + "\"");
        Process StartCameras() => Spawn("mediamtx.exe", "\"" + monitorConfig + "\"");
        Process EncodeDsa() => Spawn("ffmpeg.exe", capture.FfmpegArguments(Rectangle.Empty, true));
        Process EncodeCamera(int i) => Spawn("ffmpeg.exe", monitor.FfmpegArguments(Rectangle.Empty, true).Replace("/stream_dsa", "/stream_cam" + i));
        RecordingHost host = null;
        using (var screenProxy = new WeakUdpProxy(capture.MediaPort, impair:false))
        using (var proxy = new WeakUdpProxy(monitor.MediaPort, impair:false))
        using (var player = new WebRtcPlayer(view))
        {
            try
            {
                host = new RecordingHost(relaySettings);
                string lastStatus = "";
                host.Status += s => lastStatus = s;
                host.Start();
                await player.InitializeAsync(relaySettings.Playback, Path.Combine(Work,"WebView2-recording"));
                await UntilAsync(async () => {
                    try { return (await MediaApi(39999, "/v3/info")).ContainsKey("version"); }
                    catch (HttpRequestException) { return false; }
                }, "master starts before either sender");
                Check(Enumerable.Range(0,3).All(i=>RecordFiles(host,i).Length==0), "no source produces no empty recording");
                cameras = StartCameras(); await Task.Delay(800);
                cam1 = EncodeCamera(1); cam2 = EncodeCamera(2);
                await UntilAsync(async () => {
                    var s = Streams(await Snapshot(player));
                    return s.Skip(1).All(v=>N(v,"framesDecoded")>20 && !(bool)v["stale"]) && (bool)s[0]["stale"];
                }, "monitor arrives later; both cameras display while DSA remains absent", 50);
                screen = StartScreen(); await Task.Delay(800); dsa = EncodeDsa();
                await UntilAsync(async () => Streams(await Snapshot(player)).All(s=>N(s,"framesDecoded")>30 && !(bool)s["stale"]), "all three 1080p H.264 streams display through local relay",50);
                await UntilAsync(() => Task.FromResult(Enumerable.Range(0,3).All(i=>RecordBytes(host,i)>0) && lastStatus.StartsWith("录像中：")), "recording status reflects files growing");
                await Task.Delay(12000);
                Check(Enumerable.Range(0,3).All(i=>RecordFiles(host,i).Length==1), "continuous streams create exactly one file each");
                foreach (int i in Enumerable.Range(0,3))
                {
                    string file = RecordFiles(host,i).Single();
                    double duration = await CheckFile(file);
                    double elapsed = (DateTime.UtcNow-new FileInfo(file).CreationTimeUtc).TotalSeconds;
                    Console.WriteLine("RECORD_DURATION stream=" + i + " duration=" + duration.ToString("F3") + " wall=" + elapsed.ToString("F3"));
                    Check(Math.Abs(duration-elapsed)<=2, "recording duration matches elapsed time within two seconds");
                }
                var before = Streams(await Snapshot(player));
                await player.SelectAsync("camera2"); await Task.Delay(1500);
                Check(before.Zip(Streams(await Snapshot(player)),(a,b)=>N(a,"generation")==N(b,"generation") && N(b,"framesDecoded")>N(a,"framesDecoded")).All(b=>b), "main-picture switching preserves local peer connections");
                var paths = (System.Collections.ArrayList)(await MediaApi(39998))["items"];
                Check(paths.Cast<Dictionary<string,object>>().Where(p=>((string)p["name"]).StartsWith("stream_cam"))
                    .All(p=>((System.Collections.ArrayList)p["readers"]).Count==1), "each remote camera has exactly one reader during display and recording");
                var trafficA = await MediaApi(39998,"/v3/webrtcsessions/list");
                await Task.Delay(10000);
                var trafficB = await MediaApi(39998,"/v3/webrtcsessions/list");
                File.WriteAllText(Path.Combine(recordRoot,"upstream-traffic.json"), Json.Serialize(new { intervalSeconds=10, before=trafficA, after=trafficB }));
                long[] bytes = Enumerable.Range(0,3).Select(i=>RecordBytes(host,i)).ToArray();
                var window = Window.GetWindow(view);
                window.WindowState = WindowState.Minimized; window.Hide(); await Task.Delay(10000); window.Show(); window.WindowState = WindowState.Normal;
                Check(Enumerable.Range(0,3).All(i=>RecordBytes(host,i)>bytes[i] && RecordFiles(host,i).Length==1), "minimized and hidden window keeps recording every stream");
                using (var image = File.Create(Path.Combine(recordRoot,"webview.png")))
                    await view.CoreWebView2.CapturePreviewAsync(Microsoft.Web.WebView2.Core.CoreWebView2CapturePreviewImageFormat.Png,image);
                var stable = Streams(await Snapshot(player));
                long camBytes = RecordBytes(host,1);
                Stop(dsa); Stop(screen);
                await UntilAsync(async ()=> (bool)Streams(await Snapshot(player))[0]["stale"], "screenshot sender exit affects only DSA display");
                await Task.Delay(6000);
                var current = Streams(await Snapshot(player));
                Check(stable.Skip(1).Zip(current.Skip(1),(a,b)=>N(a,"generation")==N(b,"generation") && N(b,"framesDecoded")>N(a,"framesDecoded")).All(b=>b) &&
                    RecordBytes(host,1)>camBytes && RecordFiles(host,1).Length==1, "screenshot exit preserves camera displays and continuous recordings");
                screen=StartScreen(); await Task.Delay(800); dsa=EncodeDsa();
                await UntilAsync(async ()=> !(bool)Streams(await Snapshot(player))[0]["stale"] && RecordFiles(host,0).Length==2, "screenshot recovery automatically creates a new recording",50);
                long dsaBytes=RecordBytes(host,0); stable=Streams(await Snapshot(player));
                Stop(cam1); Stop(cam2); Stop(cameras);
                await UntilAsync(async ()=>Streams(await Snapshot(player)).Skip(1).All(s=>(bool)s["stale"]), "monitor sender exit marks only camera pictures unavailable");
                await Task.Delay(6000);
                current=Streams(await Snapshot(player));
                Check(!(bool)current[0]["stale"] && N(current[0],"generation")==N(stable[0],"generation") && RecordBytes(host,0)>dsaBytes && RecordFiles(host,0).Length==2, "monitor exit preserves DSA display and recording");
                cameras=StartCameras(); await Task.Delay(800); cam1=EncodeCamera(1);cam2=EncodeCamera(2);
                await UntilAsync(async ()=>Streams(await Snapshot(player)).Skip(1).All(s=>!(bool)s["stale"]) &&
                    RecordFiles(host,1).Length==2 && RecordFiles(host,2).Length==2, "monitor recovery automatically resumes both recordings",50);

                dsaBytes=RecordBytes(host,0);
                proxy.Paused=true;
                await UntilAsync(async ()=> {
                    var list=(System.Collections.ArrayList)(await MediaApi(39999))["items"];
                    return list.Cast<Dictionary<string,object>>().Where(p=>((string)p["name"]).StartsWith("stream_cam")).All(p=>!(bool)p["ready"]);
                }, "simulated monitor network outage disconnects only its upstream sources",60);
                Check(RecordBytes(host,0)>dsaBytes && RecordFiles(host,0).Length==2, "monitor network outage preserves the DSA recording");
                proxy.Paused=false;
                await UntilAsync(async ()=>Streams(await Snapshot(player)).All(s=>!(bool)s["stale"]) &&
                    RecordFiles(host,1).Length==3 && RecordFiles(host,2).Length==3, "network restoration reconnects and starts new camera files",60);

                camBytes=RecordBytes(host,1);
                screenProxy.Paused=true;
                await UntilAsync(async ()=> !(bool)(await MediaApi(39999,"/v3/paths/get/stream_dsa"))["ready"],
                    "simulated screenshot network outage disconnects only DSA",60);
                Check(RecordBytes(host,1)>camBytes && RecordFiles(host,1).Length==3 && RecordFiles(host,2).Length==3,
                    "screenshot network outage preserves both continuous camera recordings");
                screenProxy.Paused=false;
                await UntilAsync(async ()=>Streams(await Snapshot(player)).All(s=>!(bool)s["stale"]) && RecordFiles(host,0).Length==3,
                    "screenshot network restoration automatically starts a new recording",60);

                var oldRelay=RelayProcess(host);
                Stop(oldRelay);
                await UntilAsync(async ()=>!ReferenceEquals(RelayProcess(host),oldRelay) && Streams(await Snapshot(player)).All(s=>!(bool)s["stale"]) &&
                    RecordFiles(host,0).Length==4 && RecordFiles(host,1).Length==4 && RecordFiles(host,2).Length==4, "master relay process restarts with the same session directory",50);
                await Task.Delay(8000);
                string finalSession = host.SessionDirectory;
                await host.StopAsync();
                host = null;
                Check(File.ReadAllText(Path.Combine(finalSession,"mediamtx.log")).Contains("shutting down gracefully"), "normal close sends MediaMTX a graceful termination signal");
                foreach(int i in Enumerable.Range(0,3)) await CheckFile(Directory.GetFiles(Path.Combine(finalSession,StreamSettings.Paths[i]),"*.mp4").OrderBy(f=>f).Last());
                host = new RecordingHost(relaySettings); host.Start();
                await UntilAsync(async ()=> Streams(await Snapshot(player)).All(s=>!(bool)s["stale"]) &&
                    Enumerable.Range(0,3).All(i=>RecordBytes(host,i)>0), "senders start first; restarted master displays and records",50);
                await Task.Delay(8000);
                Stop(RelayProcess(host));
                foreach(int i in Enumerable.Range(0,3)) await CheckFile(RecordFiles(host,i).Last());
                Console.WriteLine("RECORDING_OUTPUT " + recordRoot);
                await host.StopAsync(); host=null;
            }
            finally { if(host!=null) await host.StopAsync(); }
        }
    }
}
