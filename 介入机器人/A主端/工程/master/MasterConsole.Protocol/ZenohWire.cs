using System;
using System.Buffers;
using System.Security.Cryptography;
using MessagePack;

namespace MasterConsole.Protocol
{
    /// <summary>四个位置数组的唯一编码边界；拒绝宽松转换、非有限数值与错误认证长度。</summary>
    public static class ZenohWire
    {
        private static readonly MessagePackSerializerOptions Options = MessagePackSerializerOptions.Standard
            .WithSecurity(MessagePackSecurity.UntrustedData);

        public static byte[] Serialize<T>(T value)
        {
            ValidateValue(value);
            byte[] bytes = MessagePackSerializer.Serialize(value, Options);
            ValidateBytes<T>(bytes);
            return bytes;
        }

        public static T Deserialize<T>(byte[] bytes)
        {
            ValidateBytes<T>(bytes);
            T value = MessagePackSerializer.Deserialize<T>(bytes, Options);
            ValidateValue(value);
            return value;
        }

        public static byte[] SigningBytes<T>(T frame)
        {
            byte[] bytes = Serialize(frame);
            var reader = new MessagePackReader(new ReadOnlySequence<byte>(bytes));
            int count = reader.ReadArrayHeader();
            for (int i = 0; i < count - 1; ++i) reader.Skip();
            int length = checked((int)reader.Consumed);
            var result = new byte[length + 2];
            Buffer.BlockCopy(bytes, 0, result, 0, length);
            result[length] = MessagePackCode.Bin8;
            result[length + 1] = 0;
            return result;
        }

        public static byte[] Sign<T>(T frame, byte[] key)
        {
            if (key == null || key.Length != 16) throw Invalid("会话密钥必须为 16 字节");
            using (var hmac = new HMACSHA256(key))
            {
                byte[] hash = hmac.ComputeHash(SigningBytes(frame));
                var result = new byte[16];
                Buffer.BlockCopy(hash, 0, result, 0, result.Length);
                return result;
            }
        }

        public static bool Verify<T>(T frame, byte[] key)
        {
            byte[] tag = Tag(frame);
            if (tag == null || tag.Length != 16 || key == null || key.Length != 16) return false;
            byte[] expected = Sign(frame, key);
            int difference = 0;
            for (int i = 0; i < expected.Length; ++i) difference |= expected[i] ^ tag[i];
            return difference == 0;
        }

        private static void ValidateBytes<T>(byte[] bytes)
        {
            if (bytes == null || bytes.Length == 0 || bytes.Length > 65536) throw Invalid("载荷长度错误");
            string schema;
            if (typeof(T) == typeof(HandleFrame)) schema = "bqqqbBiiffffbBiiffffsssx";
            else if (typeof(T) == typeof(CommandFrame)) schema = "bqqqibiiddtxxxx";
            else if (typeof(T) == typeof(StateFrame)) schema = "bqqbxquiiii" + "hhhhbsss" + "ffffffffffffffffffff" + "Bdd" + "bibi" + "rtffx";
            else if (typeof(T) == typeof(ForceFrame)) schema = "bqqqBsffBsffx";
            else throw Invalid("仅支持四个介入机器人协议数组");
            var reader = new MessagePackReader(new ReadOnlySequence<byte>(bytes));
            if (reader.ReadArrayHeader() != schema.Length) throw Invalid("数组字段数量错误");
            foreach (char type in schema) Read(ref reader, type);
            if (!reader.End) throw Invalid("载荷含多余字节");
        }

        private static void Read(ref MessagePackReader reader, char type)
        {
            switch (type)
            {
                case 'b': Unsigned(ref reader, byte.MaxValue); return;
                case 'h': Unsigned(ref reader, ushort.MaxValue); return;
                case 'u': Unsigned(ref reader, uint.MaxValue); return;
                case 'q': Unsigned(ref reader, ulong.MaxValue); return;
                case 'i': Signed(ref reader, int.MinValue, int.MaxValue); return;
                case 's': Signed(ref reader, sbyte.MinValue, sbyte.MaxValue); return;
                case 'B':
                    if (reader.NextMessagePackType != MessagePackType.Boolean) throw Invalid("需要布尔值");
                    reader.ReadBoolean(); return;
                case 'f':
                    if (reader.NextCode != MessagePackCode.Float32) throw Invalid("需要 float32");
                    float single = reader.ReadSingle();
                    if (float.IsNaN(single) || float.IsInfinity(single)) throw Invalid("非有限 float32");
                    return;
                case 'd':
                    if (reader.NextCode != MessagePackCode.Float64) throw Invalid("需要 float64");
                    double number = reader.ReadDouble();
                    if (double.IsNaN(number) || double.IsInfinity(number)) throw Invalid("非有限 float64");
                    return;
                case 't':
                    if (reader.NextMessagePackType != MessagePackType.String) throw Invalid("需要字符串");
                    var text = reader.ReadStringSequence();
                    if (!text.HasValue || text.Value.Length > 4096) throw Invalid("字符串过长");
                    return;
                case 'x':
                    if (reader.NextMessagePackType != MessagePackType.Binary) throw Invalid("需要 BIN 字节数组");
                    var binary = reader.ReadBytes();
                    if (!binary.HasValue || binary.Value.Length > 32) throw Invalid("认证数据过长");
                    return;
                case 'r':
                    int count = reader.ReadArrayHeader();
                    if (count > 32) throw Invalid("命令结果超过 32 项");
                    for (int i = 0; i < count; ++i)
                    {
                        if (reader.ReadArrayHeader() != 3) throw Invalid("命令结果字段数量错误");
                        Read(ref reader, 'i'); Read(ref reader, 'b'); Read(ref reader, 't');
                    }
                    return;
            }
            throw Invalid("未知字段类型");
        }

        private static void Unsigned(ref MessagePackReader reader, ulong maximum)
        {
            if (reader.NextMessagePackType != MessagePackType.Integer || reader.ReadUInt64() > maximum)
                throw Invalid("无符号整数类型或范围错误");
        }

        private static void Signed(ref MessagePackReader reader, long minimum, long maximum)
        {
            if (reader.NextMessagePackType != MessagePackType.Integer) throw Invalid("需要整数");
            long value = reader.ReadInt64();
            if (value < minimum || value > maximum) throw Invalid("整数超出范围");
        }

        private static void ValidateValue<T>(T value)
        {
            if (value is HandleFrame h)
            {
                Header(h.Version, h.SessionId, h.Sequence); TagLength(h.AuthTag);
                Direction(h.Injector1Direction); Direction(h.Injector2Direction); Direction(h.Axis4Direction);
            }
            else if (value is CommandFrame c)
            {
                if (c.Version != ZenohProtocol.Version || c.Sequence == 0 || c.CommandId < 0 || (byte)c.Command > 17)
                    throw Invalid("命令头错误");
                Length(c.NonceClient, 0, 16); Length(c.NonceServer, 0, 16); Length(c.Proof, 0, 32); TagLength(c.AuthTag);
                if (c.Command == ZenohCommand.Hello)
                {
                    if (c.SessionId != 0) throw Invalid("Hello 不得携带已认证会话");
                    Length(c.NonceClient, 16); Length(c.NonceServer, 0); Length(c.Proof, 0); Length(c.AuthTag, 0);
                }
                else if (c.Command == ZenohCommand.Authenticate)
                {
                    if (c.SessionId == 0) throw Invalid("认证会话为空");
                    Length(c.NonceClient, 16); Length(c.NonceServer, 16); Length(c.Proof, 32); Length(c.AuthTag, 0);
                }
                else
                {
                    if (c.SessionId == 0) throw Invalid("控制命令会话为空");
                    Length(c.NonceClient, 0); Length(c.NonceServer, 0); Length(c.Proof, 0);
                }
            }
            else if (value is StateFrame s)
            {
                if (s.Version != ZenohProtocol.Version || s.AuthState > 2 || s.PrepareTaskState > 3 || s.StartTaskState > 3 ||
                    s.PrepareTaskCommandId < 0 || s.StartTaskCommandId < 0) throw Invalid("状态字段错误");
                Length(s.NonceServer, 0, 16); TagLength(s.AuthTag);
                if (s.AuthState != 0) { Header(s.Version, s.SessionId, s.Sequence); Length(s.NonceServer, 16); }
                Direction(s.Injector1Direction); Direction(s.Injector2Direction); Direction(s.Axis4Direction);
                if (s.Results == null || s.Results.Length > 32) throw Invalid("命令结果数量错误");
                foreach (var result in s.Results)
                    if (result == null || result.CommandId <= 0 || result.Result > 1) throw Invalid("命令结果错误");
            }
            else if (value is ForceFrame f)
            {
                Header(f.Version, f.SessionId, f.Sequence); TagLength(f.AuthTag); Axis(f.Axis582); Axis(f.Axis587);
            }
            else throw Invalid("不支持的协议类型");
        }

        private static byte[] Tag<T>(T value)
        {
            if (value is HandleFrame h) return h.AuthTag;
            if (value is CommandFrame c) return c.AuthTag;
            if (value is StateFrame s) return s.AuthTag;
            if (value is ForceFrame f) return f.AuthTag;
            return null;
        }

        private static void Header(byte version, ulong session, ulong sequence)
        {
            if (version != ZenohProtocol.Version || session == 0 || sequence == 0) throw Invalid("协议版本、会话或序号错误");
        }
        private static void TagLength(byte[] value) => Length(value, 0, 16);
        private static void Direction(sbyte value) { if (value < -1 || value > 1) throw Invalid("方向范围错误"); }
        private static void Axis(sbyte value) { if (value < 0 || value > 2) throw Invalid("手柄 SDK 轴范围错误"); }
        private static void Length(byte[] value, int a, int b = -1)
        { if (value == null || (value.Length != a && value.Length != b)) throw Invalid("认证数据长度错误"); }
        private static MessagePackSerializationException Invalid(string message) => new MessagePackSerializationException(message);
    }
}
