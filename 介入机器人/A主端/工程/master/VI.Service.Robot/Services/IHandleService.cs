using System;
using MasterConsole.Protocol;

namespace MasterConsole.Services
{
    public interface IHandleService : IDisposable
    {
        bool SdkMissing { get; }
        bool IsOnline(int slot);
        HandleSample GetSample(int slot);
        void SetHaptic(int slot, HapticOut output);
        void ClearHaptic();
        void Start();
        void Stop();
    }
}
