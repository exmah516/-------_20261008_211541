using System;
using System.Collections.Generic;
using System.IO;
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
    public static class StreamSettings
    {
        public static StreamEndpoint[] Load(string configDirectory = null)
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
            string host = Get(dsa, "screenCutIp", "192.168.60.1");
            string port = Get(dsa, "webrtcPort", "8889");
            return new[] {
                Endpoint("dsa", "DSA", host, port, Get(dsa, "streamPath", "stream_dsa")),
                Endpoint("camera1", Get(cam1, "name", "监控 1"), Get(cam1, "ip", host), Get(cam1, "webrtcPort", port), Get(cam1, "streamPath", "stream_cam1")),
                Endpoint("camera2", Get(cam2, "name", "监控 2"), Get(cam2, "ip", host), Get(cam2, "webrtcPort", port), Get(cam2, "streamPath", "stream_cam2"))
            };
        }
        private static StreamEndpoint Endpoint(string key, string caption, string host, string port, string path)
        {
            if (!IPAddress.TryParse(host, out var ip) || ip.AddressFamily != AddressFamily.InterNetwork || ip.Equals(IPAddress.Any) || ip.GetAddressBytes()[0] >= 224)
                throw new FormatException(key + "：媒体服务 IP 必须是有效 IPv4 地址。");
            if (!int.TryParse(port, out int n) || n < 1024 || n > 65535) throw new FormatException(key + "：webrtcPort 无效。");
            if (!Regex.IsMatch(path, @"\A[a-zA-Z0-9_-]{1,64}\z")) throw new FormatException(key + "：streamPath 无效。");
            return new StreamEndpoint { key = key, caption = caption, url = "http://" + ip + ":" + n + "/" + path + "/whep" };
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
