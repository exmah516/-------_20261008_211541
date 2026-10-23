using System;

namespace MasterConsole.Protocol
{
    /// <summary>界面与手柄的本地超时设置。</summary>
    public static class ProtocolConstants
    {
        /// <summary>主端 3 s 未收到触觉帧即把手柄力输出清零。</summary>
        public const int HapticTimeoutMs = 3000;
        /// <summary>主端 2 s 未收到有效状态帧即视为数据过期。</summary>
        public const int StatusTimeoutMs = 2000;
    }

    /// <summary>StatusFrame.flags 位定义。</summary>
    [Flags]
    public enum StatusFlags : uint
    {
        None = 0,
        ControlActive = 1u << 0,
        EstopHold = 1u << 1,
        SelfCheckDone = 1u << 2,
        FfEnabled = 1u << 3,
        CalZeroed = 1u << 4,
        HostCommTimeout = 1u << 5,
        YValveClosed = 1u << 6,
        LeaseHeld = 1u << 7,
        StartupCompleted = 1u << 8,
        AdsHealthy = 1u << 9,
        FfZeroing = 1u << 10,
        GlobalReverse = 1u << 11,
        CleanForceValid = 1u << 12,
    }
}
