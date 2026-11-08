using System;
using System.Diagnostics;
using System.Threading;
using System.Runtime.InteropServices;
using MasterConsole.Services;

internal static class Program
{
    [DllImport("winmm.dll")] private static extern uint timeBeginPeriod(uint period);
    [DllImport("winmm.dll")] private static extern uint timeEndPeriod(uint period);
    private static readonly FlCatheterNative.ServoCallback Servo = _ => FlCatheterNative.deviceStatus(0);
    private sealed class Device
    {
        internal uint Serial;
        internal int Id = -1;
        internal long NextOpen, Sequence, LastZero;
        internal bool Nonzero;
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
            HostForces forces = default;
            while (!memory.Stop)
            {
                long now = Environment.TickCount64;
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
                            sample = new HostSample { Valid = 1, Tick = now, Sequence = ++d.Sequence, Buttons = buttons,
                                Encoder0 = d.Encoders[0], Encoder1 = d.Encoders[1], Joint0 = (float)d.Joints[0], Joint1 = (float)d.Joints[1],
                                Vel0 = (float)d.Velocity[0], Vel1 = (float)d.Velocity[1] };
                        }
                        ApplyForce(d, i == 0 ? forces.A : forces.B, valid, now);
                    }
                    if (i == 0) samples.A = sample; else samples.B = sample;
                }
                memory.Write(HandleHostMemory.SampleOffset, samples);
                memory.Heartbeat = Environment.TickCount64;
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
    private static void ApplyForce(Device d, HostForce force, bool valid, long now)
    {
        if (valid && force.Enable != 0 && force.Tick > 0 && now >= force.Tick && now - force.Tick < 3000)
        {
            var vector = new double[3];
            vector[force.Axis] = force.Force;
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
