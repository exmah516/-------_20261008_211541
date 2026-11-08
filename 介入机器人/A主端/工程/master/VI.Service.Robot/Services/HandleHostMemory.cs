using System;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Threading;
using MasterConsole.Protocol;

namespace MasterConsole.Services
{
    // 两个单写区；读取只尝试一次，写入交叠时保留上一份样本，不等待宿主。
    internal sealed class HandleHostMemory : IDisposable
    {
        private readonly MemoryMappedFile _file;
        private readonly MemoryMappedViewAccessor _view;
        internal const int SampleOffset = 128, ForceOffset = 512;
        internal HandleHostMemory(string name, bool create)
        {
            _file = create ? MemoryMappedFile.CreateNew(name, 1024) : MemoryMappedFile.OpenExisting(name);
            _view = _file.CreateViewAccessor();
        }
        internal long Heartbeat { get => _view.ReadInt64(0); set => _view.Write(0, value); }
        internal int Error { get => _view.ReadInt32(8); set => _view.Write(8, value); }
        internal bool Stop { get => _view.ReadInt32(12) != 0; set => _view.Write(12, value ? 1 : 0); }
        internal void Write<T>(int offset, T value) where T : struct
        {
            long version = _view.ReadInt64(offset);
            _view.Write(offset, version + 1);
            Thread.MemoryBarrier();
            _view.Write(offset + 8, ref value);
            Thread.MemoryBarrier();
            _view.Write(offset, version + 2);
        }
        internal bool Read<T>(int offset, out T value) where T : struct
        {
            long version = _view.ReadInt64(offset);
            _view.Read(offset + 8, out value);
            Thread.MemoryBarrier();
            return (version & 1) == 0 && version == _view.ReadInt64(offset);
        }
        public void Dispose() { _view.Dispose(); _file.Dispose(); }
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct HostSample
    {
        internal long Sequence, Tick;
        internal int Valid, Buttons, Encoder0, Encoder1;
        internal float Joint0, Joint1, Vel0, Vel1;
        internal HandleSample Sample => new HandleSample { Valid = Valid != 0, Buttons = (byte)Buttons,
            Encoder0 = Encoder0, Encoder1 = Encoder1, Joint0 = Joint0, Joint1 = Joint1, Vel0 = Vel0, Vel1 = Vel1 };
    }
    [StructLayout(LayoutKind.Sequential)]
    internal struct HostSamples { internal HostSample A, B; }
    [StructLayout(LayoutKind.Sequential)]
    internal struct HostForce
    {
        internal long Tick;
        internal int Enable, Axis;
        internal double Force, Torque;
    }
    [StructLayout(LayoutKind.Sequential)]
    internal struct HostForces { internal HostForce A, B; }
}
