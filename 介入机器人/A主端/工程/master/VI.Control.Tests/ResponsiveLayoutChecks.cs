using System.Globalization;
using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Media;
using System.Xml.Linq;
using VI.Control;

internal static partial class Program
{
    private static readonly List<object> ResponsiveGeometry = [];

    private static void CheckV3Mapping(string fixtures, XDocument[] currentViews)
    {
        string[] files = ["MainWindow.xaml", "InterventionPanel.xaml", "PositioningArmPanel.xaml"];
        string[] behavior = ["Command", "Click", "PreviewMouseLeftButtonDown", "PreviewMouseLeftButtonUp", "LostMouseCapture", "Checked", "Tag", "CommandParameter", "IsChecked", "GroupName", "Text"];
        for (int i = 0; i < files.Length; i++)
        {
            var before = XDocument.Load(Path.Combine(fixtures, "V3Baseline." + files[i] + ".xml"));
            foreach (var original in before.Descendants().Where(element => element.Name.LocalName is "Button" or "ToggleButton" or "RadioButton" or "TextBox"))
            {
                string? name = (string?)original.Attribute(X + "Name");
                var current = name is not null
                    ? currentViews[i].Descendants().Single(element => (string?)element.Attribute(X + "Name") == name)
                    : currentViews[i].Descendants().Single(element => element.Name == original.Name && behavior.All(key => (string?)element.Attribute(key) == (string?)original.Attribute(key)));
                string label = name ?? (string?)original.Attribute("Content") ?? original.Name.LocalName;
                Check(behavior.All(key => (string?)current.Attribute(key) == (string?)original.Attribute(key)) && EnabledPaths(current) == EnabledPaths(original),
                    "v3 retains every command, event, parameter, binding and enabled path: " + label);
                bool cylinder = name?.StartsWith("Cylinder", StringComparison.Ordinal) == true;
                Check(ToolTip(current) == (cylinder ? "{Binding Title, Mode=OneWay} |  ·  | " : "") + ToolTip(original),
                    "v3 retains tooltip content and adds only the requested cylinder full name: " + label);
                if (cylinder)
                    Check((string?)current.Attribute("Content") == (name is "Cylinder1" or "Cylinder3" ? "夹持" : "旋捻"), "cylinder display uses the requested short caption: " + name);
                else
                    Check((string?)current.Attribute("Content") == (string?)original.Attribute("Content"), "v3 retains button caption/binding: " + label);
            }
        }

        string[] keys = ["FontSizeBody", "FontSizeSmall", "FontSizeTitle", "FontSizeReadout", "ButtonHeight", "HoldMinHeight", "HoldMaxHeight", "Gap", "Pad", "DialSize", "ArmMinWidth", "ArmMaxWidth"];
        double[][] expected = [[13, 12, 14, 20, 26, 30, 64, 6, 8, 26, 236, 300], [15, 13, 16, 26, 32, 38, 72, 8, 10, 32, 300, 380], [17, 15, 19, 32, 38, 46, 84, 10, 12, 38, 360, 460]];
        foreach (var (tier, index) in new[] { ("Small", 0), ("Medium", 1), ("Large", 2) })
        {
            var dictionary = XDocument.Load(Path.Combine(fixtures, "Sizes" + tier + ".xaml"));
            for (int key = 0; key < keys.Length; key++)
                Check(double.Parse(dictionary.Root!.Elements().Single(element => (string?)element.Attribute(X + "Key") == keys[key]).Value, CultureInfo.InvariantCulture) == expected[index][key], tier + " resource matches preview CSS: " + keys[key]);
        }
        Check(!currentViews[0].Descendants(Ui + "Viewbox").Any() && !currentViews[0].Descendants().Any(element => element.Name.LocalName.EndsWith("Transform", StringComparison.Ordinal)),
            "shell has no Viewbox or whole-window scaling transform");
        var readoutBoxes = currentViews[1].Descendants(Ui + "Viewbox").Where(element => element.Descendants(Ui + "TextBlock").Any()).ToArray();
        Check(readoutBoxes.Length == 4 && readoutBoxes.All(element => (string?)element.Attribute("StretchDirection") == "UpOnly" && (string?)element.Attribute("MaxHeight") == "96"),
            "only four readout Viewboxes enlarge values and never shrink their baseline text");
    }

    private static void ArrangeResponsive(MainWindow window, Size size)
    {
        var content = (FrameworkElement)window.Content;
        for (int pass = 0; pass < 3; pass++)
        {
            content.Measure(size);
            content.Arrange(new Rect(new Point(), size));
            content.UpdateLayout();
            Pump();
        }
    }

    private static void CheckResponsiveLayout(MainWindow window, UserControl intervention, UserControl arm, string output)
    {
        foreach (var (width, height, tier, layout) in new[]
        {
            (944, 509, "Small", "Wide"), (806, 470, "Small", "Narrow"), (944, 1010, "Small", "Wide"),
            (1280, 760, "Medium", "Wide"), (1920, 1010, "Large", "Wide"), (1499, 1010, "Medium", "Wide"),
            (701, 415, "Small", "Narrow"), (560, 600, "Small", "Narrow"), (819, 700, "Small", "Narrow"), (820, 700, "Small", "Wide"),
            (1150, 639, "Small", "Wide"), (1150, 640, "Medium", "Wide"), (1500, 859, "Medium", "Wide"), (1500, 860, "Large", "Wide")
        })
        {
            ArrangeResponsive(window, new Size(width, height));
            Check((string)window.FindResource("SizeTier") == tier && (string)window.Tag == layout, $"content {width}×{height} selects {tier}/{layout}");
            Check(window.Resources.MergedDictionaries.Count == 1, "tier selection replaces the size dictionary without accumulating dictionaries");
        }

        RenderResponsive(window, intervention, arm, "01-layout-701x415.png", new Size(701, 415), output);
        var scroll = (ScrollViewer)window.FindName("BodyScroll")!;
        double fullHeight = scroll.TransformToAncestor((Visual)window.Content).Transform(new Point()).Y + scroll.ExtentHeight + (double)window.FindResource("Pad");
        var status = (FrameworkElement)window.FindName("StatusItems")!;
        Point statusBefore = status.TransformToAncestor((Visual)window.Content).Transform(new Point());
        scroll.ScrollToBottom();
        Pump();
        Check(scroll.VerticalOffset > 0 && status.TransformToAncestor((Visual)window.Content).Transform(new Point()) == statusBefore,
            "narrow content scrolls to the arm while the top status row stays fixed");
        var debug = (ToggleButton)arm.FindName("ArmDebugToggle")!;
        Rect debugAtBottom = debug.TransformToAncestor((Visual)window.Content).TransformBounds(new Rect(debug.RenderSize));
        Check(debugAtBottom.Bottom <= 415 && debugAtBottom.Top >= 0, "vertical scrolling reaches the bottom arm controls");
        scroll.ScrollToTop();
        Pump();
        RenderResponsive(window, intervention, arm, "02-layout-701-full.png", new Size(701, Math.Ceiling(fullHeight)), output);
        RenderResponsive(window, intervention, arm, "03-layout-560x600.png", new Size(560, 600), output);
        RenderResponsive(window, intervention, arm, "04-layout-944x509.png", new Size(944, 509), output);
        Check(scroll.ComputedVerticalScrollBarVisibility == Visibility.Collapsed && scroll.ScrollableHeight < 0.1,
            "944×509 normal layout has no body scrollbar");
        RenderResponsive(window, intervention, arm, "05-layout-806x470.png", new Size(806, 470), output);
        RenderResponsive(window, intervention, arm, "06-layout-944x1010.png", new Size(944, 1010), output);
        RenderResponsive(window, intervention, arm, "07-layout-1280x760.png", new Size(1280, 760), output);
        RenderResponsive(window, intervention, arm, "08-layout-1920x1010.png", new Size(1920, 1010), output);

        foreach (int width in new[] { 520, 819, 820 })
            RenderResponsive(window, intervention, arm, "boundary-" + width + ".png", new Size(width, 700), output);
        ArrangeResponsive(window, new Size(944, 509));
        foreach (double dpi in new[] { 1.25, 1.5, 2.0 })
        {
            string file = Path.Combine(output, "dpi-" + dpi.ToString(CultureInfo.InvariantCulture) + ".png");
            SaveScreenshot(window, new Size(944, 509), dpi, file);
            Check((string)window.FindResource("SizeTier") == "Small" && ((TextBlock)intervention.FindName("Force582FValue")!).FontSize == 20,
                $"DPI {dpi} keeps DIP tier and readout baseline unchanged");
        }
    }

    private static void RenderResponsive(MainWindow window, UserControl intervention, UserControl arm, string file, Size size, string output)
    {
        ArrangeResponsive(window, size);
        SaveScreenshot(window, size, 1, Path.Combine(output, file));
        var content = (FrameworkElement)window.Content;
        var scroll = (ScrollViewer)window.FindName("BodyScroll")!;
        var body = (FrameworkElement)window.FindName("BodyLayout")!;
        var viewport = scroll.TransformToAncestor(content).TransformBounds(new Rect(scroll.RenderSize));
        double pad = (double)window.FindResource("Pad");
        string label = file + ": ";
        Check(scroll.HorizontalScrollBarVisibility == ScrollBarVisibility.Disabled && scroll.ComputedHorizontalScrollBarVisibility != Visibility.Visible && scroll.ScrollableWidth < 0.1,
            label + "no horizontal scrollbar or hidden horizontal scroll range");
        Check(Math.Abs(size.Width - viewport.Right - pad) < 0.1 && Math.Abs(size.Height - viewport.Bottom - pad) < 0.1,
            label + "body viewport reaches the right/bottom outer Pad");
        Check(Math.Abs(body.ActualWidth - viewport.Width) < 0.1 && body.ActualHeight >= viewport.Height - 0.1,
            label + "body fills the available viewport");

        var elements = Visuals(content).OfType<FrameworkElement>().Where(IsRendered).ToArray();
        foreach (var element in elements)
        {
            Rect bounds = element.TransformToAncestor(content).TransformBounds(new Rect(element.RenderSize));
            Check(bounds.Left >= -0.2 && bounds.Right <= size.Width + 0.2,
                label + "horizontal bounds contain " + (element.Name.Length == 0 ? element.GetType().Name : element.Name));
            if (file == "04-layout-944x509.png")
                Check(bounds.Top >= -0.2 && bounds.Bottom <= size.Height + 0.2, label + "all controls are vertically visible: " + element.Name);
            if ((string)window.FindResource("SizeTier") == "Small")
                Check(element switch { TextBlock text => text.FontSize >= 12, Control control => control.FontSize >= 12, _ => true }, label + "small text remains at least 12 DIP: " + element.Name);
        }
        bool narrow = (string)window.Tag == "Narrow";
        var armBounds = arm.TransformToAncestor(content).TransformBounds(new Rect(arm.RenderSize));
        var interventionBounds = intervention.TransformToAncestor(content).TransformBounds(new Rect(intervention.RenderSize));
        Check(narrow ? Math.Abs(armBounds.Width - interventionBounds.Width) < 0.1 && armBounds.Top >= interventionBounds.Bottom
            : armBounds.Left >= interventionBounds.Right && armBounds.Top == interventionBounds.Top,
            label + "module placement matches wide/narrow layout");
        if (!narrow)
            Check(arm.ActualWidth >= (double)window.FindResource("ArmMinWidth") - 0.1 && arm.ActualWidth <= (double)window.FindResource("ArmMaxWidth") + 0.1,
                label + "arm width respects the active resource limits");
        foreach (string name in new[] { "Axis4Forward", "Injector1Push", "Injector2Pull" })
        {
            var button = (Button)intervention.FindName(name)!;
            Check(button.ActualHeight >= (double)window.FindResource("HoldMinHeight") - 0.1 && button.ActualHeight <= (double)window.FindResource("HoldMaxHeight") + 0.1,
                label + name + " stretches within the active hold-height limits");
            var rows = (Grid)button.Parent;
            int row = Grid.GetRow(button);
            double available = rows.RowDefinitions[row].ActualHeight - button.Margin.Top - button.Margin.Bottom;
            double expected = Math.Clamp(available, button.MinHeight, button.MaxHeight);
            Check(Math.Abs(button.ActualHeight - expected) < 0.5,
                label + name + " fills the row until the hold-height maximum");
            double rowTop = rows.RowDefinitions.Take(row).Sum(definition => definition.ActualHeight);
            var bounds = button.TransformToAncestor(rows).TransformBounds(new Rect(button.RenderSize));
            Check(Math.Abs(bounds.Top + bounds.Height / 2 - rowTop - (rows.RowDefinitions[row].ActualHeight + button.Margin.Top - button.Margin.Bottom) / 2) < 0.6,
                label + name + " centers when the row exceeds its maximum height");
        }
        for (int mode = 1; mode <= 5; mode++)
        {
            var button = (Button)arm.FindName(mode == 1 ? "CartesianForward" : $"Cartesian{mode}Forward")!;
            Check(button.ActualHeight >= (double)window.FindResource("HoldMinHeight") - 0.1 && button.ActualHeight <= (double)window.FindResource("HoldMaxHeight") + 0.1,
                label + "Cartesian mode " + mode + " respects hold-height limits");
            if (narrow)
                Check(Math.Abs(button.ActualHeight - (double)window.FindResource("HoldMinHeight")) < 0.1,
                    label + "narrow Cartesian mode " + mode + " stays at minimum row height");
        }
        var jogRows = (Grid)arm.FindName("CartesianJogRows")!;
        var home = (Button)arm.FindName("ProgramHome")!;
        var debug = (ToggleButton)arm.FindName("ArmDebugToggle")!;
        Rect jogBounds = jogRows.TransformToAncestor(arm).TransformBounds(new Rect(jogRows.RenderSize));
        Rect homeBounds = home.TransformToAncestor(arm).TransformBounds(new Rect(home.RenderSize));
        Rect debugBounds = debug.TransformToAncestor(arm).TransformBounds(new Rect(debug.RenderSize));
        Check(Grid.GetRow(jogRows) == 3 && jogBounds.Bottom <= homeBounds.Top + 0.2 && homeBounds.Bottom <= debugBounds.Top + 0.2,
            label + "arm order remains heading, five jog rows, home/stop, then debugging");
        double[] jogHeights = Enumerable.Range(0, 5).Select(index => jogRows.RowDefinitions[index * 2].ActualHeight).ToArray();
        Check(jogHeights.Max() - jogHeights.Min() < 0.2,
            label + "five arm rows divide surplus height equally");
        var auxiliaryRows = (Grid)((Button)intervention.FindName("Axis4Forward")!).Parent;
        double[] auxiliaryHeights = Enumerable.Range(0, 3).Select(index => auxiliaryRows.RowDefinitions[index * 2].ActualHeight).ToArray();
        Check(auxiliaryHeights.Max() - auxiliaryHeights.Min() < 0.2,
            label + "three auxiliary rows divide surplus height equally");
        foreach (var box in Visuals(intervention).OfType<Viewbox>().Where(box => box.Child is StackPanel))
        {
            Rect bounds = box.Child.TransformToAncestor(box).TransformBounds(new Rect(box.Child.RenderSize));
            Check(bounds.Width >= box.Child.RenderSize.Width - 0.1 && bounds.Height >= box.Child.RenderSize.Height - 0.1,
                label + "readout text never scales below its baseline");
            var readoutCard = VisualTreeHelper.GetParent(VisualTreeHelper.GetParent(box)) as Border;
            Rect lineBounds = box.Child.TransformToAncestor(readoutCard!).TransformBounds(new Rect(box.Child.RenderSize));
            Check(lineBounds.Left >= -0.1 && lineBounds.Right <= readoutCard!.ActualWidth + 0.1,
                label + "numeric value and unit remain inside their own readout card");
        }
        ResponsiveGeometry.Add(new
        {
            file, width = size.Width, height = size.Height, tier = window.FindResource("SizeTier"), layout = window.Tag,
            pad, viewport = new { viewport.X, viewport.Y, viewport.Width, viewport.Height },
            extentWidth = scroll.ExtentWidth, extentHeight = scroll.ExtentHeight, scrollableWidth = scroll.ScrollableWidth, scrollableHeight = scroll.ScrollableHeight,
            horizontalScrollbar = scroll.ComputedHorizontalScrollBarVisibility.ToString(), verticalScrollbar = scroll.ComputedVerticalScrollBarVisibility.ToString(),
            armWidth = arm.ActualWidth, controls = elements.Length
        });
        File.WriteAllText(Path.Combine(output, "responsive_geometry.json"), JsonSerializer.Serialize(ResponsiveGeometry, new JsonSerializerOptions { WriteIndented = true }));
    }
}
