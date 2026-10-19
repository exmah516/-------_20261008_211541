using System;
using System.Collections.Generic;
using System.Configuration;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace ScreenCut
{
    public partial class MainWindow : Window
    {
        private UdpClient _imageSender;
        private Socket _commandReceiver;
        private IPAddress _masterAddress;
        private int _imageCount;
        private readonly bool[] _fastSend = { false, false };
        private readonly int[] _sendTicks = { 19, 19 };
        private DateTime _lastFastCommandUtc;
        private readonly DispatcherTimer _imageTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(300) };
        private readonly DispatcherTimer _commandTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(300) };
        private readonly HikCameraRelays _cameras = new HikCameraRelays();
        private bool _loaded;

        public MainWindow()
        {
            InitializeComponent();
            txtIP.Text = ConfigurationManager.AppSettings["MasterIp"] ?? "192.168.50.1";
            // 以实际桌面像素初始化，单屏电脑不再默认截取不存在的左侧屏幕。
            var bounds = System.Windows.Forms.Screen.PrimaryScreen.Bounds;
            txtLeft.Text = txtLeft1.Text = bounds.Left.ToString();
            txtTop.Text = txtTop1.Text = bounds.Top.ToString();
            txtWidth.Text = txtWidth1.Text = Math.Min(640, bounds.Width).ToString();
            txtHeight.Text = txtHeight1.Text = Math.Min(480, bounds.Height).ToString();
            _imageTimer.Tick += TimerImage_Tick;
            _commandTimer.Tick += TimerCmd_Tick;
            Loaded += (s, e) => { _loaded = true; StartCameras(); };
            Closed += (s, e) => { StopSending(); _cameras.Dispose(); };
        }

        private IPAddress ReadMasterAddress()
        {
            if (!IPAddress.TryParse(txtIP.Text.Trim(), out IPAddress address) ||
                address.AddressFamily != AddressFamily.InterNetwork || address.Equals(IPAddress.Any))
                throw new ArgumentException("主端 IP 必须是有效的 IPv4 地址。");
            return address;
        }

        private void StartCameras()
        {
            try { cameraStatus.Text = _cameras.Start(ReadMasterAddress().ToString()); }
            catch (Exception ex) { cameraStatus.Text = "摄像头转发未启动：" + ex.Message; }
        }

        private void MasterIp_LostFocus(object sender, RoutedEventArgs e)
        {
            if (_loaded) StartCameras();
        }

        private void Button_Click_Cut1(object sender, RoutedEventArgs e) => Preview(0);
        private void Button_Click_Cut2(object sender, RoutedEventArgs e) => Preview(1);

        private void Preview(int index)
        {
            try
            {
                byte[] jpeg = CaptureJpeg(index);
                using (var stream = new MemoryStream(jpeg))
                {
                    var image = new BitmapImage();
                    image.BeginInit();
                    image.CacheOption = BitmapCacheOption.OnLoad;
                    image.StreamSource = stream;
                    image.EndInit();
                    pictureBox.Source = image;
                }
                sendStatus.Text = "截图预览成功；点击“持续传图”开始发送。";
            }
            catch (Exception ex) { sendStatus.Text = "截图失败：" + ex.Message; }
        }

        private void Button_Click_1(object sender, RoutedEventArgs e) => StartSending(36661, 36662, 2);
        private void Button_Click_2(object sender, RoutedEventArgs e) => StartSending(36663, 36664, 1);

        private void StartSending(int imagePort, int commandPort, int count)
        {
            StopSending();
            try
            {
                _masterAddress = ReadMasterAddress();
                CaptureRectangle(0);
                _imageSender = new UdpClient();
                _imageSender.Client.SendTimeout = 3000;
                _imageSender.Connect(_masterAddress, imagePort);
                _commandReceiver = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
                _commandReceiver.ExclusiveAddressUse = true;
                _commandReceiver.Bind(new IPEndPoint(IPAddress.Any, commandPort));
                _imageCount = count;
                _fastSend[0] = true;
                _fastSend[1] = false;
                _sendTicks[0] = _sendTicks[1] = 19;
                _lastFastCommandUtc = DateTime.UtcNow;
                btn_keep1.IsEnabled = btn_keep2.IsEnabled = false;
                txtIP.IsEnabled = false;
                _commandTimer.Start();
                _imageTimer.Start();
                if (_loaded) StartCameras();
                TimerImage_Tick(null, EventArgs.Empty);
            }
            catch (Exception ex)
            {
                StopSending();
                sendStatus.Text = "未开始传图：" + ex.Message;
            }
        }

        private void Button_Stop(object sender, RoutedEventArgs e)
        {
            StopSending();
            sendStatus.Text = "截图发送已停止；摄像头转发继续运行。";
        }

        private void StopSending()
        {
            _imageTimer.Stop();
            _commandTimer.Stop();
            _commandReceiver?.Close();
            _commandReceiver = null;
            _imageSender?.Close();
            _imageSender = null;
            btn_keep1.IsEnabled = btn_keep2.IsEnabled = true;
            txtIP.IsEnabled = true;
        }

        private Rectangle CaptureRectangle(int index)
        {
            TextBox[] fields = index == 0 ? new[] { txtLeft, txtTop, txtWidth, txtHeight }
                : new[] { txtLeft1, txtTop1, txtWidth1, txtHeight1 };
            var values = new int[4];
            for (int i = 0; i < values.Length; ++i)
                if (!int.TryParse(fields[i].Text, out values[i])) throw new ArgumentException("截图坐标和尺寸必须是整数。");
            var bounds = System.Windows.Forms.SystemInformation.VirtualScreen;
            if (values[2] <= 0 || values[3] <= 0 || values[0] < bounds.Left || values[1] < bounds.Top ||
                (long)values[0] + values[2] > bounds.Right || (long)values[1] + values[3] > bounds.Bottom)
                throw new ArgumentException("截图区域超出当前桌面，请先用左 0、上 0、宽 640、高 480 测试。");
            return new Rectangle(values[0], values[1], values[2], values[3]);
        }

        private byte[] CaptureJpeg(int index)
        {
            var rectangle = CaptureRectangle(index);
            if (!int.TryParse(index == 0 ? p.Text : p1.Text, out int quality) || quality < 1 || quality > 100)
                throw new ArgumentException("JPEG 质量必须为 1–100。");
            using (var bitmap = new Bitmap(rectangle.Width, rectangle.Height, PixelFormat.Format32bppArgb))
            using (var graphics = Graphics.FromImage(bitmap))
            using (var parameters = new EncoderParameters(1))
            using (var stream = new MemoryStream())
            {
                graphics.CopyFromScreen(rectangle.Location, System.Drawing.Point.Empty, rectangle.Size);
                parameters.Param[0] = new EncoderParameter(System.Drawing.Imaging.Encoder.Quality, (long)quality);
                bitmap.Save(stream, ImageCodecInfo.GetImageEncoders().First(codec => codec.MimeType == "image/jpeg"), parameters);
                return stream.ToArray();
            }
        }

        private void TimerImage_Tick(object sender, EventArgs e)
        {
            if (_imageSender == null) return;
            var results = new List<string>();
            for (int index = 0; index < _imageCount; ++index)
            {
                if (!_fastSend[index] && ++_sendTicks[index] % 20 != 0) continue;
                _sendTicks[index] = 0;
                try
                {
                    byte[] jpeg = CaptureJpeg(index);
                    // 保持旧协议：5 字节长度头 + 每包至多 8192 字节，顺序发送，错误直接反馈到界面。
                    byte[] header = { (byte)index, (byte)(jpeg.Length >> 24), (byte)(jpeg.Length >> 16),
                        (byte)(jpeg.Length >> 8), (byte)jpeg.Length };
                    _imageSender.Send(header, header.Length);
                    for (int offset = 0; offset < jpeg.Length; offset += 8192)
                        _imageSender.Client.Send(jpeg, offset, Math.Min(8192, jpeg.Length - offset), SocketFlags.None);
                    results.Add("画面" + (index + 1) + "：已发 " + jpeg.Length + " 字节");
                }
                catch (Exception ex) { results.Add("画面" + (index + 1) + "失败：" + ex.Message); }
            }
            if (results.Count > 0)
            {
                sendStatus.Text = DateTime.Now.ToString("HH:mm:ss") + " " + string.Join("；", results);
                sizeShow.Text = _masterAddress + (_imageCount == 2 ? ":36661" : ":36663");
            }
        }

        private void TimerCmd_Tick(object sender, EventArgs e)
        {
            if (_commandReceiver == null) return;
            try
            {
                // 无数据时立即返回，避免旧 ReceiveFrom 在界面线程上每次阻塞 200 ms。
                for (int i = 0; i < 32 && _commandReceiver.Poll(0, SelectMode.SelectRead); ++i)
                {
                    EndPoint remote = new IPEndPoint(IPAddress.Any, 0);
                    byte[] data = new byte[10];
                    int length = _commandReceiver.ReceiveFrom(data, ref remote);
                    if (!((IPEndPoint)remote).Address.Equals(_masterAddress) || length != 2 || data[0] > 1 || data[1] > 1) continue;
                    if (data[0] == 1 || data[1] == 1)
                    {
                        _lastFastCommandUtc = DateTime.UtcNow;
                        _fastSend[0] = data[0] == 1;
                        _fastSend[1] = data[0] != 1 && data[1] == 1;
                    }
                }
            }
            catch (SocketException ex) { sendStatus.Text = "快发命令接收失败：" + ex.Message; }
            if (DateTime.UtcNow - _lastFastCommandUtc >= TimeSpan.FromSeconds(3))
                _fastSend[0] = _fastSend[1] = false;
        }
    }
}
