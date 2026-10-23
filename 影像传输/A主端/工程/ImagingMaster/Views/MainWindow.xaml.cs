using System;
using System.Windows;
using System.Windows.Controls;
using MasterConsole.Services;

namespace MasterConsole.Views
{
    public partial class MainWindow : Window
    {
        private readonly WebRtcPlayer _player;
        private bool _closed;
        public MainWindow()
        {
            InitializeComponent(); PlaceDefaultWindow();
            _player = new WebRtcPlayer(VideoSurface);
            Loaded += async (s, e) => {
                try {
                    await _player.InitializeAsync(StreamSettings.Load());
                    if (!_closed) StartupStatus.Visibility = Visibility.Collapsed;
                }
                catch (Exception ex) { if (!_closed) { VideoSurface.Visibility = Visibility.Collapsed; StartupStatus.Text = "播放器未启动：" + ex.Message + "\n请确认已安装 Microsoft Edge WebView2 Runtime。"; } }
            };
            Closed += (s, e) => { _closed = true; _player.Dispose(); };
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
