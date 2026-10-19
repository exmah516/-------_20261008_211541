using System;
using System.Collections.Generic;
using System.Configuration;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;

namespace ScreenCut
{
    internal sealed class HikCameraRelays : IDisposable
    {
        private readonly IntPtr[] _relays = new IntPtr[2];
        private string _masterIp;
        private string _status;

        [DllImport("ImagingCameraRelay.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern IntPtr HikRelayStart(string listenIp, ushort listenPort, string masterIp,
            string cameraIp, ushort cameraPort, string cameraLocalIp);

        [DllImport("ImagingCameraRelay.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void HikRelayStop(IntPtr relay);

        internal string Start(string masterIp)
        {
            if (_masterIp == masterIp) return _status;
            Dispose();
            var results = new List<string>();
            for (int i = 0; i < _relays.Length; ++i)
            {
                string prefix = "Camera" + (i + 1);
                try
                {
                    string listenIp = Address("RelayListenIp");
                    string cameraIp = Address(prefix + "Ip");
                    string localIp = Address(prefix + "LocalIp");
                    ushort port = Port(prefix + "RelayPort");
                    _relays[i] = HikRelayStart(listenIp, port, masterIp, cameraIp, Port(prefix + "Port"), localIp);
                    results.Add("监控" + (i + 1) + (_relays[i] != IntPtr.Zero
                        ? "：监听 " + listenIp + ":" + port + " → " + cameraIp
                        : "：启动失败，检查网口地址和端口占用"));
                }
                catch (Exception ex)
                {
                    results.Add("监控" + (i + 1) + "：启动失败（" + ex.Message + "）");
                }
            }
            _masterIp = masterIp;
            return _status = string.Join("；", results);
        }

        private static string Address(string key)
        {
            string text = ConfigurationManager.AppSettings[key];
            if (!IPAddress.TryParse(text, out IPAddress ip) || ip.AddressFamily != AddressFamily.InterNetwork || ip.Equals(IPAddress.Any))
                throw new ConfigurationErrorsException("请检查 ScreenCut.exe.config 的 " + key);
            return ip.ToString();
        }

        private static ushort Port(string key)
        {
            if (!ushort.TryParse(ConfigurationManager.AppSettings[key], out ushort port) || port == 0)
                throw new ConfigurationErrorsException("请检查 ScreenCut.exe.config 的 " + key);
            return port;
        }

        public void Dispose()
        {
            for (int i = 0; i < _relays.Length; ++i)
            {
                if (_relays[i] != IntPtr.Zero) HikRelayStop(_relays[i]);
                _relays[i] = IntPtr.Zero;
            }
            _masterIp = null;
        }
    }
}
