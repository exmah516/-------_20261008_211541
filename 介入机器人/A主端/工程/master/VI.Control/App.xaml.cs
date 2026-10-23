using System.Windows;
using MasterConsole.Services;
using MasterConsole.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Robotics.HostFramework;
using Robotics.HostFramework.Contracts;

namespace VI.Control;

public partial class App : HostApplicationBase
{
    protected override HostApplicationOptions CreateOptions() => new()
    {
        ModuleDirectories =
        [
            AppContext.BaseDirectory,
            System.IO.Path.Combine(AppContext.BaseDirectory, "Modules", "Platform"),
            System.IO.Path.Combine(AppContext.BaseDirectory, "Modules", "Features")
        ],
        RequiredModuleIds = ["intervention", "positioning-arm"],
        SingleInstanceName = "VI.Control.Master",
        StopTimeout = TimeSpan.FromSeconds(8)
    };

    protected override void ConfigureServices(IServiceCollection services)
    {
        ConfigureRobotServices(services, StartupSelection.Parse(Environment.GetCommandLineArgs()[1..], AppContext.BaseDirectory));
    }

    public static void ConfigureRobotServices(IServiceCollection services, StartupSelection selection)
    {
        services.AddSingleton(selection);
        if (selection.IsSimulation)
            services.AddSingleton<IRobotLink, SimulatedRobotLink>();
        else
        {
            services.AddSingleton<IHandleService, HandleService>();
            services.AddSingleton<IRobotLink>(provider => new RemoteRobotLink(selection.Settings, provider.GetRequiredService<IHandleService>()));
        }
        services.AddSingleton<MainViewModel>();
        services.AddManagedHostedService<RobotRuntime>();
    }

    protected override IShellRegion CreateShellRegion(IServiceProvider services) => new ShellRegion();

    protected override Window CreateMainWindow(IServiceProvider services, IShellRegion region) =>
        new MainWindow((ShellRegion)region, services.GetRequiredService<MainViewModel>(), services.GetRequiredService<StartupSelection>());
}

internal sealed class RobotRuntime(IRobotLink link, IServiceProvider services) : IHostedService
{
    public Task StartAsync(CancellationToken cancellationToken)
    {
        services.GetService<IHandleService>()?.Start();
        return Task.CompletedTask;
    }

    public Task StopAsync(CancellationToken cancellationToken)
    {
        link.Disconnect();
        services.GetService<IHandleService>()?.Stop();
        return Task.CompletedTask;
    }
}
