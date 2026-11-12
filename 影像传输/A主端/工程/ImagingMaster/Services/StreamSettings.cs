using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.RegularExpressions;

namespace MasterConsole.Services
{
    public sealed class StreamEndpoint
    {
        public string key { get; set; }
        public string caption { get; set; }
        public string url { get; set; }
    }

    public sealed class RelaySettings
    {
        public StreamEndpoint[] Sources;
        public int WebRtcPort, MediaPort, ApiPort;
        public string RecordingRoot;
        public StreamEndpoint[] Playback => Sources.Select((s, i) => new StreamEndpoint {
            key = s.key, caption = s.caption,
            url = "http://127.0.0.1:" + WebRtcPort + "/" + StreamSettings.Paths[i] + "/whep"
        }).ToArray();

        public string MediaMtxConfig(string sessionDirectory)
        {
            string Yaml(string text) => "'" + text.Replace("'", "''") + "'";
            var y = new StringBuilder();
            y.AppendLine("logLevel: info\nlogDestinations: [file]\nlogFile: " + Yaml(Path.Combine(sessionDirectory, "mediamtx.log").Replace('\\', '/')));
            y.AppendLine("authMethod: internal\nauthInternalUsers:\n- user: any\n  ips: [127.0.0.1]\n  permissions:\n  - action: read\n  - action: api");
            y.AppendLine("api: true\napiAddress: 127.0.0.1:" + ApiPort);
            y.AppendLine("rtsp: false\nrtmp: false\nhls: false\nsrt: false\nmoq: false\nplayback: false");
            y.AppendLine("webrtc: true\nwebrtcAddress: 127.0.0.1:" + WebRtcPort);
            y.AppendLine("webrtcAllowOrigins: ['http://vi-imaging.local']\nwebrtcLocalUDPAddress: 127.0.0.1:" + MediaPort);
            y.AppendLine("webrtcLocalTCPAddress: 127.0.0.1:" + MediaPort + "\nwebrtcIPsFromInterfaces: false\nwebrtcAdditionalHosts: [127.0.0.1]\nwebrtcICEServers2: []");
            y.AppendLine("pathDefaults:\n  sourceOnDemand: false\n  record: true\n  recordFormat: fmp4");
            y.AppendLine("  recordPath: " + Yaml(Path.Combine(sessionDirectory, "%path", "%Y-%m-%d_%H-%M-%S-%f").Replace('\\', '/')));
            // v1.21.2 分段上限为一天；一秒分片限制异常退出时尚未落盘的尾部。
            y.AppendLine("  recordPartDuration: 1s\n  recordSegmentDuration: 24h\n  recordDeleteAfter: 0s\npaths:");
            for (int i = 0; i < Sources.Length; i++)
                y.AppendLine("  " + StreamSettings.Paths[i] + ":\n    source: " + Yaml(Sources[i].url));
            return y.ToString();
        }
    }

    public static class StreamSettings
    {
        public static readonly string[] Paths = { "stream_dsa", "stream_cam1", "stream_cam2" };
        public static StreamEndpoint[] Load(string configDirectory = null) => LoadRelay(configDirectory).Playback;

        public static RelaySettings LoadRelay(string configDirectory = null)
        {
            string Find(string name)
            {
                if (configDirectory != null) return Path.Combine(configDirectory, name);
                for (var d = new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory); d != null; d = d.Parent)
                {
                    string path = Path.Combine(d.FullName, "config", name);
                    if (File.Exists(path)) return path;
                }
                return null;
            }
            var dsa = ReadIni(Find("dsa_screencut.ini"), "");
            var cam1 = ReadIni(Find("hik_cameras.ini"), "camera1");
            var cam2 = ReadIni(Find("hik_cameras.ini"), "camera2");
            var relay = ReadIni(Find("dsa_screencut.ini"), "relay");
            var settings = new RelaySettings {
                Sources = new[] {
                    Endpoint("dsa", "DSA", Get(dsa, "screenCutIp", "192.168.2.27"), Get(dsa, "webrtcPort", "8889"), Get(dsa, "streamPath", "stream_dsa")),
                    Endpoint("camera1", Get(cam1, "name", "监控 1"), Get(cam1, "ip", "192.168.60.1"), Get(cam1, "webrtcPort", "8890"), Get(cam1, "streamPath", "stream_cam1")),
                    Endpoint("camera2", Get(cam2, "name", "监控 2"), Get(cam2, "ip", "192.168.60.1"), Get(cam2, "webrtcPort", "8890"), Get(cam2, "streamPath", "stream_cam2"))
                },
                WebRtcPort = Port(Get(relay, "webrtcPort", "8891")),
                MediaPort = Port(Get(relay, "mediaPort", "8191")),
                ApiPort = Port(Get(relay, "apiPort", "9999")),
                RecordingRoot = Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, Get(relay, "recordingRoot", "recordings")))
            };
            if (new HashSet<int> { settings.WebRtcPort, settings.MediaPort, settings.ApiPort }.Count != 3)
                throw new FormatException("主端本地中继的三个 TCP 端口不可相同。");
            return settings;
        }
        private static StreamEndpoint Endpoint(string key, string caption, string host, string port, string path)
        {
            if (!IPAddress.TryParse(host, out var ip) || ip.AddressFamily != AddressFamily.InterNetwork || ip.Equals(IPAddress.Any) || ip.GetAddressBytes()[0] >= 224)
                throw new FormatException(key + "：媒体服务 IP 必须是有效 IPv4 地址。");
            int n = Port(port);
            if (!Regex.IsMatch(path, @"\A[a-zA-Z0-9_-]{1,64}\z")) throw new FormatException(key + "：streamPath 无效。");
            return new StreamEndpoint { key = key, caption = caption, url = "whep://" + ip + ":" + n + "/" + path + "/whep" };
        }
        private static int Port(string text)
        {
            if (!int.TryParse(text, out int n) || n < 1024 || n > 65535) throw new FormatException("媒体服务端口无效：" + text);
            return n;
        }
        private static string Get(Dictionary<string, string> map, string key, string fallback) =>
            map.TryGetValue(key, out string text) && text.Length > 0 ? text : fallback;
        public static Dictionary<string, string> ReadIni(string path, string section)
        {
            var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            if (path == null || !File.Exists(path)) return result;
            string current = "";
            foreach (string raw in File.ReadAllLines(path, Encoding.UTF8))
            {
                string line = raw.Trim();
                if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#")) continue;
                if (line.StartsWith("[") && line.EndsWith("]")) { current = line.Substring(1, line.Length - 2).Trim(); continue; }
                if (!string.Equals(current, section, StringComparison.OrdinalIgnoreCase)) continue;
                int n = line.IndexOf('=');
                if (n > 0) result[line.Substring(0, n).Trim()] = line.Substring(n + 1).Trim();
            }
            return result;
        }
    }
}
