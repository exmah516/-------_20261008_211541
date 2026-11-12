using Microsoft.Win32.SafeHandles;
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace VI.Imaging
{
    internal static class ProcessJob
    {
        internal static Accounting State(SafeFileHandle job)
        {
            if (!QueryInformationJobObject(job, 1, out var state, Marshal.SizeOf<Accounting>(), IntPtr.Zero)) throw new Win32Exception();
            return state;
        }

        internal static SafeFileHandle CreateKillOnCloseJob(string name = null)
        {
            var job = CreateJobObject(IntPtr.Zero, name);
            try
            {
                if (job.IsInvalid) throw new Win32Exception();
                var limits = new ExtendedLimits { Basic = new BasicLimits { Flags = 0x2000 } };
                if (!SetInformationJobObject(job, 9, ref limits, Marshal.SizeOf<ExtendedLimits>())) throw new Win32Exception();
                return job;
            }
            catch { job.Dispose(); throw; }
        }

        internal static void Attach(SafeFileHandle job, Process process)
        {
            if (AssignProcessToJobObject(job, process.Handle)) return;
            var error = new Win32Exception();
            if (!process.HasExited) process.Kill();
            throw error;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct BasicLimits
        {
            public long ProcessTime, JobTime;
            public uint Flags;
            public UIntPtr MinWorkingSet, MaxWorkingSet;
            public uint ActiveProcessLimit;
            public UIntPtr Affinity;
            public uint PriorityClass, SchedulingClass;
        }
        [StructLayout(LayoutKind.Sequential)]
        private struct ExtendedLimits
        {
            public BasicLimits Basic;
            public ulong ReadOps, WriteOps, OtherOps, ReadBytes, WriteBytes, OtherBytes;
            public UIntPtr ProcessMemory, JobMemory, PeakProcessMemory, PeakJobMemory;
        }
        [StructLayout(LayoutKind.Sequential)]
        internal struct Accounting
        {
            public long UserTime, KernelTime, PeriodUserTime, PeriodKernelTime;
            public uint PageFaults, Total, Active, Terminated;
        }
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern SafeFileHandle CreateJobObject(IntPtr attributes, string name);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool SetInformationJobObject(SafeFileHandle job, int kind, ref ExtendedLimits limits, int size);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool QueryInformationJobObject(SafeFileHandle job, int kind, out Accounting state, int size, IntPtr returnLength);
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool AssignProcessToJobObject(SafeFileHandle job, IntPtr process);
    }
}
