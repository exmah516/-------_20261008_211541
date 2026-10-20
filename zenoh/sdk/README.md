# 本地 SDK（锁定版本）

- Zenoh router：包根 zenoh/zenohd.exe，1.10.1。
- Zenoh-C：zenoh-c-1.10.1-msvc/include、lib/zenohc.dll.lib、bin/zenohc.dll；Windows x64 MSVC。
- Zenoh-C++：zenoh-cpp-1.10.1/include；1.10.1 官方头文件 API，基于 Zenoh-C，不单独提供第二套运行库。
- msgpack-cxx：msgpack-cxx-9.0.0/include，编译定义 MSGPACK_NO_BOOST。
- 主端 C#：MessagePack 3.1.12（NuGet），VI.Service.Net 为 net472 x64 P/Invoke，不使用 Zenoh-CS。

ADS.vcxproj 和主端工程从这些固定目录取依赖；构建会把 zenohc.dll 复制到两端输出目录，ADS 输出另有 zenohd.exe。
只部署输出目录时也必须带两端相同的 remote.token（可放 EXE 旁），不能只复制 EXE。

官方来源：
- https://github.com/eclipse-zenoh/zenoh-c/releases/tag/1.10.1
- https://github.com/eclipse-zenoh/zenoh-cpp/releases/tag/1.10.1
- https://github.com/eclipse-zenoh/zenoh/releases/tag/1.10.1
- https://github.com/msgpack/msgpack-c/tree/cpp-9.0.0

C++ 服务直接使用官方 C API 保持与 C# 同一 ABI；所需 C++ header SDK 同时保存在本目录。
