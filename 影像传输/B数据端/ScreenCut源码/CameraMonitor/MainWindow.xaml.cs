using ScreenCut;
using System;
using System.Threading;
using System.Windows;
using System.Windows.Threading;

namespace CameraMonitor
{
    public partial class MainWindow : Window
    {
        private StreamingHost _host;
        private bool _closed;
        private string _pendingStatus, _pendingState, _pendingWarning;

        public MainWindow()
        {
            InitializeComponent();
            var timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(200) };
            timer.Tick += (s, e) => {
                string text = Interlocked.Exchange(ref _pendingStatus, null);
                if (text != null) status.Text = text;
                string state = Interlocked.Exchange(ref _pendingState, null);
                if (state != null) cameraState.Text = state;
                string warning = Interlocked.Exchange(ref _pendingWarning, null);
                if (warning != null) cameraWarning.Text = warning;
            };
            timer.Start();
            Loaded += async (s, e) => {
                try
                {
                    _host = new StreamingHost(StreamingSettings.Load());
                    _host.Status += text => Interlocked.Exchange(ref _pendingStatus, text);
                    _host.CameraState += text => Interlocked.Exchange(ref _pendingState, text);
                    _host.CameraWarning += text => Interlocked.Exchange(ref _pendingWarning, text);
                    await _host.StartAsync();
                }
                catch (Exception ex)
                {
                    if (!_closed) cameraState.Text = StreamingLog.Write("监控未启动：" + ex.Message);
                }
            };
            Closed += (s, e) => {
                _closed = true; timer.Stop(); _host?.Dispose();
                StreamingLog.Write("CameraMonitor 已关闭。");
            };
        }
    }
}
