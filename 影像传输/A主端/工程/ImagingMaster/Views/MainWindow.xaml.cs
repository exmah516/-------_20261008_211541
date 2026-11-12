using System;
using System.Windows;
using System.Windows.Controls;
using MasterConsole.Services;

namespace MasterConsole.Views
{
    public partial class MainWindow : Window
    {
        private readonly WebRtcPlayer _player;
        private RecordingHost _relay;
        private bool _closed, _canClose;
        public MainWindow()
        {
            InitializeComponent(); PlaceDefaultWindow();
            _player = new WebRtcPlayer(VideoSurface);
            Loaded += async (s, e) => {
                bool relayStarted = false;
                try {
                    var settings = StreamSettings.LoadRelay();
                    _relay = new RecordingHost(settings);
                    _relay.Status += text => Dispatcher.BeginInvoke(new Action(() => { if (!_closed) RecordingStatus.Text = text; }));
                    _relay.Start();
                    relayStarted = true;
                    await _player.InitializeAsync(settings.Playback);
                    if (!_closed) StartupStatus.Visibility = Visibility.Collapsed;
                }
                catch (Exception ex) { if (!_closed) { VideoSurface.Visibility = Visibility.Collapsed; StartupStatus.Text = "播放器未启动：" + ex.Message + "\n请确认配置、媒体运行库和 Microsoft Edge WebView2 Runtime。"; if (!relayStarted) RecordingStatus.Text = "未录像：" + ex.Message; } }
            };
            Closing += async (s, e) => {
                if (_canClose) return;
                e.Cancel = true;
                if (_closed) return;
                _closed = true;
                _player.Dispose();
                RecordingStatus.Text = "录像收尾中…";
                if (_relay != null) await _relay.StopAsync();
                _canClose = true;
                Close();
            };
        }
        private void PlaceDefaultWindow()
        {
            var work = SystemParameters.WorkArea;
            const double gap = 8;
            double sideWidth = Math.Max(320, work.Width * 0.35);
            double primaryWidth = work.Width - sideWidth - gap;
            if (primaryWidth < 600)
            {
                primaryWidth = 600;
                sideWidth = Math.Max(320, work.Width - primaryWidth - gap);
            }

            Left = work.Left + primaryWidth + gap;
            Top = work.Top + (work.Height + gap) / 2;
            Width = sideWidth;
            Height = (work.Height - gap) / 2;
        }


        private async void DisplaySelect_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button button && !_closed)
                try { await _player.SelectAsync(Convert.ToString(button.Tag)); }
                catch (Exception ex) { if (!_closed) StartupStatus.Text = ex.Message; }
        }
    }
}
