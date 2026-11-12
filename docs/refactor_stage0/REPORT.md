# 阶段 0：执行记录

## 0. 基线与范围

- 主仓库开始前快照：6b5ac267199508ab951d5b8f8bdb8d34933d0447；嵌套仓库已有修改：77ac7ff5ba6aa9d5f688447c1878c7b3597a05ec。均为本地提交。
- 以当前磁盘源码为准。第一、二轮报告作为调查证据保留，不回写历史行号。
- 未连接设备、未下载或激活 PLC、未启动生产控制程序、未推送。

## 1. PLC 编译链

**已确认：脚本可运行；本机未能完成 PLC 编译。**

入口：tools/plc_build.ps1。使用 TcXaeShell.DTE.15.0，复制 C从端 到临时目录，打开 250902.sln，检查项目成功载入后才调用 SolutionBuild.BuildProject。只使用离线构建接口。输出 build.log 和 result.json；失败退出 1。

本次结果：项目 250902 返回 Kind={67294A52-A4F0-11D2-AA88-00C04F688DDE}，FullName 为空（未加载项目）。编译没有开始，错误数/警告数为未知（null），不能记作 0。详见 logs/plc/build.log、logs/plc/result.json。命令行 /Rebuild 也退出 1；原始 ActivityLog 保留于 logs/plc/activity.xml。Shell COM 可以创建，单独 AddFromFile 返回“无法找到指定的模板”(0x80041FE1)。

这不是已经证明存在的 ST 编译错误。需要本机 XAE 的 TwinCAT 工程/PLC 编译组件能成功加载该工程，之后重跑脚本。遵守第1节前置条件，本阶段暂停两个未编入工程的 PLC 文件删除。已编入工程的 POU 不改。

与继续摘要的差异：250902 子目录确实没有 sln，但 C从端/250902.sln 存在。使用该实际解决方案。解决方案还引用测量工程；脚本只 BuildProject 250902。

删除前标准离线套件 run_offline_tests.cjs all：11/11 程序成功。日志见 logs/before/。调研 F 已知的定位臂旧黄金/诊断失败保留为既有问题，不能通过改动断言掩盖。
