using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Threading;
using MasterConsole.ViewModels;

namespace VI.Module.PositioningArm;

public partial class PositioningArmPanel : UserControl
{
    private readonly MainViewModel _viewModel;
    private readonly DispatcherTimer _keepaliveTimer;
    private int _jogAxis, _jogDirection, _cartesianMode, _cartesianSpeedMilli, _holdVersion;
    private bool _programReturnActive;

    public PositioningArmPanel(MainViewModel viewModel)
    {
        _viewModel = viewModel;
        InitializeComponent();
        DataContext = viewModel;
        _keepaliveTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(100) };
        _keepaliveTimer.Tick += KeepaliveTimer_Tick;
        CommandBindings.Add(new CommandBinding(ApplicationCommands.Stop, (_, e) => { StopHoldActions(); e.Handled = true; }));
        Loaded += (_, _) => _keepaliveTimer.Start();
        Unloaded += (_, _) => { _keepaliveTimer.Stop(); StopHoldActions(); };
    }

    public void StopHoldActions()
    {
        _holdVersion++;
        _jogAxis = _jogDirection = _cartesianMode = _cartesianSpeedMilli = 0;
        _programReturnActive = false;
        for (int axis = 1; axis <= 5; axis++) _viewModel.SetArmAxisJog(axis, 0);
        _ = _viewModel.StopArmAsync();
        if (Mouse.Captured is DependencyObject captured && IsAncestorOf(captured)) Mouse.Capture(null);
    }

    private async void ArmManualEnable_Click(object sender, RoutedEventArgs e)
    {
        if (sender is ToggleButton button) await _viewModel.SetArmManualEnableAsync(button.IsChecked == true);
    }

    private async void ArmAxisEnable_Click(object sender, RoutedEventArgs e)
    {
        if (sender is ToggleButton button && TryAxis(button.Tag, out int axis))
            await _viewModel.SetArmAxisEnableAsync(axis, button.IsChecked == true);
    }

    private async void ArmAxisReset_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button button && TryAxis(button.Tag, out int axis)) await _viewModel.ResetArmAxisAsync(axis);
    }

    private void ArmJogButton_Down(object sender, MouseButtonEventArgs e)
    {
        if (!_viewModel.CanOperate || sender is not Button button || !TryAxis(button.Tag, out int axis) ||
            !int.TryParse(Convert.ToString(button.CommandParameter, CultureInfo.InvariantCulture), out int direction) || direction is not (-1 or 1)) return;
        _jogAxis = axis;
        _jogDirection = direction;
        _viewModel.SetArmAxisJog(axis, direction);
        button.CaptureMouse();
        e.Handled = true;
    }

    private void ArmJogButton_Up(object sender, RoutedEventArgs e)
    {
        if (sender is not Button button || !TryAxis(button.Tag, out int axis)) return;
        if (_jogAxis == axis) _jogAxis = _jogDirection = 0;
        _viewModel.SetArmAxisJog(axis, 0);
        if (button.IsMouseCaptured) button.ReleaseMouseCapture();
        e.Handled = true;
    }

    private async void ArmCartesian_Down(object sender, MouseButtonEventArgs e)
    {
        if (!_viewModel.CanOperate || _cartesianMode != 0 || sender is not Button button) return;
        ArmCartesianError.Text = "";
        if (!int.TryParse(Convert.ToString(button.CommandParameter, CultureInfo.InvariantCulture), out int direction) || direction is not (-1 or 1) ||
            !double.TryParse(ArmCartesianSpeed.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out double speed) ||
            !double.IsFinite(speed) || speed < 0.001 || speed > 100)
        {
            ArmCartesianError.Text = "速度须在 0.001 至 100 之间。";
            return;
        }
        int version = ++_holdVersion;
        button.CaptureMouse();
        e.Handled = true;
        if (!await SendCartesianParametersAsync(false) || version != _holdVersion || !_viewModel.CanOperate) return;
        _cartesianMode = ArmCartesianMode.SelectedIndex + 1;
        _cartesianSpeedMilli = direction * (int)Math.Round(speed * 1000.0);
        if (!await _viewModel.SetArmCartesianJogAsync(_cartesianMode, _cartesianSpeedMilli))
            _cartesianMode = _cartesianSpeedMilli = 0;
    }

    private void ArmCartesian_Up(object sender, RoutedEventArgs e)
    {
        _holdVersion++;
        _cartesianMode = _cartesianSpeedMilli = 0;
        _ = _viewModel.StopArmAsync();
        if (sender is Button button && button.IsMouseCaptured) button.ReleaseMouseCapture();
        e.Handled = true;
    }

    private async void ArmProgramZero_Click(object sender, RoutedEventArgs e)
    {
        if (!_viewModel.CanOperate || _cartesianMode != 0 || _jogAxis != 0) return;
        ArmCartesianError.Text = "";
        int version = ++_holdVersion;
        if (!await SendCartesianParametersAsync(true) || version != _holdVersion) return;
        bool started = await _viewModel.ReturnArmProgramZeroAsync();
        if (version == _holdVersion) _programReturnActive = started;
    }

    private async Task<bool> SendCartesianParametersAsync(bool home)
    {
        TextBox[] inputs = [ArmLiftMin, ArmHomeTravel, ArmHomeTip, ArmHomeRotation];
        int count = home ? inputs.Length : 1;
        for (int i = 0; i < count; i++)
        {
            if (!double.TryParse(inputs[i].Text, NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ||
                !double.IsFinite(value) || Math.Abs(value) > 100000 || (i != 0 && value <= 0))
            {
                ArmCartesianError.Text = home ? "归零需要填写轴1下端、正数行程、位移和转角上限。" : "末端点动需要填写轴1下端读数。";
                return false;
            }
            if (!await _viewModel.SetArmCartesianParameterAsync(i, (int)Math.Round(value * 1000.0)))
            {
                ArmCartesianError.Text = "定位臂参数发送失败。";
                return false;
            }
        }
        return true;
    }

    private void ArmStop_Click(object sender, RoutedEventArgs e) => StopHoldActions();

    private void KeepaliveTimer_Tick(object? sender, EventArgs e)
    {
        if (!_viewModel.CanOperate && (_jogAxis != 0 || _cartesianMode != 0 || _programReturnActive))
        {
            StopHoldActions();
            return;
        }
        if (_jogAxis != 0 && _jogDirection != 0) _viewModel.SetArmAxisJog(_jogAxis, _jogDirection);
        if (_cartesianMode != 0) _ = _viewModel.SetArmCartesianJogAsync(_cartesianMode, _cartesianSpeedMilli);
        if (_programReturnActive) _viewModel.KeepArmCartesianAlive();
    }

    private static bool TryAxis(object tag, out int axis) =>
        int.TryParse(Convert.ToString(tag, CultureInfo.InvariantCulture), out axis) && axis is >= 1 and <= 5;
}
