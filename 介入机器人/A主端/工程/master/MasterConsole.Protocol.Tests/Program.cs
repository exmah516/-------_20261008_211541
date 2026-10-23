using System;
using System.IO;
using System.Linq;
using System.Text;
using MasterConsole.Protocol;

namespace MasterConsole.Protocol.Tests
{
    internal static partial class Program
    {
        private const ulong Session = 0x1122334455667788UL;
        private static int _passed;
        private static int _failed;

        [STAThread]
        private static int Main(string[] args)
        {
            try
            {
                string fixtures = args.Length > 0 ? args[0] : FindFixtures();
                byte[] key = ReadAuth(fixtures, "session_key");
                Golden(fixtures, "handle", NewHandle(), key);
                Golden(fixtures, "command", NewCommand(), key);
                Golden(fixtures, "state", NewState(), key);
                Golden(fixtures, "force", NewForce(), key);
                var hello = ZenohWire.Deserialize<CommandFrame>(ReadHex(fixtures, "hello"));
                Check("Hello 固定挑战字段", hello.Command == ZenohCommand.Hello && hello.NonceClient.Length == 16 && hello.SessionId == 0);
                Check("Hello 字节一致", ZenohWire.Serialize(hello).SequenceEqual(ReadHex(fixtures, "hello")));
                var auth = ZenohWire.Deserialize<CommandFrame>(ReadHex(fixtures, "authenticate"));
                byte[] token = Encoding.ASCII.GetBytes("zenoh-golden-token");
                Check("挑战 HMAC 黄金值", FrameAuth.ComputeAuth(token, auth.NonceClient, auth.NonceServer).SequenceEqual(auth.Proof));
                Check("会话密钥黄金值", FrameAuth.DeriveSessionKey(token, auth.NonceClient, auth.NonceServer).SequenceEqual(key));
                Check("认证帧字节一致", ZenohWire.Serialize(auth).SequenceEqual(ReadHex(fixtures, "authenticate")));
                var challenge = ZenohWire.Deserialize<StateFrame>(ReadHex(fixtures, "challenge"));
                Check("挑战状态字节一致", challenge.AuthState == 1 && ZenohWire.Serialize(challenge).SequenceEqual(ReadHex(fixtures, "challenge")));
                foreach (string path in Directory.GetFiles(fixtures, "invalid_*.hex").OrderBy(p => p))
                {
                    string name = Path.GetFileNameWithoutExtension(path);
                    byte[] bytes = FrameAuth.FromHex(File.ReadAllText(path));
                    if (name.StartsWith("invalid_handle_")) Reject<HandleFrame>(name, bytes);
                    else if (name.StartsWith("invalid_command_")) Reject<CommandFrame>(name, bytes);
                    else if (name.StartsWith("invalid_state_")) Reject<StateFrame>(name, bytes);
                    else if (name.StartsWith("invalid_force_")) Reject<ForceFrame>(name, bytes);
                    else throw new InvalidDataException(name);
                }
                byte[] frameBytes = ReadHex(fixtures, "handle");
                Reject<HandleFrame>("截断二进制", frameBytes.Take(frameBytes.Length - 1).ToArray());
                var bad = NewForce();
                bad.Force582N = float.NaN;
                CheckThrows("发送端拒绝非有限值", () => ZenohWire.Serialize(bad));
                byte[] wrongKey = (byte[])key.Clone();
                wrongKey[0] ^= 1;
                Check("错误密钥拒绝", !ZenohWire.Verify(ZenohWire.Deserialize<HandleFrame>(frameBytes), wrongKey));
                Check("空密钥拒绝", !ZenohWire.Verify(ZenohWire.Deserialize<HandleFrame>(frameBytes), new byte[0]));
                RobotServiceTests();
                if (args.Contains("--network")) NetworkSmoke(args, key);
            }
            catch (Exception e)
            {
                _failed++;
                Console.Error.WriteLine(e);
            }
            Console.WriteLine("C# 协议测试：通过 " + _passed + "，失败 " + _failed);
            return _failed == 0 ? 0 : 1;
        }

        private static void Golden<T>(string dir, string name, T sample, byte[] key)
        {
            byte[] expected = ReadHex(dir, name);
            byte[] tag = ZenohWire.Sign(sample, key);
            SetTag(sample, tag);
            byte[] bytes = ZenohWire.Serialize(sample);
            Check(name + " 编码与固定 Hex 逐字节一致", bytes.SequenceEqual(expected));
            T decoded = ZenohWire.Deserialize<T>(expected);
            Check(name + " 解码后重新编码一致", ZenohWire.Serialize(decoded).SequenceEqual(expected));
            Check(name + " 认证黄金值", ZenohWire.Verify(decoded, key));
            byte[] before = ZenohWire.Serialize(decoded);
            ZenohWire.Sign(decoded, key);
            ZenohWire.Verify(decoded, key);
            Check(name + " 认证不改变对象", before.SequenceEqual(ZenohWire.Serialize(decoded)));
            byte[] changed = (byte[])expected.Clone();
            changed[changed.Length - 1] ^= 1;
            Check(name + " 篡改认证拒绝", !ZenohWire.Verify(ZenohWire.Deserialize<T>(changed), key));
        }

        private static void SetTag<T>(T sample, byte[] tag)
        {
            if (sample is HandleFrame h) h.AuthTag = tag;
            else if (sample is CommandFrame c) c.AuthTag = tag;
            else if (sample is StateFrame s) s.AuthTag = tag;
            else if (sample is ForceFrame f) f.AuthTag = tag;
            else throw new ArgumentException("未知协议类型");
        }

        private static HandleFrame NewHandle() => new HandleFrame
        {
            Version = 1, SessionId = Session, Sequence = 100, MonotonicMs = 123456,
            Buttons582 = 5, Valid582 = true, Encoder582_0 = 1234, Encoder582_1 = -5678,
            Joint582_0 = .5f, Joint582_1 = -.25f, Velocity582_0 = 1.5f, Velocity582_1 = -2.5f,
            Buttons587 = 2, Valid587 = true, Encoder587_0 = -1, Encoder587_1 = 2,
            Joint587_0 = 1, Joint587_1 = 2, Velocity587_0 = 0, Velocity587_1 = .125f,
            Injector1Direction = 1, Injector2Direction = -1, Axis4Direction = 0
        };
        private static CommandFrame NewCommand() => new CommandFrame
        {
            Version = 1, SessionId = Session, Sequence = 101, MonotonicMs = 123457,
            CommandId = 42, Command = ZenohCommand.PreparePosition, Param1 = -1, Param2 = 2,
            Value1 = 12.25, Value2 = 125.5, Payload = "准备"
        };
        private static StateFrame NewState() => new StateFrame
        {
            Version = 1, SessionId = Session, Sequence = 102, AuthState = 2,
            NonceServer = Enumerable.Range(16, 16).Select(i => (byte)i).ToArray(), EchoMainMonotonicMs = 123456,
            Flags = 913, Mode = 0, Phase = 1, SelfCheckStatus = 4, AdsState = 2,
            Cylinder1 = 2000, Cylinder2 = 10, Cylinder3 = 2000, Cylinder4 = 10, CylinderManualMask = 5,
            Injector1Direction = 1, Injector2Direction = -1, Axis4Direction = 0,
            Force582F = .25f, Force582N = .01f, Force587F = .5f, Force587N = -.02f, CleanForceN = .3f, AdsActualHz = 100,
            AxisPos0 = 1, AxisPos1 = 2, AxisPos2 = 3, AxisPos3 = 4, AxisPos4 = 5, AxisPos5 = 6, AxisPos6 = 7,
            AxisFromLeft0 = 11, AxisFromLeft1 = 12, AxisFromLeft2 = 13, AxisFromLeft3 = 14, AxisFromLeft4 = 15, AxisFromLeft5 = 16, AxisFromLeft6 = 17,
            HasNetDistance = true, Axis1NetDistanceMm = 12.25, Axis6NetDistanceMm = -3.125,
            PrepareTaskState = 2, PrepareTaskCommandId = 40, StartTaskState = 1, StartTaskCommandId = 41,
            Results = new[] { new CommandResultFrame { CommandId = 39, Result = 0 }, new CommandResultFrame { CommandId = 38, Result = 1, Reason = "拒绝" } },
            EventText = "ready"
        };
        private static ForceFrame NewForce() => new ForceFrame
        {
            Version = 1, SessionId = Session, Sequence = 103, EchoMainMonotonicMs = 123456,
            Enable582 = true, Axis582 = 1, Force582N = .5f, Torque582Nm = .01f,
            Enable587 = false, Axis587 = 0, Force587N = 0, Torque587Nm = 0
        };

        private static void Reject<T>(string name, byte[] bytes) => CheckThrows(name, () => ZenohWire.Deserialize<T>(bytes));
        private static void CheckThrows(string name, Action action)
        {
            try { action(); Check(name, false); }
            catch (Exception) { Check(name, true); }
        }
        private static void Check(string name, bool valid)
        {
            if (valid) _passed++; else _failed++;
            Console.WriteLine((valid ? "[通过] " : "[失败] ") + name);
        }
        private static byte[] ReadHex(string dir, string name) => FrameAuth.FromHex(File.ReadAllText(Path.Combine(dir, name + ".hex")));
        private static byte[] ReadAuth(string dir, string key) => FrameAuth.FromHex(File.ReadAllLines(Path.Combine(dir, "auth.txt"))
            .Single(l => l.StartsWith(key + "=")).Substring(key.Length + 1));
        private static string FindFixtures()
        {
            for (string path = AppDomain.CurrentDomain.BaseDirectory; path != null; path = Path.GetDirectoryName(path))
            {
                string candidate = Path.Combine(path, "protocol", "fixtures", "zenoh-v1");
                if (Directory.Exists(candidate)) return candidate;
            }
            throw new DirectoryNotFoundException("找不到 protocol/fixtures/zenoh-v1。");
        }
        static partial void NetworkSmoke(string[] args, byte[] key);
    }
}

