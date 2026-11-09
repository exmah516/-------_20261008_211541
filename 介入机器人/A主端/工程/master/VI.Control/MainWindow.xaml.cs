using System.ComponentModel;
using System.Windows;
using System.Windows.Input;
using MasterConsole.ViewModels;

namespace VI.Control;

public partial class MainWindow : Window
{
    private readonly ShellRegion _region;
    private bool _closeStopIssued;
    private const double LargeWidth = 1500, LargeHeight = 860;
    private const double MediumWidth = 1150, MediumHeight = 640;
    private const double WideWidth = 820;

    public MainWindow(ShellRegion region, MainViewModel viewModel, StartupSelection selection)
    {
        _region = region;
        InitializeComponent();
        DataContext = viewModel;
        InterventionContent.Content = region.Panels.Single(panel => panel.Metadata.Id == "intervention").Content;
        PositioningArmContent.Content = region.Panels.Single(panel => panel.Metadata.Id == "positioning-arm").Content;
        SimulationNotice.Text = selection.Notice;
        SimulationBanner.Visibility = selection.IsSimulation ? Visibility.Visible : Visibility.Collapsed;
        var work = SystemParameters.WorkArea;
        Left = work.Left;
        Top = work.Top;
        Width = Math.Max(MinWidth, work.Width * 0.65 - 8);
        Height = work.Height;
    }

    private void WindowContent_SizeChanged(object sender, SizeChangedEventArgs e)
    {
        string tier = e.NewSize.Width >= LargeWidth && e.NewSize.Height >= LargeHeight ? "Large"
            : e.NewSize.Width >= MediumWidth && e.NewSize.Height >= MediumHeight ? "Medium" : "Small";
        Resources.MergedDictionaries[0] = new ResourceDictionary
        {
            Source = new Uri($"pack://application:,,,/VI.Control;component/Themes/Sizes{tier}.xaml", UriKind.Absolute)
        };
        Tag = e.NewSize.Width >= WideWidth ? "Wide" : "Narrow";
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

}
