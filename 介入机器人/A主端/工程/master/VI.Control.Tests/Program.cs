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

internal static class Program
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
        Check(((FrameworkElement)arm.FindName("CartesianControls")!).Visibility == Visibility.Collapsed, "Cartesian and home controls retain their hidden default");
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
            Check(((FrameworkElement)layoutArm.FindName("Axis1Forward")!).ActualWidth > 0, $"arm hold control remains laid out at scale {scale:0.##}");
            var scroll = Visuals(content).OfType<ScrollViewer>().First();
            Check(scroll.ScrollableWidth == 0, $"no horizontal overflow at {size.Width} × {size.Height}, scale {scale:0.##}");
            if (size.Width == 600) Check(scroll.ScrollableHeight > 0, "small window keeps controls reachable by vertical scrolling");
            string fileName = $"vi-control-{size.Width:0}x{size.Height:0}-{scale:0.##}.png";
            SaveScreenshot(mainWindow, size, scale, Path.Combine(outputDirectory, fileName));
            scroll.ScrollToBottom();
            content.UpdateLayout();
            Check(scroll.VerticalOffset >= scroll.ScrollableHeight, "bottom controls remain reachable through the shell scroll viewer");
            SaveScreenshot(mainWindow, size, scale, Path.Combine(outputDirectory, fileName.Replace(".png", "-bottom.png", StringComparison.Ordinal)));
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
        Invoke(holdArm, "ArmJogButton_Down", axis, MouseArgs(axis));
        Invoke(holdArm, "ArmJogButton_Up", axis, new RoutedEventArgs(UIElement.PreviewMouseLeftButtonUpEvent));
        Check(recordingLink.LastArmJog == (1, 0), "arm mouse release sends zero jog");
        var closeRegion = new ShellRegion();
        closeRegion.RegisterPanel(new("intervention", "介入控制", 10), () => holdIntervention);
        closeRegion.RegisterPanel(new("positioning-arm", "定位臂控制", 20), () => holdArm);
        var closeWindow = new MainWindow(closeRegion, holdVm, selection);
        ((FrameworkElement)closeWindow.Content).Measure(new Size(1000, 900));
        ((FrameworkElement)closeWindow.Content).Arrange(new Rect(0, 0, 1000, 900));
        closeWindow.UpdateLayout();
        var expanders = Visuals((DependencyObject)closeWindow.Content).OfType<Expander>().ToArray();
        var interventionExpander = expanders
            .Single(expander => expander.DataContext is ShellPanel panel && panel.Metadata.Id == "intervention");
        var armExpander = expanders
            .Single(expander => expander.DataContext is ShellPanel panel && panel.Metadata.Id == "positioning-arm");
        var injector2Pull = Visuals(holdIntervention).OfType<Button>().Single(button =>
            Convert.ToString(button.Tag) == "2" && Convert.ToString(button.CommandParameter) == "-1");
        var axis4 = (Button)holdIntervention.FindName("Axis4Forward")!;
        Invoke(holdIntervention, "HoldButton_Down", injector, MouseArgs(injector));
        Invoke(holdIntervention, "HoldButton_Down", injector2Pull, MouseArgs(injector2Pull));
        Invoke(holdIntervention, "HoldButton_Down", axis4, MouseArgs(axis4));
        interventionExpander.IsExpanded = false;
        Check(recordingLink.InjectorDirections.Count == 2 && recordingLink.InjectorDirections.Values.All(direction => direction == 0) && recordingLink.LastAxis4 == 0, "collapsing the intervention panel stops both injectors and axis 4");
        interventionExpander.IsExpanded = true;
        int stopsBeforeCollapse = recordingLink.StopArmCount;
        Invoke(holdArm, "ArmJogButton_Down", axis, MouseArgs(axis));
        armExpander.IsExpanded = false;
        Check(recordingLink.ArmJogDirections.Count == 5 && recordingLink.ArmJogDirections.Values.All(direction => direction == 0) && recordingLink.StopArmCount > stopsBeforeCollapse, "collapsing the arm panel clears held motion");
        armExpander.IsExpanded = true;
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
        Check(!shellXaml.Contains("StatusBar", StringComparison.Ordinal) && !shellXaml.Contains("LogPane", StringComparison.Ordinal), "shell has no connection status bar or log pane");
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
        Console.WriteLine("All VI.Control checks passed.");
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

    private static void Check(bool condition, string message)
    {
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
    public (int Axis, int Direction) LastArmJog { get; private set; }
    public Dictionary<int, int> ArmJogDirections { get; } = new();
    public int StopArmCount { get; private set; }
    public int DisconnectCount { get; private set; }
    public CommandResult AcquireResult { get; set; } = CommandResult.Done();

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
        if (name == "GetStats") return new LinkStats { StatusAgeMs = 0 };
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
            ArmJogDirections[LastArmJog.Axis] = LastArmJog.Direction;
            return null;
        }
        if (name == "StopArmAsync") { StopArmCount++; return Task.FromResult(CommandResult.Done()); }
        if (targetMethod.ReturnType == typeof(Task<CommandResult>)) return Task.FromResult(CommandResult.Done());
        if (targetMethod.ReturnType == typeof(Task)) return Task.CompletedTask;
        return targetMethod.ReturnType.IsValueType ? Activator.CreateInstance(targetMethod.ReturnType) : null;
    }
}
