using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Threading;
using MasterConsole.Protocol;
using MasterConsole.Services;
using MasterConsole.ViewModels;

namespace MasterConsole.Protocol.Tests
{
    internal static partial class Program
    {
        private static void RobotServiceTests()
        {
            Check("状态超时 2s 与触觉超时 3s 独立", ProtocolConstants.StatusTimeoutMs == 2000 && ProtocolConstants.HapticTimeoutMs == 3000);
            Check("手柄触觉独立在 3000ms 过期", HandleService.IsHapticFresh(2999, 0) &&
                !HandleService.IsHapticFresh(3000, 0) && !HandleService.IsHapticFresh(0, -1));
            using (var dormant = new HandleService())
            {
                Check("手柄构造不启动设备", !dormant.IsRunning && !dormant.GetSample(0).Valid && !dormant.IsOnline(0));
                dormant.Stop();
                dormant.Stop();
                Check("未启动手柄可幂等停止", !dormant.IsRunning);
            }

            var time = new RobotTestTimeProvider();
            var handles = new FakeHandleService();
            byte[] token = Encoding.ASCII.GetBytes("offline-robot-service-test-token");
            bool available = true;
            using (var link = new RemoteRobotLink(new RemoteLinkSettings { Token = token }, handles, time, () => available))
            {
                var challenge = NewState();
                challenge.AuthState = 1;
                challenge.AuthTag = Array.Empty<byte>();
                link.OnState(challenge);
                byte[] key = FrameAuth.DeriveSessionKey(token, link.ClientNonce, challenge.NonceServer);
                StateFrame State(ulong sequence)
                {
                    var frame = NewState();
                    frame.Sequence = sequence;
                    frame.Flags |= (uint)StatusFlags.LeaseHeld;
                    frame.AuthTag = ZenohWire.Sign(frame, key);
                    return frame;
                }
                ForceFrame Force(ulong sequence)
                {
                    var frame = NewForce();
                    frame.Sequence = sequence;
                    frame.AuthTag = ZenohWire.Sign(frame, key);
                    return frame;
                }

                int deliveredStates = 0;
                StatusFrame receivedState = default;
                link.StatusReceived += (_, state) => { deliveredStates++; receivedState = state; };
                link.OnState(State(1));
                Check("窗口边界只读映射到主端状态", receivedState.Axis6WindowStartFromLeftMm == 16 && receivedState.Axis6WindowEndFromLeftMm == 48);
                link.OnForce(Force(1));
                link.SetInjector(1, 1);
                link.SetInjector(2, -1);
                link.SetAxis4(1);
                Check("认证有效状态建立连接并可写力", link.IsConnected && link.HasControl && handles.Writes == 2 && handles.Last[0].Enable);
                time.Advance(1999);
                link.CheckConnection();
                Check("1999ms 状态仍新鲜", link.IsConnected && link.HasControl && link.GetStats().StatusAgeMs == 1999 && handles.Clears == 0);

                var wrongAuth = State(2);
                wrongAuth.AuthTag[0] ^= 1;
                link.OnState(wrongAuth);
                link.OnState(State(1));
                var wrongSession = State(2);
                wrongSession.SessionId++;
                wrongSession.AuthTag = ZenohWire.Sign(wrongSession, key);
                link.OnState(wrongSession);
                var wrongVersion = State(2);
                wrongVersion.Version = 2;
                link.OnState(wrongVersion);
                Check("错误认证、重复、错误会话与版本不刷新状态", deliveredStates == 1 && link.GetStats().StatusAgeMs == 1999);

                time.Advance(1);
                link.CheckConnection();
                Check("2000ms 立即断联锁定并清本地力", !link.IsConnected && !link.HasControl && handles.Clears == 1 && !handles.Last[0].Enable);
                Check("状态过期拒绝新申请和定位臂点动", !link.AcquireControlAsync().GetAwaiter().GetResult().Ok &&
                    !link.SetArmCartesianJogAsync(1, 1000).GetAwaiter().GetResult().Ok);
                link.OnForce(Force(2));
                Check("状态过期时迟到力帧不输出", handles.Writes == 2 && link.GetStats().HapticAgeMs == 2000);

                link.OnState(State(2));
                Check("新认证状态恢复连接", link.IsConnected && link.HasControl && link.GetStats().StatusAgeMs == 0);
                Check("恢复不重放连续输入", link.CaptureControlFrame().Injector1Dir == 0 &&
                    link.CaptureControlFrame().Injector2Dir == 0 && link.CaptureControlFrame().Axis4Dir == 0);
                link.OnForce(Force(2));
                Check("恢复不重放过期力帧", handles.Writes == 2);
                link.OnForce(Force(3));
                Check("恢复后新序号力帧正常输出", handles.Writes == 4 && handles.Last[0].Enable);

                var viewModel = new MainViewModel(link);
                link.OnState(State(3));
                PumpUi();
                Check("界面按链路单调接收时间判断新鲜度", viewModel.StatusFresh && viewModel.CanOperate);
                int changes = 0;
                viewModel.PropertyChanged += (_, _) => changes++;
                link.OnState(State(4));
                viewModel.Dispose();
                viewModel.Dispose();
                PumpUi();
                link.OnState(State(5));
                PumpUi();
                Check("界面释放退订并丢弃已排队回调，不销毁共享链路", changes == 0 && link.IsConnected && !handles.Disposed);

                available = false;
                Check("会话失效立即清力", !link.IsConnected && handles.Clears == 2);
                available = true;
                link.OnState(State(6));
                link.OnForce(Force(4));

                using (var entered = new ManualResetEventSlim())
                using (var release = new ManualResetEventSlim())
                {
                    handles.BeforeWrite = () =>
                    {
                        entered.Set();
                        if (!release.Wait(5000)) throw new TimeoutException("并发回调测试释放超时。");
                    };
                    var write = Task.Run(() => link.OnForce(Force(5)));
                    Check("并发测试进入力回调", entered.Wait(5000));
                    var disconnect = Task.Run(() => link.Disconnect());
                    release.Set();
                    Check("力回调与断开并发无死锁", Task.WaitAll(new[] { write, disconnect }, 5000));
                    handles.BeforeWrite = null;
                }
                Check("主动断开立即清力且不销毁注入手柄", handles.Clears == 3 && !handles.Disposed && !link.IsConnected);
                link.OnState(State(7));
                link.OnForce(Force(6));
                Check("断开后的迟到回调不复活连接或力", !link.IsConnected && handles.Writes == 8 && !handles.Last[0].Enable);
            }
            Check("链路释放不拥有共享手柄生命周期", !handles.Disposed && handles.Starts == 0 && handles.Stops == 0);
            StateReceiptTimeTest(token);
            ForceRecoveryOrderTest(token);
            IdleStateTest(token);
        }

        private static void IdleStateTest(byte[] token)
        {
            var time = new RobotTestTimeProvider();
            using var link = new RemoteRobotLink(new RemoteLinkSettings { Token = token }, null, time, () => true);
            var challenge = NewState();
            challenge.AuthState = 1;
            link.OnState(challenge);
            byte[] key = FrameAuth.DeriveSessionKey(token, link.ClientNonce, challenge.NonceServer);
            for (ulong sequence = 1; sequence <= 900; ++sequence)
            {
                var frame = NewState();
                frame.Sequence = sequence;
                frame.Flags |= (uint)StatusFlags.LeaseHeld;
                frame.AuthTag = ZenohWire.Sign(frame, key);
                link.OnState(frame);
                time.Advance(1000);
                link.CheckConnection();
            }
            Check("静止15分钟但状态持续接收不会断联或丢控制权", link.IsConnected && link.HasControl);
        }

        private static void ForceRecoveryOrderTest(byte[] token)
        {
            var time = new RobotTestTimeProvider();
            var handles = new FakeHandleService();
            using var link = new RemoteRobotLink(new RemoteLinkSettings { Token = token }, handles, time, () => true);
            var challenge = NewState();
            challenge.AuthState = 1;
            link.OnState(challenge);
            byte[] key = FrameAuth.DeriveSessionKey(token, link.ClientNonce, challenge.NonceServer);
            StateFrame State(ulong sequence)
            {
                var frame = NewState();
                frame.Sequence = sequence;
                frame.AuthTag = ZenohWire.Sign(frame, key);
                return frame;
            }
            ForceFrame Force(ulong sequence)
            {
                var frame = NewForce();
                frame.Sequence = sequence;
                frame.AuthTag = ZenohWire.Sign(frame, key);
                return frame;
            }
            link.OnState(State(1));
            link.OnForce(Force(1));
            time.Advance(2000);
            link.CheckConnection();
            int forceThreadId = 0;
            using var entered = new ManualResetEventSlim();
            using var release = new ManualResetEventSlim();
            time.TimestampRead = () =>
            {
                if (Thread.CurrentThread.ManagedThreadId != Volatile.Read(ref forceThreadId)) return;
                entered.Set();
                if (!release.Wait(5000)) throw new TimeoutException("过期力帧测试释放超时。");
            };
            var oldForce = Task.Run(() =>
            {
                Volatile.Write(ref forceThreadId, Thread.CurrentThread.ManagedThreadId);
                link.OnForce(Force(2));
            });
            Check("过期力帧先到达但等待状态恢复", entered.Wait(5000));
            link.OnState(State(2));
            release.Set();
            Check("等待的过期力帧回调结束", oldForce.Wait(5000));
            time.TimestampRead = null;
            Check("恢复状态之前到达的力帧不会越过恢复门", handles.Writes == 2 && !handles.Last[0].Enable);
            link.OnForce(Force(3));
            Check("恢复状态后到达的新力帧可写", handles.Writes == 4 && handles.Last[0].Enable);
        }

        private static void StateReceiptTimeTest(byte[] token)
        {
            var time = new RobotTestTimeProvider();
            var handles = new FakeHandleService();
            using var link = new RemoteRobotLink(new RemoteLinkSettings { Token = token }, handles, time, () => true);
            var challenge = NewState();
            challenge.AuthState = 1;
            link.OnState(challenge);
            byte[] key = FrameAuth.DeriveSessionKey(token, link.ClientNonce, challenge.NonceServer);
            var state = NewState();
            state.AuthTag = ZenohWire.Sign(state, key);
            link.OnState(state);
            var force = NewForce();
            force.AuthTag = ZenohWire.Sign(force, key);
            using var entered = new ManualResetEventSlim();
            using var received = new ManualResetEventSlim();
            using var release = new ManualResetEventSlim();
            handles.BeforeWrite = () =>
            {
                entered.Set();
                if (!release.Wait(5000)) throw new TimeoutException("接收时刻测试释放超时。");
            };
            var writer = Task.Run(() => link.OnForce(force));
            Check("接收时刻测试进入并发回调", entered.Wait(5000));
            int receiverThread = 0;
            time.TimestampRead = () => { if (Thread.CurrentThread.ManagedThreadId == receiverThread) received.Set(); };
            var receiver = Task.Run(() =>
            {
                receiverThread = Thread.CurrentThread.ManagedThreadId;
                state.Sequence++;
                state.AuthTag = ZenohWire.Sign(state, key);
                link.OnState(state);
            });
            try
            {
                Check("状态回调先记录单调接收时刻", received.Wait(5000));
                time.Advance(2000);
            }
            finally { release.Set(); }
            Check("并发状态回调完成", Task.WaitAll(new[] { writer, receiver }, 5000));
            time.TimestampRead = null;
            handles.BeforeWrite = null;
            Check("锁等待不会把 2s 前接收的状态变为新鲜", !link.IsConnected && !handles.Last[0].Enable && link.GetStats().StatusAgeMs == 2000);
        }

        private static void PumpUi()
        {
            var frame = new DispatcherFrame();
            Dispatcher.CurrentDispatcher.BeginInvoke(DispatcherPriority.Background, new Action(() => frame.Continue = false));
            Dispatcher.PushFrame(frame);
        }

        private sealed class RobotTestTimeProvider : TimeProvider
        {
            private long _milliseconds;
            public Action TimestampRead;
            public override long TimestampFrequency => 1000;
            public override long GetTimestamp()
            {
                long timestamp = Interlocked.Read(ref _milliseconds);
                TimestampRead?.Invoke();
                return timestamp;
            }
            public void Advance(long milliseconds) => Interlocked.Add(ref _milliseconds, milliseconds);
        }

        private sealed class FakeHandleService : IHandleService
        {
            public int Writes;
            public int Clears;
            public int Starts;
            public int Stops;
            public bool Disposed;
            public Action BeforeWrite;
            public HapticOut[] Last = new HapticOut[2];
            public bool SdkMissing => false;
            public bool IsOnline(int slot) => true;
            public HandleSample GetSample(int slot) => new HandleSample { Valid = true };
            public void SetHaptic(int slot, HapticOut output) { BeforeWrite?.Invoke(); Writes++; Last[slot] = output; }
            public void ClearHaptic() { Clears++; Array.Clear(Last); }
            public void Start() => Starts++;
            public void Stop() => Stops++;
            public void Dispose() => Disposed = true;
        }
    }
}
