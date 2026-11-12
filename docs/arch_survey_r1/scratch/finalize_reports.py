"""Finalize survey prose and fully spell evidence references; only survey files are written."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'docs/arch_survey_r1'
ADS = ROOT / '介入机器人/B数据端/工程/ADS项目'
MASTER = ROOT / '介入机器人/A主端/工程/master'

overview = '''# 第一轮架构调研总览

调研日期：2026-11-12。对象为 `D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/` 的当前磁盘内容，HEAD为 `1651af5636204f9c748ac9ee53a6537ae63949b6`。**已确认**：开始时已有大量 staged/unstaged 修改，报告没有用HEAD替代工作区；初始状态和文件哈希见 `scratch/baseline.json:1-4`。

引用前缀均相对于上述根：`D/`=`介入机器人/B数据端/工程/ADS项目/`；`M/`=`介入机器人/A主端/工程/master/`；`P/`=`介入机器人/C从端/250902/Untitled2/POUs/`；`U/`=`介入机器人/C从端/250902/Untitled2/`。其余引用使用根目录相对路径；行号含空行。**已确认**指代码、历史或测试直接证明；**推断**指静态推理；**未知**均列明缺失证据。离线通过不代表机械动作已验证。

## 结论摘要

**已确认**：当前三端分别为.NET 8 WPF及独立手柄宿主、C++数据端、TwinCAT 3 PLC。数据端 `main.cpp` 有5424行；主循环等待ADS快线程提供快照，目标频率100Hz，但同步取样和低频ADS调用会阻塞主线程，不能据目标频率认定主循环稳定100Hz。PLC的1ms PlcTask调用MAIN；工程编入13个POU，MAIN执行固定前置调用与gen_state分发，ArmCartJogGen由ArmCartJog间接使用。出处：`M/VI.Control/VI.Control.csproj:1-24`、`M/VI.HandleHost/VI.HandleHost.csproj:1-15`、`D/main.cpp:37,966-978,5402-5424`、`D/ads_communication.cpp:15-21,559-586`、`D/control_types.cpp:29-92`、`U/PlcTask.TcTTO:3-9`、`U/Untitled2.plcproj:33-70`、`P/MAIN.TcPOU:35-204`、`P/ArmCartJog.TcPOU:24,96-98`。

**功能一：本体轴1先行——功能正常（已确认，限当前实现和离线回归范围）。** 默认前10mm名义递送额外先行2mm，单/双手柄均有路径；仅轴6换手保留轴1进度。既有测试验证10mm输入→轴1走12mm、支撑走10mm、停手不补走及换向连续。**未知**：现场部署版本和机械位移。出处：`D/control_types.h:132-136`、`D/main.cpp:2484-2485,3064-3072,3787-3844,4053-4090`、`D/tools/dual_control_test.cpp:645-801`、`scratch/tests_native/dual_control_test.log:3,17`。

**功能二：按键进入屈曲恢复——条件变得更苛刻，功能仍有可达路径（已确认，限代码和离线测试）。** 入口是物理SN587满足 `(buttons2 & 0x07) == 0x07` 的上升沿，须持续按住；B7不是当前入口。回退中请求须等原回退交接完成，另受启动、控制、连接保持、500ms恢复静置、几何及行程门控。**推断**：按住跨保持清理可能缺少新的上升沿。**未知**：真实键帽编码及完整Active机械动作。出处：`D/main.cpp:1189-1201,1300-1307,1503-1575,1598-1605,2083-2084,3238-3294`、`D/手柄/Handle.h:6`、`D/tools/return_recovery_test.cpp:317-359`、提交 `5d738e6`、`f3353facc386049839fefbbd23863397a9b937ee`。

## 最重要的15条发现

| # | 发现 | 确定程度与出处 |
|---:|---|---|
| 1 | 当前磁盘包含调研前的未提交改动。最终基线核验未发现原有受跟踪文件内容变化；未把该检查夸大为所有untracked文件均已哈希核验。 | **已确认**：`scratch/baseline.json:1-4`、`scratch/readonly_verification.json:1-3`、`scratch/survey.py:63-71`。 |
| 2 | 当前公共Zenoh只有4个话题，Version=1；Handle/Force目标100Hz、State约30Hz、Command按需。 | **已确认**：`M/MasterConsole.Protocol/ZenohFrames.cs:5-12`、`M/VI.Service.Robot/Services/RemoteRobotLink.cs:380-426`、`D/zenoh_service.cpp:993-1017`。 |
| 3 | 手柄宿主和主端编译同一个共享内存布局文件；采样区偏移128、力区512，采用seqlock，Buttons原样传递。 | **已确认**：`M/VI.HandleHost/VI.HandleHost.csproj:10`、`M/VI.Service.Robot/Services/HandleHostMemory.cs:31-70`、`M/VI.HandleHost/Program.cs:69-93`。 |
| 4 | main有5424行，主while为966–5402，无正常退出分支；最大局部lambda是415行step_planned_return。 | **已确认**：`D/main.cpp:966,2662-3076,5402-5424`、`scratch/main_spans.json:1-14`。 |
| 5 | main等快照最长300ms，无新快照仍执行后续逻辑；ADS快线程完整读写持续失败达到1s进入重连。 | **已确认**：`D/main.cpp:969-978,1063-1068`、`D/ads_communication.cpp:16-20,923-1005`。 |
| 6 | 状态跨main长生命周期声明、多个结构体、lambda引用及线程缓存保存。B逐项列出181条main长生命周期声明和424项成员声明，包含常量/配置，不能直接相加当作独立可变状态数。 | **已确认**：`D/main.cpp:179-379,479-548,806-953`、`B_main解剖.md:390-399,1011`；精确引用索引在B后半部分。 |
| 7 | 计划回退由数据端组织夹爪稳定、异步Prepare/Commit/Clear、Busy/Done/Error、清Req和重建基准；夹爪等待从输出generation已应用后起算，部分等待无独立总期限。 | **已确认**：`D/main.cpp:2529-3076`；PLC对应 `P/handle.TcPOU:579-703`。 |
| 8 | PLC已承担平滑、扫描限速、软限位、电缸、轴4、注射器和同步轴；1ms是配置目标，实际抖动未知。 | **已确认（能力/配置）**：`U/PlcTask.TcTTO:3-9`、`P/handle.TcPOU:475-571,706-957`、`P/InjectorManual.TcPOU:23-109`、`P/Axis2Sync.TcPOU:19-89`；**未知（现场调度）**：未取得在线测量。 |
| 9 | PLC CASE30内又要求return_state不等于30，使653–663的锁存/重建块不可达。该代码矛盾来自df776f3；现场影响未知，不能据此判两项功能失效。 | **已确认（矛盾/历史）**：`P/handle.TcPOU:645-663`、提交 `df776f3691ed15e936b590bec128c0b0d5fc404d`；**未知（影响）**：缺在线状态/输出时间线。 |
| 10 | ADS快线程自行递增主机心跳；main只是发布最新期望输出。故PLC的2s主机看门狗不能覆盖每一种main业务停顿。 | **已确认（写入责任）/推断（监控边界）**：`D/ads_communication.cpp:934-935,1619-1635`、`D/main.cpp:4522`、`P/MAIN.TcPOU:68-97`。 |
| 11 | 定位臂调用传250ms，但低频ADS API忽略调用者timeout、cv.wait无总上限；底层ADS请求另设300ms。50ms轮询目标不构成执行期限。 | **已确认**：`D/arm_manual_ads_service.cpp:13-17,315,642,718`、`D/ads_communication.cpp:559-586,2044-2115`。 |
| 12 | 实验记录进入主控制循环前默认尝试以auto开启；auto是普通会话名。JSON实际键为experiment_name_raw/cleaned，records根取cwd；另有exe旁速度CSV。 | **已确认**：`D/main.cpp:66-91,953-964`、`D/experiment_recorder.cpp:399-446,901-905`。 |
| 13 | 短暂hold会清轴1先行剩余段，连接恢复和重基准本身不arm；因此恢复后可能感觉没有先行。 | **已确认（清理/入口）/推断（现场感受）**：`D/main.cpp:347-358,874-881,1300-1325,1579-1596,3064-3072`。 |
| 14 | 屈曲恢复与spacing_recovery是同一流程；Active保持轴1/2/7，按axis1输入正增量推动3/5/6等量增加。双柄实际输入来自587，旧注释/日志仍称582。 | **已确认**：`D/control_types.h:141,387-437`、`D/main.cpp:54-55,2091-2104,3322-3388`。 |
| 15 | 标准native11/11、额外可靠性5组、主端/协议/回环6组通过；定位臂round4九个EXE中4通过、5失败。生产构建0警告/0错误，PLC只做静态/翻译检查；未取得现场周期、RTT、缺帧和换手阶段耗时。 | **已确认（测试/构建）**：`F_测试现状.md:22-85,109-126`、`scratch/tests/arm/round4/results.json:1-29`；**未知（现场时序）**：`C_时间敏感动作.md:430-439`。 |

## 各部分结论

| 部分 | 结论与证据入口 |
|---|---|
| A 工程全貌 | **已确认**：工程/构建/输出、线程、共享内存、四种完整帧、ADS快速/低频/通知符号、配置和全部CSV列已列出。旧数据端协议文档55字段与当前StateFrame57字段不一致。证据：`M/MasterConsole.Protocol/ZenohFrames.cs:84-144`、`介入机器人/B数据端/协议/PROTOCOL.md:71-131`；完整盘点见A。 |
| B main解剖 | **已确认**：一拍依次推进快照、保持/输入、状态机、输出、记录/状态、命令；无“缺帧直接continue”。planned/crawl/handoff/spacing/startup等经共享状态和清标志耦合。证据：`D/main.cpp:966-1068,2083-2089,4471-4522,4608-4869,5402`；逐状态/变量与互斥关系见B。 |
| C 时间敏感动作 | **已确认**：PLC已有内部回退/平滑等执行能力，上位机仍组织夹爪交接、回退交接和恢复。历史CSV可统计记录间隔，但四套位置均无效，不能代替主循环/机械时序。证据：`D/main.cpp:2529-3076`、`P/handle.TcPOU:475-703`、`scratch/existing_records_stats.json:1`。 |
| D 超时与恢复 | **已确认**：手柄250/1500/5000ms作用不同；ADS/业务/PLC各自计时；存在250ms业务超时短于300ms底层超时、32/64位tick混用和若干无总期限等待。**推断**的连锁已与实测故障分开标记。证据：`D/手柄/手柄.cpp:52-103`、`D/main.cpp:613,649-654,2090,2760`、`D/ads_communication.cpp:1059-1066`；逐项表和“谁监视谁”见D。 |
| E 功能核查 | **已确认**：轴1先行离线正常；屈曲恢复仍可达、条件比早期严格。**未知**：某次现场未动作的唯一提交归因，缺当时按键/hold/阶段/PLC记录。源码、历史差异短摘录和测试边界见E。 |
| F 测试现状 | **已确认**：离线结果、5组既有失败及历史归属已保存；不能从断言数量推导覆盖率。构建证据：`scratch/tests/ads_build.log:25-28`、`scratch/tests/master_build.log:12-15`；既有失败记录：`介入机器人/docs/revision_round7_20261109/REPORT.md:147-151`。 |

## 两项功能的动作与依赖

**轴1先行（已确认）**：它是已武装的Follow映射，不是回退后自主走2mm。单柄选择导管递送，或合格的轴1回退完成，才arm；导丝、撤出、control关闭、hold、急停、启动、spacing、软限位等会清。完成先行差额吸收到轴3/5镜像基准，停手、清除或撤出不会让支撑轴追补该差额。出处：`D/main.cpp:347-358,874-896,1579-1596,3064-3072,3787-3844,4053-4090`。

**屈曲恢复（已确认）**：SN587低三位0x07上升沿登记、松开撤销；没有只限双柄的条件，SN587单柄有入口。ClampWait本地等150ms，不等待夹爪反馈；Active每步受40mm/s、dt最多50ms及共同剩余限制。松键后ExitSync至少100ms，误差≤0.2mm或满2s后才尝试20点同步；同步失败保留阶段重试。与“净距离恢复”是同一状态，不能当两个流程。出处：`D/main.cpp:140-147,1505-1575,3215-3388,4300-4343`、`D/control_types.h:387-437`。

**推断**：连接保持清spacing状态但未清独立pressed_prev，持续按住跨保持可能需要松开再按；现有测试没有完整证明这一现场行为。**未知**：0x07来自一枚复合编码键还是多键，需硬件/SDK键位表或无运动采样；完整物理按键→Zenoh→Active夹持/运动也未验证。出处：`D/main.cpp:1058,1300-1307,1508-1575`、`D/tools/run_offline_tests.cjs:74-89`、`D/tools/return_recovery_test.cpp:80,317-359`。

## 附件索引与只读核验

| 文件 | 内容 |
|---|---|
| [A_工程全貌.md](A_工程全貌.md) | 目录/构建/规模/线程、Zenoh/ADS/共享内存、配置、记录器。 |
| [B_main解剖.md](B_main解剖.md) | 分块、一拍顺序、模式/全部流程、状态变量逐项索引、耦合点。 |
| [C_时间敏感动作.md](C_时间敏感动作.md) | 动作序列、PLC POU/GVL清单、输出/反馈、既有记录统计。 |
| [D_超时与恢复.md](D_超时与恢复.md) | 三端阈值、到期/恢复条件、监视关系和可能连锁。 |
| [E_功能核查.md](E_功能核查.md) | 两项功能的触发链、依赖、历史差异、离线证明范围。 |
| [F_测试现状.md](F_测试现状.md) | 测试清单、结果、5组既有失败、覆盖缺口、构建/静态检查。 |
| [99_未决问题.md](99_未决问题.md) | 未确定事实、所缺证据、下一轮5个问题。 |

**已确认（本轮执行边界）**：未连接真实手柄/ADS/PLC、未启动生产ADS.exe、未下载PLC或让机器人运动；Fake SDK宿主、内存PLC与127.0.0.1回环仅用于既有离线测试。未修改源码/配置/测试，未提交或推送；产物仅写本目录。原有受跟踪文件内容核验结果为零变化：`scratch/readonly_verification.json:1-3`，检查实现 `scratch/survey.py:63-71`。离线执行边界见 `F_测试现状.md:7-16`。

测试原始日志、历史diff、统计、基线及机械引用校验结果均在 `scratch/`；引用校验只证明文件/行号存在及表格结构，不替代语义审阅。本报告不包含设计或重构方案。
'''

unresolved = '''# 未决问题

调研日期：2026-11-12。本清单记录本轮不能由当前源码、历史或离线日志确定的事实；不提出设计或修改方案。路径根为 `D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/`；`D/`=`介入机器人/B数据端/工程/ADS项目/`，`M/`=`介入机器人/A主端/工程/master/`，`P/`=`介入机器人/C从端/250902/Untitled2/POUs/`，`U/`=`介入机器人/C从端/250902/Untitled2/`，`K/`=`介入机器人/B数据端/工程/PositioningArmKinematics/`。

| # | 未决事实、确定程度 | 当前证据与边界 | 需要什么才能确定 |
|---:|---|---|---|
| 1 | **未知**：三端现场部署是否与工作区一致。 | 仓库同时有源码、exe和Boot，不能证明设备运行版本；读取AppName也不能唯一识别源码。`D/ads_communication.cpp:1070-1088`、`U/Untitled2.plcproj:20-70`；工作区版本见 `scratch/baseline.json:1-4`。 | 主端/数据端部署文件hash、PLC在线AppName/编译指纹、TMC/Boot版本、下载与启动记录。 |
| 2 | **已确认代码矛盾，未知现场后果**：PLC handle CASE30的锁存/重建不可达块。 | CASE30内要求return_state!=30，653–663不能执行。`P/handle.TcPOU:645-663`；引入提交 `df776f3691ed15e936b590bec128c0b0d5fc404d`。不是MAIN的gen_state=30。 | 与现场Boot匹配的ST/编译指纹、return_state进入/退出与轴反馈的Scope记录、该状态的动作期望。 |
| 3 | **未知**：SN587恢复键的键帽与低三位编码来源。 | 只确认 `(buttons2 & 0x07) == 0x07`，0x47也满足；注释称一枚物理键，不能据此认定是三个键。`D/control_types.h:150-151`、`D/main.cpp:1505-1513`。 | 键位表、SDK/固件说明，或不启动机器人运动的原始getSwitch采样。 |
| 4 | **推断存在边沿窗口，未知现场表现**：按住跨断流/重连后能否重新进入恢复。 | 500ms连续新样本静置会触发hold清理；spacing.reset不改外部pressed_prev，边沿每拍更新。`D/main.cpp:1189-1201,1300-1307,1508-1575`、`D/control_types.h:395-437`、提交 `f3353facc386049839fefbbd23863397a9b937ee`。 | 同步记录Buttons587、Valid、epoch、sample_age、resume_settling、handle_soft_hold、requested、phase、pressed_prev、ADS和connection hold。 |
| 5 | **代码已确认，端到端未知**：屈曲Active的实际夹持稳定与三轴步进。 | ClampWait本地150ms，不等夹爪到位/ADS applied；Active由axis1输入差分驱动3/5/6。当前runner只抽取部分进入/退出。`D/main.cpp:3322-3388`、`D/tools/run_offline_tests.cjs:74-89`、`D/tools/return_recovery_test.cpp:80,150-160`。 | ADS generation、四电缸反馈、三轴实际位置、每拍phase/输入同一时间线，或完整动作分支的无硬件夹具证据。 |
| 6 | **离线已确认，现场未知**：轴1先行的机械效果和实际配置。 | 默认2mm、前10mm输入；主端公共协议无lead设置，旧文档仍写1mm/命令24。`D/control_types.h:132-139`、`D/tools/dual_control_test.cpp:645-704`、`M/MasterConsole.Protocol/ZenohFrames.cs:14-23`、`介入机器人/A主端/工程/docs/architecture/运动流程说明.md:341`。 | 部署hash和配置、名义输入、axis1/3/5实际位置与夹爪阶段的同步记录。 |
| 7 | **未知**：主循环周期、ADS RTT和快照缺失分布。 | 目标100Hz/等待300ms/成功窗1s是代码阈值；历史CSV无main起止，RTT和失败计数为null。`D/ads_communication.cpp:15-21,923-1005`、`D/records/20261110/084418_auto/session.json:29-37`、`D/records/20261110/084418_auto/motion.csv:1`。 | 快线程QPC起止、ADS读写RTT、attempt/snapshot序号、队列drop与main消费时间的实际记录。 |
| 8 | **未知**：一次完整换手各阶段实际耗时。 | 代码有按generation起算的夹稳和新Busy/Done事件，现有records无有效轴位/阶段事件线；不能用模型周期代替。`D/main.cpp:2529-3076`、`D/records/20261108/092851_auto/events.csv:1`、`D/records/20261110/084418_auto/events.csv:1`、`scratch/existing_records_stats.json:1`。 | planned_return phase、夹爪命令/反馈、PLC state及ADS generation的同源事件记录。 |
| 9 | **部分已确认，单位和抖动未知**：NC任务周期。 | PlcTask的1000µs明确；NC XML仅给10000/50000，按100ns解释为1/5ms是推断。`U/PlcTask.TcTTO:3-9`、`介入机器人/C从端/250902/250902.tsproj:1451-1465`。 | TwinCAT在线任务属性、NC/PLC Scope时间戳和实时诊断。 |
| 10 | **未知**：Boot与当前POU源码是否一致，当前PLC能否完整编译。 | 有Boot归档，但本轮ST翻译/静态检查不等于TwinCAT编译，也未下载验证。`U/Untitled2.plcproj:20-70`、`scratch/tests/arm/round4/st_static_audit_round4.log:63`。 | 对应源码的TwinCAT构建日志、TMC/Boot编译hash与在线启动记录。 |
| 11 | **代码关系已确认，现场联动未知**：Zenoh断流时控制权与力输出组合。 | 状态500ms停止发布、主端2s失效、宿主3s力tick各自独立；force worker不直接使用state500ms门。`D/zenoh_service.cpp:767-769,773-785,993-1028,1112-1132`、`M/VI.Service.Robot/Services/RemoteRobotLink.cs:87-110,506-527`、`M/VI.HandleHost/Program.cs:104-140`。 | 三端同一单调时间基准的state/force/handle/session/hold记录。 |
| 12 | **启动尝试已确认，最终完整性未知**：记录器auto会话如何正常收尾。 | 进入主控制循环前尝试创建auto；启动写JSON，stop写final；main while无正常出口，现有session未final。`D/main.cpp:953-966,5402-5424`、`D/experiment_recorder.cpp:546,602-611,884-907`、`D/records/20261110/084418_auto/session.json:6-7`。 | 实际退出路径/外部进程管理、最终session.json、writer错误和磁盘空间记录。 |
| 13 | **调用边界已确认，实际阻塞未知**：定位臂慢队列和本体快通道是否连锁。 | 读目标50ms；调用传250ms却cv.wait无总上限，底层300ms；状态年龄200ms另检查。快慢端口/线程分开但共享设备路由。`D/arm_manual_ads_service.cpp:13-17,333-355`、`D/ads_communication.cpp:559-586,2044-2115`。 | 慢队列持续占用时的快RTT、队列长度、快照年龄和主循环状态时间线。 |
| 14 | **代码存在，运行参与程度未知**：tracking补偿。 | PI/夹爪150ms/欠账结构存在；公开Zenoh没有补偿参数入口；return_recovery夹具tracking为空替身。`D/delivery_tracking.h:42-70,92,188`、`D/protocol/cpp/zenoh_protocol.h:14-24`、`D/tools/return_recovery_test.cpp:80`。 | 实际启用参数、invalid_reason、名义/补偿/实际增量的专门离线或现场记录。 |
| 15 | **失败已确认，唯一源码归因未知**：定位臂5组既有失败。 | home274 mismatch、旧jog/round4黄金不符、diagnostics B/S1/S2/S4、Y1/S4；历史报告关联新零位/30°/s/旧边界，未逐byte排除全部其它原因。`scratch/tests/arm/round4/results.json:1-29`、`介入机器人/docs/revision_round7_20261109/REPORT.md:147-151`、`介入机器人/docs/revision_round7_smooth_20261109/REPORT.md:73-78`。 | 每个失败场景输入、首次差异字段、黄金版本与源码变更的逐项对照；现场NC限速配置。 |
| 16 | **推断，长期影响未知**：32/64位tick混用超过约49.7天的行为。 | submit使用GetTickCount64，主动作推进用GetTickCount后宽化相减；没有长期运行测试。`D/main.cpp:613,649-654,2090,2760`、`D/control_types.h:303-315`。 | 跨DWORD回绕和超过2^32msuptime的离线时钟注入结果；部署机实际uptime。 |

## 下一轮最值得深入调研的5个问题

1. **在线版本是否一致？** 三端部署hash、PLC App/Boot/TMC与当前源码的映射是什么？对应上表1、10。
2. **完整换手的真实时间线是什么？** 手柄输入、main phase、ADS generation、PLC gen_state/return_state、夹爪反馈和轴位的时间关系如何？对应2、7、8。
3. **屈曲恢复的物理键及断流重入事实是什么？** 无运动采样能否确认0x07来源；既有保持清理后，按住/松开再按分别触发哪些阶段？对应3–5。
4. **PLC CASE30不可达块在现场Boot路径中有何后果？** 实际进入、停稳、重建及退出输出是什么？对应2、10。
5. **三个时间层的真实最坏延迟与触发顺序是什么？** main、ADS快/慢、Zenoh和记录writer的延迟/丢样，如何对应当前阈值和长期时钟边界？对应7、9、11–13、16。

这些问题是待确定的事实，不构成本轮设计、修改或现场操作授权。
'''

assert len(overview.splitlines()) < 600
assert len(list(OUT.glob('*.md'))) == 8
(OUT/'00_总览.md').write_text(overview, encoding='utf-8')
(OUT/'99_未决问题.md').write_text(unresolved, encoding='utf-8')

a_path = OUT/'A_工程全貌.md'
a = a_path.read_text(encoding='utf-8-sig')
a = a.replace('未运行主端、手柄宿主或任何现场连接程序。', '未启动联机主端或加载真实SDK的手柄宿主；既有离线测试运行了Fake SDK宿主和模拟主端，执行边界见 `F_测试现状.md:7-16,52-56`。')
a = a.replace('GVLs/G.TcGVL:132`、`D/main.cpp:427-467`', 'GVLs/G.TcGVL:73`、`D/main.cpp:427-467`')
a = a.replace('D/experiment_recorder.cpp:531-544,602-611,884-907', 'D/experiment_recorder.cpp:546,602-611,884-907')

wire_source = (MASTER/'MasterConsole.Protocol/ZenohFrames.cs').read_text(encoding='utf-8-sig').splitlines()
def wire_rows(class_name, meanings):
    active = False
    rows = []
    for lineno, line in enumerate(wire_source, 1):
        if f'public sealed class {class_name}' in line:
            active = True
            continue
        if active and line.strip() == '}':
            break
        if active:
            m = re.search(r'\[Key\((\d+)\)\] public (\S+) (\w+) ', line)
            if m:
                key, typ, name = m.groups()
                rows.append(f'| {key} | `{name}` | `{typ}` | {meanings[name]} | `ZenohFrames.cs:{lineno}` |')
    assert len(rows) == len(meanings), (class_name, len(rows), len(meanings))
    return '| Key | 字段名 | 类型 | 含义 | 出处（均已确认） |\n|---:|---|---|---|---|\n' + '\n'.join(rows)

common = {'Version':'线协议版本，当前1', 'SessionId':'认证会话ID', 'Sequence':'本话题会话内单调序号', 'MonotonicMs':'主端本地单调毫秒；用于回显，不能直接与另一电脑tick相减'}
handle = dict(common)
for serial in (582, 587):
    handle[f'Buttons{serial}'] = f'SN{serial}原始按键位，不在主端重排'
    handle[f'Valid{serial}'] = f'SN{serial}样本有效位'
    for axis in (0, 1):
        handle[f'Encoder{serial}_{axis}'] = f'SN{serial}第{axis}通道编码器值，原SDK整数'
        handle[f'Joint{serial}_{axis}'] = f'SN{serial}第{axis}关节值；数据端按配置映射，线协议无单位换算'
        handle[f'Velocity{serial}_{axis}'] = f'SN{serial}第{axis}通道SDK速度；SDK单位未在仓库注明'
    handle[f'DeviceEpoch{serial}'] = f'SN{serial}每次成功重开递增的epoch，数据端据变化触发静置/重基准'
handle.update({'Injector1Direction':'注射器1持续方向，-1/0/+1', 'Injector2Direction':'注射器2持续方向，-1/0/+1', 'Axis4Direction':'轴4持续手动方向，-1/0/+1', 'AuthTag':'会话HMAC前16字节；初始默认空数组'})
command = dict(common)
command.update({'CommandId':'主端分配的按需请求ID，结果按ID关联', 'Command':'命令号，底层byte；完整枚举见下', 'Param1':'命令相关整数参数1', 'Param2':'命令相关整数参数2', 'Value1':'命令相关浮点参数1', 'Value2':'命令相关浮点参数2', 'Payload':'字符串参数，默认空；线校验最多4096字符', 'NonceClient':'Hello/Authenticate客户端随机数，16字节', 'NonceServer':'挑战随机数，16字节', 'Proof':'Authenticate的HMAC证明，32字节', 'AuthTag':'会话HMAC前16字节；握手前默认空数组'})
start = a.index('| Key | 类型 | 含义 |', a.index('### HandleFrame 字段'))
end = a.index('\n\n来源：', start)
a = a[:start] + wire_rows('HandleFrame', handle) + a[end:]
start = a.index('| Key | 类型 | 含义 |', a.index('### CommandFrame 与命令枚举'))
end = a.index('\n\n命令：', start)
a = a[:start] + wire_rows('CommandFrame', command) + a[end:]

# Expand context-only `:line` shorthand, keeping the immediately preceding file.
file_name = re.compile(r'[A-Za-z0-9_\u4e00-\u9fff./\\()+-]+\.(?:cpp|cjs|cs|h|TcPOU|TcTTO|TcGVL|TcDUT|tsproj|plcproj|vcxproj|csproj|slnx|ps1|cmd|py|md|log|txt|json|csv|xaml|props|targets)(?![A-Za-z0-9])')
def spell_context_refs(text):
    output = []
    for line in text.splitlines():
        for m in reversed(list(re.finditer(r'`:(\d[^`]*)`', line))):
            names = list(file_name.finditer(line[:m.start()]))
            if names:
                name = names[-1][0]
                line = line[:m.start()] + f'`{name}:{m[1]}`' + line[m.end():]
        output.append(line)
    return '\n'.join(output) + '\n'
a = spell_context_refs(a)
a_path.write_text(a, encoding='utf-8')

c_path = OUT/'C_时间敏感动作.md'
c = c_path.read_text(encoding='utf-8-sig')
c = spell_context_refs(c)
script = 'docs/arch_survey_r1/scratch/analyze_existing_records.py:84'
diagnostic_rows = [
    ('介入机器人/B数据端/工程/ADS项目/x64/Release/ads_fault_diagnostics.log', '3 / 3', '每行 local192.168.0.160.1.1:851 read-state错误6；fallback169.254.119.135.1.1:851错误1861；timeout1000ms', '1,3'),
    ('介入机器人/B数据端/工程/ADS项目/x64/Debug/ads_fault_diagnostics.log', '4,687 / 4,687', '每行同上两项错误与1000ms', '1,4687'),
    ('ADS项目/ADS项目/x64/Debug/ads_fault_diagnostics.log', '40 / 40', '前8行同上；后32行包含 AdsPortOpenEx failed', '1,8,9,40'),
]
for path, count, failures, nums in diagnostic_rows:
    row = f'| `{path}` | {count} | {failures} | 0 | **已确认**：`{path}:{nums}`；扫描脚本 `{script}` |'
    prefix = f'| `{path}` |'
    assert sum(line.startswith(prefix) for line in c.splitlines()) == 1, prefix
    c = '\n'.join(row if line.startswith(prefix) else line for line in c.splitlines()) + '\n'
c_path.write_text(c, encoding='utf-8')

e_path = OUT/'E_功能核查.md'
e_path.write_text(spell_context_refs(e_path.read_text(encoding='utf-8-sig')), encoding='utf-8')
print('Finalized overview, unresolved questions, frame fields and explicit references.')
