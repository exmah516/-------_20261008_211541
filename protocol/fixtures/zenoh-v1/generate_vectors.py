"""独立生成固定黄金样本；测试只读取 Hex，不在测试期间重新生成。"""
from pathlib import Path
import hashlib
import hmac
import struct

ROOT = Path(__file__).resolve().parent

class Float32(float):
    pass

def encode(value):
    if isinstance(value, bool):
        return b"\xc3" if value else b"\xc2"
    if isinstance(value, Float32):
        return b"\xca" + struct.pack(">f", value)
    if isinstance(value, float):
        return b"\xcb" + struct.pack(">d", value)
    if isinstance(value, int):
        if 0 <= value < 128:
            return bytes([value])
        if -32 <= value < 0:
            return bytes([value & 255])
        formats = [(0, 255, b"\xcc", ">B"), (0, 65535, b"\xcd", ">H"),
                   (0, 4294967295, b"\xce", ">I"), (0, 18446744073709551615, b"\xcf", ">Q"),
                   (-128, -1, b"\xd0", ">b"), (-32768, -1, b"\xd1", ">h"),
                   (-2147483648, -1, b"\xd2", ">i"), (-9223372036854775808, -1, b"\xd3", ">q")]
        for minimum, maximum, marker, fmt in formats:
            if minimum <= value <= maximum:
                return marker + struct.pack(fmt, value)
        raise ValueError(value)
    if isinstance(value, bytes):
        return (b"\xc4" + struct.pack(">B", len(value)) if len(value) <= 255 else
                b"\xc5" + struct.pack(">H", len(value))) + value
    if isinstance(value, str):
        raw = value.encode("utf-8")
        return (bytes([0xa0 + len(raw)]) if len(raw) < 32 else
                b"\xd9" + bytes([len(raw)]) if len(raw) < 256 else b"\xda" + struct.pack(">H", len(raw))) + raw
    if isinstance(value, list):
        return (bytes([0x90 + len(value)]) if len(value) < 16 else
                b"\xdc" + struct.pack(">H", len(value))) + b"".join(map(encode, value))
    raise TypeError(type(value))

def write(name, value):
    (ROOT / (name + ".hex")).write_text(encode(value).hex() + "\n", encoding="utf-8")

token = b"zenoh-golden-token"
nc, ns = bytes(range(16)), bytes(range(16, 32))
proof = hmac.digest(token, b"auth" + nc + ns, "sha256")
key = hmac.digest(token, b"key" + nc + ns, "sha256")[:16]
sid = 0x1122334455667788
f = Float32
samples = {
    "handle": [1, sid, 100, 123456, 5, True, 1234, -5678, f(.5), f(-.25), f(1.5), f(-2.5),
               2, True, -1, 2, f(1), f(2), f(0), f(.125), 1, -1, 0, b""],
    "command": [1, sid, 101, 123457, 42, 4, -1, 2, 12.25, 125.5, "准备", b"", b"", b"", b""],
    "state": [1, sid, 102, 2, ns, 123456, 913, 0, 1, 4, 2, 2000, 10, 2000, 10, 5, 1, -1, 0,
              f(.25), f(.01), f(.5), f(-.02), f(.3), f(100),
              *map(f, [1, 2, 3, 4, 5, 6, 7]), *map(f, [11, 12, 13, 14, 15, 16, 17]),
              True, 12.25, -3.125, 2, 40, 1, 41, [[39, 0, ""], [38, 1, "拒绝"]], "ready", b""],
    "force": [1, sid, 103, 123456, True, 1, f(.5), f(.01), False, 0, f(0), f(0), b""],
}
for name, value in samples.items():
    value[-1] = hmac.digest(key, encode(value), "sha256")[:16]
    write(name, value)

write("hello", [1, 0, 1, 1000, 1, 0, 0, 0, 0.0, 0.0, "", nc, b"", b"", b""])
write("authenticate", [1, sid, 2, 1001, 2, 1, 0, 0, 0.0, 0.0, "", nc, ns, proof, b""])
challenge = samples["state"].copy()
challenge[2], challenge[3], challenge[-1] = 1, 1, b""
write("challenge", challenge)

for name, value in samples.items():
    write("invalid_" + name + "_length", value[:-1])
    value = value.copy()
    value[-1] = b"\x00" * 15
    write("invalid_" + name + "_auth", value)

bad = samples["handle"].copy()
bad[8] = .5
write("invalid_handle_float64", bad)
bad[8] = f(float("nan"))
write("invalid_handle_nan", bad)
bad[8], bad[20] = f(.5), 2
write("invalid_handle_direction", bad)
bad = samples["handle"].copy()
bad[23] = "\x00" * 16
write("invalid_handle_string_tag", bad)
bad = samples["handle"].copy()
bad[4] = 256
write("invalid_handle_integer_range", bad)
bad = samples["command"].copy()
bad[8] = f(12.25)
write("invalid_command_float32", bad)
bad[8] = float("inf")
write("invalid_command_inf", bad)
bad = samples["command"].copy()
bad[11] = nc
write("invalid_command_nonce", bad)
bad = samples["command"].copy()
bad[1] = 0
write("invalid_command_session", bad)
bad = samples["command"].copy()
bad[2] = 0
write("invalid_command_sequence", bad)
bad = samples["command"].copy()
bad[5] = 255
write("invalid_command_kind", bad)
bad = samples["state"].copy()
bad[46] = [[i + 1, 0, ""] for i in range(33)]
write("invalid_state_results", bad)
bad = samples["state"].copy()
bad[46] = [[0, 0, ""]]
write("invalid_state_result_id", bad)
bad = samples["state"].copy()
bad[42] = 4
write("invalid_state_task", bad)
bad = samples["force"].copy()
bad[6] = f(float("-inf"))
write("invalid_force_inf", bad)
bad = samples["force"].copy()
bad[4] = 1
write("invalid_force_boolean", bad)

(ROOT / "auth.txt").write_text(
    "token_ascii=" + token.decode("ascii") + "\nnonce_client=" + nc.hex() +
    "\nnonce_server=" + ns.hex() + "\nauth_mac=" + proof.hex() +
    "\nsession_key=" + key.hex() + "\n", encoding="utf-8")
(ROOT / "invalid_handle_map.hex").write_text("80\n", encoding="utf-8")
(ROOT / "invalid_handle_trailing.hex").write_text(encode(samples["handle"]).hex() + "00\n", encoding="utf-8")
print(f"Created {len(list(ROOT.glob('*.hex')))} fixed Hex fixtures.")

