using System;
using System.Runtime.InteropServices;

namespace MasterConsole.Services
{
    internal sealed class HandleDeviceNotifications : IDisposable
    {
        // 与随仓库 FLCatheter.dll 内的 CyAPI 接口类一致。
        internal static readonly Guid InterfaceClass = new Guid("AE18AA60-7F6A-11D4-97DD-00010229B959");
        private readonly Callback _callback;
        private IntPtr _registration;
        [StructLayout(LayoutKind.Explicit, Size = 416)]
        private struct Filter
        {
            [FieldOffset(0)] internal int Size;
            [FieldOffset(8)] internal int Type;
            [FieldOffset(16)] internal Guid Class;
        }
        [UnmanagedFunctionPointer(CallingConvention.Winapi)]
        private delegate uint Callback(IntPtr notification, IntPtr context, int action, IntPtr data, uint size);
        [DllImport("cfgmgr32.dll")] private static extern uint CM_Register_Notification(ref Filter filter, IntPtr context, Callback callback, out IntPtr registration);
        [DllImport("cfgmgr32.dll")] private static extern uint CM_Unregister_Notification(IntPtr registration);
        internal HandleDeviceNotifications(Action<bool> changed)
        {
            _callback = (_, __, action, data, size) =>
            {
                if (action == 0 || action == 1) changed(action == 0);
                return 0;
            };
            var filter = new Filter { Size = Marshal.SizeOf<Filter>(), Type = 0, Class = InterfaceClass };
            uint result = CM_Register_Notification(ref filter, IntPtr.Zero, _callback, out _registration);
            if (result != 0) throw new InvalidOperationException("手柄插拔通知注册失败：" + result);
        }
        public void Dispose()
        {
            if (_registration != IntPtr.Zero) CM_Unregister_Notification(_registration);
            _registration = IntPtr.Zero;
            GC.KeepAlive(_callback);
        }
    }
}
