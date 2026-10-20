using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using MasterConsole.Protocol;

namespace VI.Service.Net
{
    public sealed class ZenohSession : IDisposable
    {
        private const int ConfigSize = 2064;
        private const int SessionSize = 8;
        private const int KeyExprSize = 32;
        private const int BytesSize = 40;
        private const int ClosureSize = 24;
        private const int SubscriberOptionsSize = 4;
        private const int PublisherSize = 120;
        private const int PublisherOptionsSize = 32;
        private readonly object _publishLock = new object();
        private readonly Dictionary<string, IntPtr> _keys = new Dictionary<string, IntPtr>(StringComparer.Ordinal);
        private readonly List<Subscription> _subscriptions = new List<Subscription>();
        private readonly List<IDisposable> _publishers = new List<IDisposable>();
        private IntPtr _session;
        private IntPtr _loanedSession;
        private volatile bool _disposing;

        public bool IsAvailable { get { lock (_publishLock) return AvailableLocked; } }
        private bool AvailableLocked => !_disposing && _loanedSession != IntPtr.Zero && !Native.z_session_is_closed(_loanedSession);

        public ZenohSession(string endpoint)
        {
            if (IntPtr.Size != 8) throw new PlatformNotSupportedException("Zenoh-C 运行库要求 x64 进程。");
            if (string.IsNullOrWhiteSpace(endpoint) || !endpoint.StartsWith("tcp/", StringComparison.Ordinal) ||
                endpoint.IndexOfAny(new[] { '\'', '"', '\\', '\r', '\n' }) >= 0)
                throw new ArgumentException("需要 tcp/主机:端口 格式的地址。", nameof(endpoint));
            _session = Marshal.AllocHGlobal(SessionSize);
            Zero(_session, SessionSize);
            IntPtr config = Marshal.AllocHGlobal(ConfigSize);
            bool configOwned = false, sessionOwned = false;
            try
            {
                Zero(config, ConfigSize);
                string json = "{mode:'client',connect:{timeout_ms:3000,exit_on_failure:true,endpoints:['" + endpoint + "']}}";
                IntPtr jsonPtr = Utf8(json);
                try { Ensure(Native.zc_config_from_str(config, jsonPtr), "create config"); configOwned = true; }
                finally { Marshal.FreeHGlobal(jsonPtr); }
                configOwned = false; // z_open 消费配置，失败返回时同样不能再次释放。
                Ensure(Native.z_open(_session, config, IntPtr.Zero), "open session");
                sessionOwned = true;
                _loanedSession = Native.z_session_loan(_session);
                if (_loanedSession == IntPtr.Zero) throw new InvalidOperationException("Zenoh returned an invalid session.");
            }
            catch
            {
                if (configOwned) Native.z_config_drop(config);
                if (sessionOwned) Native.z_session_drop(_session);
                Marshal.FreeHGlobal(_session);
                _session = _loanedSession = IntPtr.Zero;
                throw;
            }
            finally { Marshal.FreeHGlobal(config); }
        }

        public Publisher<T> Advertise<T>(string key)
        {
            lock (_publishLock)
            {
            if (!AvailableLocked) throw new ObjectDisposedException(nameof(ZenohSession));
            IntPtr publisher = Marshal.AllocHGlobal(PublisherSize);
            IntPtr options = Marshal.AllocHGlobal(PublisherOptionsSize);
            Zero(publisher, PublisherSize);
            Zero(options, PublisherOptionsSize);
            try
            {
                Native.z_publisher_options_default(options);
                Marshal.WriteInt32(options, 8, 1); // 拥塞时丢弃本次样本，调用方不缓存或补发。
                Marshal.WriteByte(options, 16, 1); // 高频控制直接发送，避免批处理等待。
                Ensure(Native.z_declare_publisher(_loanedSession, publisher, GetKey(key), options), "advertise " + key);
                var result = new Publisher<T>(this, publisher, Native.z_publisher_loan(publisher));
                _publishers.Add(result);
                return result;
            }
            catch
            {
                Marshal.FreeHGlobal(publisher);
                throw;
            }
            finally { Marshal.FreeHGlobal(options); }
            }
        }

        public void Subscribe<T>(string key, Action<T> callback)
        {
            if (callback == null) throw new ArgumentNullException(nameof(callback));
            lock (_publishLock)
            {
            if (!AvailableLocked) throw new ObjectDisposedException(nameof(ZenohSession));
            IntPtr closure = Marshal.AllocHGlobal(ClosureSize);
            IntPtr options = Marshal.AllocHGlobal(SubscriberOptionsSize);
            Zero(closure, ClosureSize);
            Zero(options, SubscriberOptionsSize);
            var subscription = new Subscription<T>(this, callback);
            try
            {
                Native.z_subscriber_options_default(options);
                Native.z_closure_sample(closure, subscription.NativeCallback, IntPtr.Zero, IntPtr.Zero);
                Ensure(Native.z_declare_background_subscriber(_loanedSession, GetKey(key), closure, options), "subscribe " + key);
                _subscriptions.Add(subscription);
            }
            finally
            {
                Marshal.FreeHGlobal(closure);
                Marshal.FreeHGlobal(options);
            }
            }
        }

        public static byte[] Serialize<T>(T value) => ZenohWire.Serialize(value);
        public static T Deserialize<T>(byte[] bytes) => ZenohWire.Deserialize<T>(bytes);

        private void Publish<T>(Publisher<T> publisher, byte[] payload)
        {
            lock (_publishLock)
            {
                if (!AvailableLocked || publisher._publisher == IntPtr.Zero) throw new ObjectDisposedException(nameof(ZenohSession));
                IntPtr bytes = Marshal.AllocHGlobal(BytesSize);
                Zero(bytes, BytesSize);
                try
                {
                    Ensure(Native.z_bytes_copy_from_buf(bytes, payload, new UIntPtr((uint)payload.Length)), "copy payload");
                    Ensure(Native.z_publisher_put(publisher._loanedPublisher, bytes, IntPtr.Zero), "publish sample");
                }
                finally { Marshal.FreeHGlobal(bytes); }
            }
        }

        private IntPtr GetKey(string name)
        {
            if (_keys.TryGetValue(name, out var existing)) return Native.z_keyexpr_loan(existing);
            IntPtr owned = Marshal.AllocHGlobal(KeyExprSize);
            Zero(owned, KeyExprSize);
            IntPtr text = Utf8(name);
            try { Ensure(Native.z_keyexpr_from_str(owned, text), "create key expression"); }
            catch { Marshal.FreeHGlobal(owned); throw; }
            finally { Marshal.FreeHGlobal(text); }
            IntPtr loan = Native.z_keyexpr_loan(owned);
            _keys.Add(name, owned);
            return loan;
        }

        private void OnSample<T>(IntPtr sample, Action<T> callback)
        {
            if (_disposing) return;
            try
            {
                IntPtr payload = Native.z_sample_payload(sample);
                if (payload == IntPtr.Zero) return;
                int length = checked((int)Native.z_bytes_len(payload).ToUInt64());
                if (length <= 0 || length > 65536) return;
                Reader reader = Native.z_bytes_get_reader(payload);
                byte[] bytes = new byte[length];
                UIntPtr read = Native.z_bytes_reader_read(ref reader, bytes, new UIntPtr((uint)length));
                if (read.ToUInt64() != (ulong)length) return;
                T value = Deserialize<T>(bytes);
                if (!_disposing) callback(value);
            }
            catch { /* 丢弃单条格式错误的网络样本，不中断 Zenoh 回调线程。 */ }
        }

        public void Dispose()
        {
            lock (_publishLock)
            {
                if (_disposing) return;
                _disposing = true;
            }
            foreach (var publisher in _publishers) publisher.Dispose();
            // 原生 drop 可能等待回调；不能持有回调发送命令时需要的锁。
            Native.z_session_drop(_session);
            foreach (var subscription in _subscriptions) subscription.Dispose();
            GC.KeepAlive(_subscriptions);
            foreach (var key in _keys.Values)
            {
                Native.z_keyexpr_drop(key);
                Marshal.FreeHGlobal(key);
            }
            Marshal.FreeHGlobal(_session);
            _session = _loanedSession = IntPtr.Zero;
            _keys.Clear();
            _subscriptions.Clear();
            _publishers.Clear();
        }

        private static IntPtr Utf8(string value)
        {
            byte[] data = System.Text.Encoding.UTF8.GetBytes(value + "\0");
            IntPtr ptr = Marshal.AllocHGlobal(data.Length);
            Marshal.Copy(data, 0, ptr, data.Length);
            return ptr;
        }

        private static void Zero(IntPtr ptr, int size)
        {
            for (int i = 0; i < size; ++i) Marshal.WriteByte(ptr, i, 0);
        }

        private static void Ensure(sbyte result, string operation)
        {
            if (result < 0) throw new InvalidOperationException("Zenoh failed to " + operation + " (" + result + ").");
        }

        public sealed class Publisher<T> : IDisposable
        {
            private readonly ZenohSession _owner;
            internal IntPtr _publisher;
            internal IntPtr _loanedPublisher;
            internal Publisher(ZenohSession owner, IntPtr publisher, IntPtr loanedPublisher)
            { _owner = owner; _publisher = publisher; _loanedPublisher = loanedPublisher; }
            public bool IsAvailable { get { lock (_owner._publishLock) return _publisher != IntPtr.Zero && _owner.AvailableLocked; } }
            public void Publish(T value)
            {
                if (!IsAvailable) throw new InvalidOperationException("Zenoh publisher is unavailable.");
                _owner.Publish(this, Serialize(value));
            }
            public void Dispose()
            {
                lock (_owner._publishLock)
                {
                if (_publisher == IntPtr.Zero) return;
                Native.z_publisher_drop(_publisher);
                Marshal.FreeHGlobal(_publisher);
                _publisher = _loanedPublisher = IntPtr.Zero;
                }
            }
        }

        private abstract class Subscription : IDisposable
        {
            public abstract void Dispose();
        }

        private sealed class Subscription<T> : Subscription
        {
            private readonly ZenohSession _owner;
            private readonly Action<T> _callback;
            public readonly SampleCallback NativeCallback;
            public Subscription(ZenohSession owner, Action<T> callback)
            {
                _owner = owner; _callback = callback; NativeCallback = Receive;
            }
            private void Receive(IntPtr sample, IntPtr context) => _owner.OnSample(sample, _callback);
            public override void Dispose() { GC.KeepAlive(NativeCallback); }
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void SampleCallback(IntPtr sample, IntPtr context);

        [StructLayout(LayoutKind.Sequential)]
        private struct Reader { public ulong A, B, C; }

        private static class Native
        {
            private const string Library = "zenohc.dll";
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern sbyte zc_config_from_str(IntPtr config, IntPtr json);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void z_config_drop(IntPtr config);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern sbyte z_open(IntPtr session, IntPtr config, IntPtr options);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr z_session_loan(IntPtr session);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void z_session_drop(IntPtr session);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] [return: MarshalAs(UnmanagedType.I1)] internal static extern bool z_session_is_closed(IntPtr session);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern sbyte z_keyexpr_from_str(IntPtr key, IntPtr expression);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr z_keyexpr_loan(IntPtr key);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void z_keyexpr_drop(IntPtr key);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void z_closure_sample(IntPtr closure, SampleCallback callback, IntPtr drop, IntPtr context);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void z_subscriber_options_default(IntPtr options);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void z_publisher_options_default(IntPtr options);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern sbyte z_declare_publisher(IntPtr session, IntPtr publisher, IntPtr key, IntPtr options);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr z_publisher_loan(IntPtr publisher);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void z_publisher_drop(IntPtr publisher);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern sbyte z_declare_background_subscriber(IntPtr session, IntPtr key, IntPtr callback, IntPtr options);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern sbyte z_bytes_copy_from_buf(IntPtr bytes, [In] byte[] data, UIntPtr length);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern sbyte z_publisher_put(IntPtr publisher, IntPtr bytes, IntPtr options);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr z_sample_payload(IntPtr sample);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern UIntPtr z_bytes_len(IntPtr bytes);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern Reader z_bytes_get_reader(IntPtr bytes);
            [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern UIntPtr z_bytes_reader_read(ref Reader reader, [Out] byte[] destination, UIntPtr length);
        }
    }
}
