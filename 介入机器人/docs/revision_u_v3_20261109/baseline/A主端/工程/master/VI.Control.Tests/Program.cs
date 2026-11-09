using System.IO;
using System.Reflection;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using MasterConsole.Services;
using MasterConsole.Protocol;
using MasterConsole.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Robotics.HostFramework;
using Robotics.HostFramework.Contracts;
using VI.Control;

internal static partial class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            Run(args.Length == 0 ? Path.Combine(AppContext.BaseDirectory, "UiChecks") : args[0]);
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex);
            return 1;
        }
    }

    private static void Run(string outputDirectory)
    {
        var application = new Application { ShutdownMode = ShutdownMode.OnExplicitShutdown };
        application.Resources.MergedDictionaries.Add(new ResourceDictionary
        {
            Source = new Uri("pack://application:,,,/VI.Control;component/Themes/LightClinical.xaml", UriKind.Absolute)
        });
        var selection = StartupSelection.Parse(["--sim"], AppContext.BaseDirectory);
        var serviceCollection = new ServiceCollection();
        App.ConfigureRobotServices(serviceCollection, selection);
        string modulesDirectory = Path.Combine(AppContext.BaseDirectory, "Modules", "Features");
        var loader = new ModuleLoader();
        Check(loader.LoadFromDirectory(modulesDirectory).Count == 2, "feature assemblies are copied and discovered dynamically");
        loader.ConfigureServices(serviceCollection);
        using var provider = serviceCollection.BuildServiceProvider();
        var link = provider.GetRequiredService<IRobotLink>();
        Check(link is SimulatedRobotLink && !link.IsConnected, "simulation creates an inert simulated link");
        Check(provider.GetService<IHandleService>() is null, "simulation never constructs the handle SDK service");
        var vm = provider.GetRequiredService<MainViewModel>();
        Check(!vm.CanOperate && vm.ConnectButtonText == "连接从端", "app startup does not connect or acquire control");

        var modules = loader.CreateModules(provider, ["intervention", "positioning-arm"]);
        Check(modules.Select(module => module.Metadata.Id).SequenceEqual(["intervention", "positioning-arm"]), "required feature modules keep stable order");
        var region = new ShellRegion();
        loader.RegisterModules(region, provider);
        Check(region.Panels.Count == 2, "both feature modules register a shell panel");
        Check(region.Panels[0].Content.GetType().Name == "InterventionPanel" && region.Panels[1].Content.GetType().Name == "PositioningArmPanel", "module factories load compiled WPF BAML views");

        var intervention = (UserControl)region.Panels[0].Content;
        var arm = (UserControl)region.Panels[1].Content;
        Check(((FrameworkElement)arm.FindName("CartesianControls")!).Visibility == Visibility.Visible, "Cartesian five-axis jog controls are visible");
        Check(VisualTreeHelper.GetDpi(intervention).DpiScaleX > 0, "WPF controls report valid device DPI");
        Directory.CreateDirectory(outputDirectory);
        foreach (var size in new[] { new Size(600, 560), new Size(1000, 900), new Size(1920, 1080) })
        foreach (double scale in new[] { 1.0, 1.25, 1.5 })
        {
            var layoutRegion = new ShellRegion();
            loader.RegisterModules(layoutRegion, provider);
            var mainWindow = new MainWindow(layoutRegion, vm, selection);
            var content = (FrameworkElement)mainWindow.Content;
            content.Measure(size);
            content.Arrange(new Rect(new Point(), size));
            content.UpdateLayout();
            var layoutIntervention = (UserControl)layoutRegion.Panels[0].Content;
            var layoutArm = (UserControl)layoutRegion.Panels[1].Content;
            Check(layoutIntervention.ActualWidth > 0 && layoutArm.ActualWidth > 0, $"both module panels arrange at scale {scale:0.##}");
            Check(((FrameworkElement)layoutIntervention.FindName("Injector1Push")!).ActualWidth > 0, $"intervention hold control remains laid out at scale {scale:0.##}");
            Check(((FrameworkElement)layoutArm.FindName("CartesianForward")!).ActualWidth > 0, $"arm hold control remains laid out at scale {scale:0.##}");
            var design = (FrameworkElement)mainWindow.FindName("DesignSurface")!;
            var viewbox = (Viewbox)mainWindow.FindName("DesignViewbox")!;
            Check(design.ActualWidth == 1280 && design.ActualHeight == 800, "window keeps the fixed 1280 × 800 design surface");
            Check(viewbox.Stretch == Stretch.Uniform && Math.Abs(layoutArm.ActualWidth - 384) < 0.01,
                $"uniform scaling retains the 384 DIP arm column at {size.Width} × {size.Height}, scale {scale:0.##}");
            Check(TextOptions.GetTextFormattingMode(design) == TextFormattingMode.Ideal,
                "scaled content uses Ideal text formatting");
            var dpiImage = new RenderTargetBitmap((int)Math.Round(size.Width * scale), (int)Math.Round(size.Height * scale), 96 * scale, 96 * scale, PixelFormats.Pbgra32);
            dpiImage.Render(content);
            Check(dpiImage.PixelWidth == (int)Math.Round(size.Width * scale) && dpiImage.PixelHeight == (int)Math.Round(size.Height * scale) && Math.Abs(dpiImage.DpiX - 96 * scale) < 0.01,
                $"original window/DPI render case remains covered at {size.Width} × {size.Height}, scale {scale:0.##}");
            mainWindow.Close();
        }

        var recording = DispatchProxy.Create<IRobotLink, RecordingRobotLink>();
        var recordingLink = (RecordingRobotLink)(object)recording;
        var holdVm = new MainViewModel(recording);
        recordingLink.SetConnected(true, true);
        Pump();
        Check(holdVm.CanOperate, "fresh authorized fake state unlocks hold controls");
        Check(holdVm.Force582F == "0.120 N" && holdVm.Force582N == "25.000 N·mm" && holdVm.Force587F == "0.300 N" && holdVm.Force587N == "-9.000 N·mm", "all four readouts preserve force and torque units");
        var holdIntervention = (UserControl)Activator.CreateInstance(intervention.GetType(), holdVm)!;
        var wireForceButton = (System.Windows.Controls.Primitives.ToggleButton)holdIntervention.FindName("GuidewireForceFeedback")!;
        var catheterForceButton = (System.Windows.Controls.Primitives.ToggleButton)holdIntervention.FindName("CatheterForceFeedback")!;
        var forceHoldButton = (System.Windows.Controls.Primitives.ToggleButton)holdIntervention.FindName("ForceHold")!;
        var zeroForceButton = (Button)holdIntervention.FindName("ForceZero")!;
        Pump();
        catheterForceButton.Command.Execute(null);
        Check(recordingLink.LastForce == (true, 1), "catheter force button targets only physical587");
        wireForceButton.Command.Execute(null);
        Check(recordingLink.LastForce == (true, 2), "guidewire force button targets only physical582");
        recordingLink.PublishForce(StatusFlags.FfEnabled | StatusFlags.Force582Enabled | StatusFlags.CalZeroed | StatusFlags.ForceHoldEnabled);
        Pump();
        Check(wireForceButton.IsChecked == true && catheterForceButton.IsChecked == false && holdVm.FfCaption == "零点已采集",
            "independent feedback switches follow acknowledged state");
        wireForceButton.Command.Execute(null);
        Check(recordingLink.LastForce == (false, 2), "enabled wire button sends explicit off for only wire");
        zeroForceButton.Command.Execute(null);
        Check(recordingLink.ForceZeroCount == 1, "zero button sends a standalone calibration command");
        Check((string)forceHoldButton.Content == "关闭保持", "hold caption reflects enabled state");
        forceHoldButton.Command.Execute(null);
        Check(!recordingLink.LastForceHold, "hold button sends explicit off");
        recordingLink.PublishForce(StatusFlags.None); Pump();
        Check((string)forceHoldButton.Content == "开启保持", "hold caption reflects disabled state");
        forceHoldButton.Command.Execute(null);
        Check(recordingLink.LastForceHold, "hold button sends explicit on");
        Check(recordingLink.StopArmCount == 0 && recordingLink.DisconnectCount == 0,
            "feedback controls do not stop motion or disconnect");
        Check(holdIntervention.FindName("Axis6WindowProgress") is null, "axis6 window is commented out rather than displayed");
        foreach (var (position, expectedPercent, near) in new[] { (117f, 50.0, false), (102f, 3.125, true), (132f, 96.875, true), (101f, 0.0, true), (133f, 100.0, true) })
        {
            recordingLink.PublishWindow(position, 101, 133);
            Pump();
            Check(holdVm.Axis6WindowAvailable && Math.Abs(holdVm.Axis6WindowPercent - expectedPercent) < 0.001 &&
                holdVm.Axis6WindowNearBoundary == near, "window position and both boundary warnings use live read-only feedback");
            Check(holdVm.Axis6WindowWarning == (near ? "接近导丝机构窗口位置，将要触发机构快退" : ""), "window warning has exactly the requested wording");
            Check(holdVm.CanOperate && recordingLink.StopArmCount == 0 && recordingLink.DisconnectCount == 0,
                "window warning does not stop motion or change operation availability");
        }
        recordingLink.PublishWindow(627, 611, 643);
        Pump();
        Check(holdVm.Axis6WindowPositionText == "16.0 / 32.0 mm" && holdVm.Axis6WindowPercent == 50,
            "moving axis5 window keeps axis6 relative position unchanged");
        recordingLink.PublishWindow(627, 0, 0);
        Pump();
        Check(!holdVm.Axis6WindowAvailable && holdVm.Axis6WindowWarning == "", "unavailable window does not show a false boundary warning");
        recordingLink.PublishWindow(float.NaN, 611, 643);
        Pump();
        Check(!holdVm.Axis6WindowAvailable && holdVm.Axis6WindowPercent == 0, "nonfinite position keeps the retained progress property at zero");
        recordingLink.PublishWindow(627, 611, 643);
        recordingLink.StatusAgeMs = ProtocolConstants.StatusTimeoutMs;
        Pump();
        Check(!holdVm.Axis6WindowAvailable && holdVm.Axis6WindowWarning == "", "expired telemetry clears the position warning");
        recordingLink.StatusAgeMs = 0;
        recordingLink.SetConnected(false, false);
        Pump();
        Check(!holdVm.Axis6WindowAvailable && holdVm.Axis6WindowPercent == 0, "disconnect clears the retained position property");
        recordingLink.SetConnected(true, true);
        Pump();
        var injector = (Button)holdIntervention.FindName("Injector1Push")!;
        Invoke(holdIntervention, "HoldButton_Down", injector, MouseArgs(injector));
        Invoke(holdIntervention, "HoldButton_Up", injector, new RoutedEventArgs(UIElement.PreviewMouseLeftButtonUpEvent));
        Check(recordingLink.LastInjector == (1, 0), "injector mouse release sends stop");
        Invoke(holdIntervention, "HoldButton_Down", holdIntervention.FindName("Axis4Forward")!, MouseArgs(injector));
        ((Button)holdIntervention.FindName("Axis4Forward")!).RaiseEvent(new MouseEventArgs(Mouse.PrimaryDevice, Environment.TickCount)
        {
            RoutedEvent = Mouse.LostMouseCaptureEvent
        });
        Check(recordingLink.LastAxis4 == 0, "lost mouse capture path stops axis 4");

        var holdArm = (UserControl)Activator.CreateInstance(arm.GetType(), holdVm)!;
        var axis = (Button)holdArm.FindName("Axis1Forward")!;
        var speedFast = (RadioButton)holdArm.FindName("ArmSpeedFast")!;
        var speedMedium = (RadioButton)holdArm.FindName("ArmSpeedMedium")!;
        var speedSlow = (RadioButton)holdArm.FindName("ArmSpeedSlow")!;
        Pump();
        Check(speedMedium.IsChecked == true && speedFast.IsChecked == false && speedSlow.IsChecked == false,
            "positioning arm starts in medium speed");
        foreach (var (selector, scale) in new[] { (speedFast, 2.0), (speedSlow, 0.5), (speedFast, 2.0), (speedMedium, 1.0) })
        {
            selector.SetCurrentValue(RadioButton.IsCheckedProperty, true);
            Pump();
            Check(new[] { speedFast, speedMedium, speedSlow }.Count(button => button.IsChecked == true) == 1,
                "speed selector keeps exactly one level selected");
            var expectedRates = scale switch
            {
                2.0 => new[] { 30.0, 30.0, 3.0, 1.2, 2.4 },
                1.0 => new[] { 15.0, 15.0, 2.5, 0.8, 1.6 },
                _ => new[] { 5.0, 5.0, 1.25, 0.4, 0.8 }
            };
            for (int mode = 1; mode <= expectedRates.Length; mode++)
            {
                Check(holdVm.GetArmCartesianJogSpeed(mode) == expectedRates[mode - 1],
                    $"Cartesian mode {mode} uses {scale} speed tier");
                Check(holdVm.SetArmCartesianJogAsync(mode,
                    (int)Math.Round(holdVm.GetArmCartesianJogSpeed(mode) * 1000)).GetAwaiter().GetResult() &&
                    recordingLink.LastArmCartesianJog == (mode,
                        (int)Math.Round(expectedRates[mode - 1] * 1000)) &&
                    recordingLink.LastArmCartesianSpeedScale == scale,
                    $"Cartesian mode {mode} sends its speed tier");
            }
            for (int armAxis = 1; armAxis <= 5; armAxis++)
            {
                holdVm.SetArmAxisJog(armAxis, 1);
                Check(recordingLink.LastArmJog == (armAxis, 1) && recordingLink.LastArmSpeedScale == scale,
                    $"shared speed {scale} reaches arm axis {armAxis}");
                holdVm.SetArmAxisJog(armAxis, 0);
            }
        }
        Invoke(holdArm, "ArmCartesian_Down", holdArm.FindName("CartesianForward")!, MouseArgs(axis));
        Check(recordingLink.LastArmCartesianJog == (1, 15000), "Cartesian X jog uses medium rate without parameter input");
        int stopsBeforeStale = recordingLink.StopArmCount;
        int commandsBeforeStale = recordingLink.ArmCartesianJogCount;
        recordingLink.StatusAgeMs = ProtocolConstants.StatusTimeoutMs;
        Invoke(holdVm, "RefreshStats");
        Pump();
        Check(!holdVm.CanOperate && ((Button)holdArm.FindName("CartesianForward")!).IsEnabled &&
            ((Button)holdArm.FindName("CartesianReverse")!).IsEnabled,
            "Cartesian hold buttons remain enabled when status becomes stale");
        Invoke(holdArm, "KeepaliveTimer_Tick", null!, EventArgs.Empty);
        Check(recordingLink.StopArmCount == stopsBeforeStale &&
            recordingLink.ArmCartesianJogCount == commandsBeforeStale + 1 &&
            recordingLink.LastArmCartesianJog == (1, 15000),
            "stale status preserves the held Cartesian request and sends keepalive");
        recordingLink.StatusAgeMs = 0;
        Invoke(holdVm, "RefreshStats");
        int statusChanges = 0;
        holdVm.PropertyChanged += (_, e) => { if (e.PropertyName == nameof(MainViewModel.ArmCartesianJogStatusText)) statusChanges++; };
        foreach (uint error in new uint[] { 3001, 3002, 3003, 0x4B00 })
        {
            recordingLink.PublishCartJog(2, error, true, 1.25);
            Pump();
            string expectedText = error switch
            {
                3001 => "末端点动参数无效",
                3002 => "驱动未就绪（总使能/上电/轴故障/复位/单轴点动/通信超时/急停）",
                3003 => "外部设定点发生器使能超时",
                _ => "NC 错误 0x4B00"
            };
            Check(holdVm.ArmCartesianJogStatusText == expectedText + $"（错误码 {error}）",
                $"Cartesian error {error} displays its code and Chinese reason before the limit hint");
        }
        foreach (uint state in new uint[] { 0, 1, 2, 3, 4 })
        {
            recordingLink.PublishCartJog(state, 0, true, 1.25);
            Pump();
            Check(holdVm.ArmCartesianJogStatusText == (state is 2 or 3
                ? "接近奇异位形或关节限速，已自动降速" : ""),
                $"Cartesian limit hint follows state {state}");
        }
        recordingLink.PublishCartJog(2, 0, false, 5);
        Pump();
        Check(statusChanges == 10 && holdVm.ArmCartesianJogStatusText == "",
            "Cartesian status property notifies and clears after the condition ends");
        Invoke(holdArm, "ArmCartesian_Up", holdArm.FindName("CartesianForward")!,
            new RoutedEventArgs(UIElement.PreviewMouseLeftButtonUpEvent));
        Invoke(holdArm, "ArmProgramZero_Click", holdArm.FindName("ProgramHome")!, new RoutedEventArgs());
        Check(recordingLink.ArmProgramZeroCount == 1, "Program home sends directly without parameter input");
        Invoke(holdArm, "ArmStop_Click", holdArm.FindName("ProgramHome")!, new RoutedEventArgs());
        Invoke(holdArm, "ArmJogButton_Down", axis, MouseArgs(axis));
        Invoke(holdArm, "ArmJogButton_Up", axis, new RoutedEventArgs(UIElement.PreviewMouseLeftButtonUpEvent));
        Check(recordingLink.LastArmJog == (1, 0), "arm mouse release sends zero jog");
        var closeRegion = new ShellRegion();
        closeRegion.RegisterPanel(new("intervention", "介入控制", 10), () => holdIntervention);
        closeRegion.RegisterPanel(new("positioning-arm", "定位臂控制", 20), () => holdArm);
        var closeWindow = new MainWindow(closeRegion, holdVm, selection);
        var designSize = new Size(1280, 800);
        ((FrameworkElement)closeWindow.Content).Measure(designSize);
        ((FrameworkElement)closeWindow.Content).Arrange(new Rect(new Point(), designSize));
        closeWindow.UpdateLayout();
        Check(!Visuals((DependencyObject)closeWindow.Content).OfType<Expander>().Any(), "shell no longer wraps module panels in collapsing expanders");
        Check(typeof(MainWindow).GetMethod("Panel_Collapsed", BindingFlags.NonPublic | BindingFlags.Instance) is null,
            "removed outer panel collapse path has no stop handler");
        var injector2Pull = Visuals(holdIntervention).OfType<Button>().Single(button =>
            Convert.ToString(button.Tag) == "2" && Convert.ToString(button.CommandParameter) == "-1");
        var axis4 = (Button)holdIntervention.FindName("Axis4Forward")!;
        Invoke(holdIntervention, "HoldButton_Down", injector, MouseArgs(injector));
        Invoke(holdIntervention, "HoldButton_Down", injector2Pull, MouseArgs(injector2Pull));
        Invoke(holdIntervention, "HoldButton_Down", axis4, MouseArgs(axis4));
        int stopsBeforeReveal = recordingLink.StopArmCount;
        ((System.Windows.Controls.Primitives.ToggleButton)holdIntervention.FindName("PrepareToggle")!).IsChecked = true;
        ((System.Windows.Controls.Primitives.ToggleButton)holdIntervention.FindName("PrepareToggle")!).IsChecked = false;
        ((System.Windows.Controls.Primitives.ToggleButton)holdArm.FindName("ArmDebugToggle")!).IsChecked = true;
        ((System.Windows.Controls.Primitives.ToggleButton)holdArm.FindName("ArmDebugToggle")!).IsChecked = false;
        Check(recordingLink.StopArmCount == stopsBeforeReveal && recordingLink.LastAxis4 == 1,
            "manual preparation and debugging reveal only changes layout");
        Invoke(holdArm, "ArmJogButton_Down", axis, MouseArgs(axis));
        Invoke(holdIntervention, "HoldButton_Down", injector2Pull, MouseArgs(injector2Pull));
        Invoke(holdIntervention, "HoldButton_Down", axis4, MouseArgs(axis4));
        int stopsBeforeFocusChange = recordingLink.StopArmCount;
        typeof(Window).GetMethod("OnDeactivated", BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(closeWindow, [EventArgs.Empty]);
        Check(recordingLink.StopArmCount == stopsBeforeFocusChange, "focus changes do not issue a global stop");
        closeWindow.Close();
        Check(recordingLink.StopArmCount > stopsBeforeFocusChange && recordingLink.ArmJogDirections.Values.All(direction => direction == 0) && recordingLink.LastInjector == (2, 0) && recordingLink.LastAxis4 == 0, "window close stops arm and intervention holds before host shutdown");
        holdVm.Dispose();

        string shellXaml = File.ReadAllText(Path.Combine(AppContext.BaseDirectory, "Fixtures", "MainWindow.xaml"));
        Check(!shellXaml.Contains("Deactivated", StringComparison.Ordinal), "switching focus does not invoke a global stop handler");
        Check(shellXaml.Contains("SlaveStatus", StringComparison.Ordinal) && !shellXaml.Contains("LogPane", StringComparison.Ordinal), "shell now hosts the requested connection capsules and no log pane");
        var acquireFailure = DispatchProxy.Create<IRobotLink, RecordingRobotLink>();
        var acquireFailureLink = (RecordingRobotLink)(object)acquireFailure;
        acquireFailureLink.AcquireResult = CommandResult.Rejected("ADS state is stale.");
        using (var acquireFailureVm = new MainViewModel(acquireFailure))
        {
            acquireFailureVm.ConnectCommand.Execute(null);
            Check(acquireFailureVm.IsConnected && acquireFailureLink.DisconnectCount == 0,
                "control acquisition rejection does not tear down a live link");
            Check(acquireFailureVm.LastWarning.Contains("ADS state is stale.", StringComparison.Ordinal),
                "control acquisition rejection remains visible with the data-end reason");
        }
        CheckStartupSelection();
        CheckRevisionU(intervention.GetType(), arm.GetType(), outputDirectory);
        Console.WriteLine($"All VI.Control checks passed ({_checks} assertions). ");
    }

    private static void CheckStartupSelection()
    {
        string fixture = Path.Combine(Path.GetTempPath(), "VI.Control-startup-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(Path.Combine(fixture, "config"));
            string token = Path.Combine(fixture, "config", "remote.token");
            File.WriteAllText(token, "0123456789abcdef");
            var remote = StartupSelection.Parse(["--host", "127.0.0.1"], Path.Combine(fixture, "output", "bin"));
            Check(!remote.IsSimulation && remote.Settings.Host == "127.0.0.1" && remote.Settings.Token.Length == 16, "startup finds an ancestor config token and preserves host override");
            Check(StartupSelection.Parse(["--sim", "--token", token], fixture).IsSimulation, "explicit simulation overrides a valid remote token");
            File.WriteAllText(token, "short");
            var invalid = StartupSelection.Parse(["--token", token], fixture);
            Check(invalid.IsSimulation && invalid.Notice.Contains("模拟模式", StringComparison.Ordinal), "invalid token produces an explicit simulation notice");
        }
        finally { Directory.Delete(fixture, true); }
    }

    private static MouseButtonEventArgs MouseArgs(IInputElement source) => new(Mouse.PrimaryDevice, Environment.TickCount, MouseButton.Left)
    {
        RoutedEvent = UIElement.PreviewMouseLeftButtonDownEvent,
        Source = source
    };

    private static void Invoke(object instance, string method, params object[] args) =>
        instance.GetType().GetMethod(method, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic)!.Invoke(instance, args);

    private static void Pump() => Dispatcher.CurrentDispatcher.Invoke(() => { }, DispatcherPriority.ApplicationIdle);

    private static void SaveScreenshot(Window window, Size logicalSize, double dpiScale, string fileName)
    {
        var bitmap = new RenderTargetBitmap((int)Math.Round(logicalSize.Width * dpiScale), (int)Math.Round(logicalSize.Height * dpiScale), 96 * dpiScale, 96 * dpiScale, PixelFormats.Pbgra32);
        var background = new DrawingVisual();
        using (var drawing = background.RenderOpen()) drawing.DrawRectangle(window.Background, null, new Rect(new Point(), logicalSize));
        bitmap.Render(background);
        bitmap.Render((Visual)window.Content);
        var encoder = new PngBitmapEncoder();
        encoder.Frames.Add(BitmapFrame.Create(bitmap));
        using var file = File.Create(fileName);
        encoder.Save(file);
    }

    private static IEnumerable<DependencyObject> Visuals(DependencyObject parent)
    {
        yield return parent;
        for (int index = 0; index < VisualTreeHelper.GetChildrenCount(parent); index++)
            foreach (var child in Visuals(VisualTreeHelper.GetChild(parent, index))) yield return child;
    }

    private static int _checks;

    private static void Check(bool condition, string message)
    {
        _checks++;
        if (!condition) throw new InvalidOperationException("FAIL: " + message);
        Console.WriteLine("PASS: " + message);
    }
}

public class RecordingRobotLink : DispatchProxy
{
    private readonly Dictionary<string, Delegate?> _events = new();
    private bool _connected;
    private bool _control;
    public (int Index, int Direction) LastInjector { get; private set; }
    public Dictionary<int, int> InjectorDirections { get; } = new();
    public int LastAxis4 { get; private set; }
    public (bool Enable, int Channel) LastForce { get; private set; }
    public bool LastForceHold { get; private set; }
    public int ForceZeroCount { get; private set; }
    public void PublishForce(StatusFlags flags)
    {
        _events.TryGetValue("StatusReceived", out var handler);
        ((EventHandler<StatusFrame>?)handler)?.Invoke(this, new StatusFrame
        {
            Flags = StatusFlags.LeaseHeld | flags, AxisPos = new float[7], AxisFromLeft = new float[7]
        });
    }
    public (int Axis, int Direction) LastArmJog { get; private set; }
    public (int Mode, int SpeedMilli) LastArmCartesianJog { get; private set; }
    public int ArmProgramZeroCount { get; private set; }
    public int ArmCartesianJogCount { get; private set; }
    public void PublishCartJog(uint state, uint error, bool limited, double speed)
    {
        _events.TryGetValue("StatusReceived", out Delegate? handler);
        ((EventHandler<StatusFrame>?)handler)?.Invoke(this, new StatusFrame
        {
            Flags = StatusFlags.LeaseHeld, AxisPos = new float[7], AxisFromLeft = new float[7],
            ArmCjogState = state, ArmCjogError = error, ArmCjogLimited = limited, ArmCjogPathSpeed = speed
        });
    }
    public double LastArmCartesianSpeedScale { get; private set; }
    public double LastArmSpeedScale { get; private set; }
    public Dictionary<int, int> ArmJogDirections { get; } = new();
    public int StopArmCount { get; private set; }
    public int DisconnectCount { get; private set; }
    public CommandResult AcquireResult { get; set; } = CommandResult.Done();
    public double StatusAgeMs { get; set; }
    public bool Handle582Online { get; set; }
    public bool Handle587Online { get; set; }

    public void PublishUiState(StatusFlags extra = StatusFlags.None, string recovery = "", float catheterAngle = 0, float wireAngle = 0)
    {
        _events.TryGetValue("StatusReceived", out Delegate? handler);
        ((EventHandler<StatusFrame>?)handler)?.Invoke(this, new StatusFrame
        {
            Flags = StatusFlags.LeaseHeld | StatusFlags.AdsHealthy | StatusFlags.ControlActive |
                StatusFlags.SelfCheckDone | StatusFlags.CalZeroed | StatusFlags.FfEnabled |
                StatusFlags.Force582Enabled | StatusFlags.Force587Enabled | extra,
            RecoveryText = recovery,
            AxisPos = [0, catheterAngle, 0, 0, 0, 0, wireAngle], AxisFromLeft = new float[7]
        });
    }

    public void PublishWindow(float position, float start, float end)
    {
        _events.TryGetValue("StatusReceived", out Delegate? handler);
        ((EventHandler<StatusFrame>?)handler)?.Invoke(this, new StatusFrame
        {
            Flags = StatusFlags.LeaseHeld | StatusFlags.AdsHealthy | StatusFlags.ControlActive,
            AxisPos = new float[7], AxisFromLeft = new[] { 0f, 0f, 0f, 0f, 0f, position, 0f },
            Axis6WindowStartFromLeftMm = start, Axis6WindowEndFromLeftMm = end
        });
    }

    public void SetConnected(bool connected, bool control)
    {
        _connected = connected;
        _control = control;
        _events.TryGetValue("ConnectionChanged", out Delegate? connectionChanged);
        _events.TryGetValue("StatusReceived", out Delegate? statusReceived);
        ((EventHandler?)connectionChanged)?.Invoke(this, EventArgs.Empty);
        ((EventHandler<StatusFrame>?)statusReceived)?.Invoke(this, new StatusFrame
        {
            Flags = StatusFlags.LeaseHeld,
            Force582F = 0.12f,
            Force582N = 0.025f,
            Force587F = 0.3f,
            Force587N = -0.009f,
            AxisPos = new float[7],
            AxisFromLeft = new float[7]
        });
    }

    protected override object? Invoke(MethodInfo? targetMethod, object?[]? args)
    {
        string name = targetMethod!.Name;
        if (name.StartsWith("add_", StringComparison.Ordinal) || name.StartsWith("remove_", StringComparison.Ordinal))
        {
            string eventName = name[(name.StartsWith("add_", StringComparison.Ordinal) ? 4 : 7)..];
            _events.TryGetValue(eventName, out Delegate? existing);
            _events[eventName] = name.StartsWith("add_", StringComparison.Ordinal)
                ? Delegate.Combine(existing, (Delegate)args![0]!)
                : Delegate.Remove(existing, (Delegate)args![0]!);
            return null;
        }
        if (name == "get_Name") return "Test link";
        if (name == "get_IsConnected") return _connected;
        if (name == "get_HasControl") return _control;
        if (name == "GetStats") return new LinkStats { StatusAgeMs = StatusAgeMs, Handle582Online = Handle582Online, Handle587Online = Handle587Online };
        if (name == "ConnectAsync") { SetConnected(true, false); return Task.CompletedTask; }
        if (name == "Disconnect") { DisconnectCount++; SetConnected(false, false); return null; }
        if (name == "AcquireControlAsync") return Task.FromResult(AcquireResult);
        if (name == "SetInjector")
        {
            LastInjector = ((int)args![0]!, (int)args[1]!);
            InjectorDirections[LastInjector.Index] = LastInjector.Direction;
            return null;
        }
        if (name == "SetAxis4") { LastAxis4 = (int)args![0]!; return null; }
        if (name == "SetArmAxisJog")
        {
            LastArmJog = ((int)args![0]!, (int)args[1]!);
            LastArmSpeedScale = (double)args[2]!;
            ArmJogDirections[LastArmJog.Axis] = LastArmJog.Direction;
            return null;
        }
        if (name == "SetArmCartesianJogAsync")
        {
            ArmCartesianJogCount++;
            LastArmCartesianJog = ((int)args![0]!, (int)args[1]!);
            LastArmCartesianSpeedScale = (double)args[2]!;
            return Task.FromResult(CommandResult.Done());
        }
        if (name == "ReturnArmProgramZeroAsync") { ArmProgramZeroCount++; return Task.FromResult(CommandResult.Done()); }
        if (name == "SetForceFeedbackAsync") { LastForce = ((bool)args![0]!, (int)args[1]!); return Task.FromResult(CommandResult.Done()); }
        if (name == "SetForceFeedbackHoldAsync") { LastForceHold = (bool)args![0]!; return Task.FromResult(CommandResult.Done()); }
        if (name == "ZeroForceSensorAsync") { ForceZeroCount++; return Task.FromResult(CommandResult.Done()); }
        if (name == "StopArmAsync") { StopArmCount++; return Task.FromResult(CommandResult.Done()); }
        if (targetMethod.ReturnType == typeof(Task<CommandResult>)) return Task.FromResult(CommandResult.Done());
        if (targetMethod.ReturnType == typeof(Task)) return Task.CompletedTask;
        return targetMethod.ReturnType.IsValueType ? Activator.CreateInstance(targetMethod.ReturnType) : null;
    }
}
