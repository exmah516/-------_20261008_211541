using System;
using System.Configuration;
using System.Drawing;
using System.IO;
using System.Windows;
using System.Windows.Media.Imaging;
using System.Threading.Tasks;

namespace ScreenCut
{
    public partial class MainWindow : Window
    {
        private StreamingHost _host;
        private string _activeMaster;
        private bool _closed;
        public MainWindow()
        {
            InitializeComponent();
            StartButton.IsEnabled = false;
            txtIP.Text = ConfigurationManager.AppSettings["MasterIp"] ?? "192.168.50.1";
            var b = System.Windows.Forms.Screen.PrimaryScreen.Bounds;
            int Read(string key, int fallback) => int.TryParse(ConfigurationManager.AppSettings[key], out int n) ? n : fallback;
            txtLeft.Text = Read("CaptureLeft", b.Left).ToString();
            txtTop.Text = Read("CaptureTop", b.Top).ToString();
            txtWidth.Text = Read("CaptureWidth", b.Width).ToString();
            txtHeight.Text = Read("CaptureHeight", b.Height).ToString();
            Loaded += async (s, e) => {
                try { await EnsureHostAsync(); }
                catch (Exception ex) { if (!_closed) sendStatus.Text = ex.Message; }
                finally { if (!_closed) StartButton.IsEnabled = true; }
            };
            Closed += (s, e) => { _closed = true; _host?.Dispose(); };
        }
        private async Task EnsureHostAsync()
        {
            string master = StreamingSettings.Ipv4(txtIP.Text.Trim());
            if (_host != null && _activeMaster == master) return;
            _host?.Dispose(); _host = null;
            var settings = StreamingSettings.Load();
            settings.MasterIp = master;
            var host = new StreamingHost(settings);
            _host = host; _activeMaster = master;
            host.Status += text => {
                if (!_closed) Dispatcher.BeginInvoke(new Action(() => { if (!_closed) cameraStatus.Text = text; }));
            };
            try { await host.StartAsync(); }
            catch { host.Dispose(); if (_host == host) _host = null; throw; }
            if (!_closed) sendStatus.Text = "媒体服务就绪；预览区域后点击“开始传图”。摄像头自动接入。";
        }
        private Rectangle Region()
        {
            if (!int.TryParse(txtLeft.Text, out int x) || !int.TryParse(txtTop.Text, out int y) ||
                !int.TryParse(txtWidth.Text, out int w) || !int.TryParse(txtHeight.Text, out int h))
                throw new ArgumentException("截图坐标和尺寸必须是整数。");
            return StreamingSettings.ValidateRegion(x, y, w, h, System.Windows.Forms.SystemInformation.VirtualScreen);
        }
        private void Preview_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                var r = Region();
                using (var bmp = new Bitmap(r.Width, r.Height))
                using (var g = Graphics.FromImage(bmp))
                using (var stream = new MemoryStream())
                {
                    g.CopyFromScreen(r.Location, System.Drawing.Point.Empty, r.Size);
                    bmp.Save(stream, System.Drawing.Imaging.ImageFormat.Png); stream.Position = 0;
                    var image = new BitmapImage(); image.BeginInit(); image.CacheOption = BitmapCacheOption.OnLoad;
                    image.StreamSource = stream; image.EndInit(); image.Freeze(); pictureBox.Source = image;
                }
                sendStatus.Text = "区域预览成功。";
            }
            catch (Exception ex) { sendStatus.Text = "预览失败：" + ex.Message; }
        }
        private async void Start_Click(object sender, RoutedEventArgs e)
        {
            StartButton.IsEnabled = false;
            try
            {
                var r = Region();
                await EnsureHostAsync();
                if (_closed) return;
                var config = ConfigurationManager.OpenExeConfiguration(ConfigurationUserLevel.None);
                void Set(string key, string value)
                {
                    if (config.AppSettings.Settings[key] == null) config.AppSettings.Settings.Add(key, value);
                    else config.AppSettings.Settings[key].Value = value;
                }
                Set("MasterIp", _activeMaster); Set("CaptureLeft", r.X.ToString()); Set("CaptureTop", r.Y.ToString());
                Set("CaptureWidth", r.Width.ToString()); Set("CaptureHeight", r.Height.ToString());
                config.Save(ConfigurationSaveMode.Modified); ConfigurationManager.RefreshSection("appSettings");
                _host.StartDsa(r);
                sendStatus.Text = "DSA 正在传输 H.264；停止传图不影响两路监控。";
            }
            catch (Exception ex) { if (!_closed) sendStatus.Text = "未开始传图：" + ex.Message; }
            finally { if (!_closed) StartButton.IsEnabled = true; }
        }
        private void Stop_Click(object sender, RoutedEventArgs e)
        {
            _host?.StopDsa(); sendStatus.Text = "DSA 已停止；两路监控继续运行。";
        }
    }
}
