# E 两项关键功能核查

> 基线：2026-11-12 当前磁盘源码，HEAD `1651af5636204f9c748ac9ee53a6537ae63949b6`，包括本轮开始前已经存在的未提交改动。本文没有把 HEAD 的版本替代磁盘代码。下列“正常”限于静态实现和既有离线测试；设备运动效果另列未知。

## E.0 路径与结论

本附件路径均相对于调研根 `D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/`。为便于复查，使用以下唯一前缀：`D/` = `介入机器人/B数据端/工程/ADS项目/`，`M/` = `介入机器人/A主端/工程/master/`，`P/` = `介入机器人/C从端/250902/Untitled2/POUs/`。`D/main.cpp:347` 即该目录内 main.cpp 的物理行号，不是旧路径或 HEAD 行号。

| 功能 | 本轮明确判断 | 确定程度与证据边界 |
|---|---|---|
| 本体轴1先行 | **功能正常（当前实现与离线回归范围）**。默认前10mm名义输入额外先行2mm；单/双手柄都有实现，轴6单独换手不消耗或重新武装轴1段。 | **已确认**：`D/control_types.h:132-136`、`D/main.cpp:2484-2485,3064-3072,3787-3844,4053-4090`；`docs/arch_survey_r1/scratch/tests_native/dual_control_test.log:3,17`。**未知**：当前现场PLC/主端是否部署同版本，机械相对位移是否达到该值。 |
| 物理按键进入屈曲恢复 | **条件变得更苛刻，功能仍有可达路径**。入口为物理SN587的低三位掩码0x07；不是B7。要跨过恢复静置和运动门控；回退中按键现在须保持到原回退交接完成。 | **已确认**：`D/main.cpp:1503-1513,1549-1575,1598-1605,2083-2084,3238-3294`；提交 `5d738e6`、`f3353facc386049839fefbbd23863397a9b937ee`；110断言通过：`docs/arch_survey_r1/scratch/tests_native/return_recovery_test.log:207`。**推断**：连接保持清请求但不重置按键前值时，持续按住可能无法重生上升沿。**未知**：真实键帽、完整Active动作与设备效果。 |

**已确认**：“净距离恢复/间距恢复”和提示词中的“屈曲恢复”在当前数据端均指 `SpacingRecoveryState spacing_recovery`；没有找到另一个独立的屈曲恢复状态机（`D/main.cpp:288,1508-1513,3322-3388`、`D/control_types.h:387-437`）。实际作用为固定轴1并让轴3/5/6等位移增加，不是让轴1自行后退。

## E.1 共用的操作者→主端→Zenoh→数据端→PLC链路

以下表内事实为**已确认**；物理设备行为与布局尺寸的现场一致性见E.7。

| 环节 | 当前动作、数据和门控 | 出处 |
|---|---|---|
| 宿主打开手柄 | 独立 `VI.HandleHost` 创建serial582/587两个Device；目标200Hz。SDK `getSwitch(out byte buttons, d.Id)`，原样写入Buttons，未取反或重排低三位。 | `M/VI.HandleHost/Program.cs:32,69-93`、`M/VI.Service.Robot/Services/FlCatheterNative.cs:49` |
| 共享内存 | HostSample中Buttons为int；`ToSample()`只转byte。A/B分别582/587；采样区采用seqlock。 | `M/VI.Service.Robot/Services/HandleHostMemory.cs:31-58`、`M/VI.Service.Robot/Services/HandleService.cs:13,64-77` |
| 主端取样 | `GetSample(0/1)`分别取582/587；stop、零Tick、重复Sequence可把Valid置false，不修改Buttons。 | `M/VI.Service.Robot/Services/HandleService.cs:64-77` |
| 主端100Hz发送 | `CaptureControlFrame`取两样本；HandleLoop只在连接/状态/认证条件满足时发帧；Buttons582/587分别原样发送。 | `M/VI.Service.Robot/Services/RemoteRobotLink.cs:291-303,380-426` |
| Zenoh帧 | Topic `vi/ads/handle/v1`；协议Version1；Key4/12分别Buttons582/587，byte；独立Valid和epoch，带认证tag。 | `M/MasterConsole.Protocol/ZenohFrames.cs:5-12,25-54` |
| 数据端认证、缓存 | 认证、控制权、session、sequence通过后，slot0=582、slot1=587；无效帧不覆盖最后有效缓存。 | `D/zenoh_service.cpp:636-665`、`D/手柄/remote_handle_bridge.h:18-22,61-90` |
| 数据端逻辑手柄 | 物理SN587固定axis1、582固定axis6。单柄时两个逻辑输入可指同一柄，但物理恢复键仍直接读取 `handle_axis1.buttons2`。poll还实施新鲜度/epoch/恢复静置。 | `D/main.cpp:54-55,130-151,1503-1509`、`D/手柄/手柄.cpp:44-103` |
| 模式和运动输出 | 双柄B6为任一按住即全局撤出；屈曲恢复掩码独立判断。正常运动构建受ADS/手柄/fault/estop门控；生成pos、电缸与标志。 | `D/main.cpp:1503-1547,2083-2084,3780-3846,3322-3388` |
| 下发ADS | pos复制到 `AdsOutputCommand.refer`，加电缸及flags，经 `publish_output` 交给快ADS线程；快写不是主线程同步直接写每个符号。 | `D/main.cpp:4471-4522`、`D/ads_communication.cpp:1638-1667` |
| PLC执行 | `_handle`调用handle；refer经平滑/限速/软限位进入位置生成，电缸值进输出；planned return由Req触发PLC10/20/30/31流程。 | `P/MAIN.TcPOU:193-199`、`P/handle.TcPOU:250-319,475-545,579-709` |

**已确认**：当前公共命令没有先行或屈曲恢复设置。主端 `IRobotLink` 和WPF命令注册不含这些入口，Zenoh枚举0–19也不含旧 `SetAxis1PostReturnLead=24` / `SetSpacingRecovery=18`；当前18是ForceZero。旧本地枚举仍保留在数据端，但不能当作当前Zenoh编号。出处：`M/VI.Service.Robot/ViewModels/MainViewModel.cs:76-110`、`M/VI.Service.Robot/Services/IRobotLink.cs:67-96`、`M/MasterConsole.Protocol/ZenohFrames.cs:14-23`、`D/control_messages.h:144-181`、`D/zenoh_service.cpp:816-890`。

## E.2 轴1先行：入口、状态与实际含义

| 项目 | 事实 | 确定程度与出处 |
|---|---|---|
| 意图与默认值 | 注释明确：单/双柄导管正向递送，每次轴1计划回退后，前10mm名义输入额外2mm；0关闭、上限5mm。 | **已确认**：`D/control_types.h:132-136` |
| 跨周期状态 | active、progress_mm、applied_extra_mm；clear把三者归零；arm按lead是否大于1e-6设置active，并清两项累计。 | **已确认**：`D/main.cpp:257-259,347-358`；完整声明见B.11 |
| 正常武装入口 | planned return完成后的最后交接：CatheterDelivery，或Dual且包含axis1且非reverse。先arm，后return_recovery.completed/reset。 | **已确认**：`D/main.cpp:3064-3072` |
| 单柄额外入口 | `apply_mode_selection(CatheterDelivery)`会重新arm；只在single_handle_mode。不是单独物理“先行键”。 | **已确认**：`D/main.cpp:874-896` |
| 双柄公式 | gain=clamp(lead,0,5)/span，默认0.2；只消耗实际获准的名义正向量 `max(-d1,0)`，前10mm附加最多2mm。轴1的数值坐标减少表示此处递送。 | **已确认**：`D/main.cpp:3787-3829,3834-3844` |
| 支撑轴影响 | extra同时加到axis3_base_rel/axis5_base_rel，再作镜像，因此轴3/5只走名义量。停手、清映射或撤出不会因差额“补走”2mm。 | **已确认**：`D/main.cpp:3831-3844,4082-4086`；测试 `D/tools/dual_control_test.cpp:654-662,786-800` |
| 窗口/间距 | 额外量和名义量共同占轴1自身5..30mm窗口；g约束按轴5名义增量算，不把先行差额算成支撑位移。 | **已确认**：`D/control_types.h:99-107`、`D/main.cpp:3794-3826`；边界测试 `D/tools/dual_control_test.cpp:668-702` |
| 单柄公式 | 用本拍名义输入累计progress，按累计目标extra减已用extra，只加尚未应用部分；受constrain_axis1_follow_cmd_abs限幅。 | **已确认**：`D/main.cpp:4053-4090` |
| 无输入 | 不自动产生先行；完整前10mm要有获准名义输入和窗口行程，不能解释为回退结束后独立跑2mm。 | **已确认**：`D/main.cpp:3825-3826,4057-4060`、`D/tools/dual_control_test.cpp:657-658,702` |

**已确认**：从操作者到动作的轴1链为SN587关节采样→主端100Hz帧→数据端滤波差分/方向及窗口门控→正常Follow名义增量→已武装段的extra→pos[0]/镜像pos[2,4]→AdsOutputCommand→PLC。轴1先行不要求额外按键；双柄递送要求两柄B6均松开，换手中方向冻结。出处：E.1，以及 `D/main.cpp:1523-1535,2091-2108,3780-3846`。

### E.2.1 清除和保留矩阵

| 事件/条件 | 先行状态的处理 | 确定程度与出处 |
|---|---|---|
| 导丝模式非None、撤出、lead≈0、control=false | 清active/progress/extra。 | **已确认**：`D/main.cpp:1579-1582,1596` |
| connection hold、ADS hold、回退fault hold、急停 | 同上；连接恢复/重建基准本身没有再arm。 | **已确认**：`D/main.cpp:1583-1586,1596,1300-1326`；arm调用仅 `D/main.cpp:880,3069` |
| startup、emergency、spacing active/requested、轴6软限位 | 同上。 | **已确认**：`D/main.cpp:1587-1596` |
| 新回退包含轴1 | 开始回退时清旧段，合格正向交接结束后新arm。 | **已确认**：`D/main.cpp:2484-2485,3064-3072` |
| 双柄仅轴6回退 | 保留先行progress，不重arm；回退占用期间不走正常Follow，恢复后继续剩余段。 | **已确认**：`D/main.cpp:1592-1593,2484-2485,3064-3066`；测试 `D/tools/dual_control_test.cpp:728-731,749-782` |
| 已完成10mm | active=false，差额保留在镜像基准；以后正常1:1，直到下一次合格arm。 | **已确认**：`D/main.cpp:3829-3834,4085-4090`；`D/tools/dual_control_test.cpp:654-662` |

**推断**：一次短暂保持也会终止尚未走完的先行段；恢复连接后若仅在原位继续推、没有新合格轴1回退或单柄模式选择，操作者会看不到原段剩余先行。该清理是当前显式规则，不是映射代码被删除。证据：`D/main.cpp:1579-1596,353-358,880,3069`。

## E.3 屈曲恢复：按键、进入、动作和退出

### E.3.1 哪个键，怎样进入

| 条件层 | 当前规则 | 确定程度与出处 |
|---|---|---|
| 物理来源 | 直接读取物理SN587；mask=0x07，要求bit0/1/2同时置位；0x47也满足。B6=0x40可叠加；B7=0x80本身不进入恢复。 | **已确认**：`D/main.cpp:54-55,1505-1513`、`D/control_types.h:97,150-151`；`D/tools/dual_control_test.cpp:365,376-388` |
| 硬件丝印 | 注释称一枚“物理587屈曲恢复键”。源码不能判断低三位来自一枚复合编码键还是多枚键；没有查到键帽、固件或SDK编码表。 | **未知**：所缺材料见E.7；注释来源 `D/control_types.h:150` |
| 按住请求 | 上升沿保存原模式/override并requested=true；下降沿requested=false；每拍更新pressed_prev。不是按一次锁存直到再次按下。 | **已确认**：`D/main.cpp:1549-1575` |
| 单/双手柄 | 入口没有 `!single_handle_mode` 限制；只有SN587的单柄也可触发。只有SN582时两个逻辑输入虽然可共用582，但物理587缓存不会由582代替。 | **已确认**：`D/main.cpp:140-147,1505-1509,3238-3249`；**推断**：仅582单柄没有该物理入口 |
| 连接/主运动门 | 必须没有ADS、手柄、回退fault、急停保持，control/startup/emergency之一有效，且read_plc_state成功，才执行进入/Active分支。 | **已确认**：`D/main.cpp:2083-2084` |
| 明确前提 | startup.completed且phase=Done；control_active；无axis6_soft_limit_hold；没有planned_return。 | **已确认**：`D/main.cpp:3238-3249` |
| 几何前提 | axis5距左限位加0.2不能小于axis3距左限位；g=axis6_from_left-axis5_from_left在1..33mm的±0.2容差内；共同剩余行程>0.2mm。 | **已确认**：`D/main.cpp:3240-3245,3264-3274`、`D/control_types.h:104-107,154` |
| 实际模式前提 | 拒绝提示写“导管递送Follow”，但布尔前提没有显式检查guidewire_mode=None或!reverse；不能把提示当作真正门控。 | **已确认**：`D/main.cpp:3246-3262` |
| 回退中按键 | 只登记requested，保留原PLC回退；等原planned_return交接结束，仍按住才进入。提前松开则不进入。 | **已确认**：`D/main.cpp:1598-1605,3238`、`D/tools/return_recovery_test.cpp:340-359` |

### E.3.2 每个状态的输出、时钟和转移

| 状态 | 触发与转移 | 轴/电缸/标志和时钟 | 确定程度与出处 |
|---|---|---|---|
| Idle | 请求有效且前提/几何通过→ClampWait；否则reset，当前边沿被消费。 | 锁存轴1/2/7保持位、轴3/5/6实际位作为命令；清轴1组/轴6旧return request，四电缸恢复Automatic；moved/pending=0，清力输出。 | **已确认**：`D/main.cpp:3251-3294` |
| ClampWait | `GetTickCount()`与phase_t0差≥150ms→Active。 | cyl1=450、cyl2=1000、cyl3=550、cyl4=100；轴1/2/7保持，轴3/5/6先保持；pending清零。计时自本地进入起，不等ADS applied或夹爪到位反馈。 | **已确认**：`D/main.cpp:2090,3284-3288,3322-3341`、`D/control_types.h:146,149,203-218` |
| Active | 持续按住；只消费正的axis1_linear_increment_mm；剩余≤0.2则清pending并提示上限。 | 正增量累积pending，非正立即清pending；dt用DWORD毫秒差、封顶50ms；每拍step≤40mm/s×dt及共同剩余；轴3/5/6命令等量增加，轴1/2/7保持。 | **已确认**：`D/main.cpp:3343-3388`、`D/control_types.h:142-145,154` |
| Active输入来源 | axis1逻辑输入滤波差×750×-1，死区0.02mm。 | 双柄来自SN587；单柄来自锁定的唯一逻辑输入。日志写“582手柄”与当前固定角色/变量不符。 | **已确认**：`D/main.cpp:130-151,2091-2104,3340,3349-3351`、`D/control_types.h:81-82,141,157` |
| ExitSync | ClampWait/Active期间松键→ExitSync；pending=0。 | 保持最后3/5/6命令，至少等100ms；误差均≤0.2或累计2s后允许按实际位同步。还要有效ADS且无ADS/手柄/急停/fault hold。 | **已确认**：`D/main.cpp:3229-3235,4300-4312` |
| ExitSync→Idle | `sync_all(20)`成功才reset/control_active=true；单次失败保留ExitSync重试。 | 恢复保存override；单柄apply保存ModeSelection，双柄清override，下一拍按当前两B6；按位置条件释放axis3递送stop锁存。 | **已确认**：`D/main.cpp:4314-4343` |

**已确认**：共同剩余是三者最小值：axis3上限650、axis5上限665、axis6上限min(665,660)=660，均为距左限位坐标。到上限不会自动变成ExitSync，仍需松键；配置665不能据此声称轴6恢复可达665。出处：`D/main.cpp:3215-3226,3375-3388,3229-3235`。

### E.3.3 近期保持、静置和重建基准的影响

| 事件 | 当前处理 | 确定程度与出处 |
|---|---|---|
| ADS重连/connection hold进入 | 除ExitSync外reset spacing；清力；planned_return另行保留。 | **已确认**：`D/main.cpp:1058,1300-1307` |
| 急停或PLC fault hold | reset spacing并停止control，取消回退/清力；后续走统一恢复。 | **已确认**：`D/main.cpp:1761-1786` |
| 自检状态/复位/重初始化 | 相关分支reset spacing；不能沿用旧恢复目标。 | **已确认**：`D/main.cpp:1370,1916,1975,5211,5327` |
| 紧急回撤 | 当前planned仍active时请求被忽略；能进入时独占输出并reset spacing；先行由下一拍emergency门控清除。 | **已确认**：`D/main.cpp:5191-5218,1587-1596`；实际输出 `D/main.cpp:3297-3313` |
| epoch改变、断流>1500ms、重新打开 | `Handle::poll/init`进入resume_settling；连续有效新样本500ms后置rebase请求；静置时主循环handle_soft_hold=true。 | **已确认**：`D/手柄/手柄.cpp:33-39,56-92`、`D/手柄/Handle.h:6`、`D/main.cpp:1189-1201` |
| 键一直按住跨清理 | SpacingRecoveryState.reset不改外部pressed_prev；边沿检测仍每拍更新它。只要恢复后仍是同一按下电平，就没有新的press_edge。 | **已确认（代码）**：`D/control_types.h:395-437`、`D/main.cpp:872,1508-1513,1575`；**推断（现场表现）**：可能需要松开再按才能重入 |
| 只有按键没有有效正向增量 | 可激活ClampWait/Active，但轴3/5/6不走；非正输入清pending。 | **已确认**：`D/main.cpp:3349-3357,3366-3373` |

## E.4 Git历史：何时改变了行为

时间均来自提交记录，UTC+08。路径移动前用 `B_从端电脑/工程/ADS项目/`；`git log --follow`追踪当前main，不把“某标识首次出现在当前目录”当作功能诞生。以下是最近相关轮次，不穷举无关格式改动。

| 提交/日期 | 触及的功能与行为变化 | 判断与当前对应 |
|---|---|---|
| `b91c6ec` 初始导入 | 已有mask恢复和prerequisites/ClampWait/Active/ExitSync；仓库内不能追出更早设计来源。 | **已确认（历史存在）**：该提交旧路径main.cpp/control_types.h；**未知**：导入前来源。 |
| `3d691de6bed5539f89e277cb328d47241b32739d`，2026-10-11 | 删除角色交换选项，固定587轴1、582轴6；恢复物理587取值由serial判断改为直接handle_axis1。 | **已确认**：改变角色定义，没有删除恢复键。当前 `D/main.cpp:54-55,1505`。 |
| `cd64bf33ac082e6809c576975144064bb5b2ec33`，2026-10-12 | 删除B7模式选择；双柄方向统一任一B6。低三位恢复保留，恢复期间冻结方向。 | **已确认**：旧“按B7选某模式”行为已移除；不能将B7当屈曲恢复。当前 `D/main.cpp:1508-1547`。 |
| `ab264fe`，2026-10-20 | 删除数据端本地UI、键盘和本地SDK入口；内部VisCommand枚举仍留。 | **已确认**：旧本地调整入口停止可用，物理恢复链仍留；并非删掉全部屈曲恢复。当前公共链见E.1。 |
| `60f6ff0`，2026-10-20 | 网络迁移到Zenoh+MessagePack；保留两柄独立按键字段。 | **已确认**：现行四topic/认证路径由此后的实现承担，旧pipe/TCP说明不等于现接口。 |
| `db4ce078f9d0255cad5f9e4f341d613b43310533`，2026-10-20 | 默认lead由0改2mm，扩展双柄先行；axis6-only换手保留进度；extra吸收进镜像基准。 | **已确认**：增强并修正先行行为，不是关闭。当前 `D/control_types.h:134`、`D/main.cpp:2484-2485,3787-3844`。 |
| `5d738e6`，2026-10-29 | 原回退保持到自然交接；spacing请求不再打断回退；axis6恢复上限增加min软限；ExitSync失败改为保留重试。 | **已确认**：按键时机与可用行程变得更受约束，退出失败不再永久锁死。当前 `D/main.cpp:1598-1605,3221-3223,3238,4343`。 |
| `1d1614e`，2026-11-08 | 新ReturnRecovery/Clear/自动续做；急停保存未完回退意图；spacing属于exclusive门控；急停处理仍reset spacing。 | **已确认**：恢复的是回退意图，不能据此认为spacing会自动续做。当前 `D/main.cpp:1752-1786`、`D/return_recovery.h:17-62`。 |
| `f3353facc386049839fefbbd23863397a9b937ee`，2026-11-09 | epoch/断流后连续新样本静置500ms；新增resume_settling参与handle hold；rebase必须等手柄hold解除。 | **已确认**：新增可阻断E.3入口的条件。**推断**：若保持触发reset且按住不松，会呈现恢复键无反应。当前 `D/main.cpp:1189-1201,1247-1248,1300-1307`。 |
| HEAD→本轮磁盘 | main/control_types还存在既有力反馈/本地阻尼/记录调整；mapping与spacing主体仍在，不能把本轮行为都归给最近提交。 | **已确认**：初始 `scratch/baseline.json` 的status/SHA256；当前源码行见E.2/E.3。本轮没有为未提交差异虚构提交哈希。 |

### E.4.1 历史差异短摘录

以下每段均小于20行，只保留直接改变行为的连续差异；完整源码以提交哈希和路径追溯。

**已确认**：`3d691de6bed5539f89e277cb328d47241b32739d`，旧路径main.cpp，物理键来源改为固定角色（原实现仍是物理587）：

```diff
- const unsigned char physical_587_buttons =
-     handle_axis1.serial() == 587 ? handle_axis1.buttons2 : handle_axis6.buttons2;
+ const unsigned char physical_587_buttons = handle_axis1.buttons2;
```

**已确认**：`cd64bf33ac082e6809c576975144064bb5b2ec33`，旧路径main.cpp，B7边沿被删除；恢复边沿的上下文保留：

```diff
      !spacing_recovery_button_pressed && spacing_recovery_button_pressed_prev;
- const bool catheter_b7_pressed = (axis1_buttons & cfg.btn_b7) != 0;
- const bool guidewire_b7_pressed = (axis6_buttons & cfg.btn_b7) != 0;
- const bool catheter_b7_press_edge =
-     catheter_b7_pressed && !catheter_mode_button_pressed_prev;
- const bool guidewire_b7_press_edge =
-     guidewire_b7_pressed && !guidewire_mode_button_pressed_prev;
```

**已确认**：`db4ce078f9d0255cad5f9e4f341d613b43310533`，当前路径control_types.h和main.cpp；默认开启且保留轴6-only期间的进度：

```diff
- double axis1_post_return_lead_mm = 0.0;
+ double axis1_post_return_lead_mm = 2.0;
```

```diff
- clear_axis1_delivery_mapping();
+ // 双手柄仅轴6换手时保留轴1先行进度，待恢复Follow继续。
+ if (mode != PlannedReturnMode::Dual || axis1_active) clear_axis1_delivery_mapping();
```

**已确认**：`5d738e6`，当前路径main.cpp；恢复等待原回退，且剩余行程受660软限约束：

```diff
- if (spacing_recovery.requested && !spacing_recovery.active())
+ if (spacing_recovery.requested && !spacing_recovery.active() && !planned_return.active())
```

```diff
- const double axis6_remaining = cfg.spacing_recovery_axis6_max_from_left_mm -
+ const double axis6_remaining = (std::min)(cfg.spacing_recovery_axis6_max_from_left_mm,
+     cfg.axis6_soft_limit_from_left_mm) -
      ((spacing_recovery.axis6_cmd_rel + plc_init_pos[5]) - plc_leftlimit[5]);
```

**已确认**：`f3353facc386049839fefbbd23863397a9b937ee`，当前路径main.cpp；新增手柄恢复静置门：

```diff
  handle_soft_hold_active = single_handle_mode
-     ? (!handle1_poll_ok && !handle6_poll_ok)
-     : (!handle1_poll_ok || !handle6_poll_ok);
+     ? ((!handle1_poll_ok && !handle6_poll_ok) || axis1_input_handle->resume_settling())
+     : (!handle1_poll_ok || !handle6_poll_ok || handle_axis1.resume_settling() || handle_axis6.resume_settling());
```

## E.5 离线测试结果及证明范围

本轮运行既有测试，生产源码/测试文件未改。现代runner抽取生产分支生成include到scratch，并使用内存PLC/ADS；不启动生产ADS.exe或手柄宿主。执行细节与全部失败见F。

| 测试 | 本轮结果 | 覆盖范围与边界 | 出处 |
|---|---|---|---|
| dual_control_test / post_return_lead | PASS；整个EXE 122,359断言 | 10mm→12/10mm、停止不补走、跨窗口、g约束、0.5mm/s换手门、跨换手、单柄镜像连续和清理。未证明真实夹爪/PLC时序。 | **已确认**：`D/tools/dual_control_test.cpp:645-801`、`docs/arch_survey_r1/scratch/tests_native/dual_control_test.log:3,17` |
| mapping helpers/guard/complete | PASS，包含于上项 | runner从当前main抽取生命周期和单/双柄映射；不是只验证另写的数学公式。 | **已确认**：`D/tools/run_offline_tests.cjs:41-45`、`D/tools/dual_control_test.cpp:707-747` |
| dual_control_test / direction+recovery | PASS，包含于上项 | 0x07/0x47、原方向、B6恢复、B7不切换；部分场景直接设置Active，不代表实际完整激活链。 | **已确认**：`D/tools/dual_control_test.cpp:330-395` |
| return_recovery_test | PASS，110断言 | 回退中按键保持/提前松开、交接、ExitSync单次失败后重试、660上限、再次恢复不重放旧回退。 | **已确认**：`D/tools/return_recovery_test.cpp:317-359`、`docs/arch_survey_r1/scratch/tests_native/return_recovery_test.log:207` |
| 屈曲恢复Active完整步进 | 没有专门完整测试证明 | runner的enter截取在emergency分支前结束；并未抽取执行整个Active3/5/6递增分支，tracking也是空替身。 | **已确认（夹具边界）**：`D/tools/run_offline_tests.cjs:74-89`、`D/tools/return_recovery_test.cpp:80,150-160`；**未知**：完整端到端实际动作 |
| 主端按键传递/协议 | 现有协议、宿主fake、回环测试通过 | 未使用真实587键帽/SDK运动设备；不能证明现场一枚键实际发0x07。 | **已确认（离线）**：`M/MasterConsole.Protocol.Tests/HandleHostTests.cs:33-132`、`docs/arch_survey_r1/scratch/tests/zenoh_cross_language.log:188-242` |

## E.6 与提示词、旧注释或文档不符的事实

| 材料描述 | 当前源码事实 | 确定程度与出处 |
|---|---|---|
| “按某键进入屈曲恢复”可能理解为B7 | 当前恢复是物理587低三位掩码0x07的按住/松开；B7模式选择已于cd64bf33删除。 | **已确认**：`D/main.cpp:1505-1513`、提交 `cd64bf33ac082e6809c576975144064bb5b2ec33` |
| 恢复注释/日志说“582手柄仅控制轴3/5/6” | 实际用axis1_linear_increment，双柄固定来自587；只有单柄输入别名可能来自582，但仅582缺物理587恢复键入口。 | **已确认（实现/文字差异）**：`D/control_types.h:141`、`D/main.cpp:54-55,140-147,2091-2104,3340,3349` |
| 拒绝提示写“导管递送Follow” | 代码前提未显式检查guidewire None或非reverse；按实际条件记录，不能据提示添加前提。 | **已确认**：`D/main.cpp:3246-3262` |
| 旧文档：WPF先行默认1mm、命令24 | 当前默认2mm；WPF/Zenoh没有该配置入口。旧本地枚举不能作为当前公共命令。 | **已确认**：`介入机器人/A主端/工程/docs/architecture/运动流程说明.md:341`、`介入机器人/A主端/工程/docs/architecture/ADS通讯说明.md:439,457-458`；当前证据E.1/E.2 |
| 认为“功能在某轮被删掉” | 两个主体均在；先行db4ce07默认开启，屈曲入口保留。能定位到条件变化的提交，但不能在缺现场事件记录时断言某次未动作唯一由某提交引起。 | **已确认（代码/历史）**：E.2–E.4；**未知（具体故障事件归因）**：缺按键/hold/phase/PLC同步时序 |

## E.7 尚需的事实证据

| 未知/推断事项 | 需要什么才能确认 |
|---|---|
| 物理恢复键的键帽、低三位编码来源 | SN587键位表、SDK/固件说明，或不连机器人的原始getSwitch采样记录。当前只确认0x07位条件。 |
| 按住键跨连接保持的现场表现 | 同一时间轴上的Buttons587、Valid、epoch、sample age、resume_settling、requested、phase、pressed_prev与connection_hold记录；当前是代码推断，现有测试未覆盖完整场景。 |
| 完整Active3/5/6动作、150ms后夹持是否已稳定 | 真实夹爪状态/位置与refer/ADS applied/PLC反馈时间线；现有测试只部分执行进入和退出。 |
| 当前设备是否运行这份代码 | 主端/数据端可执行文件hash和PLC下载应用/工程版本指纹。仅仓库源文件和Boot产物不能证明。 |
| 先行2mm的机械效果和安全窗口 | 已同步时间的名义输入、轴1/3/5实际位、夹持状态、planned-return阶段；现有records无有效位置/事件，无法证明机械相对位移。 |

上述证据缺口属于本轮边界；本附件不提出修改方案，也没有为核查而连接设备。
