using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows;
using MasterConsole.Controls;

// 使用真正的摄像头控件、配置、登录和解码链路，不创建机器人或手柄服务。
class Program
{
    [DllImport("HCNetSDK.dll")] static extern int NET_DVR_GetRealPlayerIndex(int realHandle);
    [DllImport("PlayCtrl.dll")] static extern uint PlayM4_GetPlayedFrames(int port);
    [DllImport("PlayCtrl.dll")] static extern bool PlayM4_GetPictureSize(int port, out int width, out int height);
    [DllImport("PlayCtrl.dll")] static extern bool PlayM4_GetBMP(int port, byte[] buffer, uint size, out uint returned);

    [STAThread]
    static int Main(string[] args)
    {
        int camera = args.Length > 0 ? int.Parse(args[0]) : 1;
        string output = args.Length > 1 ? Path.GetFullPath(args[1]) : null;
        var app = new Application();
        var view = new HikCameraView { Tag = camera };
        var window = new Window { Title = "海康取流验证（无机器人控制）", Width = 960, Height = 600, Content = view };
        int result = 1;
        window.Loaded += async (s, e) =>
        {
            try
            {
                var clock = Stopwatch.StartNew();
                while (clock.Elapsed < TimeSpan.FromSeconds(15))
                {
                    await Task.Delay(200);
                    if (view.StatusText == "失败") throw new Exception(view.DetailText);
                    if (view.StatusText != "在线") continue;
                    var session = typeof(HikCameraView).GetField("_session", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(view);
                    int real = (int)session.GetType().GetProperty("RealHandle", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(session);
                    int port = NET_DVR_GetRealPlayerIndex(real);
                    if (port < 0) continue;
                    uint first = PlayM4_GetPlayedFrames(port);
                    await Task.Delay(1000);
                    uint last = PlayM4_GetPlayedFrames(port);
                    if (last <= first) continue;
                    int width, height;
                    if (!PlayM4_GetPictureSize(port, out width, out height) || width <= 0 || height <= 0)
                        throw new Exception("解码图像尺寸无效");
                    if (output != null)
                    {
                        var buffer = new byte[checked(width * height * 4 + 4096)];
                        uint returned;
                        if (!PlayM4_GetBMP(port, buffer, (uint)buffer.Length, out returned) || returned > buffer.Length)
                            throw new Exception("获取解码图像失败");
                        Directory.CreateDirectory(Path.GetDirectoryName(output));
                        using (var stream = new MemoryStream(buffer, 0, (int)returned))
                        using (var bitmap = new System.Drawing.Bitmap(stream))
                            bitmap.Save(output, System.Drawing.Imaging.ImageFormat.Png);
                    }
                    Console.WriteLine("PASS camera=" + camera + " decodedFrames=" + first + "->" + last + " size=" + width + "x" + height);
                    result = 0;
                    break;
                }
                if (result != 0) throw new TimeoutException("15秒内没有连续解码视频帧：" + view.DetailText);
            }
            catch (Exception ex) { Console.Error.WriteLine("FAIL: " + ex.Message); }
            finally { view.Dispose(); window.Close(); }
        };
        app.Run(window);
        return result;
    }
}
