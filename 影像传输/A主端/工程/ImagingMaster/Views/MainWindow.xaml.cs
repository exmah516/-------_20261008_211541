using System;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using MasterConsole.Controls;
using MasterConsole.Services;

namespace MasterConsole.Views
{
    public partial class MainWindow : Window
    {
        private readonly DsaJpegReceiver _dsaReceiver;
        private readonly DsaImageView[] _dsaViews;
        private const int DefaultDsaStream = 0;

        public MainWindow()
        {
            InitializeComponent();
            PlaceDefaultWindow();
            _dsaViews = new[] { DsaImageView, DsaImageView1, EcgImageView };
            DsaImageView.StaleAfterSeconds = 2;
            _dsaReceiver = new DsaJpegReceiver(DsaJpegReceiverSettings.Load());
            _dsaReceiver.FrameReceived += DsaFrameReceived;
            _dsaReceiver.StatusChanged += DsaStatusChanged;
            _dsaReceiver.Start();
            Closed += (s, e) =>
            {
                _dsaReceiver.Dispose();
                foreach (var view in _dsaViews) view.Dispose();
                HikCamera1.Dispose();
                HikCamera2.Dispose();
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

        private void DsaFrameReceived(int stream, byte[] jpeg, DateTime receivedUtc)
        {
            if (stream >= 0 && stream < _dsaViews.Length)
                _dsaViews[stream].PublishJpeg(jpeg, receivedUtc);
        }

        private void DsaStatusChanged(int stream, string status, string detail)
        {
            if (stream >= 0 && stream < _dsaViews.Length)
                _dsaViews[stream].UpdateReceiverStatus(status, detail);
        }

        private void DsaFast_Click(object sender, RoutedEventArgs e) => SetDsaSpeed(true);
        private void DsaSlow_Click(object sender, RoutedEventArgs e) => SetDsaSpeed(false);

        private void SetDsaSpeed(bool fast)
        {
            for (int i = 0; i < _dsaViews.Length; i++)
                _dsaViews[i].StaleAfterSeconds = fast && i == DefaultDsaStream ? 2 : 10;
            if (fast && !_dsaReceiver.SendSpeedCommand(true, DefaultDsaStream))
                _dsaViews[DefaultDsaStream].UpdateReceiverStatus("等待", "快发命令未发送，请检查 dsa_screencut.ini");
            if (!fast) _dsaReceiver.SendSpeedCommand(false, DefaultDsaStream);
        }

        private void DisplaySelect_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button button)
                SetDisplayLayout(Convert.ToString(button.Tag, CultureInfo.InvariantCulture));
        }

        private void SetDisplayLayout(string selected)
        {
            UIElement[] views = { DsaViews, HikCamera1, HikCamera2 };
            string[] keys = { "dsa", "camera1", "camera2" };
            int main = Array.IndexOf(keys, selected);
            if (main < 0) main = 0;
            SetDsaSpeed(string.Equals(selected, "dsa", StringComparison.OrdinalIgnoreCase));
            for (int i = 0, aux = 0; i < views.Length; i++)
            {
                if (i == main)
                {
                    Grid.SetColumn(views[i], 0);
                    Grid.SetRow(views[i], 0);
                    Grid.SetRowSpan(views[i], 2);
                }
                else
                {
                    Grid.SetColumn(views[i], 2);
                    Grid.SetRow(views[i], aux++);
                    Grid.SetRowSpan(views[i], 1);
                }
            }
            bool dsa = string.Equals(selected, "dsa", StringComparison.OrdinalIgnoreCase);
            DisplayTopRow.Height = new GridLength(dsa ? 3 : 2, GridUnitType.Star);
            DisplayBottomRow.Height = new GridLength(dsa ? 2 : 3, GridUnitType.Star);
        }
    }
}
