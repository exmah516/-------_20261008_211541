using System.Collections.ObjectModel;
using System.Windows;
using Robotics.HostFramework.Contracts;

namespace VI.Control;

public sealed record ShellPanel(ModuleMetadata Metadata, FrameworkElement Content);

public sealed class ShellRegion : IShellRegion
{
    public ObservableCollection<ShellPanel> Panels { get; } = [];

    public void RegisterPanel(ModuleMetadata metadata, Func<FrameworkElement> factory)
    {
        ArgumentNullException.ThrowIfNull(metadata);
        ArgumentNullException.ThrowIfNull(factory);
        if (Panels.Any(panel => panel.Metadata.Id == metadata.Id))
            throw new InvalidOperationException("重复的面板 ID：" + metadata.Id);
        var panel = new ShellPanel(metadata, factory());
        int index = 0;
        while (index < Panels.Count && Panels[index].Metadata.Order <= metadata.Order) index++;
        Panels.Insert(index, panel);
    }
}
