# Zenoh v1 固定黄金样本

本目录的 Hex 由独立 Python 编码器预制。C# 与 C++ 测试只读取样本，不在运行时生成预期值。

- handle.hex：顶层 24 字段，两个手柄各 8 字段完全扁平化。
- command.hex：15 字段，UTF-8 文本和 float64 参数。
- state.hex：49 字段、float32 状态、float64 净距离、两个任务与两项回执。
- force.hex：13 字段、float32 力/扭矩。
- hello.hex、authenticate.hex、challenge.hex：认证握手。
- invalid_*.hex：数组长度、类型、浮点宽度、非有限值、认证长度和字段范围错误。

整数按 MessagePack 最短格式编码；float32 固定使用 ca，float64 固定使用 cb。认证数据使用 BIN。每帧 HMAC 为末尾认证字段替换为 c400 后，对完整位置数组计算 HMAC-SHA256，并取前 16 字节。黄金令牌仅用于测试。

运行入口为 介入机器人/B数据端/工程/ADS项目/tests/run-zenoh-tests.ps1。测试使用本机回环端口 17447、内存控制快照和真实 Zenoh 原生库；不会访问 PLC、手柄或真实运动设备。编译产物统一输出至 ADS项目/tests/zenoh/.build。

2026-10-20 本机验证：C++ 协议测试 54 项通过，服务边界测试通过；C# 协议与原生跨语言测试 79 项通过。连续 3 秒测得 handle 100.0 Hz、force 100.0 Hz、state 30.3 Hz；同时验证 500 ms 连接判定、长中断重新认证、路由器自建退出及外部路由器复用保留。

2026-10-21 本机复验：最新 C# 协议、机器人服务与原生跨语言测试 109 项通过。连续 3 秒测得 handle 100.0 Hz、force 100.0 Hz、state 30.3 Hz；状态新鲜度/连接判定为严格小于 2000 ms，并验证短/长中断恢复、重新认证、同一链路重连和路由器所有权清理。

