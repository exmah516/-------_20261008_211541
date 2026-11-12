using System;
using System.Diagnostics;
using System.Threading;
using System.Runtime.InteropServices;
using MasterConsole.Services;

internal static class Program
{
    internal const double DAMPING_B = 0.01; // N / getEncVel 第一通道原始单位。
    internal const int DAMPING_SIGN = 1;
    internal const double DAMPING_MAX_N = 0.5;
    internal const double FORCE_SLEW_N_PER_S = 8;
    internal const double FORCE_MAX_N = 1.2;
    internal const bool SHOW_FORCE_CONSOLE = false;
    [DllImport("winmm.dll")] private static extern uint timeBeginPeriod(uint period);
    [DllImport("winmm.dll")] private static extern uint timeEndPeriod(uint period);
    private static readonly FlCatheterNative.ServoCallback Servo = _ => FlCatheterNative.deviceStatus(0);
    internal sealed class Device
    {
        internal uint Serial;
        internal ulong DeviceEpoch;
        internal int Id = -1;
        internal long NextOpen, Sequence, LastZero;
        internal bool Nonzero, ForceActive, DampingClamped;
        internal double EnvironmentForce, DampingForce, OutputForce;
        internal readonly double[] Velocity = new double[2], Joints = new double[2];
        internal readonly int[] Encoders = new int[2];
    }
    private static int Main(string[] args)
    {
        using var memory = new HandleHostMemory(args[0], false);
        var devices = new[] { new Device { Serial = 582 }, new Device { Serial = 587 } };
        bool servoStarted = false;
        bool preciseTimer = timeBeginPeriod(1) == 0;
        try
        {
            long deadline = Stopwatch.GetTimestamp();
            long previousForceTimestamp = deadline, lastForcePrint = deadline;
            HostForces forces = default;
            while (!memory.Stop)
            {
                long now = Environment.TickCount64;
                long forceTimestamp = Stopwatch.GetTimestamp();
                double dt = (forceTimestamp - previousForceTimestamp) / (double)Stopwatch.Frequency;
                previousForceTimestamp = forceTimestamp;
                var samples = new HostSamples();
                if (memory.Read(HandleHostMemory.ForceOffset, out HostForces update)) forces = update;
                for (int i = 0; i < 2; ++i)
                {
                    Device d = devices[i];
                    if (d.Id < 0 && now >= d.NextOpen)
                    {
                        d.NextOpen = now + 1000;
                        d.Id = FlCatheterNative.openDevice(d.Serial);
                        if (d.Id >= 0)
                        {
                            d.DeviceEpoch = memory.NextDeviceEpoch(i);
                            if (!servoStarted) { FlCatheterNative.startServoLoop(Servo, IntPtr.Zero); servoStarted = true; }
                            FlCatheterNative.enableForces(true, d.Id);
                        }
                    }
                    HostSample sample = default;
                    if (d.Id >= 0)
                    {
                        // SDK 状态是缓存值；实机拔出时是否更新仍需现场核实。
                        bool valid = FlCatheterNative.deviceStatus(d.Id) >= 0;
                        if (valid)
                        {
                            FlCatheterNative.getEncVel(d.Velocity, d.Id);
                            FlCatheterNative.getJoints(d.Joints, d.Id);
                            FlCatheterNative.getEncoders(d.Encoders, d.Id);
                            FlCatheterNative.getSwitch(out byte buttons, d.Id);
                            sample = new HostSample { Valid = 1, DeviceEpoch = d.DeviceEpoch, Tick = now, Sequence = ++d.Sequence, Buttons = buttons,
                                Encoder0 = d.Encoders[0], Encoder1 = d.Encoders[1], Joint0 = (float)d.Joints[0], Joint1 = (float)d.Joints[1],
                                Vel0 = (float)d.Velocity[0], Vel1 = (float)d.Velocity[1] };
                        }
                        ApplyForce(d, i == 0 ? forces.A : forces.B, valid, now, dt);
                    }
                    if (i == 0) samples.A = sample; else samples.B = sample;
                }
                memory.Write(HandleHostMemory.SampleOffset, samples);
                memory.Heartbeat = Environment.TickCount64;
                if (SHOW_FORCE_CONSOLE && forceTimestamp - lastForcePrint >= Stopwatch.Frequency / 5)
                {
                    Console.WriteLine("   " + FormatForce(devices[0]) + "   " + FormatForce(devices[1]));
                    lastForcePrint = forceTimestamp;
                }
                deadline += Stopwatch.Frequency / 200;
                while (Stopwatch.GetTimestamp() < deadline)
                {
                    if (deadline - Stopwatch.GetTimestamp() > Stopwatch.Frequency / 500) Thread.Sleep(1);
                    else Thread.SpinWait(50);
                }
            }
            foreach (Device d in devices) if (d.Id >= 0) FlCatheterNative.sendForce(new double[3], 0, d.Id);
            if (servoStarted) { FlCatheterNative.stopServoLoop(); FlCatheterNative.closeDevice(); }
            return 0;
        }
        catch (DllNotFoundException) { memory.Error = 1; return 1; }
        catch (BadImageFormatException) { memory.Error = 2; return 2; }
        catch (EntryPointNotFoundException) { memory.Error = 3; return 3; }
        finally { if (preciseTimer) timeEndPeriod(1); }
    }
    internal static void UpdateForce(Device d, HostForce force, bool valid, long now, double dt)
    {
        d.ForceActive = valid && force.Enable != 0 && force.Tick > 0 && now >= force.Tick && now - force.Tick < 3000;
        if (!d.ForceActive)
        {
            d.EnvironmentForce = 0;
            return;
        }
        double damping = -DAMPING_SIGN * DAMPING_B * d.Velocity[0];
        d.DampingForce = Math.Clamp(damping, -DAMPING_MAX_N, DAMPING_MAX_N);
        d.DampingClamped = damping != d.DampingForce;
        double step = FORCE_SLEW_N_PER_S * dt;
        d.EnvironmentForce += Math.Clamp(force.Force - d.EnvironmentForce, -step, step);
        d.OutputForce = Math.Clamp(d.EnvironmentForce + d.DampingForce, -FORCE_MAX_N, FORCE_MAX_N);
    }
    internal static string FormatForce(Device d)
        => d.ForceActive
            ? FormattableString.Invariant($"[{d.Serial}] 速度原始值={d.Velocity[0]:+0.0000;-0.0000;+0.0000}  阻尼力={d.DampingForce:+0.000;-0.000;+0.000} N  环境力={d.EnvironmentForce:+0.000;-0.000;+0.000} N  输出={d.OutputForce:+0.000;-0.000;+0.000} N  阻尼限幅={(d.DampingClamped ? "是" : "否")}")
            : $"[{d.Serial}] 力反馈未生效";

    private static void ApplyForce(Device d, HostForce force, bool valid, long now, double dt)
    {
        UpdateForce(d, force, valid, now, dt);
        if (d.ForceActive)
        {
            var vector = new double[3];
            vector[force.Axis] = d.OutputForce;
            FlCatheterNative.sendForce(vector, force.Torque, d.Id);
            d.Nonzero = true;
        }
        else if (d.Nonzero || now - d.LastZero >= 100)
        {
            FlCatheterNative.sendForce(new double[3], 0, d.Id);
            d.Nonzero = false;
            d.LastZero = now;
        }
    }
}
