using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using MasterConsole.ViewModels;

namespace VI.Control;

public partial class MainWindow : Window
{
    private readonly ShellRegion _region;
    private bool _closeStopIssued;

    public MainWindow(ShellRegion region, MainViewModel viewModel, StartupSelection selection)
    {
        _region = region;
        InitializeComponent();
        DataContext = region;
        SimulationNotice.Text = selection.Notice;
        SimulationBanner.Visibility = selection.IsSimulation ? Visibility.Visible : Visibility.Collapsed;
        var work = SystemParameters.WorkArea;
        Left = work.Left;
        Top = work.Top;
        Width = Math.Max(MinWidth, work.Width * 0.65 - 8);
        Height = work.Height;
    }

    protected override void OnClosing(CancelEventArgs e)
    {
        if (!_closeStopIssued)
        {
            _closeStopIssued = true;
            foreach (var panel in _region.Panels) StopPanel(panel.Content);
        }
        base.OnClosing(e);
    }

    private static void StopPanel(IInputElement panel)
    {
        ApplicationCommands.Stop.Execute(null, panel);
    }

    private void Panel_Collapsed(object sender, RoutedEventArgs e)
    {
        if (sender is Expander { DataContext: ShellPanel panel } && ReferenceEquals(sender, e.OriginalSource))
            StopPanel(panel.Content);
    }
}
