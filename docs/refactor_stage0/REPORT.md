# 阶段 0：执行记录

## 0. 基线与范围

- 主仓库开始前快照：b3090075e6a396b3da7531bcd05d35dcf6e48d4e；嵌套仓库已有修改：77ac7ff5ba6aa9d5f688447c1878c7b3597a05ec。均为本地提交。
- 以当前磁盘源码为准。第一、二轮报告作为调查证据保留，不回写历史行号。
- 未连接设备、未下载或激活 PLC、未启动生产控制程序、未推送。

## 1. PLC 编译链

**已确认：脚本可运行；本机未能完成 PLC 编译。**

入口：tools/plc_build.ps1。使用 TcXaeShell.DTE.15.0，复制 C从端 到临时目录，打开 250902.sln，检查项目成功载入后才调用 SolutionBuild.BuildProject。只使用离线构建接口。输出 build.log 和 result.json；失败退出 1。

本次结果：项目 250902 返回 Kind={67294A52-A4F0-11D2-AA88-00C04F688DDE}，FullName 为空（未加载项目）。编译没有开始，错误数/警告数为未知（null），不能记作 0。详见 logs/plc/build.log、logs/plc/result.json。命令行 /Rebuild 也退出 1；原始 ActivityLog 保留于 logs/plc/activity.xml。Shell COM 可以创建，单独 AddFromFile 返回“无法找到指定的模板”(0x80041FE1)。

这不是已经证明存在的 ST 编译错误。需要本机 XAE 的 TwinCAT 工程/PLC 编译组件能成功加载该工程，之后重跑脚本。遵守第1节前置条件，本阶段暂停两个未编入工程的 PLC 文件删除。已编入工程的 POU 不改。

工程路径核查：250902 子目录没有 sln，实际解决方案为 C从端/250902.sln。解决方案还引用测量工程；脚本只 BuildProject 250902。

删除前标准离线套件 run_offline_tests.cjs all：11/11 程序成功。日志见 logs/before/。调研 F 已知的定位臂旧黄金/诊断失败保留为既有问题，不能通过改动断言掩盖。

## 2. 删除前行为基线

已确认：完整生产 main.cpp 主循环、真实 motion_sync/plc_io/force_feedback/Handle 编译到离线进程；仅时钟、ADS/Zenoh/记录文件/定位臂服务替换为内存端。不加载设备 DLL。34 个输入场景，27,197 个拍末快照，含7 refer、4电缸、fast、Req/clear mask、租约输出、49个主要状态字段。浮点用十六进制无损文本，逐字节比较。生成后第二次编译、运行全部相等；覆盖断言通过。正式源哈希见 tools/refactor_baseline/golden/manifest.json。

R2-8 的51条原断言已复制到 tools/refactor_baseline/behavior_probes.cpp，生产片段抽取运行通过；现有11个离线程序全部通过。正式黄金输入从未启用单手柄初始化，后续不需要更换输入。内存PLC按100Hz推进，模拟请求、Busy/Done/Error边沿、单调generation及失败结果；不宣称模拟TwinCAT的1ms轨迹动力学。各阶段内瞬时状态仍由原有专项断言覆盖。

运行：python tools/refactor_baseline/run.py；python tools/refactor_baseline/run_behavior_probes.py；node 介入机器人/B数据端/工程/ADS项目/tools/run_offline_tests.cjs all <日志目录>。

早期 810bafc 只保存现有套件清单；本节的完整黄金提交才完成逐拍基线。早期草稿黄金已从scratch清理，未作为正式预期值提交。

## 3. 逐组删除记录

### 3.1.1 单手柄

已确认：删除启动单柄锁定/3秒等待、重映射、单柄跟随、换向保护、ModeSelection及保存还原、相关内部命令RequestModeSwitch/SetReverseMode。输入固定587→轴1、582→轴6；缺任何一柄仍走原双柄保持。实验会话的single_handle_mode元数据也删除。

34场景/27,197拍逐值相等（logs/delete1/result.json），11个保留测试程序通过（logs/delete1_tests.log）。R2-8原51条中仅删除单柄分支的4条断言，47条保留断言通过（logs/delete1_probes.log）。删除dual_control_test的test_single_mapping_mirror、main_recovery_test的一条单柄恢复断言；B6与恢复双柄断言保留。抽取边界改用仍存在的片段。

### 3.1.2 导丝独立模式

已确认：删除guidewire_mode模块、独立crawl与专用同步/窗口锁定、四个非Dual计划回退模式及Axis1/Axis6重基准分支。Dual=5、Both=2保留原数值，公共状态mode仍发送0。Dual使用的rebase_axis1/rebase_dual、轴6基准/旋转/跟随窗口仍在。黄金34场景/27,197拍逐值相等，11个离线程序、47条R2断言通过（logs/delete2*）。删除两个单柄/独立窗口兼容断言和两个仅验证独立模式保存的断言；保留控制与同步断言。已编入PLC的接口不变。

### 3.1.3 追踪补偿

已确认：删除 delivery_tracking 模块、控制器及全部调用、换手欠账和两个无公共映射的内部参数命令。公共状态字段维持原布局，双柄下继续发送关闭、误差0、增益1。净距离仍使用的四电缸手动覆盖扫描保留并改名 manual_cylinder_override_active。黄金34场景/27,197拍逐值相等，11个离线程序和47条保留断言通过（logs/delete3*）；本组没有删除保留断言。

### 3.1.4 紧急回撤

已确认：删除 EmergencyRetractDevice 命令、固定夹持撤出动作、到位处理、速度备份/恢复及全部生产 emergency_retract 状态。删除测试中仅用于验证该门控的两次循环项，其他控制断言不变。抽取边界改用仍在的轴6限位入口。黄金34场景/27,197拍逐值相等，11个离线程序、47条保留断言通过（logs/delete4*）。

### 3.1.5 内部启动序列

已确认：删除 ExecuteStartup、标准/恢复启动序列模块、direct positioning 两阶段、SetStartupAxisPos/AxisDeg/Speed 与 pending_startup。公共 SelectDirectControl、startup.completed、StartupPhase::Done=11、PLC loading_ready 消费和自检后的重同步保留；协议中的 startup_phase/waiting/completed 字段保留。黄金34场景/27,197拍逐值相等，11个离线程序、47条保留断言通过（logs/delete5*）。电缸测试移除仅检查已删除启动占用门的场景，其他断言未改。

### 3.1.6 无公共映射的内部命令

已确认：清单余下10项内部命令及枚举删除；RequestModeSwitch/SetReverseMode、两项tracking命令已随前组删除。三个 StartControl 抽取边界统一换为仍存在的 default，保留断言不变。主端 ToggleForceFeedback 方法发送公共 SetForceFeedback，不引用被删内部枚举，无需修改。自动实验记录、物理恢复键、公共电缸Position/Reset、力反馈Set/Hold均保留。黄金34场景/27,197拍逐值相等，11个离线程序、47条保留断言通过（logs/delete6*）。

### 3.1.7 不可达退出块

已确认：while(true) 后的退出清理块删除，循环内无退出该无限循环的公共路径。黄金34场景/27,197拍逐值相等，11个离线程序、47条保留断言通过（logs/delete7*）。未增加退出机制。

### 3.1.8 残余状态与说明

已确认：删除只自清零/无读取的旧模式和启动字段、未调用的 PLC/坐标辅助、无调用者的旧软限位保持 lambda；当前 Follow 候选目标和计划回退目标的软限位校验仍在。电缸 Position 保留原数值 3。主端和数据端运动说明移除当前章节中的已删模块与流程，历史变更日志单独保留。黄金 34 场景/27,197 拍完全相等（logs/delete8_final/result.json）；全部现有离线套件退出 0，47 条保留 R2 断言通过。专项净距离测试通过（logs/delete8_distance.log）。PLC 两个未编入文件因离线编译未开始而暂停删除；已编入 POU 无改动。


## 4. 事件记录

已确认：提交fa79580使events.csv增加ads_snapshot_sequence列，session schema由8升到9；事件/原因集中在experiment_events.h。新增32个事件码，共34个；仅观察原有状态/通信结果，不改控制量。取消Clear的结果用一个仅记录序号继续观察；新增三项长驻记录状态。完整码表见事件码表.md，主端/数据端实验记录说明同步更新。

最终真实主循环事件检查：36场景、293项检查、1,155条事件，全部34个码出现；从事件重建五项逐拍状态与黄金逐拍一致。34个旧黄金场景仍27,197拍逐字节相等，未改输入或黄金。真实CSV序列化/schema9/新序号列也由recording_feedback_test通过。证据：logs/timeline_final_check/event_checks.json、result.json和logs/all_final_check/recording_feedback_test.log。

历史手柄/网络回放起初因旧标记和新增事件fixture字段缺失而失败。最终66dbcf2仅适配抽取/声明，控制断言未变，两类回放已通过；ef86365的中间恢复操作未改生产源码。日志分别保留在logs/regression和logs/replay_final。

## 5. 最终验收与规模

主端、宿主、数据端构建均0警告0错误；标准11测试、R2保留47断言、WPF 11621、managed协议178、C++协议84、跨语言回环200、归零链路5场景、网络6场景及第五轮恢复/ST均通过。定位臂仍有R1已列明的5项旧黄金/算法失败，不能声明全部既有测试全绿。完整表和日志见验证结果.md。

同一脚本统计：main删除前5424行、删除后964115a为3429行、加事件后3597行；main长驻可变声明165→117→120（对象/数组各计1）；内部命令含None为39→20。来源size_comparison.json及scratch/inventory.py。当前地图和声明表见B_main解剖_精简.md。

PLC编译仍未开始，两个PLC删除项暂停；状态30不可达块与旧接口保留。未下载/激活/连接设备或推送。未决事项见99_未决问题.md。本报告没有提出后续设计。
