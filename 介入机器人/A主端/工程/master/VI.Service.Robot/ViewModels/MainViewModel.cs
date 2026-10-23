using System;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using System.Windows.Threading;
using MasterConsole.Protocol;
using MasterConsole.Services;

namespace MasterConsole.ViewModels
{
    /// <summary>电缸 1–4 的一个按钮项。电缸 1/3 打开值=2000，电缸 2/4 打开值=10。</summary>
    public sealed class CylinderItem : ObservableObject
    {
        private bool _engaged;
        private ushort _currentValue;

        public CylinderItem(int index)
        {
            Index = index;
            OpenValue = (index == 1 || index == 3) ? 2000 : 10;
        }

        public int Index { get; }
        public int OpenValue { get; }
        public string Title => $"电缸 {Index}";
        public string Caption => $"打开 = {OpenValue}";

        /// <summary>只认从端回报的手动覆盖状态（与旧 AdsControlUI 一致）。</summary>
        public bool Engaged { get => _engaged; set => SetField(ref _engaged, value); }
        public ushort CurrentValue { get => _currentValue; set { if (SetField(ref _currentValue, value)) OnPropertyChanged(nameof(CurrentText)); } }
        public string CurrentText => $"当前值 {CurrentValue}";

        /// <summary>命令被拒绝或完成后强制按钮重新读取状态，避免本地点击后显示与真实状态不一致。</summary>
        public void Refresh() => OnPropertyChanged(nameof(Engaged));
    }

    public sealed class LogItem
    {
        public string Time { get; set; }
        public string Level { get; set; }
        public string Text { get; set; }
    }

    public sealed class MainViewModel : ObservableObject, IDisposable
    {
        private const int MaxLogItems = 300;

        private readonly IRobotLink _link;
        private readonly Dispatcher _ui;
        private readonly DispatcherTimer _statsTimer;
        private StatusFrame _s;
        private bool _hasStatus;
        private bool _connecting;
        private bool _disposed;

        private string _catheterText = "28";
        private string _wireText = "610";
        private string _prepareError = "";
        private LinkStats _stats = new LinkStats();
        private string _armFeedbackText = "定位臂命令待发送";

        public MainViewModel(IRobotLink link)
        {
            _link = link;
            _ui = Dispatcher.CurrentDispatcher;

            Cylinders = new ObservableCollection<CylinderItem>
            {
                new CylinderItem(1), new CylinderItem(2), new CylinderItem(3), new CylinderItem(4),
            };

            ConnectCommand = new RelayCommand(ToggleConnect);
            PrepareCommand = new RelayCommand(Prepare);
            ForceFeedbackCommand = new RelayCommand(ToggleForceFeedback);
            CylinderCommand = new RelayCommand(p => ToggleCylinder(p));
            YValveCommand = new RelayCommand(ToggleYValve);
            StartControlCommand = new RelayCommand(StartControl);

            _link.ConnectionChanged += OnConnectionChanged;
            _link.StatusReceived += OnStatusReceived;
            _link.LogReceived += OnLogReceived;

            _statsTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(250) };
            _statsTimer.Tick += OnStatsTick;
            _statsTimer.Start();

            AddLog("info", "主端控制台已启动。当前使用：" + _link.Name + "。");
        }

        // ============================================================ 集合与命令

        public ObservableCollection<CylinderItem> Cylinders { get; }
        public ObservableCollection<LogItem> Log { get; } = new ObservableCollection<LogItem>();

        public ICommand ConnectCommand { get; }
        public ICommand PrepareCommand { get; }
        public ICommand ForceFeedbackCommand { get; }
        public ICommand CylinderCommand { get; }
        public ICommand YValveCommand { get; }
        public ICommand StartControlCommand { get; }

        // ============================================================ 连接与控制权

        public string LinkName => _link.Name;
        public bool IsConnected => _link.IsConnected;
        public bool HasControl => _link.HasControl;
        public bool StatusFresh => _hasStatus && IsConnected && _link.GetStats().StatusAgeMs < ProtocolConstants.StatusTimeoutMs;

        /// <summary>所有运动/动作类按钮的总开关：已连接 + 持有控制权 + 状态数据新鲜。</summary>
        public bool CanOperate => IsConnected && HasControl && StatusFresh;

        public string ConnectButtonText => _connecting ? "连接中…" : (IsConnected ? "断开" : "连接从端");

        private async void ToggleConnect()
        {
            if (_connecting) return;
            if (IsConnected) { _link.Disconnect(); _hasStatus = false; RaiseConnectionProps(); return; }
            _connecting = true;
            RaiseConnectionProps();
            try
            {
                await _link.ConnectAsync();
                var result = await _link.AcquireControlAsync();
                if (!result.Ok)
                {
                    AddLog("warn", "自动获取控制权失败：" + result.Reason + (IsConnected ? " 链路仍保持连接。" : " 链路未连接。"));
                }
                else if (!IsConnected)
                {
                    AddLog("warn", "控制权申请完成前链路已中断，请重新连接从端。");
                }
                else if (!HasControl)
                {
                    AddLog("warn", "控制权申请已确认，等待从端状态同步；操作区暂时保持锁定。");
                }
            }
            catch (Exception ex)
            {
                _link.Disconnect();
                AddLog("error", "连接失败：" + ex.Message);
            }
            finally { _connecting = false; RaiseConnectionProps(); }
        }

        private void RaiseConnectionProps()
        {
            if (!IsConnected) _hasStatus = false;
            foreach (var name in StatusPropertyNames) OnPropertyChanged(name);
            OnPropertyChanged(nameof(IsConnected));
            OnPropertyChanged(nameof(HasControl));
            OnPropertyChanged(nameof(CanOperate));
            OnPropertyChanged(nameof(CanStartControl));
            OnPropertyChanged(nameof(ConnectButtonText));
            OnPropertyChanged(nameof(StatusFresh));
            OnPropertyChanged(nameof(OperateHintText));
        }

        public string OperateHintText
        {
            get
            {
                if (!IsConnected) return "未连接从端：操作区已锁定。";
                if (!HasControl) return _connecting ? "正在自动获取控制权…" : "控制权已失效：请断开后重新连接从端。";
                if (!StatusFresh) return "状态数据已过期：操作区已锁定。";
                return "";
            }
        }

        // ============================================================ 状态区（只读）

        private bool Has(StatusFlags f) => _hasStatus && _s.Has(f);

        public bool EstopHold => Has(StatusFlags.EstopHold);
        public bool ControlActive => Has(StatusFlags.ControlActive);
        public bool FfEnabled => Has(StatusFlags.FfEnabled);
        public bool FfZeroing => Has(StatusFlags.FfZeroing);
        public bool YValveClosed => Has(StatusFlags.YValveClosed);
        public bool AdsHealthy => Has(StatusFlags.AdsHealthy);

        /// <summary>显示从端已生效的全局方向，换手期间保持当前方向。</summary>
        public string ModeText => !IsConnected ? "—" : (Has(StatusFlags.GlobalReverse) ? "撤出" : "递送");
        public string PhaseText => !_hasStatus ? "未连接"
            : ControlActive ? "手柄控制中"
            : Has(StatusFlags.SelfCheckDone) ? "待开始控制"
            : "待机（未到准备位置）";

        /// <summary>已到达准备位置、尚未进入手柄控制时可点“开始控制”。</summary>
        public bool CanStartControl => CanOperate && !ControlActive;

        public string Force582F => FormatForce(_s.Force582F, "N");
        public string Force582N => FormatTorque(_s.Force582N);
        public string Force587F => FormatForce(_s.Force587F, "N");
        public string Force587N => FormatTorque(_s.Force587N);

        public string Axis1NetDistance => FormatDistance(_s.Axis1NetDistanceMm);
        public string Axis6NetDistance => FormatDistance(_s.Axis6NetDistanceMm);

        private string FormatDistance(double mm)
            => IsConnected && StatusFresh && _s.HasNetDistance && !double.IsNaN(mm) && !double.IsInfinity(mm)
                ? mm.ToString("0.000", CultureInfo.InvariantCulture) + " mm" : "—";

        // 轴2(导管)和轴7(导丝)的角度指示器指针坐标计算
        public double Axis2PointerX => CalculatePointerX(_hasStatus ? _s.AxisPos[1] : 0.0);
        public double Axis2PointerY => CalculatePointerY(_hasStatus ? _s.AxisPos[1] : 0.0);
        public double Axis7PointerX => CalculatePointerX(_hasStatus ? _s.AxisPos[6] : 0.0);
        public double Axis7PointerY => CalculatePointerY(_hasStatus ? _s.AxisPos[6] : 0.0);

        private double CalculatePointerX(double angleDeg)
        {
            // 0度指向上方，顺时针旋转
            // 角度增加时向右转
            double angleRad = (angleDeg - 90.0) * System.Math.PI / 180.0;
            return 20.0 * System.Math.Cos(angleRad);
        }

        private double CalculatePointerY(double angleDeg)
        {
            // Canvas中Y轴向下
            double angleRad = (angleDeg - 90.0) * System.Math.PI / 180.0;
            return 20.0 * System.Math.Sin(angleRad);
        }

        private string FormatForce(float v, string unit)
            => _hasStatus ? v.ToString("0.000", CultureInfo.InvariantCulture) + " " + unit : "—";

        private string FormatTorque(float v)
            => _hasStatus ? (v * 1000.0f).ToString("0.000", CultureInfo.InvariantCulture) + " N·mm" : "—";

        // ============================================================ 操作区

        public string CatheterText { get => _catheterText; set => SetField(ref _catheterText, value); }
        public string WireText { get => _wireText; set => SetField(ref _wireText, value); }
        public string PrepareError { get => _prepareError; private set => SetField(ref _prepareError, value); }

        public string PrepareStatusText
        {
            get
            {
                if (!_hasStatus) return "等待从端状态";
                if (Has(StatusFlags.SelfCheckDone)) return "已到达器械准备位置";
                switch (_s.SelfCheckStatus)
                {
                    case 1: return "等待开始";
                    case 2: return "运行中";
                    case 3: return "PLC 拒绝目标参数";
                    case 4: return "从端拒绝命令或 ADS 写入失败";
                    case 5: return "请求已发送，等待 PLC 确认";
                    default: return "待机";
                }
            }
        }

        public string FfCaption => FfZeroing ? "正在自动零点采集…" : (FfEnabled ? "已开启" : "已关闭（开启前自动采集零点）");
        public string YValveCaption => YValveClosed ? "Y 阀已关闭（再次点击取消并打开）" : "Y 阀已打开";

        public string Injector1Text => InjectorText(_hasStatus ? _s.InjectorActive1 : 0);
        public string Injector2Text => InjectorText(_hasStatus ? _s.InjectorActive2 : 0);
        public string Axis4Text => !_hasStatus ? "停止" : (_s.Axis4Active > 0 ? "前进中" : (_s.Axis4Active < 0 ? "后退中" : "停止"));
        private static string InjectorText(int dir) => dir > 0 ? "推进中" : (dir < 0 ? "回拉中" : "停止");

        private async void Prepare()
        {
            PrepareError = "";
            if (!double.TryParse(CatheterText, NumberStyles.Float, CultureInfo.InvariantCulture, out double cath) ||
                !double.TryParse(WireText, NumberStyles.Float, CultureInfo.InvariantCulture, out double wire) ||
                double.IsNaN(cath) || double.IsInfinity(cath) || double.IsNaN(wire) || double.IsInfinity(wire))
            {
                PrepareError = "请输入有效的数值。";
                return;
            }
            if (cath < 5 || cath > 95) { PrepareError = "导管搓捻机构位置范围为 5–95 mm。"; return; }
            if (wire < 10 || wire > 639) { PrepareError = "Y阀及导丝机构位置范围为 10–639 mm。"; return; }
            await Report("进入器械准备位置", await _link.PreparePositionAsync(cath, wire));
        }

        private async void StartControl()
        {
            PrepareError = "";
            var r = await _link.StartControlAsync();
            if (!r.Ok) PrepareError = r.Reason;
            await Report("开始控制", r);
        }

        private async void ToggleForceFeedback()
        {
            if (FfZeroing) return; // 零点采集期间不响应
            bool target = !FfEnabled;
            await Report(target ? "开启力反馈" : "关闭力反馈", await _link.SetForceFeedbackAsync(target));
            OnPropertyChanged(nameof(FfEnabled));
        }

        private async void ToggleCylinder(object parameter)
        {
            if (!(parameter is CylinderItem item)) return;
            bool target = !item.Engaged;
            await Report($"电缸 {item.Index}", await _link.SetCylinderAsync(item.Index, target));
            item.Refresh();
        }

        private async void ToggleYValve()
        {
            bool target = !YValveClosed;
            await Report(target ? "关闭 Y 阀" : "打开 Y 阀", await _link.SetYValveClosedAsync(target));
            OnPropertyChanged(nameof(YValveClosed));
        }

        /// <summary>注射器按住推/拉：由窗口的鼠标按下/抬起事件调用。direction：-1 拉，0 停，+1 推。</summary>
        public void SetInjector(int index, int direction)
        {
            if (direction != 0 && !CanOperate) return;
            _link.SetInjector(index, direction);
        }

        /// <summary>轴4点动按住：-1 后退，0 停，+1 前进。</summary>
        public void SetAxis4(int direction)
        {
            if (direction != 0 && !CanOperate) return;
            _link.SetAxis4(direction);
        }

        /// <summary>窗口失焦、关闭等场景：所有“按住才动”的动作一律归零。</summary>
        public void StopAllHoldActions()
        {
            _link.SetInjector(1, 0);
            _link.SetInjector(2, 0);
            _link.SetAxis4(0);
            for (int axis = 1; axis <= 5; axis++) _link.SetArmAxisJog(axis, 0);
            _ = _link.StopArmAsync();
        }

        private Task Report(string what, CommandResult r)
        {
            if (!r.Ok) AddLog("warn", $"{what}被拒绝：{r.Reason}");
            return Task.CompletedTask;
        }

        // ============================================================ 链路区

        public string SessionText => _stats.Session == 0 ? "—" : "0x" + _stats.Session.ToString("X8");
        public string RttText => IsConnected ? _stats.RttMs.ToString("0") + " ms" : "—";
        public string StatusAgeText => IsConnected && !double.IsNaN(_stats.StatusAgeMs) ? _stats.StatusAgeMs.ToString("0") + " ms" : "—";
        public string HapticAgeText => IsConnected && !double.IsNaN(_stats.HapticAgeMs) ? _stats.HapticAgeMs.ToString("0") + " ms" : "—";
        public string StatusHzText => IsConnected ? _stats.StatusHz.ToString("0") + " Hz" : "—";
        public string DroppedText => _stats.DroppedFrames.ToString();
        public string AdsText => !_hasStatus ? "—" : (AdsHealthy ? "正常" : "异常");

        /// <summary>主端本机手柄：两只都在线时指示灯亮；SDK 缺失时文字提示。</summary>
        public bool HandlesOnline => _stats.Handle582Online && _stats.Handle587Online;
        public string HandleText => _stats.HandleSdkMissing
            ? "手柄 SDK 未加载（缺 FLCatheter.dll）"
            : "582 " + (_stats.Handle582Online ? "在线" : "离线") + " / 587 " + (_stats.Handle587Online ? "在线" : "离线");
        public string ArmFeedbackText => _armFeedbackText;

        public async Task SetArmManualEnableAsync(bool enabled) =>
            await ReportArm("定位臂总使能", await _link.SetArmManualEnableAsync(enabled));
        public async Task SetArmAxisEnableAsync(int axis, bool enabled) =>
            await ReportArm($"定位臂轴{axis}{(enabled ? "上电" : "断电")}", await _link.SetArmAxisEnableAsync(axis, enabled));
        public async Task ResetArmAxisAsync(int axis) =>
            await ReportArm($"定位臂轴{axis}复位", await _link.ResetArmAxisAsync(axis));
        public void SetArmAxisJog(int axis, int direction) => _link.SetArmAxisJog(axis, direction);
        public async Task<bool> SetArmCartesianJogAsync(int mode, int speedMilli)
        {
            var result = await _link.SetArmCartesianJogAsync(mode, speedMilli);
            await ReportArm("定位臂末端点动", result);
            return result.Ok;
        }
        public async Task StopArmAsync() => await ReportArm("定位臂停止", await _link.StopArmAsync());
        public async Task<bool> ReturnArmProgramZeroAsync()
        {
            var result = await _link.ReturnArmProgramZeroAsync();
            await ReportArm("定位臂程序归零", result);
            return result.Ok;
        }
        public async Task<bool> SetArmCartesianParameterAsync(int field, int valueMilli)
        {
            var result = await _link.SetArmCartesianParameterAsync(field, valueMilli);
            await ReportArm("定位臂参数", result);
            return result.Ok;
        }
        public void KeepArmCartesianAlive() => _link.KeepArmCartesianAlive();

        private Task ReportArm(string what, CommandResult result)
        {
            _armFeedbackText = result.Ok ? $"{what}：已发送" : $"{what}失败：{result.Reason}";
            OnPropertyChanged(nameof(ArmFeedbackText));
            if (!result.Ok) AddLog("warn", _armFeedbackText);
            return Task.CompletedTask;
        }

        private void RefreshStats()
        {
            _stats = _link.GetStats();
            OnPropertyChanged(nameof(SessionText));
            OnPropertyChanged(nameof(RttText));
            OnPropertyChanged(nameof(StatusAgeText));
            OnPropertyChanged(nameof(HapticAgeText));
            OnPropertyChanged(nameof(StatusHzText));
            OnPropertyChanged(nameof(DroppedText));
            OnPropertyChanged(nameof(AdsText));
            OnPropertyChanged(nameof(HandlesOnline));
            OnPropertyChanged(nameof(HandleText));
            OnPropertyChanged(nameof(ModeText));
            OnPropertyChanged(nameof(Axis1NetDistance));
            OnPropertyChanged(nameof(Axis6NetDistance));
            // 状态过期是由时间推移产生的，需要在这里重新评估。
            OnPropertyChanged(nameof(StatusFresh));
            OnPropertyChanged(nameof(CanOperate));
            OnPropertyChanged(nameof(CanStartControl));
            OnPropertyChanged(nameof(OperateHintText));
        }

        // ============================================================ 状态帧入口

        private void OnStatus(StatusFrame f)
        {
            if (!IsConnected) return;
            _s = f;
            _hasStatus = true;

            foreach (var c in Cylinders)
            {
                c.Engaged = (f.CylinderManualMask & (1 << (c.Index - 1))) != 0;
                switch (c.Index)
                {
                    case 1: c.CurrentValue = f.Cylinder1; break;
                    case 2: c.CurrentValue = f.Cylinder2; break;
                    case 3: c.CurrentValue = f.Cylinder3; break;
                    default: c.CurrentValue = f.Cylinder4; break;
                }
            }

            foreach (var name in StatusPropertyNames) OnPropertyChanged(name);
            OnPropertyChanged(nameof(CanOperate));
            OnPropertyChanged(nameof(CanStartControl));
            OnPropertyChanged(nameof(OperateHintText));
        }

        private static readonly string[] StatusPropertyNames =
        {
            nameof(EstopHold), nameof(ControlActive), nameof(FfEnabled), nameof(FfZeroing), nameof(YValveClosed),
            nameof(AdsHealthy), nameof(ModeText), nameof(PhaseText), nameof(CanStartControl),
            nameof(Force582F), nameof(Force582N), nameof(Force587F), nameof(Force587N),
            nameof(Axis1NetDistance), nameof(Axis6NetDistance),
            nameof(Axis2PointerX), nameof(Axis2PointerY), nameof(Axis7PointerX), nameof(Axis7PointerY),
            nameof(PrepareStatusText), nameof(FfCaption), nameof(YValveCaption), nameof(Injector1Text), nameof(Injector2Text), nameof(Axis4Text),
            nameof(StatusFresh),
        };

        // ============================================================ 日志

        /// <summary>供窗口写入一条日志（须在 UI 线程调用）。</summary>
        public void PostLog(string level, string text) => AddLog(level, text);

        public string LastWarning { get; private set; } = "";

        private void AddLog(string level, string text, DateTime? time = null)
        {
            if (level == "warn" || level == "error")
            { LastWarning = text; OnPropertyChanged(nameof(LastWarning)); }
            Log.Insert(0, new LogItem
            {
                Time = (time ?? DateTime.Now).ToString("HH:mm:ss"),
                Level = level,
                Text = text,
            });
            while (Log.Count > MaxLogItems) Log.RemoveAt(Log.Count - 1);
        }

        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;
            _statsTimer.Stop();
            _statsTimer.Tick -= OnStatsTick;
            _link.ConnectionChanged -= OnConnectionChanged;
            _link.StatusReceived -= OnStatusReceived;
            _link.LogReceived -= OnLogReceived;
        }

        private void OnConnectionChanged(object sender, EventArgs args) => Dispatch(RaiseConnectionProps);
        private void OnStatusReceived(object sender, StatusFrame frame) => Dispatch(() => OnStatus(frame));
        private void OnLogReceived(object sender, LogMessage message)
        {
            if (_disposed || _ui.HasShutdownStarted) return;
            if (_ui.CheckAccess()) { AddLog(message.Level, message.Text, message.Time); return; }
            Dispatch(() => AddLog(message.Level, message.Text, message.Time));
        }
        private void OnStatsTick(object sender, EventArgs args) { if (!_disposed) RefreshStats(); }

        private void Dispatch(Action action)
        {
            if (_disposed || _ui.HasShutdownStarted) return;
            _ui.BeginInvoke(new Action(() => { if (!_disposed) action(); }));
        }
    }
}
