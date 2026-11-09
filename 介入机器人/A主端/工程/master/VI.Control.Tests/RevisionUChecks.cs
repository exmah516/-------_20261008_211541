using System.Globalization;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;
using System.Xml.Linq;
using MasterConsole.Protocol;
using MasterConsole.Services;
using MasterConsole.ViewModels;
using VI.Control;

internal static partial class Program
{
    private static readonly XNamespace Ui = "http://schemas.microsoft.com/winfx/2006/xaml/presentation";
    private static readonly XNamespace X = "http://schemas.microsoft.com/winfx/2006/xaml";

    private static void CheckRevisionU(Type interventionType, Type armType, string output)
    {
        string fixtures = Path.Combine(AppContext.BaseDirectory, "Fixtures");
        string[] files = ["MainWindow.xaml", "InterventionPanel.xaml", "PositioningArmPanel.xaml"];
        var oldViews = files.Select(file => XDocument.Load(Path.Combine(fixtures, "Baseline." + file + ".xml"))).ToArray();
        var newViews = files.Select(file => XDocument.Load(Path.Combine(fixtures, file))).ToArray();
        var newControls = newViews.SelectMany(view => view.Descendants()).ToArray();
        var rows = new List<string> { "原文件,原控件,新控件,Command或事件,Tag,CommandParameter,IsEnabled,ToolTip,结果" };
        string[] actions = ["Command", "Click", "PreviewMouseLeftButtonDown", "PreviewMouseLeftButtonUp", "LostMouseCapture", "Checked"];
        for (int fileIndex = 0; fileIndex < files.Length; fileIndex++)
        foreach (var original in oldViews[fileIndex].Descendants().Where(element => element.Name.LocalName is "Button" or "ToggleButton" or "RadioButton" or "TextBox"))
        {
            string oldName = (string?)original.Attribute(X + "Name") ?? (string?)original.Attribute("Content") ?? "电缸模板";
            bool cylinder = ((string?)original.Attribute("Command"))?.Contains("CylinderCommand", StringComparison.Ordinal) == true;
            XElement[] mapped;
            if (cylinder) mapped = Enumerable.Range(1, 4).Select(index => newControls.Single(element => (string?)element.Attribute(X + "Name") == "Cylinder" + index)).ToArray();
            else if (original.Attribute(X + "Name") is XAttribute name)
                mapped = [newControls.Single(element => (string?)element.Attribute(X + "Name") == name.Value)];
            else mapped = [newControls.Single(element => element.Name == original.Name &&
                actions.Concat(["Tag", "CommandParameter"]).All(key => (string?)element.Attribute(key) == (string?)original.Attribute(key)))];

            foreach (var current in mapped)
            {
                string newName = (string?)current.Attribute(X + "Name") ?? (string?)current.Attribute("Content") ?? oldName;
                string label = oldName + " → " + newName;
                foreach (string key in actions)
                {
                    string before = (string?)original.Attribute(key) ?? "";
                    string after = (string?)current.Attribute(key) ?? "";
                    Check(cylinder && key == "Command" ? after.Contains("DataContext.CylinderCommand", StringComparison.Ordinal) : before == after, label + " preserves " + key);
                }
                bool cartesian = (string?)original.Attribute("PreviewMouseLeftButtonDown") == "ArmCartesian_Down";
                Check((string?)current.Attribute("Tag") == (cartesian ? "1" : (string?)original.Attribute("Tag")), label + " preserves Tag or explicitly assigns the former X mode");
                Check((string?)current.Attribute("CommandParameter") == (string?)original.Attribute("CommandParameter"), label + " preserves CommandParameter");
                Check(EnabledPaths(original) == EnabledPaths(current), label + " preserves local and inherited IsEnabled paths");
                Check(cylinder ? ToolTip(current) == "{Binding Title, Mode=OneWay} |  ·  | " + ToolTip(original) : ToolTip(original) == ToolTip(current),
                    label + " preserves ToolTip (cylinder adds the explicitly requested full name)");
                foreach (string key in new[] { "IsChecked", "Text", "GroupName" })
                    Check((string?)original.Attribute(key) == (string?)current.Attribute(key), label + " preserves " + key);
                rows.Add(string.Join(",", new[] { files[fileIndex], oldName, newName, string.Join("; ", actions.Select(key => (string?)current.Attribute(key)).Where(value => value is not null)), (string?)current.Attribute("Tag") ?? "", (string?)current.Attribute("CommandParameter") ?? "", EnabledPaths(current), ToolTip(current), "通过" }.Select(Csv)));
            }
        }
        var cartesianButtons = newControls.Where(element => (string?)element.Attribute("PreviewMouseLeftButtonDown") == "ArmCartesian_Down").ToArray();
        Check(cartesianButtons.Length == 10, "all ten Cartesian buttons exist");
        for (int mode = 1; mode <= 5; mode++)
        foreach (int direction in new[] { -1, 1 })
        {
            var button = cartesianButtons.Single(element => (string?)element.Attribute("Tag") == mode.ToString(CultureInfo.InvariantCulture) && (string?)element.Attribute("CommandParameter") == direction.ToString(CultureInfo.InvariantCulture));
            Check((string?)button.Attribute("PreviewMouseLeftButtonUp") == "ArmCartesian_Up" && (string?)button.Attribute("LostMouseCapture") == "ArmCartesian_Up" && EnabledPaths(button) == "",
                $"Cartesian mode {mode}, direction {direction} keeps release/capture-stop events and no enabled binding");
            rows.Add(string.Join(",", new[] { "PositioningArmPanel.xaml", "末端模式选择 + 原方向按钮", (string?)button.Attribute(X + "Name") ?? "", "ArmCartesian_Down / ArmCartesian_Up / LostMouseCapture", mode.ToString(), direction.ToString(), "无", "无", "通过" }.Select(Csv)));
        }
        var oldTheme = XDocument.Load(Path.Combine(fixtures, "Baseline.LightClinical.xaml.xml"));
        var theme = XDocument.Load(Path.Combine(fixtures, "LightClinical.xaml"));
        var keys = theme.Root!.Elements().Attributes(X + "Key").Select(attribute => attribute.Value).ToHashSet();
        foreach (string key in oldTheme.Root!.Elements().Attributes(X + "Key").Select(attribute => attribute.Value))
            Check(keys.Contains(key), "theme retains original resource key " + key);
        var axis6Comment = newViews[1].DescendantNodes().OfType<XComment>().Single(comment => comment.Value.Contains("Axis6WindowProgress", StringComparison.Ordinal));
        var commentedAxis6 = XElement.Parse("<Root xmlns=\"" + Ui + "\" xmlns:x=\"" + X + "\">" + axis6Comment.Value[(axis6Comment.Value.IndexOf('<'))..] + "</Root>").Elements().Single();
        var originalAxis6 = oldViews[1].Descendants(Ui + "Border").Single(border => border.Descendants().Any(element => (string?)element.Attribute(X + "Name") == "Axis6WindowProgress"));
        Check(XNode.DeepEquals(originalAxis6, commentedAxis6), "entire axis6 window block remains unchanged inside the requested XAML comment");
        Check(!newViews[0].Descendants(Ui + "Expander").Any(), "module outer expanders are removed");
        Check(!newViews[2].Descendants(Ui + "ComboBox").Any(), "Cartesian ComboBox is replaced by five explicit mode rows");
        Check(!newViews[1].Descendants(Ui + "ItemsControl").Any(), "four cylinder controls bind their individual collection items");
        foreach (var (file, property) in new[]
        {
            ("InterventionPanel.xaml", "RecoveryText"), ("InterventionPanel.xaml", "OperateHintText"), ("InterventionPanel.xaml", "LastWarning"),
            ("InterventionPanel.xaml", "Force582F"), ("InterventionPanel.xaml", "Force582N"), ("InterventionPanel.xaml", "Force587F"), ("InterventionPanel.xaml", "Force587N"),
            ("InterventionPanel.xaml", "PrepareStatusText"), ("InterventionPanel.xaml", "PrepareError"), ("InterventionPanel.xaml", "FfCaption"),
            ("InterventionPanel.xaml", "YValveCaption"), ("InterventionPanel.xaml", "Axis4Text"), ("InterventionPanel.xaml", "Injector1Text"), ("InterventionPanel.xaml", "Injector2Text"),
            ("PositioningArmPanel.xaml", "ArmFeedbackText"), ("PositioningArmPanel.xaml", "ArmCartesianJogStatusText")
        })
        {
            string binding = "{Binding " + property + "}";
            Check(oldViews[Array.IndexOf(files, file)].Descendants().Attributes("Text").Any(attribute => attribute.Value == binding) && newControls.Attributes("Text").Any(attribute => attribute.Value == binding || attribute.Value.StartsWith("{Binding " + property + ",", StringComparison.Ordinal)), property + " retains its display binding after relocation");
            rows.Add(string.Join(",", new[] { file, property, property is "RecoveryText" or "OperateHintText" or "LastWarning" ? "MainWindow 提示行" : "原绑定迁入对应卡片/行", "Text=" + binding, "", "", "只读显示", "", "通过" }.Select(Csv)));
        }
        File.WriteAllLines(Path.Combine(output, "ui_mapping.csv"), rows, System.Text.Encoding.UTF8);
        foreach (var (kind, prefix, axis, cylinders) in new[] { ("导丝", "Guidewire", 7, new[] { 3, 4 }), ("导管", "Catheter", 2, new[] { 1, 2 }) })
        {
            var card = newControls.Single(element => (string?)element.Attribute(X + "Name") == prefix + "ForceFeedback").Ancestors(Ui + "Border").First();
            foreach (string property in new[] { $"Axis{axis}PointerX", $"Axis{axis}PointerY", $"Axis{axis}AngleText" })
                Check(card.Descendants().Attributes().Any(attribute => attribute.Value == "{Binding " + property + "}"), kind + " card displays " + property);
            foreach (int index in cylinders)
                Check(card.Descendants().Any(element => (string?)element.Attribute(X + "Name") == "Cylinder" + index), kind + " card contains cylinder " + index);
        }
        var hintBindings = newViews[0].Descendants(Ui + "Grid").Single(element => (string?)element.Attribute(X + "Name") == "HintMessages").Elements(Ui + "TextBlock").Select(element => (string?)element.Attribute("Text"));
        Check(hintBindings.SequenceEqual(["{Binding OperateHintText}", "{Binding RecoveryText}", "{Binding LastWarning}"]), "hint messages retain their required order");

        CheckV3Mapping(fixtures, newViews);
        CheckDisplayAndMotion(interventionType, armType, output);
    }

    private static string EnabledPaths(XElement element) => string.Join(" & ", element.AncestorsAndSelf().Attributes("IsEnabled").Select(attribute => attribute.Value).Order());
    private static string ToolTip(XElement element) => (string?)element.Attribute("ToolTip") ??
        (element.Elements().SingleOrDefault(child => child.Name.LocalName.EndsWith(".ToolTip", StringComparison.Ordinal)) is XElement toolTip
            ? string.Join(" | ", toolTip.Descendants(Ui + "Run").Select(run => (string?)run.Attribute("Text"))) : "");
    private static bool IsRendered(DependencyObject element)
    {
        for (DependencyObject? parent = element; parent is not null; parent = VisualTreeHelper.GetParent(parent))
            if (parent is UIElement { Visibility: not Visibility.Visible }) return false;
        return true;
    }

    private static string Csv(string value) => "\"" + value.Replace("\"", "\"\"", StringComparison.Ordinal) + "\"";

    private static void CheckDisplayAndMotion(Type interventionType, Type armType, string output)
    {
        var link = System.Reflection.DispatchProxy.Create<IRobotLink, RecordingRobotLink>();
        var recording = (RecordingRobotLink)(object)link;
        using var vm = new MainViewModel(link);
        Check(vm.Axis2AngleText == "—" && vm.Axis7AngleText == "—", "angles have no fabricated value without status");
        var intervention = (UserControl)Activator.CreateInstance(interventionType, vm)!;
        var readoutParts = (System.Windows.Data.IValueConverter)intervention.FindResource("ReadoutPart");
        foreach (var (text, number, unit) in new[] { ("0.000 N", "0.000", "N"), ("-9.000 N·mm", "-9.000", "N·mm"), ("25.000 N·mm", "25.000", "N·mm"), ("—", "—", "") })
        {
            Check((string)readoutParts.Convert(text, typeof(string), "Value", CultureInfo.InvariantCulture) == number &&
                (string)readoutParts.Convert(text, typeof(string), "Unit", CultureInfo.InvariantCulture) == unit,
                "readout presentation splits only the existing formatted text: " + text);
        }
        var arm = (UserControl)Activator.CreateInstance(armType, vm)!;
        var region = new ShellRegion();
        region.RegisterPanel(new("intervention", "介入控制", 10), () => intervention);
        region.RegisterPanel(new("positioning-arm", "定位臂控制", 20), () => arm);
        var window = new MainWindow(region, vm, new StartupSelection(false, new RemoteLinkSettings(), ""));
        recording.SetConnected(true, true);
        recording.Handle582Online = recording.Handle587Online = true;
        recording.PublishUiState(recovery: "提示：导丝手柄采样中断后恢复，已在当前位置重建基准。");
        Invoke(vm, "RefreshStats");
        Pump();
        Check(ReferenceEquals(window.DataContext, vm) && ReferenceEquals(intervention.DataContext, vm) && ReferenceEquals(arm.DataContext, vm), "shell and both module panels keep the constructor MainViewModel");
        foreach (string name in new[] { "SlaveStatus", "AdsStatus", "WireHandleStatus", "CatheterHandleStatus" })
            Check(((SolidColorBrush)((Border)window.FindName(name)!).Background).Color == (Color)ColorConverter.ConvertFromString("#E7F4EC"), name + " uses its connected green capsule");
        var prepToggle = (ToggleButton)intervention.FindName("PrepareToggle")!;
        var debugToggle = (ToggleButton)arm.FindName("ArmDebugToggle")!;
        var prepDetails = (FrameworkElement)intervention.FindName("PrepareDetails")!;
        var debugDetails = (FrameworkElement)arm.FindName("ArmDebugDetails")!;
        Check(prepToggle.IsChecked == false && debugToggle.IsChecked == false && prepDetails.Visibility == Visibility.Collapsed && debugDetails.Visibility == Visibility.Collapsed,
            "preparation and arm debugging both start manually collapsed");
        Check(((Border)window.FindName("SimulationBanner")!).Visibility == Visibility.Collapsed, "non-simulation screenshot hides only the simulation banner");
        foreach (var (index, title, open) in new[] { (1, "导管夹持", 2000), (2, "导管旋捻", 10), (3, "导丝夹持", 2000), (4, "导丝旋捻", 10) })
        {
            var button = (ToggleButton)intervention.FindName("Cylinder" + index)!;
            Check(ReferenceEquals(button.DataContext, vm.Cylinders[index - 1]) && ReferenceEquals(button.CommandParameter, vm.Cylinders[index - 1]) && ReferenceEquals(button.Command, vm.CylinderCommand), $"cylinder {index} keeps its command and exact item parameter");
            Check(vm.Cylinders[index - 1].Title == title && vm.Cylinders[index - 1].OpenValue == open, $"cylinder {index} changes only its display title");
        }
        var notifications = new HashSet<string>();
        vm.PropertyChanged += (_, args) => notifications.Add(args.PropertyName ?? "");
        recording.Handle587Online = false;
        Invoke(vm, "RefreshStats");
        Pump();
        Check(vm.Handle582Online && !vm.Handle587Online && notifications.Contains(nameof(MainViewModel.Handle582Online)) && notifications.Contains(nameof(MainViewModel.Handle587Online)), "individual handle passthrough properties refresh and notify separately");
        Check(((SolidColorBrush)((Border)window.FindName("CatheterHandleStatus")!).Background).Color == (Color)ColorConverter.ConvertFromString("#F1F4F7"), "offline catheter changes only its own capsule to gray");
        recording.Handle587Online = true;
        Invoke(vm, "RefreshStats");
        recording.PublishUiState(catheterAngle: 12.34f, wireAngle: -45.67f);
        Pump();
        Check(vm.Axis2AngleText == "12.3°" && vm.Axis7AngleText == "-45.7°" && notifications.Contains(nameof(MainViewModel.Axis2AngleText)) && notifications.Contains(nameof(MainViewModel.Axis7AngleText)), "axis2 catheter and axis7 guidewire angle text uses live one-decimal telemetry");
        Check(((FrameworkElement)window.FindName("HintRow")!).Visibility == Visibility.Collapsed, "three empty hint strings collapse the whole hint row");
        recording.SetConnected(true, false);
        Pump();
        Check(((TextBlock)window.FindName("SlaveStatusText")!).Text == "已连接 · 无控制权", "slave capsule distinguishes a live link without control");
        Check(prepToggle.IsChecked == false && debugToggle.IsChecked == false, "connection state never expands the manual sections");
        recording.SetConnected(true, true);
        recording.PublishUiState();
        Pump();
        recording.PublishUiState(StatusFlags.GlobalReverse);
        Pump();
        Check(vm.ModeText == "撤出", "direction label uses the acknowledged reverse mode");

        for (int mode = 1; mode <= 5; mode++)
        foreach (int direction in new[] { -1, 1 })
        {
            string name = mode == 1 ? (direction == 1 ? "CartesianForward" : "CartesianReverse") : $"Cartesian{mode}{(direction == 1 ? "Forward" : "Reverse")}";
            var button = (Button)arm.FindName(name)!;
            Invoke(arm, "ArmCartesian_Down", button, MouseArgs(button));
            Check(recording.LastArmCartesianJog == (mode, direction * (int)Math.Round(vm.GetArmCartesianJogSpeed(mode) * 1000)), $"Cartesian button {name} sends its own mode and direction");
            int requests = recording.ArmCartesianJogCount;
            Invoke(arm, "KeepaliveTimer_Tick", null!, EventArgs.Empty);
            Check(recording.ArmCartesianJogCount == requests + 1, $"Cartesian button {name} retains held-request keepalive");
            int stops = recording.StopArmCount;
            button.RaiseEvent(new System.Windows.Input.MouseEventArgs(System.Windows.Input.Mouse.PrimaryDevice, Environment.TickCount) { RoutedEvent = System.Windows.Input.Mouse.LostMouseCaptureEvent });
            Check(recording.StopArmCount > stops, $"Cartesian button {name} stops on lost capture");
        }
        var yaw = (Button)arm.FindName("Cartesian5Forward")!;
        Invoke(arm, "ArmCartesian_Down", yaw, MouseArgs(yaw));
        ((RadioButton)arm.FindName("ArmSpeedFast")!).SetCurrentValue(ToggleButton.IsCheckedProperty, true);
        Pump();
        Check(recording.LastArmCartesianJog == (5, 2400), "changing speed while held immediately resends the recorded yaw mode at its new speed");
        for (int mode = 1; mode <= 5; mode++)
        {
            double speed = vm.GetArmCartesianJogSpeed(mode);
            string expected = mode is 4 or 5 ? $"{speed:0.##}°/s" : $"{speed:0.##} mm/s";
            Check(((TextBlock)arm.FindName("ArmCartesianRate" + mode)!).Text == expected, $"speed row {mode} refreshes from GetArmCartesianJogSpeed with the original units");
        }
        Invoke(arm, "ArmCartesian_Up", yaw, new RoutedEventArgs(UIElement.PreviewMouseLeftButtonUpEvent));
        ((RadioButton)arm.FindName("ArmSpeedMedium")!).SetCurrentValue(ToggleButton.IsCheckedProperty, true);
        recording.PublishUiState(recovery: "提示：导丝手柄采样中断后恢复，已在当前位置重建基准。");
        CheckResponsiveLayout(window, intervention, arm, output);
        recording.SetConnected(true, true);
        Pump();
        Check(vm.Force582N == "25.000 N·mm" && vm.Force587N == "-9.000 N·mm", "narrow readout geometry also covers the original signed multi-digit telemetry");
        RenderResponsive(window, intervention, arm, "readouts-520x700.png", new Size(520, 700), output);
        recording.PublishUiState(recovery: "提示：导丝手柄采样中断后恢复，已在当前位置重建基准。");
        Pump();
        prepToggle.IsChecked = true;
        Pump();
        Check(prepDetails.Visibility == Visibility.Visible, "preparation expands only from its manual toggle");
        RenderResponsive(window, intervention, arm, "09-preparation-expanded-944x509.png", new Size(944, 509), output);
        prepToggle.IsChecked = false;
        debugToggle.IsChecked = true;
        Pump();
        Check(debugDetails.Visibility == Visibility.Visible, "arm debugging expands only from its manual toggle");
        RenderResponsive(window, intervention, arm, "10-arm-debug-expanded-944x509.png", new Size(944, 509), output);
        debugToggle.IsChecked = false;
        recording.PublishUiState();
        Pump();
        recording.Handle582Online = recording.Handle587Online = false;
        recording.SetConnected(false, false);
        Pump();
        Invoke(vm, "RefreshStats");
        Pump();
        Check(!((Button)intervention.FindName("Axis4Forward")!).IsEnabled && ((Button)window.FindName("ConnectButton")!).IsEnabled && ((Button)arm.FindName("CartesianForward")!).IsEnabled && ((Button)arm.FindName("ArmStop")!).IsEnabled,
            "disconnection preserves intervention lock and unrestricted Cartesian/stop buttons");
        Check(((TextBlock)window.FindName("SlaveStatusText")!).Text == "未连接", "slave capsule shows disconnected state");
        foreach (string name in new[] { "SlaveStatus", "AdsStatus", "WireHandleStatus", "CatheterHandleStatus" })
            Check(((SolidColorBrush)((Border)window.FindName(name)!).Background).Color == (Color)ColorConverter.ConvertFromString("#F1F4F7"), name + " uses its disconnected gray capsule");
        RenderResponsive(window, intervention, arm, "11-disconnected-944x509.png", new Size(944, 509), output);
        vm.PostLog("warn", "界面提示测试");
        Pump();
        var messages = ((Grid)((Border)window.FindName("HintRow")!).Child).Children.OfType<TextBlock>().ToArray();
        Check(messages[0].Text == vm.OperateHintText && messages[1].Visibility == Visibility.Collapsed && messages[2].Text == "界面提示测试" && messages[2].Visibility == Visibility.Visible,
            "empty hint entries disappear while nonempty operation/warning messages remain in order");
        window.Close();
    }
}
