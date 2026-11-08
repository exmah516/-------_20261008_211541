# 第五轮补充：四项变化与结果

更新日期：2026-11-08。本报告仅记录本次四项补充；生产 PLC 源码没有新增改动。

## 1. R4：任一拔出通知同时暂停两柄

HandleService.DeviceChanged(FALSE) 现在同步清空 582／587 的采样缓存和两路力目标，设置宿主重启请求。重启等待期间不接收新的力目标，也不再用旧宿主缓存覆盖 invalid。保持主端原有的 10 ms HandleFrame 发送。

监管线程先将共享内存中的双路力目标清零并设置 Stop，使正常宿主沿既有退出路径对两柄执行原生零力；若宿主阻塞，复用既有 1 s 退出等待后终止进程。两次实际启动仍至少间隔 1 s；原生调用仍只在宿主进程中执行。系统通知继续按已核实的接口类注册，不再要求把接口路径映射到 582／587。

V5 使用生产 VI.HandleHost 加载模拟 SDK，并从测试入口注入相同的拔出／插入回调；没有连接或加载真实手柄 SDK。最终一次运行：

| 测量／断言 | 结果 |
|---|---:|
| 拔出回调返回时两柄均 invalid | 通过，同步断言 |
| 拔出通知到模拟 SDK 两路力及扭矩均为零 | 6.4 ms |
| 拔出触发的新宿主：Process.Start 前计时至主端缓存两柄均有效 | 45.8 ms |
| 插入触发的新宿主：同口径至两柄均有效 | 52.0 ms |
| 宿主采样频率 | 200.0 Hz |
| SDK／监管独立测试运行 | 151 通过，0 失败 |

启动到恢复时间用 Stopwatch 高分辨率时钟测量，包含进程创建、运行时启动、模拟 SDK 打开和共享内存／主端轮询。它不包含“距上一次启动不足 1 s”时必须等待的剩余间隔，也不是实机 USB 枚举／驱动恢复耗时。

仍覆盖崩溃、1.5 s 卡住、插入、拔出、单柄 SDK 状态离线、3 s 力新鲜度、DLL／入口缺失，以及每种监管故障下 10,000 次缓存操作不等待原生调用。额外确认拔出前两路力均非零、拔出后清零、重启不补发旧力或重启期间提交的力。

命令：`python -X utf8 tools/reliability_round5/run_host_sdk.py`。
证据：[host_run.log](host_run.log)、[宿主完整结果](../host_sdk/results.log)。真实 USB 拔插仍需现场确认；本次已消除原先“无法区分单柄而停止实施”的软件缺口。

## 2. R3：100 Hz 接收速度记录与待代入计算

新增 HandleVelocityRecorder，运行时始终开启，不新增配置开关。每次程序运行在 ADS.exe 同目录生成 `handle_velocity_<启动TickCount64>.csv`，启动时打印完整路径。独立 100 Hz 采样线程只读 remote_handle_bridge，复用既有 AsyncCsvWriter 异步写盘，不调用 Handle::poll、不修改样本时间戳、力输出、控制基准或控制条件；退出时排空写入并打印每柄两关节的绝对最大速度、丢行数和写盘错误码。

每行包含：相对记录时间、两柄接收时刻／有效位、原始四个 SDK 速度字段，以及四个关节量接收差分速度。失效值写 NaN，不把重复缓存当作新的差分速度。

SDK 头文件只声明 getEncVel，没有单位说明。因此保留原始速度而不假定它等于关节单位每秒；另外按同一来源的 getJoints 数据计算。差分值采用接收时间口径，含网络调度和时钟量化影响，不等同于SDK内部物理瞬时速度：

`v[s,j] = (joint[s,j](新) − joint[s,j](旧)) / ((rx_ms(新) − rx_ms(旧)) / 1000)`。

令 `M[s,j] = max(|v[s,j]|)`。j=0 的单位为程序所用关节0量／s，j=1 为关节1量／s，直接与源码映射配套。若现场先确认 SDK 速度单位及比例，也可将标定后的 SDK 速度最大值代入。

停顿 1.5 s 的估计追赶量：

`Δjoint[s,j] = M[s,j] × 1.5 s`。

按源码换算：

`D_linear[s] = 750 × M[s,0] × 1.5 mm`；
`D_rotation[s] = 57.29577951308232 × M[s,1] × 1.5 deg`。

双柄／单柄映射仍由既有模式决定；要给不依赖映射选择的估计，可取 `M0=max(M582,0,M587,0)` 和 `M1=max(M582,1,M587,1)`。由手柄参考引起的直线轴1／3／5／6用 `D=1125×M0 mm`；旋转轴2／7及适用的轴4跟随路径用 `D=85.94366926962348×M1 deg`。轴4独立手动动作不是由手柄采样间隔决定，不套用这个位移式。

PLC 限速下的理想追赶时间估计（静止开始、终点停稳、没有其它运动与限位）：

| 轴／路径 | 源码限制 | 时间估计，D 取上式换算后的轴位移 |
|---|---|---|
| 1／3／5／6 直线 | 轨迹 vmax=80 mm/s，amax=2000 mm/s² | `D≤3.2 mm: T=2√(D/2000) s`；否则 `T=D/80+0.04 s` |
| 2 旋转参考 | 7.5°／1 ms 扫描，无该组直线轨迹滤波 | `T=ceil(D/7.5)×0.001 s` |
| 4／7 旋转参考 | 1.5°／1 ms 扫描，无该组直线轨迹滤波 | `T=ceil(D/1.5)×0.001 s` |

以上是理想限速／加速度估计，不是系统最坏恢复时间。既有主端低通、PLC 平均滤波／二阶轨迹及 NC／机械响应会增加时间，滤波原有响应的条件计算仍见主报告。100 Hz 记录的最大值是采样得到的最大值，不能证明采样间峰值或下一次操作的全局上界；现场数据尚未代入，不报告虚构的最大位移或实机时间。

验证：四个原始速度保真、绝对最大值、差分单位、重复／失效样本、桥接缓存及力输出不变、真实 CSV 落盘和退出排空均通过。最终完整离线运行 1 s 写入 100 行，时间跨度 1000 ms，dropped=0，writer_error=0（注入的速度值只用于测试，不能当作现场速度）。

命令：`node 'B数据端/工程/ADS项目/tools/run_offline_tests.cjs' handle docs/reliability_round5/supplement/handle`。
证据：[速度定向日志](velocity.log)、[最终全量日志](../final_native.log)、[最终测试 CSV](../final_native/handle_velocity_test.csv)。

## 3. 原有测试数量核对

上一份答复把 WPF 的去重标签数当作执行次数，并只列了 Zenoh C# 数量，统计表达有误；没有删减原测试。已按实际执行次数和标签出现次数逐项核对，结果如下：

| 项目 | 用户提及／上一份答复 | 核对事实与本次结果 |
|---|---|---|
| WPF | 第四轮182，本轮114 | 第四轮、第五轮基线、上次第五轮运行、本次补跑均为182次通过；114个不同标签，68次是同标签的参数场景重复执行。四份有序通过输出完全相同 |
| Zenoh C++ golden | 第四轮81，上次答复未列 | 始终81／0，本次重新运行81／0，逐条输出及原测试源码一致；C++服务边界程序也执行并返回0，该程序自身没有数字计数器 |
| Zenoh C# 原基础测试 | 116 | 原116项完整保留。实际第五轮修改前开启网络测试的基线为149=116基础+33网络 |
| 上次第五轮 C# | 168 | `116基础+33原网络+12宿主监管+7新增网络=168`；其中新增网络为V7两项、V6五项 |
| 本次完整 C# | 新增补充 | `116+33+18宿主监管+7新增网络=174`，174／0。比168多6项：插入立即双柄invalid1项；拔出启动、立即invalid、自动重启、启动间隔、缓存不阻塞5项 |
| V5模拟 SDK 独立运行 | 上次140，本次151 | `116基础+18监管+17模拟SDK=151`。这是另一次不带网络的运行，与174不能相加当作不同用例数；相较140增加6项监管和5项模拟SDK断言 |

WPF 重复标签来自不同窗口尺寸／DPI、各速度档和轴号循环；相同文字不等于同一场景重复计错。已同时比较有序结果和逐标签次数，不再只比较标签集合。

[test_inventory.csv](test_inventory.csv) 列出每个 WPF、C++ golden、C# 完整运行标签在基线、上次第五轮及本次的次数；[test_counts.json](test_counts.json) 列出19项原第五轮新增和6项本次新增的完整名称。原 WPF Program.cs／工程文件、RobotServiceTests.cs、C++ protocol_tests.cpp／service_tests.cpp 均与第五轮修改前的存档逐字节一致，哈希附在结果中。所有原有通过断言的数量均未减少，**无删除、无跳过，无需恢复测试**。

命令：

    $dotnet = & 'A主端/工程/tools/Get-ViDotnet.ps1'
    & $dotnet build 'A主端/工程/master/VI.Control.Tests.slnx' -c Release -p:Platform=x64 -warnaserror -v:minimal
    & 'A主端/工程/build/Release/bin/VI.Control.Tests/VI.Control.Tests.exe' docs/reliability_round5/supplement/ui
    & powershell.exe -NoProfile -File 'B数据端/工程/ADS项目/tests/run-zenoh-tests.ps1' -DotnetPath $dotnet
    python -X utf8 tools/reliability_round5/test_count_audit.py

证据：[WPF完整日志](wpf.log)、[Zenoh完整日志](zenoh.log)、[逐项核对日志](test_count_audit.log)。主端、WPF、宿主及数据端构建仍为0警告0错误，日志在上级build目录。

## 4. PLC正常路径对拍补齐

保留原4820周期并新增完整 SelfCheck、普通 err 完整分派链和四直线轴回退。老版本取 baseline/source.zip，当前版本取工作区 POU；均从真实 ST 翻译，逐周期比较共同 G 输出和共同内部状态。所有场景最大数值差均为0。

| 场景 | 周期数 | 覆盖／完成证据 |
|---|---:|---|
| 正常 handle 初始化及七轴跟随 | 1000 | 从 init_done=FALSE 开始，完成初始化与正弦参考跟随 |
| 原正常单轴计划回退 | 40 | 启动、运动、重新Enable、Done，撤Req后确认空闲 |
| 原普通非限位err | 2500 | 注入普通轴错误至230 ms，清除后到_clear_err |
| 原clear_err | 250 | 完成200 ms稳定等待并请求handle重初始化 |
| 原SelfCheck完成块 | 10 | 保留原测试；不再用此项代替完整流程 |
| **完整SelfCheck：MAIN发起→self_check_done** | **5686** | 七轴stu从0开始，不跳写中间状态；四直线轴遍历0/1/2/3/6/7/71/72/8/10，旋转轴2/7遍历0/1/2/10，轴4遍历0/10；实际达到self_check_done=TRUE及_handle |
| 原MAIN普通等待分派 | 20 | _self_check未发起等待态 |
| 原Axis2Sync正常同步 | 1000 | 同步正常路径 |
| **普通err完整链：MAIN→err→clear_err→handle** | **435** | 包含恢复后handle初始化，最终handle_reinit_done=TRUE且estop_hold_req=FALSE |
| **轴1/3/5/6正常计划回退完整链** | **108** | 各轴遍历0/10/20/30/31，按运动FB反馈到目标，均Done后撤Req，确认Req/Busy/Done为FALSE |
| **合计** | **11049** | **最大差0；总29228个检查，既有新增出口62项保持通过** |

SelfCheck 通过 MAIN 原请求校验发起，使用真实 TON 的1 ms逻辑时钟和同一组明确的轴／FB反馈。补齐了测试时钟替身的 TON.ET 输出，确保原SelfCheck的3/2/1/0 s回退错开逻辑真正运行；不再直接把stu设为10来声称覆盖全过程。记录每拍七轴状态与完成位，见 [完整SelfCheck轨迹](../st/selfcheck_full_trace.csv)；场景汇总见 [normal_scenarios.csv](../st/normal_scenarios.csv)。

MAIN 中新增的 err.enter 属于新FB接口输入，不作为“旧版本内部字段”比较；err／clear_err／handle 的共同内部状态另行逐拍比较。未触发本轮新出口时，以上完整链对拍一致。没有改动生产 ST，也没有把 NC替身当作驱动器模型：速度／位置、Done／Busy、限位、Power 等输出按脚本提供，MAIN的FBD action依然是显式hook。

命令：`python -X utf8 tools/reliability_round5/run_st.py`。
证据：[完整ST运行日志](st_run.log)、[ST原始结果](../st/results.log)。PLC未在XAE原生编译，未下载／激活；完整SelfCheck离线流程通过不替代现场机械寻参。
