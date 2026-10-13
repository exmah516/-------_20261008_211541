// 只实例化 ViewModel 与去除设备控件/事件的 XAML，不启动主窗口的硬件服务。
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Markup;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using System.Xml.Linq;
using MasterConsole.Protocol;
using MasterConsole.Services;
using MasterConsole.ViewModels;

class MasterUiCheck
{
    static void Check(bool ok, string text)
    {
        if (!ok) throw new Exception(text);
        Console.WriteLine("PASS: " + text);
    }
    static void Pump() { Dispatcher.CurrentDispatcher.Invoke(new Action(delegate { }), DispatcherPriority.ApplicationIdle); }
    static StatusFrame Status(bool distance)
    {
        return new StatusFrame { HasNetDistance = distance, Axis1NetDistanceMm = 125.25,
            Axis6NetDistanceMm = -3.5, Force582F = 0.12f, Force587N = 0.025f,
            AxisPos = new float[7], AxisFromLeft = new float[7],
            Flags = StatusFlags.LeaseHeld | StatusFlags.AdsHealthy | StatusFlags.SelfCheckDone };
    }
    [STAThread]
    static int Main(string[] args)
    {
        SynchronizationContext.SetSynchronizationContext(new DispatcherSynchronizationContext());
        var app = new Application();
        var link = new FakeLink();
        using (var vm = new MainViewModel(link))
        {
            vm.ConnectCommand.Execute(null);
            link.Emit(Status(true)); Pump();
            Check(link.AcquireCount == 1 && !vm.CanOperate, "connection requests ownership automatically and waits for authorization");
            vm.ConnectCommand.Execute(null);
            Check(link.AcquireCount == 1, "repeated connect clicks do not duplicate acquisition");
            link.Complete(CommandResult.Done()); Pump();
            Check(vm.HasControl && vm.CanOperate, "authorized fresh connection unlocks operations");
            Check(vm.Axis1NetDistance == "125.250 mm" && vm.Axis6NetDistance == "-3.500 mm", "signed millimeter display");
            link.Emit(Status(false)); Pump();
            Check(vm.Axis1NetDistance == "—", "legacy status shows unavailable distance");
            link.Emit(Status(true)); Pump();
            typeof(MainViewModel).GetField("_lastStatusUtc", BindingFlags.Instance | BindingFlags.NonPublic)
                .SetValue(vm, DateTime.UtcNow.AddSeconds(-2));
            Check(!vm.CanOperate && vm.Axis1NetDistance == "—", "stale status locks operations and hides old distance");
            vm.ConnectCommand.Execute(null); Pump();
            Check(!vm.IsConnected && !vm.HasControl && vm.Axis1NetDistance == "—", "disconnect clears ownership and displayed distance");
            link.Next(); vm.ConnectCommand.Execute(null);
            link.Complete(CommandResult.Rejected("test denial")); Pump();
            Check(!vm.IsConnected && !vm.CanOperate && vm.LastWarning.Contains("test denial"), "denial disconnects cleanly and explains retry");
            link.Next(); vm.ConnectCommand.Execute(null);
            link.Complete(CommandResult.Rejected("从端无响应（超时）")); Pump();
            Check(!vm.IsConnected && !vm.HasControl, "timeout cannot grant ownership");
            link.Next(); vm.ConnectCommand.Execute(null);
            link.Disconnect(); link.Complete(CommandResult.Done()); Pump();
            Check(!vm.IsConnected && !vm.CanOperate, "disconnect during acquisition cannot unlock operations");
            link.Next(); vm.ConnectCommand.Execute(null); link.Complete(CommandResult.Done());
            link.Emit(Status(true)); Pump();
            Check(link.AcquireCount == 5 && vm.CanOperate, "reconnection automatically reacquires ownership");
        }
        var previewLink = new FakeLink();
        using (var preview = new MainViewModel(previewLink))
        {
            preview.ConnectCommand.Execute(null);
            previewLink.Complete(CommandResult.Done());
            previewLink.Emit(Status(true)); Pump();
            Render(args[0], args[1], app, preview, previewLink);
        }
        return 0;
    }

    static void Render(string project, string output, Application app, MainViewModel vm, FakeLink link)
    {
        app.Resources.MergedDictionaries.Add((ResourceDictionary)XamlReader.Parse(
            File.ReadAllText(Path.Combine(project, "master/MasterConsole/Themes/LightClinical.xaml"))));
        var doc = XDocument.Parse(File.ReadAllText(Path.Combine(project, "master/MasterConsole/Views/MainWindow.xaml")));
        XNamespace x = "http://schemas.microsoft.com/winfx/2006/xaml";
        XNamespace wpf = "http://schemas.microsoft.com/winfx/2006/xaml/presentation";
        doc.Root.Attribute(x + "Class").Remove();
        var events = new HashSet<string> { "Deactivated", "Click", "PreviewMouseLeftButtonDown", "PreviewMouseLeftButtonUp", "LostMouseCapture" };
        foreach (var attr in doc.Descendants().Attributes().Where(a => events.Contains(a.Name.LocalName)).ToArray()) attr.Remove();
        foreach (var control in doc.Descendants().Where(e => e.Name.NamespaceName.Contains("MasterConsole.Controls")).ToArray())
        {
            var placeholder = new XElement(wpf + "Border", new XAttribute("Background", "#F1F5F9"));
            foreach (var attr in control.Attributes().Where(a => a.Name.LocalName.StartsWith("Grid.") || a.Name == x + "Name"))
                placeholder.Add(new XAttribute(attr));
            control.ReplaceWith(placeholder);
        }
        var window = (Window)XamlReader.Parse(doc.ToString().Replace("clr-namespace:MasterConsole.Converters", "clr-namespace:MasterConsole.Converters;assembly=MasterConsole"));
        window.DataContext = vm;
        var content = (FrameworkElement)window.Content;
        ((Panel)content).Background = window.Background;
        Directory.CreateDirectory(output);
        foreach (int width in new[] { 1480, 1180 })
        {
            int height = width == 1480 ? 880 : 720;
            link.Emit(Status(true)); Pump();
            content.Measure(new Size(width, height));
            content.Arrange(new Rect(0, 0, width, height));
            content.UpdateLayout(); Pump();
            var bitmap = new RenderTargetBitmap(width, height, 96, 96, PixelFormats.Pbgra32);
            bitmap.Render(content);
            var encoder = new PngBitmapEncoder(); encoder.Frames.Add(BitmapFrame.Create(bitmap));
            using (var file = File.Create(Path.Combine(output, "master-ui-" + width + ".png"))) encoder.Save(file);
            Check(HasCollapsedParent((FrameworkElement)window.FindName("ArmCartesianMode")), "temporary arm controls remain collapsed at " + width);
            var texts = Visuals(content).OfType<TextBlock>().ToArray();
            var distance = texts.Single(t => BoundTo(t, "Axis1NetDistance"));
            var force = texts.Single(t => BoundTo(t, "Force582F"));
            Check(distance.FontFamily.Equals(force.FontFamily) && distance.FontSize == force.FontSize,
                "distance and force share typography at " + width);
            Check(distance.ActualWidth > 0 && distance.ActualWidth <= 390, "distance fits right panel at " + width);
            Check(!Visuals(content).OfType<GroupBox>().Any(g => Convert.ToString(g.Header) == "定位臂控制"), "no nested arm group box");
            Check(!doc.ToString().Contains("AcquireCommand") && !doc.ToString().Contains("七轴位置 / 行程"), "obsolete controls removed");
        }
        window.Close();
    }
    static bool BoundTo(TextBlock text, string path)
    {
        var binding = BindingOperations.GetBinding(text, TextBlock.TextProperty);
        return binding != null && binding.Path.Path == path;
    }
    static bool HasCollapsedParent(FrameworkElement item)
    {
        while (item != null)
        {
            if (item.Visibility == Visibility.Collapsed) return true;
            item = item.Parent as FrameworkElement;
        }
        return false;
    }
    static IEnumerable<DependencyObject> Visuals(DependencyObject item)
    {
        yield return item;
        for (int i = 0; i < VisualTreeHelper.GetChildrenCount(item); i++)
            foreach (var child in Visuals(VisualTreeHelper.GetChild(item, i))) yield return child;
    }
    class FakeLink : IRobotLink
    {
        public string Name { get { return "离线验证"; } }
        public bool IsConnected { get; private set; }
        public bool HasControl { get; private set; }
        public int AcquireCount;
        TaskCompletionSource<CommandResult> pending = new TaskCompletionSource<CommandResult>();
        public event EventHandler ConnectionChanged;
        public event EventHandler<StatusFrame> StatusReceived;
        public event EventHandler<LogMessage> LogReceived { add { } remove { } }
        void Changed() { if (ConnectionChanged != null) ConnectionChanged(this, EventArgs.Empty); }
        public Task ConnectAsync() { IsConnected = true; Changed(); return Task.FromResult(0); }
        public void Disconnect() { IsConnected = HasControl = false; Changed(); }
        public Task<CommandResult> AcquireControlAsync() { AcquireCount++; return pending.Task; }
        public void Complete(CommandResult result) { HasControl = IsConnected && result.Ok; pending.SetResult(result); Changed(); }
        public void Next() { pending = new TaskCompletionSource<CommandResult>(); }
        public void Emit(StatusFrame status) { if (StatusReceived != null) StatusReceived(this, status); }
        public void Dispose() { }
        public LinkStats GetStats() { return new LinkStats(); }
        static Task<CommandResult> Done() { return Task.FromResult(CommandResult.Done()); }
        public Task<CommandResult> ReleaseControlAsync() { HasControl = false; return Done(); }
        public Task<CommandResult> PreparePositionAsync(double a, double b) { return Done(); }
        public Task<CommandResult> StartControlAsync() { return Done(); }
        public Task<CommandResult> SetArmManualEnableAsync(bool on) { return Done(); }
        public Task<CommandResult> SetArmAxisEnableAsync(int axis, bool on) { return Done(); }
        public Task<CommandResult> ResetArmAxisAsync(int axis) { return Done(); }
        public Task<CommandResult> SetArmCartesianJogAsync(int mode, int speed) { return Done(); }
        public Task<CommandResult> StopArmAsync() { return Done(); }
        public Task<CommandResult> ReturnArmProgramZeroAsync() { return Done(); }
        public Task<CommandResult> SetArmCartesianParameterAsync(int field, int value) { return Done(); }
        public void SetArmAxisJog(int axis, int direction) { }
        public void KeepArmCartesianAlive() { }
        public Task<CommandResult> SetForceFeedbackAsync(bool on) { return Done(); }
        public Task<CommandResult> SetCylinderAsync(int index, bool on) { return Done(); }
        public Task<CommandResult> SetYValveClosedAsync(bool on) { return Done(); }
        public void SetInjector(int index, int direction) { }
        public void SetAxis4(int direction) { }
    }
}
