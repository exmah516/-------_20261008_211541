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
        Loaded += (_, _) => { UpdateCartesianRate(); _keepaliveTimer.Start(); };
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
        if (sender is ToggleButton button)
        {
            bool enabled = button.IsChecked == true;
            if (enabled) await _viewModel.SetArmManualEnableAsync(true);
            for (int axis = 1; axis <= 5; axis++)
                await _viewModel.SetArmAxisEnableAsync(axis, enabled);
            if (!enabled) await _viewModel.SetArmManualEnableAsync(false);
        }
    }

    private async void ArmAxisReset_Click(object sender, RoutedEventArgs e)
    {
        for (int axis = 1; axis <= 5; axis++)
            await _viewModel.ResetArmAxisAsync(axis);
    }

    private void ArmSpeed_Checked(object sender, RoutedEventArgs e)
    {
        UpdateCartesianRate();
        UpdateActiveCartesianJog();
    }

    private void UpdateCartesianRate()
    {
        if (ArmCartesianRate1 is null) return;
        TextBlock[] rates = [ArmCartesianRate1, ArmCartesianRate2, ArmCartesianRate3, ArmCartesianRate4, ArmCartesianRate5];
        for (int mode = 1; mode <= 5; mode++)
        {
            if (mode == 3) continue;
            double speed = _viewModel.GetArmCartesianJogSpeed(mode);
            rates[mode - 1].Text = mode is 4 or 5 ? $"{speed:0.##}°/s" : $"{speed:0.##} mm/s";
        }
    }

    private void UpdateActiveCartesianJog()
    {
        if (_cartesianMode == 0 || _cartesianSpeedMilli == 0) return;
        int mode = _cartesianMode;
        if (mode is < 1 or > 5) return;
        int direction = Math.Sign(_cartesianSpeedMilli);
        _cartesianSpeedMilli = direction * (int)Math.Round(_viewModel.GetArmCartesianJogSpeed(mode) * 1000.0);
        _ = _viewModel.SetArmCartesianJogAsync(_cartesianMode, _cartesianSpeedMilli);
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
            !TryAxis(button.Tag, out int mode))
        {
            ArmCartesianError.Text = "请选择有效的末端点动方向。";
            return;
        }
        double speed = _viewModel.GetArmCartesianJogSpeed(mode);
        if (!double.IsFinite(speed) || speed <= 0 || speed > 100)
        {
            ArmCartesianError.Text = "当前末端点动速度无效。";
            return;
        }
        int version = ++_holdVersion;
        button.CaptureMouse();
        e.Handled = true;
        if (version != _holdVersion || !_viewModel.CanOperate) return;
        _cartesianMode = mode;
        _cartesianSpeedMilli = direction * (int)Math.Round(speed * 1000.0);
        bool sent = await _viewModel.SetArmCartesianJogAsync(_cartesianMode, _cartesianSpeedMilli);
        if (version == _holdVersion && !sent)
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
        if (!_viewModel.CanOperate || _cartesianMode != 0 || _jogAxis != 0)
        {
            ArmCartesianError.Text = !_viewModel.CanOperate
                ? "归零失败：尚未取得控制权或状态未就绪"
                : "归零失败：1008（请先松开点动按钮）";
            return;
        }
        ArmCartesianError.Text = "";
        int version = ++_holdVersion;
        _programReturnActive = true;
        bool started = await _viewModel.ReturnArmProgramZeroAsync();
        if (version == _holdVersion) _programReturnActive = started;
    }

    private void ArmStop_Click(object sender, RoutedEventArgs e) => StopHoldActions();

    private void KeepaliveTimer_Tick(object? sender, EventArgs e)
    {
        if (!_viewModel.CanOperate && (_jogAxis != 0 || _programReturnActive))
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
