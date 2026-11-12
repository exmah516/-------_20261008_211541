using System;
using MasterConsole.Services;
using HostProgram = global::Program;

namespace MasterConsole.Protocol.Tests
{
    internal static partial class Program
    {
        private static void LocalForceTests()
        {
            var d = new HostProgram.Device { Serial = 582 };
            var force = new HostForce { Enable = 1, Tick = 100, Force = .6 };
            bool Near(double a, double b) => Math.Abs(a - b) < 1e-12;
            void Tick(double dt = .005) => HostProgram.UpdateForce(d, force, true, 100, dt);
            d.Velocity[0] = 20;
            Tick(.1);
            Check("本地正速度减力", Near(d.DampingForce, -.2) && Near(d.OutputForce, .4));
            d.Velocity[0] = -20; Tick();
            Check("本地负速度增力", Near(d.DampingForce, .2) && Near(d.OutputForce, .8));
            foreach (double velocity in new[] { 50.0, -50.0, 80.0, -80.0 })
            {
                d.Velocity[0] = velocity; Tick();
                Check("阻尼限幅及标记 " + velocity, Near(Math.Abs(d.DampingForce), .5)
                    && d.DampingClamped == (Math.Abs(velocity) > 50));
            }
            force.Force = .2; d.Velocity[0] = 80; Tick(.1);
            Check("打印正速度和截断标记", HostProgram.FormatForce(d) ==
                "[582] 速度原始值=+80.0000  阻尼力=-0.500 N  环境力=+0.200 N  输出=-0.300 N  阻尼限幅=是");
            d.Serial = 587; d.Velocity[0] = -20; Tick();
            Check("打印负速度和未截断标记", HostProgram.FormatForce(d) ==
                "[587] 速度原始值=-20.0000  阻尼力=+0.200 N  环境力=+0.200 N  输出=+0.400 N  阻尼限幅=否");
            d.EnvironmentForce = 0; d.Velocity[0] = 0;
            force.Force = 1.2;
            double elapsed = 0;
            bool slewOk = true;
            // 不等间隔阶跃及反向阶跃，检查实际 dt 对每拍变化的约束。
            foreach (double target in new[] { 1.2, -1.2 })
            {
                force.Force = target;
                foreach (double dt in new[] { .002, .008, .013, .027, .1 })
                {
                    double before = d.EnvironmentForce;
                    Tick(dt); elapsed += dt;
                    slewOk &= Math.Abs(d.EnvironmentForce - before) <= 8 * dt + 1e-12;
                }
            }
            Check("变化率按实际间隔限于8N每秒", slewOk);
            Check("0到1.2N约150ms且不越过目标", Near(elapsed, .3) && Near(d.EnvironmentForce, 0));
            foreach (double target in new[] { 10.0, -10.0 })
            {
                force.Force = target; d.Velocity[0] = -target * 10; Tick(2);
                Check("最终输出限幅 " + target, Near(d.OutputForce, Math.Sign(target) * 1.2));
            }
            foreach (var gate in new[] { (0, 100L, true, 100L), (1, 0L, true, 100L),
                (1, 101L, true, 100L), (1, 100L, true, 3100L), (1, 100L, false, 100L) })
            {
                force.Enable = gate.Item1; force.Tick = gate.Item2;
                HostProgram.UpdateForce(d, force, gate.Item3, gate.Item4, .005);
                Check("无效输出复位环境力且打印未生效", !d.ForceActive && d.EnvironmentForce == 0
                    && HostProgram.FormatForce(d) == "[587] 力反馈未生效");
            }
            force.Enable = 1; force.Tick = 100; force.Force = 1; d.Velocity[0] = 0;
            HostProgram.UpdateForce(d, force, true, 3099, .005);
            Check("3秒内重新有效从零起坡", d.ForceActive && Near(d.EnvironmentForce, .04));
        }
    }
}
