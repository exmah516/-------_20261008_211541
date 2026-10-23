using MasterConsole.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Robotics.HostFramework.Contracts;

namespace VI.Module.PositioningArm;

[FeatureModule]
public sealed class PositioningArmModule : IModule
{
    public ModuleMetadata Metadata { get; } = new("positioning-arm", "定位臂控制", 20);

    public void Register(IShellRegion region, IServiceProvider services) =>
        region.RegisterPanel(Metadata, () => new PositioningArmPanel(services.GetRequiredService<MainViewModel>()));
}
