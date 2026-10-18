using System;
using System.Windows;
using MasterConsole.Controls;
using MasterConsole.Services;

namespace MasterConsole.Views
{
    public partial class MainWindow : Window
    {
        private readonly VtkMotionReceiver _motionReceiver;

        public MainWindow()
        {
            InitializeComponent();
            PlaceDefaultWindow();
            _motionReceiver = new VtkMotionReceiver();
            _motionReceiver.StateReceived += state => RobotModelViewport.PublishDsaArmState(state);
            _motionReceiver.Start();
            Closed += (s, e) =>
            {
                _motionReceiver.Dispose();
                RobotModelViewport.Dispose();
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
            Top = work.Top;
            Width = sideWidth;
            Height = (work.Height - gap) / 2;
        }

        private void ResetView_Click(object sender, RoutedEventArgs e) => RobotModelViewport.ViewX();
    }
}
