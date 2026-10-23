using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using MasterConsole.ViewModels;

namespace VI.Module.Intervention;

public partial class InterventionPanel : UserControl
{
    private readonly MainViewModel _viewModel;

    public InterventionPanel(MainViewModel viewModel)
    {
        _viewModel = viewModel;
        InitializeComponent();
        DataContext = viewModel;
        CommandBindings.Add(new CommandBinding(ApplicationCommands.Stop, (_, e) => { StopHoldActions(); e.Handled = true; }));
        Unloaded += (_, _) => StopHoldActions();
    }

    public void StopHoldActions()
    {
        _viewModel.SetInjector(1, 0);
        _viewModel.SetInjector(2, 0);
        _viewModel.SetAxis4(0);
        if (Mouse.Captured is DependencyObject captured && IsAncestorOf(captured)) Mouse.Capture(null);
    }

    private void HoldButton_Down(object sender, MouseButtonEventArgs e)
    {
        if (sender is not Button button || !TryParse(button, out string target, out int direction)) return;
        button.CaptureMouse();
        Apply(target, direction);
        e.Handled = true;
    }

    private void HoldButton_Up(object sender, RoutedEventArgs e)
    {
        if (sender is not Button button || !TryParse(button, out string target, out _)) return;
        Apply(target, 0);
        if (button.IsMouseCaptured) button.ReleaseMouseCapture();
        e.Handled = true;
    }

    private void Apply(string target, int direction)
    {
        if (target == "A4") _viewModel.SetAxis4(direction);
        else _viewModel.SetInjector(int.Parse(target, CultureInfo.InvariantCulture), direction);
    }

    private static bool TryParse(Button button, out string target, out int direction)
    {
        target = Convert.ToString(button.Tag, CultureInfo.InvariantCulture) ?? "";
        direction = 0;
        return (target is "1" or "2" or "A4") &&
               int.TryParse(Convert.ToString(button.CommandParameter, CultureInfo.InvariantCulture), out direction) &&
               (direction is -1 or 1);
    }
}
