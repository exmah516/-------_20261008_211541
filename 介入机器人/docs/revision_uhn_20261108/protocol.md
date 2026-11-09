# H：设备会话号与主端

## 改动边界

调用链：VI.HandleHost 成功 openDevice → HostSample → HandleHostMemory → HandleService 缓存 → ControlFrame.HandleA/B → RemoteRobotLink → HandleFrame → ZenohWire HMAC/严格解码 → ZenohService.on_handle → remote_handle_bridge::Sample.device_epoch → Handle::poll。

每设备计数保存在主端监管器与共享内存头部。宿主每次成功打开该设备时 NextDeviceEpoch(slot) 加 1；重启前监管器读取两个计数，启动新宿主时种回共享内存；Stop/Start 同一服务也保留计数。原有宿主重启周期和 USB 事件路径不变。SDK 负状态原来只置 invalid，并不重新 openDevice；本次没有添加 SDK 重开路径。

HandleFrame 原先 24 项，现为 26 项：索引23 DeviceEpoch582（ulong），24 DeviceEpoch587（ulong），25 AuthTag。严格校验 ulong，签名包含这两项。PROTOCOL.md 已规定现有位置数组扩展保留版本1/Topic，此次遵循该惯例，不提供旧长度兼容。主端、VI.HandleHost、数据端必须一起部署。

## 验证

- 固定黄金向量使用非零且不同的7/9，C#、C++逐字节对拍。
- 两侧桥接保存7/9分别断言；伪造 epoch 导致原HMAC失效。
- 负数/字符串 epoch 拒绝。
- crash/stall/USB插入/USB拔出四类宿主重启，断言 epoch 增加1。
- 真实VI.HandleHost加载模拟FLCatheter.dll，双柄重开各自增加1；保留原V5清力、3s新鲜度、SDK缺失和200Hz断言。
- 验证主端结构HandleSample没有其他序列化边界/内存封送调用者；线上边界仍只走显式HandleFrame。

详见 test_counts.json、test_inventory.csv、zenoh.log、host_sdk/results.log。
