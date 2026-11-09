using System;
using System.Security.Cryptography;
using System.Text;

namespace MasterConsole.Protocol
{
    /// <summary>
    /// 握手认证：token 不在链路上传输，双方用 nonce 做 HMAC 挑战，并派生 Zenoh 帧认证密钥。
    /// 见 protocol/PROTOCOL.md 第 4.1 节。
    /// </summary>
    public static class FrameAuth
    {
        public static byte[] NewNonce()
        {
            byte[] n = new byte[16];
            using (var rng = RandomNumberGenerator.Create()) rng.GetBytes(n);
            return n;
        }

        /// <summary>auth = HMAC-SHA256(token, "auth" ‖ nonce_c ‖ nonce_s)，整段 32 字节。</summary>
        public static byte[] ComputeAuth(byte[] token, byte[] nonceC, byte[] nonceS)
            => Hmac(token, "auth", nonceC, nonceS);

        /// <summary>session_key = HMAC-SHA256(token, "key" ‖ nonce_c ‖ nonce_s) 的前 16 字节。</summary>
        public static byte[] DeriveSessionKey(byte[] token, byte[] nonceC, byte[] nonceS)
        {
            byte[] full = Hmac(token, "key", nonceC, nonceS);
            byte[] key = new byte[16];
            Buffer.BlockCopy(full, 0, key, 0, 16);
            return key;
        }

        public static string ToHex(byte[] data)
        {
            var sb = new StringBuilder(data.Length * 2);
            foreach (byte b in data) sb.Append(b.ToString("x2"));
            return sb.ToString();
        }

        public static byte[] FromHex(string hex)
        {
            hex = (hex ?? "").Trim();
            if (hex.Length % 2 != 0) throw new FormatException("hex 长度必须为偶数");
            byte[] data = new byte[hex.Length / 2];
            for (int i = 0; i < data.Length; i++)
                data[i] = Convert.ToByte(hex.Substring(i * 2, 2), 16);
            return data;
        }

        private static byte[] Hmac(byte[] token, string label, byte[] nonceC, byte[] nonceS)
        {
            byte[] lab = Encoding.ASCII.GetBytes(label);
            byte[] msg = new byte[lab.Length + nonceC.Length + nonceS.Length];
            Buffer.BlockCopy(lab, 0, msg, 0, lab.Length);
            Buffer.BlockCopy(nonceC, 0, msg, lab.Length, nonceC.Length);
            Buffer.BlockCopy(nonceS, 0, msg, lab.Length + nonceC.Length, nonceS.Length);
            using (var h = new HMACSHA256(token)) return h.ComputeHash(msg);
        }
    }

}
