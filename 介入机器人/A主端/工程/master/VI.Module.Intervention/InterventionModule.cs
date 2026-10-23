using MasterConsole.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Robotics.HostFramework.Contracts;

namespace VI.Module.Intervention;

[FeatureModule]
public sealed class InterventionModule : IModule
{
    public ModuleMetadata Metadata { get; } = new("intervention", "介入控制", 10);

    public void Register(IShellRegion region, IServiceProvider services) =>
        region.RegisterPanel(Metadata, () => new InterventionPanel(services.GetRequiredService<MainViewModel>()));
}
