using MessagePack;

namespace MasterConsole.Protocol
{
    public static class ZenohProtocol
    {
        public const byte Version = 1;
        public const string HandleTopic = "vi/ads/handle/v1";
        public const string CommandTopic = "vi/ads/command/v1";
        public const string StateTopic = "vi/ads/state/v1";
        public const string ForceTopic = "vi/ads/force/v1";
    }

    public enum ZenohCommand : byte
    {
        Hello = 0, Authenticate = 1, Acquire = 2, Release = 3,
        PreparePosition = 4, StartControl = 5, ForceFeedback = 6,
        Cylinder = 7, ArmManualEnable = 8, ArmAxisEnable = 9,
        ArmAxisReset = 10, ArmAxisJog = 11, ArmCartesianJog = 12,
        ArmStop = 13, ArmProgramZero = 14, ArmCartesianParameter = 15,
        ArmCartesianAlive = 16, YValve = 17
    }

    [MessagePackObject]
    public sealed class HandleFrame
    {
        [Key(0)] public byte Version { get; set; }
        [Key(1)] public ulong SessionId { get; set; }
        [Key(2)] public ulong Sequence { get; set; }
        [Key(3)] public ulong MonotonicMs { get; set; }
        [Key(4)] public byte Buttons582 { get; set; }
        [Key(5)] public bool Valid582 { get; set; }
        [Key(6)] public int Encoder582_0 { get; set; }
        [Key(7)] public int Encoder582_1 { get; set; }
        [Key(8)] public float Joint582_0 { get; set; }
        [Key(9)] public float Joint582_1 { get; set; }
        [Key(10)] public float Velocity582_0 { get; set; }
        [Key(11)] public float Velocity582_1 { get; set; }
        [Key(12)] public byte Buttons587 { get; set; }
        [Key(13)] public bool Valid587 { get; set; }
        [Key(14)] public int Encoder587_0 { get; set; }
        [Key(15)] public int Encoder587_1 { get; set; }
        [Key(16)] public float Joint587_0 { get; set; }
        [Key(17)] public float Joint587_1 { get; set; }
        [Key(18)] public float Velocity587_0 { get; set; }
        [Key(19)] public float Velocity587_1 { get; set; }
        [Key(20)] public sbyte Injector1Direction { get; set; }
        [Key(21)] public sbyte Injector2Direction { get; set; }
        [Key(22)] public sbyte Axis4Direction { get; set; }
        [Key(23)] public byte[] AuthTag { get; set; } = new byte[0];
    }

    [MessagePackObject]
    public sealed class CommandFrame
    {
        [Key(0)] public byte Version { get; set; }
        [Key(1)] public ulong SessionId { get; set; }
        [Key(2)] public ulong Sequence { get; set; }
        [Key(3)] public ulong MonotonicMs { get; set; }
        [Key(4)] public int CommandId { get; set; }
        [Key(5)] public ZenohCommand Command { get; set; }
        [Key(6)] public int Param1 { get; set; }
        [Key(7)] public int Param2 { get; set; }
        [Key(8)] public double Value1 { get; set; }
        [Key(9)] public double Value2 { get; set; }
        [Key(10)] public string Payload { get; set; } = "";
        [Key(11)] public byte[] NonceClient { get; set; } = new byte[0];
        [Key(12)] public byte[] NonceServer { get; set; } = new byte[0];
        [Key(13)] public byte[] Proof { get; set; } = new byte[0];
        [Key(14)] public byte[] AuthTag { get; set; } = new byte[0];
    }

    [MessagePackObject]
    public sealed class CommandResultFrame
    {
        [Key(0)] public int CommandId { get; set; }
        [Key(1)] public byte Result { get; set; }
        [Key(2)] public string Reason { get; set; } = "";
    }

    [MessagePackObject]
    public sealed class StateFrame
    {
        [Key(0)] public byte Version { get; set; }
        [Key(1)] public ulong SessionId { get; set; }
        [Key(2)] public ulong Sequence { get; set; }
        [Key(3)] public byte AuthState { get; set; }
        [Key(4)] public byte[] NonceServer { get; set; } = new byte[0];
        [Key(5)] public ulong EchoMainMonotonicMs { get; set; }
        [Key(6)] public uint Flags { get; set; }
        [Key(7)] public int Mode { get; set; }
        [Key(8)] public int Phase { get; set; }
        [Key(9)] public int SelfCheckStatus { get; set; }
        [Key(10)] public int AdsState { get; set; }
        [Key(11)] public ushort Cylinder1 { get; set; }
        [Key(12)] public ushort Cylinder2 { get; set; }
        [Key(13)] public ushort Cylinder3 { get; set; }
        [Key(14)] public ushort Cylinder4 { get; set; }
        [Key(15)] public byte CylinderManualMask { get; set; }
        [Key(16)] public sbyte Injector1Direction { get; set; }
        [Key(17)] public sbyte Injector2Direction { get; set; }
        [Key(18)] public sbyte Axis4Direction { get; set; }
        [Key(19)] public float Force582F { get; set; }
        [Key(20)] public float Force582N { get; set; }
        [Key(21)] public float Force587F { get; set; }
        [Key(22)] public float Force587N { get; set; }
        [Key(23)] public float CleanForceN { get; set; }
        [Key(24)] public float AdsActualHz { get; set; }
        [Key(25)] public float AxisPos0 { get; set; }
        [Key(26)] public float AxisPos1 { get; set; }
        [Key(27)] public float AxisPos2 { get; set; }
        [Key(28)] public float AxisPos3 { get; set; }
        [Key(29)] public float AxisPos4 { get; set; }
        [Key(30)] public float AxisPos5 { get; set; }
        [Key(31)] public float AxisPos6 { get; set; }
        [Key(32)] public float AxisFromLeft0 { get; set; }
        [Key(33)] public float AxisFromLeft1 { get; set; }
        [Key(34)] public float AxisFromLeft2 { get; set; }
        [Key(35)] public float AxisFromLeft3 { get; set; }
        [Key(36)] public float AxisFromLeft4 { get; set; }
        [Key(37)] public float AxisFromLeft5 { get; set; }
        [Key(38)] public float AxisFromLeft6 { get; set; }
        [Key(39)] public bool HasNetDistance { get; set; }
        [Key(40)] public double Axis1NetDistanceMm { get; set; }
        [Key(41)] public double Axis6NetDistanceMm { get; set; }
        [Key(42)] public byte PrepareTaskState { get; set; }
        [Key(43)] public int PrepareTaskCommandId { get; set; }
        [Key(44)] public byte StartTaskState { get; set; }
        [Key(45)] public int StartTaskCommandId { get; set; }
        [Key(46)] public CommandResultFrame[] Results { get; set; } = new CommandResultFrame[0];
        [Key(47)] public string EventText { get; set; } = "";
        [Key(48)] public byte[] AuthTag { get; set; } = new byte[0];
    }

    [MessagePackObject]
    public sealed class ForceFrame
    {
        [Key(0)] public byte Version { get; set; }
        [Key(1)] public ulong SessionId { get; set; }
        [Key(2)] public ulong Sequence { get; set; }
        [Key(3)] public ulong EchoMainMonotonicMs { get; set; }
        [Key(4)] public bool Enable582 { get; set; }
        [Key(5)] public sbyte Axis582 { get; set; }
        [Key(6)] public float Force582N { get; set; }
        [Key(7)] public float Torque582Nm { get; set; }
        [Key(8)] public bool Enable587 { get; set; }
        [Key(9)] public sbyte Axis587 { get; set; }
        [Key(10)] public float Force587N { get; set; }
        [Key(11)] public float Torque587Nm { get; set; }
        [Key(12)] public byte[] AuthTag { get; set; } = new byte[0];
    }
}
