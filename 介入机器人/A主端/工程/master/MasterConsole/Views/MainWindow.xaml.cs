using System;
using System.Globalization;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Threading;
using MasterConsole.Services;
using MasterConsole.ViewModels;

namespace MasterConsole.Views
{
    public partial class MainWindow : Window
    {
        private readonly MainViewModel _vm;
        // CreateLink 的结果说明，窗口建好后写进事件日志，避免“静默退回模拟链路”。
        private static string _linkNotice;
        private static string _linkTokenPath;
        private readonly DispatcherTimer _armKeepaliveTimer;
        private int _armJogAxis;
        private int _armJogDirection;
        private int _armCartesianMode;
        private int _armCartesianSpeedMilli;
        private bool _armProgramReturnActive;

        public MainWindow()
        {
            InitializeComponent();
            PlaceDefaultWindow();
            _vm = new MainViewModel(CreateLink());
            DataContext = _vm;
            _armKeepaliveTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(100) };
            _armKeepaliveTimer.Tick += ArmKeepaliveTimer_Tick;
            _armKeepaliveTimer.Start();
            if (_linkNotice != null) _vm.PostLog("warn", _linkNotice);
            else if (_linkTokenPath != null) _vm.PostLog("info", "已加载密钥：" + _linkTokenPath);
            Closed += (s, e) =>
            {
                _armKeepaliveTimer.Stop();
                _vm.StopAllHoldActions();
                _vm.Dispose();
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

            Left = work.Left;
            Top = work.Top;
            Width = primaryWidth;
            Height = work.Height;
        }
        /// <summary>
        /// 链路选择：找到 remote.token 就连真实从端，否则用模拟链路。
        /// 命令行：--sim 强制模拟；--host &lt;地址&gt; 指定从端地址（默认 192.168.60.1，即从端电脑）；
        /// --token &lt;文件&gt; 指定密钥文件。密钥文件默认在 exe 同目录、其上两级的 config 目录中查找。
        /// </summary>
        private static IRobotLink CreateLink()
        {
            string[] args = Environment.GetCommandLineArgs();
            string host = new RemoteLinkSettings().Host;
            string tokenPath = null;
            bool sim = false;
            for (int i = 1; i < args.Length; i++)
            {
                if (args[i] == "--sim") sim = true;
                else if (args[i] == "--host" && i + 1 < args.Length) host = args[++i];
                else if (args[i] == "--token" && i + 1 < args.Length) tokenPath = args[++i];
            }
            if (sim)
            {
                _linkNotice = "已按 --sim 参数使用模拟链路，命令不会发给从端。";
                return new SimulatedRobotLink();
            }

            // 从 exe 所在目录逐级向上查找（exe 通常在 bin/Debug/net472 下，密钥在项目根的 config 目录）。
            var candidates = new System.Collections.Generic.List<string>();
            if (!string.IsNullOrEmpty(tokenPath)) candidates.Add(tokenPath);
            string dir = AppDomain.CurrentDomain.BaseDirectory;
            for (int level = 0; level < 10 && !string.IsNullOrEmpty(dir); level++)
            {
                candidates.Add(Path.Combine(dir, "remote.token"));
                candidates.Add(Path.Combine(dir, "config", "remote.token"));
                dir = Path.GetDirectoryName(dir.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
            }
            string rejected = null;
            foreach (string c in candidates)
            {
                if (!File.Exists(c)) continue;
                byte[] token = Encoding.UTF8.GetBytes(File.ReadAllText(c, Encoding.UTF8).Trim());
                if (token.Length < 16) { rejected = c; continue; }
                _linkNotice = null;
                _linkTokenPath = c;
                // 手柄服务随链路创建：应用启动即开始打开本机手柄，连接从端时已就绪。
                return new RemoteRobotLink(
                    new RemoteLinkSettings { Host = host, Token = token },
                    new HandleService());
            }
            _linkNotice = rejected != null
                ? "密钥文件太短（至少 16 个字符）：" + rejected + "。已退回模拟链路，命令不会发给从端。"
                : "未找到 remote.token，已退回模拟链路，命令不会发给从端。请把密钥文件放到 config 目录或 exe 同目录。";
            return new SimulatedRobotLink();
        }

        // ---------------- 按住才动的按钮：注射器 1/2 推拉、轴4点动 ----------------
        // Tag："1"/"2" 为注射器编号，"A4" 为轴4；CommandParameter 为方向（+1 / -1）。
        // 按下发送方向，抬起/失去鼠标捕获/窗口失焦一律归零，保证松手即停。

        private void HoldButton_Down(object sender, MouseButtonEventArgs e)
        {
            if (!(sender is Button b) || !TryParse(b, out string target, out int dir)) return;
            b.CaptureMouse();
            Apply(target, dir);
        }

        private void HoldButton_Up(object sender, RoutedEventArgs e)
        {
            if (!(sender is Button b) || !TryParse(b, out string target, out _)) return;
            if (b.IsMouseCaptured) b.ReleaseMouseCapture();
            Apply(target, 0);
        }

        private async void ArmManualEnable_Click(object sender, RoutedEventArgs e)
        {
            if (!(sender is ToggleButton button)) return;
            await _vm.SetArmManualEnableAsync(button.IsChecked == true);
        }

        private async void ArmAxisEnable_Click(object sender, RoutedEventArgs e)
        {
            if (!(sender is ToggleButton button) || !int.TryParse(Convert.ToString(button.Tag, CultureInfo.InvariantCulture), out int axis)) return;
            await _vm.SetArmAxisEnableAsync(axis, button.IsChecked == true);
        }

        private async void ArmAxisReset_Click(object sender, RoutedEventArgs e)
        {
            if (!(sender is Button button) || !int.TryParse(Convert.ToString(button.Tag, CultureInfo.InvariantCulture), out int axis)) return;
            await _vm.ResetArmAxisAsync(axis);
        }

        private void ArmJogButton_Down(object sender, MouseButtonEventArgs e)
        {
            if (!(sender is Button button) || !TryArmJog(button, out int axis, out int direction)) return;
            _armJogAxis = axis;
            _armJogDirection = direction;
            _vm.SetArmAxisJog(axis, direction);
            button.CaptureMouse();
            e.Handled = true;
        }

        private void ArmJogButton_Up(object sender, RoutedEventArgs e)
        {
            if (!(sender is Button button) || !int.TryParse(Convert.ToString(button.Tag, CultureInfo.InvariantCulture), out int axis)) return;
            if (_armJogAxis == axis) { _armJogAxis = 0; _armJogDirection = 0; }
            _vm.SetArmAxisJog(axis, 0);
            if (button.IsMouseCaptured) button.ReleaseMouseCapture();
        }

        private async void ArmCartesian_Down(object sender, MouseButtonEventArgs e)
        {
            if (_armCartesianMode != 0 || !(sender is Button button)) return;
            ArmCartesianError.Text = "";
            if (!int.TryParse(Convert.ToString(button.CommandParameter, CultureInfo.InvariantCulture), out int direction) ||
                !double.TryParse(ArmCartesianSpeed.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out double speed) ||
                double.IsNaN(speed) || double.IsInfinity(speed) || speed < 0.001 || speed > 100)
            {
                ArmCartesianError.Text = "速度须在 0.001 至 100 之间。";
                return;
            }
            if (!await SendArmCartesianParametersAsync(false)) return;
            _armCartesianMode = ArmCartesianMode.SelectedIndex + 1;
            _armCartesianSpeedMilli = direction * (int)Math.Round(speed * 1000.0);
            if (!await _vm.SetArmCartesianJogAsync(_armCartesianMode, _armCartesianSpeedMilli))
            {
                _armCartesianMode = 0;
                _armCartesianSpeedMilli = 0;
                return;
            }
            button.CaptureMouse();
            e.Handled = true;
        }

        private void ArmCartesian_Up(object sender, RoutedEventArgs e)
        {
            if (_armCartesianMode == 0) return;
            _armCartesianMode = 0;
            _armCartesianSpeedMilli = 0;
            _ = _vm.StopArmAsync();
            if (sender is Button button && button.IsMouseCaptured) button.ReleaseMouseCapture();
        }

        private async void ArmProgramZero_Click(object sender, RoutedEventArgs e)
        {
            if (_armCartesianMode != 0 || _armJogAxis != 0) return;
            ArmCartesianError.Text = "";
            if (!await SendArmCartesianParametersAsync(true)) return;
            _armProgramReturnActive = await _vm.ReturnArmProgramZeroAsync();
        }

        private async Task<bool> SendArmCartesianParametersAsync(bool home)
        {
            TextBox[] inputs = { ArmLiftMin, ArmHomeTravel, ArmHomeTip, ArmHomeRotation };
            int count = home ? inputs.Length : 1;
            for (int i = 0; i < count; i++)
            {
                if (!double.TryParse(inputs[i].Text, NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ||
                    double.IsNaN(value) || double.IsInfinity(value) || Math.Abs(value) > 100000 || (i != 0 && value <= 0))
                {
                    ArmCartesianError.Text = home
                        ? "归零需要填写轴1下端、正数行程、位移和转角上限。"
                        : "末端点动需要填写轴1下端读数。";
                    return false;
                }
                if (!await _vm.SetArmCartesianParameterAsync(i, (int)Math.Round(value * 1000.0)))
                {
                    ArmCartesianError.Text = "定位臂参数发送失败。";
                    return false;
                }
            }
            return true;
        }

        private void ArmStop_Click(object sender, RoutedEventArgs e)
        {
            _armProgramReturnActive = false;
            _armCartesianMode = 0;
            _armJogAxis = 0;
            _armJogDirection = 0;
            _ = _vm.StopArmAsync();
        }

        private void ArmKeepaliveTimer_Tick(object sender, EventArgs e)
        {
            if (_armJogAxis != 0 && _armJogDirection != 0) _vm.SetArmAxisJog(_armJogAxis, _armJogDirection);
            if (_armCartesianMode != 0) _ = _vm.SetArmCartesianJogAsync(_armCartesianMode, _armCartesianSpeedMilli);
            if (_armProgramReturnActive) _vm.KeepArmCartesianAlive();
        }

        private static bool TryArmJog(Button button, out int axis, out int direction)
        {
            axis = direction = 0;
            return int.TryParse(Convert.ToString(button.Tag, CultureInfo.InvariantCulture), out axis) && axis >= 1 && axis <= 5 &&
                   int.TryParse(Convert.ToString(button.CommandParameter, CultureInfo.InvariantCulture), out direction) &&
                   (direction == -1 || direction == 1);
        }

        private void Apply(string target, int direction)
        {
            switch (target)
            {
                case "1": _vm.SetInjector(1, direction); break;
                case "2": _vm.SetInjector(2, direction); break;
                case "A4": _vm.SetAxis4(direction); break;
            }
        }

        private static bool TryParse(Button b, out string target, out int dir)
        {
            target = Convert.ToString(b.Tag, CultureInfo.InvariantCulture);
            dir = 0;
            if (target != "1" && target != "2" && target != "A4") return false;
            return int.TryParse(Convert.ToString(b.CommandParameter, CultureInfo.InvariantCulture), out dir) &&
                   (dir == 1 || dir == -1);
        }
    }
}
