using System;
using System.Collections.Concurrent;
using System.Threading;
using System.Threading.Tasks;
using MasterConsole.Protocol;
using VI.Service.Net;

namespace MasterConsole.Services
{
    public sealed class RemoteLinkSettings
    {
        public string Host { get; set; } = "192.168.60.1";
        public string Endpoint { get; set; }
        public byte[] Token { get; set; }
    }

    /// <summary>Zenoh + MessagePack 远程链路。IRobotLink 保持不变，UI 不需要知道传输协议。</summary>
    public sealed class RemoteRobotLink : IRobotLink
    {
        private const int ControlPeriodMs = 10;
        private const int StateTimeoutMs = ProtocolConstants.StatusTimeoutMs;
        private readonly RemoteLinkSettings _cfg;
        private readonly IHandleService _handles;
        private readonly TimeProvider _time;
        private readonly long _startedAt;
        private readonly Func<bool> _sessionAvailable;
        private readonly object _lock = new object();
        private readonly ConcurrentDictionary<int, TaskCompletionSource<CommandResult>> _pending =
            new ConcurrentDictionary<int, TaskCompletionSource<CommandResult>>();
        private ZenohSession _zenoh;
        private ZenohSession.Publisher<HandleFrame> _handlePublisher;
        private ZenohSession.Publisher<CommandFrame> _commandPublisher;
        private CancellationTokenSource _cts;
        private Thread _handleThread;
        private ulong _sessionId;
        private long _handleSequence;
        private long _commandSequence;
        private int _nextCommandId;
        private int _inj1, _inj2, _axis4;
        private long _lastStateMs = -1;
        private long _lastHapticMs = -1;
        private double _rttMs = double.NaN;
        private bool _hasControl;
        private bool _controlRequested;
        private int _controlRequestGeneration;
        private bool _authSent;
        private byte[] _nonceClient = FrameAuth.NewNonce();
        private byte[] _nonceServer;
        private byte[] _sessionKey;
        private ulong _lastStateSequence;
        private bool _hasStateSequence;
        private ulong _lastForceSequence;
        private bool _hasForceSequence;
        private long _lastHandshakeMs;
        private bool _reportedConnected;
        private bool _authenticated;
        private long _statusCount;
        private long _statusStartedMs;
        private volatile bool _initializing;
        private bool _acceptingCallbacks;
        private bool _forceGateOpen;
        private int _callbackGeneration;
        private long _callbackOrdinal;
        private long _forceGateOpenedAtOrdinal;

        public RemoteRobotLink(RemoteLinkSettings cfg, IHandleService handles = null, TimeProvider timeProvider = null)
        {
            _cfg = cfg ?? throw new ArgumentNullException(nameof(cfg));
            _handles = handles;
            _time = timeProvider ?? TimeProvider.System;
            _startedAt = _time.GetTimestamp();
            _sessionAvailable = () => _zenoh != null && _zenoh.IsAvailable;
        }

        internal RemoteRobotLink(RemoteLinkSettings cfg, IHandleService handles, TimeProvider timeProvider, Func<bool> sessionAvailable)
            : this(cfg, handles, timeProvider)
        {
            _sessionAvailable = sessionAvailable ?? throw new ArgumentNullException(nameof(sessionAvailable));
            _acceptingCallbacks = true;
        }

        internal byte[] ClientNonce => (byte[])_nonceClient.Clone();

        private long NowMs => (long)_time.GetElapsedTime(_startedAt).TotalMilliseconds;

        private bool IsFreshLocked(long now) => _acceptingCallbacks && _authenticated && _sessionAvailable() &&
            _lastStateMs >= 0 && now - _lastStateMs >= 0 && now - _lastStateMs < StateTimeoutMs;

        private void ExpireStateLocked(long now)
        {
            if (IsFreshLocked(now)) return;
            _hasControl = false;
            _inj1 = _inj2 = _axis4 = 0;
            if (!_forceGateOpen) return;
            _forceGateOpen = false;
            _handles?.ClearHaptic();
        }

        public string Name => "Zenoh 远程链路 " + _cfg.Host;
        public bool IsConnected
        {
            get
            {
                lock (_lock)
                {
                    long now = NowMs;
                    ExpireStateLocked(now);
                    return IsFreshLocked(now);
                }
            }
        }
        public bool HasControl { get { lock (_lock) return _hasControl && IsConnected; } }
        public event EventHandler ConnectionChanged;
        public event EventHandler<StatusFrame> StatusReceived;
        public event EventHandler<LogMessage> LogReceived;

        public async Task ConnectAsync()
        {
            ZenohSession session;
            lock (_lock)
            {
                if (_zenoh != null) return;
                if (_cfg.Token == null || _cfg.Token.Length < 16) throw new InvalidOperationException("连接认证密钥缺失或长度不足。");
                string endpoint = string.IsNullOrWhiteSpace(_cfg.Endpoint) ? "tcp/" + _cfg.Host + ":7447" : _cfg.Endpoint;
                _initializing = true;
                int generation = ++_callbackGeneration;
                try
                {
                    _zenoh = new ZenohSession(endpoint);
                    _handlePublisher = _zenoh.Advertise<HandleFrame>(ZenohProtocol.HandleTopic);
                    _commandPublisher = _zenoh.Advertise<CommandFrame>(ZenohProtocol.CommandTopic);
                    _zenoh.Subscribe<StateFrame>(ZenohProtocol.StateTopic, frame => OnState(frame, generation));
                    _zenoh.Subscribe<ForceFrame>(ZenohProtocol.ForceTopic, frame => OnForce(frame, generation));
                }
                catch
                {
                    var failed = _zenoh; _zenoh = null; _handlePublisher = null; _commandPublisher = null;
                    failed?.Dispose();
                    _initializing = false;
                    throw;
                }
                _authSent = false;
                _nonceClient = FrameAuth.NewNonce();
                _nonceServer = null;
                _sessionKey = null;
                _sessionId = 0;
                _hasStateSequence = false;
                _hasForceSequence = false;
                _authenticated = _reportedConnected = false;
                _forceGateOpen = false;
                _lastStateMs = _lastHapticMs = -1;
                _statusCount = 0; _statusStartedMs = NowMs;
                _lastHandshakeMs = NowMs;
                _handleSequence = 0;
                _commandSequence = 0;
                _acceptingCallbacks = true;
                _initializing = false;
                _cts = new CancellationTokenSource();
                CancellationToken token = _cts.Token;
                _handleThread = new Thread(() => HandleLoop(token)) { IsBackground = true, Name = "Zenoh-Handle-100Hz" };
                _handleThread.Start();
                session = _zenoh;
            }
            SendCommand(ZenohCommand.Hello, 0, 0, 0, 0, "", _nonceClient, null, null, 0);
            Log("info", "已打开 Zenoh 会话，等待 ADS 状态帧。");
            ConnectionChanged?.Invoke(this, EventArgs.Empty);
            // 界面随后立即申请控制权，必须先等认证后的状态帧建立连接。
            long started = NowMs;
            while (!IsConnected)
            {
                if (_zenoh != session || !session.IsAvailable)
                    throw new InvalidOperationException("等待 ADS 状态时 Zenoh 会话已断开。");
                if (NowMs - started >= 10000)
                    throw new TimeoutException("10 秒内未收到通过认证的 ADS 状态帧，请检查数据端程序及两端认证密钥。");
                await Task.Delay(10).ConfigureAwait(false);
            }
        }

        public void Disconnect() => Shutdown("链路已断开。", "warn");
        public void Dispose() => Shutdown(null, null);

        public async Task<CommandResult> AcquireControlAsync()
        {
            int generation;
            lock (_lock) generation = ++_controlRequestGeneration;
            var result = await RequestAsync(ZenohCommand.Acquire, 0, 0, 0, 0, 5000).ConfigureAwait(false);
            lock (_lock)
            {
                if (generation == _controlRequestGeneration && _acceptingCallbacks) _controlRequested = result.Ok;
            }
            return result;
        }
        public Task<CommandResult> ReleaseControlAsync()
        {
            lock (_lock) { _controlRequested = false; ++_controlRequestGeneration; }
            return RequestAsync(ZenohCommand.Release, 0, 0, 0, 0, 5000);
        }
        public Task<CommandResult> PreparePositionAsync(double catheterMm, double wireMm)
            => RequestAsync(ZenohCommand.PreparePosition, 0, 0, catheterMm, wireMm, 330000);
        public Task<CommandResult> StartControlAsync() => RequestAsync(ZenohCommand.StartControl, 0, 0, 0, 0, 10000);
        public Task<CommandResult> SetArmManualEnableAsync(bool enabled)
            => RequestAsync(ZenohCommand.ArmManualEnable, enabled ? 1 : 0, 0, 0, 0, 5000);
        public Task<CommandResult> SetArmAxisEnableAsync(int axis, bool enabled)
            => RequestAsync(ZenohCommand.ArmAxisEnable, axis, enabled ? 1 : 0, 0, 0, 5000);
        public Task<CommandResult> ResetArmAxisAsync(int axis)
            => RequestAsync(ZenohCommand.ArmAxisReset, axis, 0, 0, 0, 5000);
        public Task<CommandResult> SetArmCartesianJogAsync(int mode, int speedMilli)
            => Task.FromResult(IsConnected && SendCommand(ZenohCommand.ArmCartesianJog, mode, speedMilli, 0, 0, "", null, null, null, 0)
                ? CommandResult.DoneWithReason("已发送") : CommandResult.Rejected("点动命令未发送"));
        public Task<CommandResult> StopArmAsync() => RequestAsync(ZenohCommand.ArmStop, 0, 0, 0, 0, 5000);
        public Task<CommandResult> ReturnArmProgramZeroAsync() => RequestAsync(ZenohCommand.ArmProgramZero, 0, 0, 0, 0, 5000);
        public Task<CommandResult> SetArmCartesianParameterAsync(int field, int valueMilli)
            => RequestAsync(ZenohCommand.ArmCartesianParameter, field, valueMilli, 0, 0, 5000);
        public void SetArmAxisJog(int axis, int direction, double speedScale = 1.0)
        {
            if (direction != 0 && speedScale != 0.5 && speedScale != 1.0 && speedScale != 2.0) return;
            SendCommand(ZenohCommand.ArmAxisJog, axis, direction, direction == 0 ? 0 : speedScale, 0, "", null, null, null, 0);
        }
        public void KeepArmCartesianAlive()
            => SendCommand(ZenohCommand.ArmCartesianAlive, 0, 0, 0, 0, "", null, null, null, 0);
        public Task<CommandResult> SetForceFeedbackAsync(bool enable)
            => RequestAsync(ZenohCommand.ForceFeedback, enable ? 1 : 0, 0, 0, 0, 15000);
        public Task<CommandResult> SetCylinderAsync(int index, bool engaged)
            => RequestAsync(ZenohCommand.Cylinder, index, engaged ? 1 : 0, 0, 0, 10000);
        public Task<CommandResult> SetYValveClosedAsync(bool closed)
            => RequestAsync(ZenohCommand.YValve, closed ? 1 : 0, 0, 0, 0, 10000);
        public void SetInjector(int index, int direction)
        {
            lock (_lock)
            {
                ExpireStateLocked(NowMs);
                if (!_hasControl) direction = 0;
                direction = Math.Max(-1, Math.Min(1, direction));
                if (index == 1) _inj1 = direction; else if (index == 2) _inj2 = direction;
            }
        }
        public void SetAxis4(int direction)
        {
            lock (_lock)
            {
                ExpireStateLocked(NowMs);
                _axis4 = !_hasControl ? 0 : Math.Max(-1, Math.Min(1, direction));
            }
        }

        internal ControlFrame CaptureControlFrame()
        {
            lock (_lock)
            {
                ExpireStateLocked(NowMs);
                return new ControlFrame
                {
                    HandleA = _handles != null ? _handles.GetSample(0) : default,
                    HandleB = _handles != null ? _handles.GetSample(1) : default,
                    Injector1Dir = (sbyte)_inj1, Injector2Dir = (sbyte)_inj2, Axis4Dir = (sbyte)_axis4
                };
            }
        }

        public LinkStats GetStats()
        {
            lock (_lock)
            {
                long now = NowMs;
                ExpireStateLocked(now);
                return new LinkStats
                {
                    Session = _sessionId > uint.MaxValue ? 0 : (uint)_sessionId,
                    RttMs = _rttMs,
                    StatusAgeMs = _lastStateMs < 0 ? double.NaN : now - _lastStateMs,
                    HapticAgeMs = _lastHapticMs < 0 ? double.NaN : now - _lastHapticMs,
                    StatusHz = now > _statusStartedMs ? Interlocked.Read(ref _statusCount) * 1000.0 / (now - _statusStartedMs) : 0, DroppedFrames = 0,
                    Handle582Online = _handles != null && _handles.IsOnline(0),
                    Handle587Online = _handles != null && _handles.IsOnline(1),
                    HandleSdkMissing = _handles != null && _handles.SdkMissing,
                };
            }
        }

        private Task<CommandResult> RequestAsync(ZenohCommand command, int p1, int p2, double v1, double v2, int timeoutMs)
        {
            if (!IsConnected) return Task.FromResult(CommandResult.Rejected("未连接"));
            int id = Interlocked.Increment(ref _nextCommandId);
            var tcs = new TaskCompletionSource<CommandResult>(TaskCreationOptions.RunContinuationsAsynchronously);
            _pending[id] = tcs;
            try
            {
                if (!SendCommand(command, p1, p2, v1, v2, "", null, null, null, id))
                { _pending.TryRemove(id, out _); return Task.FromResult(CommandResult.Rejected("命令未发送：发布者不可用或会话未认证")); }
            }
            catch (Exception ex) { _pending.TryRemove(id, out _); return Task.FromResult(CommandResult.Rejected("发送失败：" + ex.Message)); }
            return WaitResultAsync(id, tcs, timeoutMs);
        }

        private async Task<CommandResult> WaitResultAsync(int id, TaskCompletionSource<CommandResult> tcs, int timeoutMs)
        {
            if (await Task.WhenAny(tcs.Task, Task.Delay(timeoutMs)) != tcs.Task)
            { _pending.TryRemove(id, out _); return CommandResult.Rejected("数据端无响应（超时）"); }
            return await tcs.Task;
        }

        private bool SendCommand(ZenohCommand command, int p1, int p2, double v1, double v2, string payload,
            byte[] nonceClient, byte[] nonceServer, byte[] proof, int commandId)
        {
            ZenohSession.Publisher<CommandFrame> publisher;
            CommandFrame frame;
            byte[] key;
            lock (_lock)
            {
                publisher = _commandPublisher;
                if (publisher == null || !publisher.IsAvailable) return false;
                bool handshake = command == ZenohCommand.Hello || command == ZenohCommand.Authenticate;
                if (!handshake)
                {
                    ExpireStateLocked(NowMs);
                    if (!IsFreshLocked(NowMs)) return false;
                }
                key = _sessionKey;
                frame = new CommandFrame
                {
                    Version = ZenohProtocol.Version, SessionId = command == ZenohCommand.Hello ? 0 : _sessionId,
                    Sequence = unchecked((ulong)Interlocked.Increment(ref _commandSequence)),
                    MonotonicMs = (ulong)NowMs, CommandId = commandId,
                    Command = command, Param1 = p1, Param2 = p2, Value1 = v1, Value2 = v2,
                    Payload = payload ?? "", NonceClient = nonceClient ?? new byte[0],
                    NonceServer = nonceServer ?? new byte[0], Proof = proof ?? new byte[0], AuthTag = new byte[0]
                };
                if (command != ZenohCommand.Hello && command != ZenohCommand.Authenticate) frame.AuthTag = ZenohWire.Sign(frame, key);
                try { publisher.Publish(frame); return true; }
                catch (Exception ex) { Log("warn", "命令发送失败：" + ex.Message); return false; }
            }
        }

        private void HandleLoop(CancellationToken token)
        {
            bool preciseTimer = TimeBeginPeriod(1) == 0;
            try
            {
                long next = 0;
                while (!token.IsCancellationRequested)
                {
                    long now = NowMs;
                    if (now < next) { Thread.Sleep((int)Math.Max(1, next - now)); continue; }
                    next += ControlPeriodMs;
                    if (next <= now) next = now + ControlPeriodMs;
                    CheckConnection(now);
                    lock (_lock)
                    {
                        now = NowMs;
                        ExpireStateLocked(now);
                        byte[] key = _authenticated ? _sessionKey : null;
                        ulong session = _sessionId;
                        var publisher = _handlePublisher;
                        if (!IsFreshLocked(now) || key == null || publisher == null || !publisher.IsAvailable) continue;
                        var control = CaptureControlFrame();
                        if (!IsFreshLocked(NowMs)) continue;
                        var a = control.HandleA;
                        var b = control.HandleB;
                        try
                        {
                            var frame = new HandleFrame
                            {
                                Version = ZenohProtocol.Version, SessionId = session, Sequence = unchecked((ulong)Interlocked.Increment(ref _handleSequence)),
                                MonotonicMs = (ulong)now, Buttons582 = a.Buttons, Valid582 = a.Valid,
                                Encoder582_0 = a.Encoder0, Encoder582_1 = a.Encoder1, Joint582_0 = a.Joint0,
                                Joint582_1 = a.Joint1, Velocity582_0 = a.Vel0, Velocity582_1 = a.Vel1,
                                Buttons587 = b.Buttons, Valid587 = b.Valid, Encoder587_0 = b.Encoder0,
                                Encoder587_1 = b.Encoder1, Joint587_0 = b.Joint0, Joint587_1 = b.Joint1,
                                Velocity587_0 = b.Vel0, Velocity587_1 = b.Vel1,
                                Injector1Direction = control.Injector1Dir, Injector2Direction = control.Injector2Dir,
                                Axis4Direction = control.Axis4Dir, AuthTag = new byte[0]
                            };
                            frame.AuthTag = ZenohWire.Sign(frame, key);
                            publisher.Publish(frame);
                        }
                        catch (Exception ex) { Log("warn", "手柄帧发送失败：" + ex.Message); }
                    }
                }
            }
            finally { if (preciseTimer) TimeEndPeriod(1); }
        }

        [System.Runtime.InteropServices.DllImport("winmm.dll", EntryPoint = "timeBeginPeriod")]
        private static extern uint TimeBeginPeriod(uint milliseconds);
        [System.Runtime.InteropServices.DllImport("winmm.dll", EntryPoint = "timeEndPeriod")]
        private static extern uint TimeEndPeriod(uint milliseconds);

        internal void OnState(StateFrame frame) => OnState(frame, _callbackGeneration);

        private void OnState(StateFrame f, int generation)
        {
            long callbackOrdinal = Interlocked.Increment(ref _callbackOrdinal);
            long receivedMs = NowMs;
            if (_initializing) return;
            if (f.Version != ZenohProtocol.Version || f.SessionId == 0) return;
            StatusFrame status;
            CommandResultFrame[] results;
            lock (_lock)
            {
                if (!_acceptingCallbacks || generation != _callbackGeneration || !_sessionAvailable()) return;
                if (f.AuthState == 1)
                {
                    if (_authenticated || _authSent || f.NonceServer == null || f.NonceServer.Length != 16) return;
                    _sessionId = f.SessionId;
                    _nonceServer = f.NonceServer;
                    _sessionKey = FrameAuth.DeriveSessionKey(_cfg.Token, _nonceClient, _nonceServer);
                    _authSent = SendCommand(ZenohCommand.Authenticate, 0, 0, 0, 0, "", _nonceClient, _nonceServer,
                        FrameAuth.ComputeAuth(_cfg.Token, _nonceClient, _nonceServer), 0);
                    return;
                }
                if (f.AuthState != 2 || f.SessionId != _sessionId || _sessionKey == null || !ZenohWire.Verify(f, _sessionKey)) return;
                if (_hasStateSequence && f.Sequence <= _lastStateSequence) return;
                long now = NowMs;
                ExpireStateLocked(now);
                bool restoreControl = !_authenticated && _controlRequested;
                _lastStateSequence = f.Sequence;
                _hasStateSequence = _authenticated = true;
                _lastStateMs = receivedMs;
                _hasControl = (f.Flags & (uint)StatusFlags.LeaseHeld) != 0;
                if (!_forceGateOpen) _forceGateOpenedAtOrdinal = callbackOrdinal;
                _forceGateOpen = true;
                ExpireStateLocked(now);
                // 自动重认证只恢复先前的控制权申请，不补发运动指令或旧输入。
                if (restoreControl && !_hasControl && IsFreshLocked(now))
                    SendCommand(ZenohCommand.Acquire, 0, 0, 0, 0, "", null, null, null, 0);
                Interlocked.Increment(ref _statusCount);
                status = new StatusFrame
                {
                    EchoTsMs = (uint)Math.Min(uint.MaxValue, f.EchoMainMonotonicMs), Flags = (StatusFlags)f.Flags,
                    Mode = f.Mode, Phase = f.Phase, SelfCheckStatus = f.SelfCheckStatus, AdsState = f.AdsState,
                    Cylinder1 = f.Cylinder1, Cylinder2 = f.Cylinder2, Cylinder3 = f.Cylinder3, Cylinder4 = f.Cylinder4,
                    CylinderManualMask = f.CylinderManualMask, InjectorActive1 = f.Injector1Direction,
                    InjectorActive2 = f.Injector2Direction, Axis4Active = f.Axis4Direction,
                    Force582F = f.Force582F, Force582N = f.Force582N, Force587F = f.Force587F,
                    Force587N = f.Force587N, CleanForceN = (f.Flags & (uint)StatusFlags.CleanForceValid) != 0 ? f.CleanForceN : float.NaN, AdsActualHz = f.AdsActualHz,
                    AxisPos = new[] { f.AxisPos0, f.AxisPos1, f.AxisPos2, f.AxisPos3, f.AxisPos4, f.AxisPos5, f.AxisPos6 },
                    AxisFromLeft = new[] { f.AxisFromLeft0, f.AxisFromLeft1, f.AxisFromLeft2, f.AxisFromLeft3, f.AxisFromLeft4, f.AxisFromLeft5, f.AxisFromLeft6 },
                    HasNetDistance = f.HasNetDistance, Axis1NetDistanceMm = f.Axis1NetDistanceMm, Axis6NetDistanceMm = f.Axis6NetDistanceMm,
                    Axis6WindowStartFromLeftMm = f.Axis6WindowStartFromLeftMm, Axis6WindowEndFromLeftMm = f.Axis6WindowEndFromLeftMm
                };
                if (f.EchoMainMonotonicMs != 0 && f.EchoMainMonotonicMs <= (ulong)now)
                {
                    long rtt = now - (long)f.EchoMainMonotonicMs;
                    if (rtt >= 0 && rtt < 10000) _rttMs = double.IsNaN(_rttMs) ? rtt : _rttMs * .8 + rtt * .2;
                }
                results = f.Results ?? Array.Empty<CommandResultFrame>();
            }
            foreach (var result in results)
                if (_pending.TryRemove(result.CommandId, out var tcs))
                    tcs.TrySetResult(result.Result == 0 ? CommandResult.DoneWithReason(result.Reason) : CommandResult.Rejected(result.Reason));
            CompleteLongTask(f.PrepareTaskCommandId, f.PrepareTaskState, "准备位置");
            CompleteLongTask(f.StartTaskCommandId, f.StartTaskState, "开始控制");
            StatusReceived?.Invoke(this, status);
            NotifyConnection();
        }

        internal void OnForce(ForceFrame frame) => OnForce(frame, _callbackGeneration);

        private void OnForce(ForceFrame f, int generation)
        {
            long callbackOrdinal = Interlocked.Increment(ref _callbackOrdinal);
            long receivedMs = NowMs;
            if (_initializing) return;
            lock (_lock)
            {
                if (!_acceptingCallbacks || generation != _callbackGeneration || !_authenticated || f.Version != ZenohProtocol.Version ||
                    f.SessionId != _sessionId || _sessionKey == null || !ZenohWire.Verify(f, _sessionKey)) return;
                if (_hasForceSequence && f.Sequence <= _lastForceSequence) return;
                _hasForceSequence = true; _lastForceSequence = f.Sequence;
                long now = NowMs;
                ExpireStateLocked(now);
                if (!_forceGateOpen || callbackOrdinal <= _forceGateOpenedAtOrdinal || !IsFreshLocked(now) ||
                    now - receivedMs >= ProtocolConstants.HapticTimeoutMs) return;
                _lastHapticMs = receivedMs;
                _handles?.SetHaptic(0, new HapticOut { Enable = f.Enable582, Axis = f.Axis582, ForceN = f.Force582N, TorqueNm = f.Torque582Nm });
                _handles?.SetHaptic(1, new HapticOut { Enable = f.Enable587, Axis = f.Axis587, ForceN = f.Force587N, TorqueNm = f.Torque587Nm });
            }
        }

        private void CompleteLongTask(int commandId, byte state, string name)
        {
            if (commandId <= 0 || (state != 2 && state != 3)) return;
            if (_pending.TryRemove(commandId, out var tcs))
                tcs.TrySetResult(state == 2 ? CommandResult.Done() : CommandResult.Rejected(name + "任务失败"));
        }

        internal void CheckConnection() => CheckConnection(NowMs);

        private void CheckConnection(long now)
        {
            byte[] nonce = null;
            bool reset = false;
            lock (_lock)
            {
                ExpireStateLocked(now);
                long last = Interlocked.Read(ref _lastStateMs);
                if ((last < 0 || now - last > 3000) && now - _lastHandshakeMs >= 3000)
                {
                    reset = _authenticated;
                    _authenticated = _authSent = _hasStateSequence = _hasForceSequence = false;
                    _sessionKey = null; _sessionId = 0; _nonceServer = null;
                    nonce = _nonceClient = FrameAuth.NewNonce();
                    _lastHandshakeMs = now;
                }
            }
            if (reset) RejectPending("连接已失效，命令不补发");
            if (nonce != null) SendCommand(ZenohCommand.Hello, 0, 0, 0, 0, "", nonce, null, null, 0);
            NotifyConnection();
        }

        private void NotifyConnection()
        {
            bool changed;
            lock (_lock)
            {
                bool connected = IsConnected;
                changed = connected != _reportedConnected;
                _reportedConnected = connected;
            }
            if (changed) ConnectionChanged?.Invoke(this, EventArgs.Empty);
        }

        private void RejectPending(string reason)
        {
            foreach (var item in _pending)
                if (_pending.TryRemove(item.Key, out var tcs)) tcs.TrySetResult(CommandResult.Rejected(reason));
        }

        private void Shutdown(string message, string level)
        {
            ZenohSession session;
            CancellationTokenSource cancellation;
            Thread thread;
            lock (_lock)
            {
                session = _zenoh; cancellation = _cts; thread = _handleThread;
                _acceptingCallbacks = false;
                ++_callbackGeneration;
                _zenoh = null; _handlePublisher = null; _commandPublisher = null; _hasControl = false;
                _cts = null; _handleThread = null;
                _authenticated = false; _controlRequested = false; ++_controlRequestGeneration;
                _sessionId = 0; _sessionKey = null;
                _inj1 = _inj2 = _axis4 = 0;
                Interlocked.Exchange(ref _lastStateMs, -1);
                Interlocked.Exchange(ref _lastHapticMs, -1);
                _forceGateOpen = false;
                _handles?.ClearHaptic();
            }
            cancellation?.Cancel();
            if (thread != null && thread != Thread.CurrentThread) thread.Join();
            session?.Dispose();
            cancellation?.Dispose();
            RejectPending("链路已断开");
            if (message != null) Log(level ?? "info", message);
            NotifyConnection();
        }

        private void Log(string level, string text) => LogReceived?.Invoke(this, new LogMessage { Level = level, Text = text });
    }
}
