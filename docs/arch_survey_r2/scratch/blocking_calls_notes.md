# blocking_calls.csv 导出范围与核验

本清单是 R2-9 的审计底稿，不是延迟预算。CSV 严格为 `位置、调用、最坏阻塞时长的依据` 三列，UTF-8 BOM，426 行数据。生成器为本目录 `export_blocking_calls.py`；机械校验为 `audit_blocking_calls.py`；逐候选结果见 `blocking_calls_audit.json`。

## 1. 范围

| 项目 | 边界与证据 |
|---|---|
| 主线程 | 已确认：数据端 `main()` 从初始化、主循环到退出和局部对象析构；入口 `介入机器人/B数据端/工程/ADS项目/main.cpp:37`，退出 `main.cpp:5404-5424`。不把后台 worker 的全部 I/O 称为主循环 I/O。 |
| 包装调用与底层原语 | 已确认：CSV 同时给主线程调用点、同步辅助调用和底层等待原语；这些行可能属于同一条调用链，不能相加。`sync_all` 与 `plc_io::write_refer` 最后均会进入低频请求等待，依据 `motion_sync.cpp:406-456`、`plc_io.cpp:330-344`、`ads_communication.cpp:559-583`。 |
| 当前无公共入口分支 | 已确认：内部 ExecuteStartup/导丝独立等仍有编译在 main 中的调用路径，CSV保留其阻塞语义并引用 R2-3，不把它们等同于当前UI可触发入口。入口证据见 `../R2-3_不可达与无入口.md` 首张清单及第2、3节。 |
| 所列显式等待 | 已确认：条件变量、Sleep、同步网络库调用、文件 I/O、控制台输出、无超时 mutex、线程创建/回收。每行包含位置和确定程度；无源码上界时标“未设上界／未知”。 |
| 非等待操作 | 已确认：原子load/store、当前快照的纯字段复制、数学/窗口裁剪不因名字含 ADS/command 就当作网络等待；原子回退命令队列详见下表。常规内存分配和时钟查询不在“显式阻塞调用”的枚举范围。 |

以上短路径均相对于项目根 `D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/`；CSV 的位置列使用完整绝对路径。

## 2. 有数值的等待与实际边界

| 路径 | 源码请求值 | 最坏墙钟时间能否由源码确定 |
|---|---:|---|
| 主循环等快照 | 300ms | 已确认：`main.cpp:56-57,969-970` → `ads_communication.cpp:247-252`。CV相对等待有300ms参数；初始取锁、唤醒后重新取锁、调度没有整次调用上界。 |
| 零点连续3帧 | 每帧500ms，最多3次；任一失败立即返回 | 已确认：`main.cpp:759-778`。最多提出1500ms的CV等待请求，不能写为整个函数保证1500ms以内。 |
| 退出等快照 | 50ms | 已确认：`main.cpp:5414`。相同mutex/调度边界。 |
| 单/双手柄均值采样 | 每样本Sleep(10) | 已确认：`control_types.cpp:29-51,54-91`。20/30样本会提出200/300ms睡眠；`Handle::poll`只取本地桥缓存的mutex，另有调度等待；不是网络采样等待。 |
| router启动循环 | 最多40×Sleep(100)，每轮还先`endpoint_ready(...,100)`；首探测150ms，末探测250ms | 已确认：`zenoh_service.cpp:270-308`。`endpoint_ready`先`getaddrinfo`，再对每个地址非阻塞connect/select；DNS未设超时、地址数不固定，因此不能把router启动总上界写成4秒或8.4秒。`zenoh_service.cpp:96-129`。 |
| router终止等待 | WaitForSingleObject(...,2000) | 已确认：`zenoh_service.cpp:311-322`。只给终止后的单次等待参数，不是整个stop的上界。 |
| 底层ADS | 快通道握手1000ms、运行300ms；低频端口300ms | 已确认：`ads_communication.cpp:1019,1060,2073`。这些是底层端口调用配置，不是主线程低频队列等待的截止时间。 |

## 3. 无上界路径

| 路径 | 事实与证据 |
|---|---|
| 低频ADS请求 | 已确认：`read/write/read_sum/write_sum`建请求，`submit_low_frequency_request`在Running或SoftHold接收排队，随后明确`(void)timeout_ms`并`request->cv.wait`。没有超时等待，`ads_communication.cpp:440-583`。实际最坏时长未知。 |
| 参数容易混淆处 | 已确认：`main.cpp:1396,1408,1425,1442`末尾35/7/3/2分别是符号数量count；它们省略timeout，默认250ms。`ads_communication.h:204-215`。`main.cpp:850`的100和`:1350`的50才是显式传入timeout。所有这些timeout随后均被丢弃。 |
| 停止与join | 已确认：`main.cpp:5416-5420`依次等手柄记录器、实验记录器、定位臂worker、ADS两个worker、Zenoh worker。全部是无超时join，`handle_velocity_recorder.h:45-60`、`experiment_recorder.cpp:593-619`、`arm_manual_ads_service.cpp:125-138`、`ads_communication.cpp:228-240`、`zenoh_service.cpp:194-229`。 |
| ArmManual退出依赖 | 已确认：stop先设置停止请求并join；worker停止分支仍调用update_cartesian与write_dirty_request，这些可等待低频ADS，`arm_manual_ads_service.cpp:525-529,641-642`；因此不能由50ms读取周期推导退出上界。 |
| 通知注销 | 已确认：注册表锁与回调共用；注销先取同一把锁排除回调，再同步注销ADS通知。没有“轮询回调计数”实现。`ads_communication.cpp:110-118,1965-1986`。 |
| mutex | 已确认：快照、输出、统计、事件、坐标缓存、桥缓存、ArmManual状态、Zenoh状态/命令队列、记录统计均使用不带超时的mutex。各锁逐位置列入CSV。它们本身不是网络等待。 |
| 文件 I/O | 已确认：寻参配置/记录读取、寻参原子写、ADS故障边沿日志、CSV启动、元数据写入均无完成截止时间。`homing_record.h:40-82`、`main.cpp:1236-1238,1745-1747`、`async_csv_writer.h:34-87`、`experiment_recorder.cpp:1024-1032`。实际最坏时长未知。 |
| token读取量 | 已确认：`load_token`先用istreambuf_iterator读到EOF，再判长度16..4096；不是最多读取4096字节。`zenoh_service.cpp:249-261`。 |
| 记录器启动与停止 | 已确认：start会join前一次stop线程、建目录/文件/表头flush、写初始JSON。stop_async正常新建线程后返回，但会先join旧stop线程；新建线程抛异常时本线程执行完整stop_worker。`experiment_recorder.cpp:457-548,563-619`。 |
| 控制台 | 已确认：main中141个含std::cout/std::cerr的源码行全部逐条入CSV；stdout目标及flush未设截止时间。同步helper中的输出也逐条保留。控制台实际阻塞时间未知。 |
| Zenoh会话/实体 | 已确认：main初始化同步调用z_open和publisher/subscriber声明，退出同步z_drop；这里没有显式调用超时。`zenoh_service.cpp:324-373`。库内部最坏耗时无法由仓库代码确定。 |

## 4. 没有误列为网络阻塞的路径

| 路径 | 事实与证据 |
|---|---|
| planned return提交/查询 | 已确认：submit只校验、复制到固定容量环队列并操作atomic；result只查询atomic槽，无mutex/CV/网络请求，`ads_communication.cpp:300-411`。Clear的compare_exchange_weak可因竞争重试，源码没有重试次数上限；这是CPU重试，非线程阻塞等待。 |
| generation/refresh请求 | 已确认：applied generation是atomic load；refresh/watchdog请求是atomic store，`ads_communication.cpp:290-298,414-417,435-438`。对应main调用位置在审计JSON的excluded数组。 |
| 常态记录enqueue | 已确认：只复制到环形缓冲并更新atomic，满则丢行；不做文件I/O，`async_csv_writer.h:115-132`。统计更新/健康检查可能取锁或触发停止，另列CSV。 |
| read_plc_state/read_force_sample | 已确认：当前main明确设置非空ctx.ads_service，走latest_snapshot内存副本，只有快照mutex；备用直连分支不在当前main运行路径，`main.cpp:207`、`plc_io.cpp:197-217,256-278`。 |
| 回退后rebase | 已确认：生产rebase直接使用当前滤波手柄值/缓存实际位置，无poll/Sleep，也不调用write_refer；`motion_sync.cpp:158-249,251-340`。常规sync_axis*/sync_all具有Sleep及低频写，已分开列。 |
| Zenoh状态/命令接口 | 已确认：publish_state/poll_command/command_processed只取本地mutex与队列；网络publish由worker执行，`zenoh_service.cpp:376-520,985-1017`。 |

## 5. 机械核验与限制

- 已确认：`audit_blocking_calls.py`校验三列名称、CSV每行无多余字段、每个位置在相应当前文件行数以内、所有行有确定程度标记。结果见 `blocking_calls_audit.json`。
- 已确认：main中按“可能等待的服务API/手柄init-poll-close-setforce/寻参文件操作/输出流”正则筛出210个候选行，210个都有CSV行；未覆盖候选为0。main的141个输出行全部覆盖。
- 已确认：辅助模块与退出析构通过人工阅读调用链补列；正则覆盖是候选核验，不是AST全程序调用图证明。CSV不提供测得的最坏执行时间。
- 未知：mutex竞争、磁盘/控制台/Zenoh库调用的实际最长耗时，以及现场系统的调度尾延迟；这些不能由本轮离线源码审计得到数值。
