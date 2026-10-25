using System;
using System.Collections.Specialized;
using System.Configuration;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;

namespace ScreenCut
{
    public sealed class CameraSettings
    {
        public bool Enabled;
        public string Ip, LocalIp, Username, Password;
        public int RtspPort, Channel;
        public string SourceUrl(int localPort) => "rtsp://" + Uri.EscapeDataString(Username) + ":" +
            Uri.EscapeDataString(Password) + "@127.0.0.1:" + localPort + "/Streaming/Channels/" + Channel;
    }

    public sealed class StreamingSettings
    {
        public string MasterIp, ListenIp;
        public int WebRtcPort, MediaPort, RtspPort, ApiPort, VideoWidth, VideoHeight, VideoFps, BitrateKbps;
        public readonly CameraSettings[] Cameras = new CameraSettings[2];

        public static StreamingSettings Load(NameValueCollection values = null, string cameraIniPath = null)
        {
            bool readCameraIni = values == null || cameraIniPath != null;
            values = values ?? ConfigurationManager.AppSettings;
            string Get(string key, string fallback) => values[key] ?? fallback;
            int Number(string key, int fallback, int min, int max)
            {
                if (!int.TryParse(Get(key, fallback.ToString(CultureInfo.InvariantCulture)), out int n) || n < min || n > max)
                    throw new ConfigurationErrorsException(key + " 超出允许范围 " + min + "–" + max);
                return n;
            }
            var s = new StreamingSettings {
                MasterIp = Ipv4(Get("MasterIp", "192.168.50.1")),
                ListenIp = Ipv4(Get("RelayListenIp", "192.168.60.1")),
                WebRtcPort = Number("WebRtcPort", 8889, 1024, 65535),
                MediaPort = Number("WebRtcMediaPort", 8189, 1024, 65535),
                RtspPort = Number("LocalRtspPort", 8554, 1024, 65535),
                ApiPort = Number("LocalApiPort", 9997, 1024, 65535),
                VideoWidth = Number("VideoWidth", 960, 320, 1920),
                VideoHeight = Number("VideoHeight", 540, 180, 1080),
                VideoFps = Number("VideoFps", 25, 10, 30),
                BitrateKbps = Number("VideoBitrateKbps", 512, 128, 3000)
            };
            if ((s.VideoWidth & 1) != 0 || (s.VideoHeight & 1) != 0)
                throw new ConfigurationErrorsException("H.264 视频宽高必须为偶数。");
            if (new System.Collections.Generic.HashSet<int> { s.WebRtcPort, s.MediaPort, s.RtspPort, s.ApiPort }.Count != 4)
                throw new ConfigurationErrorsException("四个 TCP 服务端口不可相同。");
            for (int i = 0; i < 2; i++)
            {
                string p = "Camera" + (i + 1);
                if (!bool.TryParse(Get(p + "Enabled", "true"), out bool enabled))
                    throw new ConfigurationErrorsException(p + "Enabled 必须是 true/false。");
                s.Cameras[i] = new CameraSettings {
                    Enabled = enabled,
                    Ip = Ipv4(Get(p + "Ip", i == 0 ? "192.168.1.64" : "192.168.1.32")),
                    LocalIp = Ipv4(Get(p + "LocalIp", i == 0 ? "192.168.1.96" : "192.168.1.97")),
                    Username = Get(p + "Username", "admin"), Password = Get(p + "Password", ""),
                    RtspPort = Number(p + "RtspPort", 554, 1, 65535),
                    Channel = Number(p + "RtspChannel", 102, 101, 99999)
                };
                if (readCameraIni) ReadCameraCredentials(s.Cameras[i], i + 1, cameraIniPath);
            }
            return s;
        }

        private static void ReadCameraCredentials(CameraSettings camera, int index, string path)
        {
            for (var d = new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory); path == null && d != null; d = d.Parent)
            {
                string candidate = Path.Combine(d.FullName, "config", "hik_cameras.ini");
                if (File.Exists(candidate)) { path = candidate; break; }
            }
            if (path == null) return;
            string section = "";
            int channel = camera.Channel / 100, stream = camera.Channel % 100 - 1;
            foreach (string raw in File.ReadAllLines(path, Encoding.UTF8))
            {
                string line = raw.Trim();
                if (line.StartsWith(";") || line.StartsWith("#")) continue;
                if (line.StartsWith("[") && line.EndsWith("]")) { section = line.Substring(1, line.Length - 2); continue; }
                if (!section.Equals("camera" + index, StringComparison.OrdinalIgnoreCase)) continue;
                int eq = line.IndexOf('='); if (eq <= 0) continue;
                string key = line.Substring(0, eq).Trim().ToLowerInvariant(), value = line.Substring(eq + 1).Trim();
                if (key == "username") camera.Username = value;
                if (key == "password") camera.Password = value;
                if (key == "channel" && (!int.TryParse(value, out channel) || channel < 1 || channel > 999))
                    throw new ConfigurationErrorsException("相机 channel 必须为 1–999。");
                if (key == "stream" && (!int.TryParse(value, out stream) || stream < 0 || stream > 1))
                    throw new ConfigurationErrorsException("相机 stream 必须为 0（主码流）或 1（子码流）。");
            }
            camera.Channel = channel * 100 + stream + 1;
        }

        public static string Ipv4(string text)
        {
            if (!IPAddress.TryParse(text, out var ip) || ip.AddressFamily != AddressFamily.InterNetwork ||
                ip.Equals(IPAddress.Any) || ip.GetAddressBytes()[0] >= 224)
                throw new ConfigurationErrorsException("IPv4 地址无效：" + text);
            return ip.ToString();
        }

        private static string Yaml(string text) => "'" + text.Replace("'", "''") + "'";
        public string MediaMtxConfig(int[] cameraPorts)
        {
            // 只有 WebRTC 信令/媒体对外；RTSP 发布、相机接入、管理 API 均限定本机。
            var y = new StringBuilder();
            y.AppendLine("logLevel: info\nlogDestinations: [stdout]\nreadTimeout: 3s\nwriteTimeout: 3s\nwriteQueueSize: 32\nudpMaxPayloadSize: 1200");
            y.AppendLine("authMethod: internal\nauthInternalUsers:");
            y.AppendLine("- user: any\n  ips: [127.0.0.1]\n  permissions:\n  - action: publish\n    path: stream_dsa\n  - action: read\n  - action: api");
            y.AppendLine("- user: any\n  ips: [" + MasterIp + "]\n  permissions:\n  - action: read");
            y.AppendLine("api: true\napiAddress: 127.0.0.1:" + ApiPort);
            y.AppendLine("rtsp: true\nrtspAddress: 127.0.0.1:" + RtspPort + "\nrtspTransports: [tcp]\nrtmp: false\nhls: false\nsrt: false\nmoq: false\nplayback: false");
            y.AppendLine("webrtc: true\nwebrtcAddress: " + ListenIp + ":" + WebRtcPort);
            y.AppendLine("webrtcAllowOrigins: ['http://vi-imaging.local']\nwebrtcLocalUDPAddress: " + ListenIp + ":" + MediaPort);
            // Keep UDP as the low-latency path, but expose TCP on the same port for SD-WAN links that drop ICE UDP.
            y.AppendLine("webrtcLocalTCPAddress: " + ListenIp + ":" + MediaPort + "\nwebrtcIPsFromInterfaces: false\nwebrtcAdditionalHosts: [" + ListenIp + "]\nwebrtcICEServers2: []");
            y.AppendLine("paths:\n  stream_dsa:\n    source: publisher");
            for (int i = 0; i < 2; i++)
                if (Cameras[i].Enabled)
                    y.AppendLine("  stream_cam" + (i + 1) + ":\n    source: " + Yaml(Cameras[i].SourceUrl(cameraPorts[i])) +
                        "\n    rtspTransport: tcp\n    sourceOnDemand: false");
            return y.ToString();
        }

        public string FfmpegArguments(Rectangle region, bool testPattern = false)
        {
            string input = testPattern ? "-re -f lavfi -i testsrc2=size=" + VideoWidth + "x" + VideoHeight + ":rate=" + VideoFps + " " :
                "-f gdigrab -framerate " + VideoFps + " -draw_mouse 0 -offset_x " + region.X + " -offset_y " + region.Y +
                " -video_size " + region.Width + "x" + region.Height + " -i desktop ";
            return "-hide_banner -loglevel warning -nostats -nostdin " + input +
                "-an -vf \"scale=" + VideoWidth + ":" + VideoHeight + ":force_original_aspect_ratio=decrease:force_divisible_by=2," +
                "pad=" + VideoWidth + ":" + VideoHeight + ":(ow-iw)/2:(oh-ih)/2,format=yuv420p\" " +
                "-c:v libx264 -preset veryfast -tune zerolatency -threads 2 -profile:v baseline -level:v 4.0 " +
                "-b:v " + BitrateKbps + "k -minrate " + BitrateKbps + "k -maxrate " + BitrateKbps + "k -bufsize " + (BitrateKbps / 10) + "k " +
                "-g " + VideoFps + " -keyint_min " + VideoFps + " -bf 0 -x264-params \"nal-hrd=cbr:force-cfr=1:scenecut=0\" " +
                "-flush_packets 1 -muxdelay 0 -rtsp_transport tcp -f rtsp rtsp://127.0.0.1:" + RtspPort + "/stream_dsa";
        }

        public static Rectangle ValidateRegion(int x, int y, int w, int h, Rectangle desktop)
        {
            if (w < 2 || h < 2 || x < desktop.Left || y < desktop.Top || (long)x + w > desktop.Right || (long)y + h > desktop.Bottom)
                throw new ArgumentException("截图区域必须在当前桌面范围内，宽高至少为 2 像素。");
            return new Rectangle(x, y, w, h);
        }

        public static string FindRuntime(string filename)
        {
            for (var d = new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory); d != null; d = d.Parent)
                foreach (string sub in new[] { "runtime", Path.Combine("依赖", "runtime") })
                {
                    string p = Path.Combine(d.FullName, sub, filename);
                    if (File.Exists(p)) return p;
                }
            throw new FileNotFoundException("缺少媒体运行库，请先运行影像传输目录的准备依赖.ps1：" + filename);
        }
    }
}
