# 定位臂 PLC 末端点动接口与交付报告

日期：2026-11-04。目标：TwinCAT 3.1.4024、C++17、WPF。**当前未完成全部验收，不能按完成标准交付为可投运版本。** 三端实现和离线证据已保存；T10、T11 失败，运动学 Z 方向修正未提交，从完全折叠程序零位启动 X/Y 的算法也缺少可行定义。PLC 未构建、未下载、未激活、未执行 Scope 对比。

## 1. 未完成项与规格冲突

| 项目 | 证据 | 本次处理 |
|---|---|---|
| §5.4 逐周期 `σ += v_c·dt` 与 T10 连续速度曲线积分误差 ≤1e−6 冲突 | Z 快档按住 2 s：4.5015 mm，解析积分 4.5 mm，误差 0.00150000000008 mm；俯仰/偏航各 1.8006°，解析积分 1.8°，误差 0.000600000000053° | 保留规定的积分语句与原阈值，T10 记 FAIL；没有改为其它积分器 |
| §5.4 位置用当前速度积分、速度用周期起点雅可比，与 T11 梯形一致性 ≤1e−3 冲突 | T11 最大 1.48628374195 °/s，发生在 T9 离开边界反向；普通单轴 T10 也达到 0.00224999699882 | 保留规定的运算次序，T11 记 FAIL；未增加平滑、夹紧或额外限速 |
| §3.2 将运动学 Z 改成 −q1 与 §8.2 归零逐位不变冲突 | 构建目录中仅改变 FK、Jacobian 和 Z 增量逆解的 q1 符号：100 组基线有 48 个输出不同；保留旧符号时 1700 个 double 及 error/already_home 全部相同 | 停止 Z 符号项，生产源码保留 +q1；PLC Z+ 仍按现场事实命令轴1减小。新旧坐标含义存在差异，未宣称已完成 Z 向上坐标修正 |
| §5.4 假定起点位于可逆环带，但程序零位机械 q3=0 位于环带外 | 机械零位腕点半径 20 mm；规定下界 20.0684214471 mm。按给定算法 X+ 持续 2 s，实际路径速度=0、boundary=TRUE、q3=0 | 停止该起步场景；没有发明进入环带的路径、误差码或奇异恢复策略 |
| §5.4 的“q3”使用标准两连杆肘角，源码机械 q3 有 180° 偏移 | 源码第二连杆角为 q2+q3+π；机械 q3=0 为折叠，±180° 为伸直 | 算法中显式区分机械 q3 与有效肘角 α=q3+180°；Q3_MARGIN 约束 α 到伸直/折叠的距离。保持真实运动学和机械角约定 |
| NC 最大速度不完整 | Axis1 Maximum=30；Axis2–5 只有手动速度字段，没有 Maximum | 按 §4.2 的全轴回退规则全部用 10；**待用户按 NC 参数填写**，没有读取运行时 NC 参数 |

T10 的离散偏差不是浮点容差问题。起步 1 s 后恒速到 2 s，用当前周期速度的矩形积分会比连续积分多 `v·dt/2`；Z 快档正好为 0.0015 mm。单轴 T11 的差值为 `(v_k−v_(k−1))/2`，峰值约 `0.75·v·dt/T_ACC`；Z 快档即 0.00225 mm/s，高于阈值。

相关输出：`acceptance_cart_jog/arm_cart_jog_tests.log`、`home_regression.log`、`home_z_negative.log`。这些失败会使离线脚本返回 1，不会被跳过或转为成功。

## 2. 修改范围与链路

末端点动现在为：PositioningArmPanel 按下立即发送原 ArmCartesianJog，按住每 100 ms 重发；ArmManualAdsService 只保存模式、方向、速度和 500 ms 租约，约每 50 ms 使用一次 ADS SumWrite 写四个请求字段；ArmCartJog 在原 1 ms 定位臂任务中管理五轴所有权和 Tc2_MC2 握手，ArmCartJogGen 逐周期计算设定点。

归零仍为 mode 0 的原 17 个 LREAL 单段曲线。ArmCartesian 只接受 `[16]=0.0`，保留五次进度与弯曲插补。旧连续曲线排队、Hermite 插补及续段重试已删除。单轴点动仍为原 MC_MoveVelocity。

上位机只取消 CartesianForward/CartesianReverse 的 CanOperate 禁用绑定；按下仍检查 CanOperate。保活回调中只有单轴点动或归零继续因 CanOperate=false 停止；末端按住会继续重发。MouseUp/LostMouseCapture 停止、换档和换模式发送保持原入口。

此次任务开始时仓库已有未提交修改。前态来自改动前保存的工作区副本 `C:\Users\Sui\AppData\Local\Temp\arm_cjog_before_20261104`，不是 Git HEAD。`program_zero.cpp` 与该副本逐字节相同；未修改介入机器人7轴、力反馈、手柄、注射器、轴4电机或实验记录的行为。现有协议/界面测试会覆盖部分共用代码，但对应产品逻辑未改。

旧《定位臂项目修改与接口说明》docx 已过时；第16项不再存在点动含义。以本报告、源码及当前需求为准。

## 3. 符号推导与源码依据

本节代码行以当前工作区为准；完整检索索引在 `acceptance_cart_jog/source_index.txt`。

| 简称 | 仓库相对路径 |
|---|---|
| H | `B数据端/工程/PositioningArmKinematics/include/positioning_arm/kinematics.h` |
| K | `B数据端/工程/PositioningArmKinematics/src/kinematics.cpp` |
| P | `B数据端/工程/PositioningArmKinematics/src/program_zero.cpp` |
| C | `B数据端/工程/ADS项目/arm_cartesian_motion.h` |
| JG | `C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU` |
| J | `C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU` |
| R | `B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h` |
| VM | `A主端/工程/master/VI.Service.Robot/ViewModels/MainViewModel.cs` |

PLC q1 单位 mm，q2–q5 单位 °。运动学内部长度为 m、角度为 rad；[C:40–46](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/arm_cartesian_motion.h:40>) 使用 calibration `{0, scale, 1}`，轴1 scale=0.001，其余 π/180。

[H:22](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/include/positioning_arm/kinematics.h:22>) 给出 L1=0.200 m、L2=0.180 m。[K:86–93](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:86>) 给出：

```text
Wx = 200 sin(q2) + 180 sin(q2+q3+π)
Wy = −200 cos(q2) − 180 cos(q2+q3+π)
```

这里三角函数中的角为 rad。令 α=q3+π，即标准两连杆公式。分支取 sign(sin α)=sign(−sin q3)；机械 q3=0 时保留上次分支，初值 +1。[JG:74–81](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:74>) 与 [R:44–50](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:44>) 使用同一式。闭式逆解把 q2、q3 分别展开到上一指令附近的 360° 等价值，不把机械角强制折回一个周期。

[K:95–97](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:95>) 给出工具姿态 `Rz(q2+q3+q4−π/2)·Rx(q5)`，因此偏航 `yaw=q2+q3+q4−90°`，偏航+对应轴4+；平移时 q4 补偿为 `anchor4+(anchor2+anchor3)−(q2+q3)`。工具纵向向量为局部 `(0,−1,0)`，绕局部 X 旋转后竖直分量为 `−sin(q5)`，在T10使用的姿态附近，几何仰角 elevation=−q5；[K:226](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:226>) 的 `next[4] -= command.pitch` 也确认俯仰+对应轴5−。[JG:87–98](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:87>) 因而 Z/俯仰/偏航分别使用轴1−、轴5−、轴4+。俯仰和偏航只转对应轴，没有 TCP 定点旋转。

原有 FK 在 [K:84](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:84>) 为 `z=q1` 加工具竖直分量，[K:113](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:113>) 的第一列为 +UnitZ，[K:225](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:225>) 的 Z 增量为 q1+=Δz，故 ∂z/∂q1=+1。该符号与现场确认相反；本次因归零逐位回归冲突而保留，**§3.2 这一项未实现**。

当前计算基底仍采用 Eigen 右手旋转和叉乘（[K:95–97](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:95>)、120–121），没有翻转 X/Y，也没有改变坐标系手性。新增平移不改变手性。若对同一空间向量完整进行 diag(1,1,−1) 反射，行列式为 −1，会改变手性；单改升降轴映射、却保留工具旋转项，并不能据此宣称整个坐标系已完成反射。当前代码未建立与现场 Z 向上事实一致的新基座坐标系。

### 3.1 基座平移及全部 Z 使用点

[H:25](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/include/positioning_arm/kinematics.h:25>) 新增 `base_offset_mm=(0,0,0)`，只做平移。[K:100](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:100>) 在正解结果加偏移；[K:116–117](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:116>) 同时平移 Jacobian 关节原点；[K:274–275](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:274>) 的绝对腕点输入扣除 x/y 偏移。Z 逆解是增量式，没有独立的绝对 Z 输入，平移自然相消。OFFSET 测试验证非零 `(20,−30,40) mm` 的 FK、Jacobian 和平移逆解。

| 使用点 | 当前关系与影响 |
|---|---|
| [K:84](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:84>)、98–100，FK 升降/工具竖直分量 | 仍为 +q1，工具 z=−tool_length·sin(q5)；输出加 base_offset_mm.z/1000 |
| [K:113](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:113>)、115–117，Jacobian 升降列/轴原点 | +UnitZ 未改；偏移加入轴原点，叉乘中的位置差抵消平移 |
| [K:120–136](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:120>)，平面及俯仰 Jacobian | 使用同一平移后的原点；未改变角速度列与符号 |
| [K:153–160](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:153>)、254–259、362–366，平面奇异度/原 solveJog | 消费上述 Jacobian；末端 PLC 点动不再调用这些入口 |
| [K:211](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:211>)、223–238，Z/俯仰增量逆解 | Z+ 仍为 q1 增大，俯仰+为 q5 减小；旧 Z API 与新 PLC Z+的符号不一致，已列未完成 |
| [K:246–275](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:246>)，平面逆解目标 | 当前 FK 加增量形成目标，x/y 解腕点时减偏移；Z 不参与两连杆方程 |
| [K:376](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:376>)、385，原 solveJog Z 入口 | 转发到旧增量逆解，生产末端点动链路已不使用 |
| [P:99](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/program_zero.cpp:99>)、112–119、208–209，归零代价/梯度/位移界 | 读取 FK/Jacobian。数学上平移抵消，但 q1 符号变更引起浮点减法舍入变化，最终 48 个曲线输出位模式不同 |
| [C:19–74](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/ADS项目/arm_cartesian_motion.h:19>)，唯一生产运动学调用者 | 只规划归零 mode0，默认基座偏移为0，逐位回归通过 |
| `tests/kinematics_test.cpp`:124–138、171、210–226 | 保留旧 FK Z、编码换算和旧增量 API 断言；现有20项全部通过，不能作为现场 Z 修正已完成的证据 |
| `tests/arm_cart_jog_tests.cpp` 的 T2/T10/OFFSET | T2 检查 Z 保持，T10 检查 PLC Z+轴1减小，OFFSET 检查平移一致性 |

全仓库调用检索没有发现其它生产代码直接调用 PositioningArmKinematics 的 FK/Jacobian/逆解。基础配置的有限值校验位于构造配置边界；没有在 PLC/参考模型内部加入 NaN 校验、夹紧或额外裕量。

## 4. PLC 与状态接口

### 4.1 GVL G

位置：`C从端/250902/Untitled2/GVLs/G.TcGVL`:107–116。

| 变量 | 类型 | 方向 | 含义 |
|---|---|---|---|
| G.arm_cjog_mode | INT | 数据端→PLC | 0 无请求，1 X，2 Y，3 Z，4 俯仰，5 偏航 |
| G.arm_cjog_dir | INT | 数据端→PLC | ±1；mode=0 时0 |
| G.arm_cjog_speed | LREAL | 数据端→PLC | (0,100]；平移 mm/s，旋转 °/s |
| G.arm_cjog_alive | UDINT | 数据端→PLC | 活动请求每次写入加1，零请求不再续写 |
| G.arm_cjog_owned | BOOL | PLC内部/数据端只读 | 正占用五轴外部设定点发生器 |
| G.arm_cjog_state | UDINT | PLC→数据端 | 0空闲、1使能、2运动、3减速、4释放 |
| G.arm_cjog_error | UDINT | PLC→数据端 | 0、3001–3003 或原 NC ErrorID，保持至下次起步清零 |
| G.arm_cjog_limited | BOOL | PLC→数据端 | 本周期 k<1 或可达边界停止 |
| G.arm_cjog_path_speed | LREAL | PLC→数据端 | 已缩放的非负路径速度，mm/s 或 °/s |
| G.arm_cjog_if_version | UDINT | PLC→数据端 | 初值1 |

四个请求字段在同一个 `write_sum` 中提交（`arm_manual_ads_service.cpp`:629–633）；此处只保证使用一次 ADS SumWrite，不宣称 ADS 子项与 PLC 扫描之间具有事务原子性。

### 4.2 请求与阶段机

[J:39–48](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:39>)：alive 变化时 age=0，否则加实际周期。合法请求且 alive_age<0.5 s 为 held。`mode=0` 正常松开，不产生3001。合法请求在单一 ADS 边界判定；Gen 接受已判定的 held，不再次校验。

| 阶段 | 动作 | 转移 |
|---|---|---|
| 0 空闲 | mode非零且非法报3001；held且归零无所有权、request=ack时检查 drive_ready；就绪则以 G.arm_act_pos 锚定、速度/加速度0、feed方向1，取得所有权 | 就绪→10；未就绪报3002并保持0 |
| 10 使能 | 五轴 MC_ExtSetPointGenEnable，绝对位置；owned期间每周期 Feed(p,0,0) | 全部 Enabled→25；使能超过2s报3003→40 |
| 25 运行 | 每周期 Gen 与 Feed；模式/方向变化先减到0再重新锚定，阶段和所有权保持 | 无held且实际路径速度0连续2周期→40；驱动未就绪报3002→40 |
| 40 停用 | 五轴 MC_ExtSetPointGenDisable | 全停用：正常→60，故障→50 |
| 50 停止 | MC_Stop，使用原 G.arm_jog_dec/jerk | 全Done→60 |
| 60 释放 | 等待 MC_Stop不再Busy | owned=FALSE→0 |

Enable/Disable/Stop 任一 Error 保存 ErrorID；阶段<40时进入40，阶段≥40保持该阶段。Fault 路径包含 MC_Stop，正常减速结束不再额外调用 MC_Stop。Feed 仅在 owned 且阶段10/25。速度非零更新方向符号，零速保持上一值。[J:123–143](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:123>) 映射为对外0–4状态；25且target_speed=0为减速状态3。

drive_ready 在 [J:51–57](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:51>) 与原 ArmCartesian:63–69 相同：总使能、主机通信/急停、1ms周期、五轴上电Done、使能请求、轴故障、复位及单轴点动请求。归零起步额外拒绝 cjog_owned，报2001但不写点动请求；ArmManual 在复位和 allow_jog 两处排除 cjog_owned。

### 4.3 时间与状态传播

| 事件 | 行为 |
|---|---|
| 正常松开/失去鼠标捕获 | 原 ArmStop；数据端立即标脏写0/0/0/alive；PLC用0.5s减速 |
| 上位机→数据端中断 | 数据端500ms租约到期，写一次零请求后停止写入 |
| 数据端→PLC中断 | alive连续0.5s不变，PLC按松开减速，无错误码 |
| 驱动失去就绪 | 3002，停用→MC_Stop→释放 |
| 换高档/同向重新按下 | 从当前路径速度用1s三次曲线过渡 |
| 换低档/松开/换模式/换向 | 用0.5s减速；换模式/换向到零后在阶段25锚定新请求 |
| 关节限速/边界 | limited=TRUE，不写错误码；边界保持前一指令点 |

snapshot→main.cpp 的 vs→Zenoh StateFrame→RemoteRobotLink→StatusFrame 贯通 `state/error/limited/path_speed` 四字段。[VM:391–400](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Service.Robot/ViewModels/MainViewModel.cs:391>)：错误优先于降速提示；limited且state2/3时显示“接近奇异位形或关节限速，已自动降速”，其余为空。保留该规格原文，没有另加边界提示规则。

| 错误 | 中文显示 |
|---|---|
| 3001 | 末端点动参数无效（错误码 3001） |
| 3002 | 驱动未就绪（总使能/上电/轴故障/复位/单轴点动/通信超时/急停）（错误码 3002） |
| 3003 | 外部设定点发生器使能超时（错误码 3003） |
| 其它非零 | NC 错误 0x{ErrorID:X}，同时显示十进制错误码 |

协议沿用仓库此前扩展状态字段时的 Version=1 及 v1 topic。StateFrame 51→55项：索引50为uint32 State、51为uint32 Error、52为bool Limited、53为float64 PathSpeed、54为AuthTag。两端严格长度/类型校验和黄金样本同步更新；主端、数据端须同步部署，旧51项帧会被拒绝，没有兼容回退。ArmCartesianJog 命令格式未变；speedScale 对新末端点动完全忽略，仍供原单轴点动使用。

## 5. 常量与 ST/C++ 逐句对照

所有单位见变量或本表。生产速度档唯一来源 [VM:406–417](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Service.Robot/ViewModels/MainViewModel.cs:406>)，数据端/PLC不硬编码档位；测试中列出速度表只是验收输入。

| 常量 | 值 | ST 定义/生产来源 | C++参考定义 |
|---|---|---|---|
| PI | 3.14159265358979323846 | [JG:24](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:24>) | [R:8](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:8>) |
| RAD | PI/180 | [JG:25](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:25>) | [R:9](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:9>) |
| L1/L2 | 200/180 mm | [H:22](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/include/positioning_arm/kinematics.h:22>)；[JG:26–27](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:26>)转换为mm | [R:10](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:10>) |
| T_ACC | 1.0 s | [JG:28](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:28>) | [R:11](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:11>) |
| T_DEC | 0.5 s | [JG:29](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:29>) | [R:11](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:11>) |
| Q3_MARGIN | 0.5° | [JG:30](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:30>)，有效肘角距伸直/折叠 | [R:11](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:11>) |
| JOINT_VCAP[1..5] | 10,10,10,10,10；轴1 mm/s，其余 °/s | [JG:31](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:31>) | [R:12](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:12>)（C++索引0..4） |
| ALIVE_TIMEOUT | 0.5 s | [J:6](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:6>) | Gen无此常量；T8按J的age判据驱动held |
| ENABLE_TIMEOUT | 2.0 s | [J:7](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:7>) | Gen无轴使能流程 |
| kCartesianJogLeaseMs | 500 ms | `arm_manual_ads_service.cpp`:17 | 不属于Gen |
| dt | PLC实际周期，任务1000µs | [J:29](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:29>) | 所有验收固定0.001s |

NC取值依据：`C从端/250902/250902.tsproj`:7611–7653 将定位臂五轴映射到 NC Axis1–5；1470为Axis1 `Maximum="30"`；1613、1755、1898、2040为Axis2–5 Velo，只含SlowManual/FastManual。未把手动速度当最大速度。因数据不完整，按需求全部回退10；待用户按NC参数填写后，须同步修改JG/R的同名常量并重跑验收。

| 档位 | X/Y mm/s | Z mm/s | 俯仰/偏航 °/s |
|---|---:|---:|---:|
| 快 | 30 | 3 | 1.2 |
| 中 | 15 | 2.5 | 0.8 |
| 慢 | 5 | 1.25 | 0.4 |

### 5.1 运算对应

| ST 段及行 | C++参考行 | 对应操作 |
|---|---|---|
| A，[JG:50–58](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:50>) | [R:23–30](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:23>) | 初始化指令、锚点前状态、速度过渡；保存前周期关节速度 |
| B，[JG:59–69](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:59>) | [R:31–40](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:31>) | held匹配锁存模式/方向得到target；目标变化重置v0/v1/T/τ；先τ+=dt再计算三次速度 |
| C，[JG:70–83](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:70>) | [R:41–52](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:41>) | 零速换模式/方向锚定；保留/选择肘分支；计算初始腕点；start周期不移动 |
| D，[JG:84–99](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:84>) | [R:53–65](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:53>) | 单轴k限速、σ矩形积分；轴1/5负、轴4正；其它轴逐位保持 |
| E，[JG:100–111](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:100>) | [R:66–77](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:66>) | 从当前指令点求单位路径Jacobian逆，rad/s换°/s；g4=−g2−g3 |
| F，[JG:112–123](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:112>) | [R:78–88](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:78>) | 轴2/3/4等比k；求下一腕点和半径平方；按Q3_MARGIN计算环带 |
| G，[JG:124–128](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:124>) | [R:89–92](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:89>) | 出环带时回退到前一腕点、速度0、boundary=TRUE |
| H，[JG:129–136](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:129>) | [R:93–99](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:93>) | 同分支ACOS闭式逆解；利用L1>L2的正分母ATAN求q2 |
| I，[JG:137–149](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:137>) | [R:100–111](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:100>) | 分别展开多圈角；q4补偿；指令速度用本周期起点g；σ累加 |
| J，[JG:150–152](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:150>) | [R:112–113](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:112>) | limited=(k<1)或boundary；a=(v−previous_v)/dt |

ATAN2未引入新PLC库：腕点方位用 `ACOS(−wy/r)`，wx<0时取负；另一个反正切分母 `L1+L2·cosα≥20mm` 恒正。ST和C++使用相同写法与运算次序。参考模型数组索引相差1，赋值数组/清零循环为等价逐元素操作。

`tests/translate_cart_jog_st.py` 消费JG全部1173个语句词元，生成228行比对用C++。`arm_cart_jog_equivalence.cpp` 逐周期比较五轴p/v/a、path_speed、target_speed、limited、boundary、分支和锁存模式/方向，并校验输出有限；同名常量逐项比较。两分支×五个首模式，包含升降档、换模式、释放再按、反向，共120000周期，所有数值最大差0。该检查证明模型与保存的ST语句一致，**不等同于TwinCAT编译器产物或MC2运行时验证**。

## 6. 离线验收结果（§8.1–8.6）

### 6.1 构建

| 对象 | 配置与结果 | 日志 |
|---|---|---|
| 数据端 ADS.sln | VS2022 MSBuild，v142，Release/x64，最终Rebuild成功，0警告、0错误；修改前构建也成功且无编译警告记录 | `ads_before.log`、`ads_final.log` |
| WPF MasterConsole.sln | 仓库Get-ViDotnet解析的SDK，Release/x64，0警告、0错误；修改前同为0/0 | `wpf_before.log`、`wpf_final.log` |
| WPF测试工程 | VI.Control.Tests，Release/x64，0警告、0错误；182项检查通过 | `wpf_tests_build.log`、`wpf_tests.log` |
| C++离线模型/ADS mock/运动学 | MSVC14.29，C++17、/O2、/MD、/W3；各测试程序构建成功 | `*_build.log` |
| PLC | **未构建**。本机TcSysSrv正在运行，安装目录含ADS/Runtime；未找到XAE/PLC编译器，Components/Plc无编译器文件，VS无可用TwinCAT工程构建组件 | `st_static_audit.log`、`environment.log` |

独立CMake配置曾在本机进程退出−1073740791；未把它当作源码编译失败，也未改动无关工具链。离线脚本直接使用仓库生产配置对应的MSVC14.29编译。旧归零捕获曾用/W4输出现有Eigen C4127等警告；生产解决方案的最终Rebuild为0警告，没有以这些第三方测试告警冒充新增生产告警。

### 6.2 归零回归（先捕获再修改）

随机种子20261104、mt19937、100组起点：q1∈[100,400]mm；q2–q4∈[−360,360]°；q5∈[−60,60]°；固定五轴速度5、加速度50（PLC单位）。改动前旧plan_arm_curve生成 `tests/home_baseline.txt`，保存每个输入/输出double的uint64位模式，以及error/already_home。100组均error=0、already_home=false。

基线SHA256：`6c1af930624100da6a7109eb3b58f655e4d2ca87ef33cb59b4a8b08c6c7e118b`。

新生产代码验证100组、1700个double加两个状态字段，mismatches=0。另有已有“已在零位”测试及ADS归零提交/完成互斥检查通过。`program_zero.cpp` 未改；PLC原归零公式与阶段语句静态逐条比对通过。构建目录中Z负号实验mismatches=48，实验文件未用于生产工程，也未覆盖基线。

### 6.3 T1–T12

真值使用实际 PositioningArmKinematics，校准和PLC单位与plan_arm_curve相同，轴1–4不设范围，轴5真值配置±60°；新点动发生器本身没有该限位。所有周期dt=0.001s。T1随机1000组含两分支；T2遍历X/Y×三档×有效肘角30/90/150°，连续3s再松开，选取没有到边界的方向。

| 测试 | 结果 | 实测值及范围 |
|---|---|---|
| T1 | PASS | 腕点最大误差1.25055521494e−12mm；逆解回代最大角误差2.0783375021e−13rad |
| T2 | PASS | 最大直线偏离5.13478148889e−13mm；Z/偏航/俯仰变化的合并最大值5.08888749034e−14（mm或°）；轴1/5逐位不变，无边界停顿 |
| T3 | PASS | 到慢档5mm/s用时1.000s且单调；峰加速度7.49999mm/s²；松开0.500s归零；距离1.2475mm，相对1.25mm误差−0.2% |
| T4 | PASS | 下方15种停止距离；X/Y/Z均<15mm |
| T5 | PASS（Gen） | 15→30实际路径速度1.000s，30→5用0.500s；速度不经过0；该场景达到实际目标值，其它构型仍受k限制 |
| T6 | PASS（Gen+阶段静态核验） | X→Y在0.500s零速锚定，随后Y起步；Gen无错误输出，阶段代码无离开25的换模式分支；未运行PLC轴握手 |
| T7 | PASS | 松开0.2s后同向再按，最低3.24mm/s、未到0，重新达到5mm/s |
| T8 | PASS（age判据+Gen） | 第500个alive不变周期开始减速；停止距离1.2475mm；原错误路径不参与。未宣称执行了PLC运行时 |
| T9 | PASS | 伸直方向与反向各周期关节速度≤10；缩放时limited为真；边界有效肘角0.503297271926°，反向后20.1968505949°；分支不变、有限值通过 |
| T10 | **FAIL** | 轴隔离和三个正方向通过；连续积分误差Z=0.00150000000008mm，俯仰/偏航各0.000600000000053°，超1e−6 |
| T11 | **FAIL** | 除当周期boundary外检查每周期；最大1.48628374195°/s，T9反向，超1e−3 |
| T12 | PASS | Z三档≤轴1 cap10；旋转三档≤轴4/5 cap10；WPF逐项核对唯一速度表 |
| OFFSET/FINITE | PASS | 基座平移的FK/Jacobian/IK一致，所有常规场景指令p/v/a/path_speed为有限值 |

停止距离不含通信延迟，单位按模式。均使用规定矩形积分，因此比连续公式 `v·T_DEC/2` 小0.2%。

| 模式 | 快 | 中 | 慢 |
|---|---:|---:|---:|
| X mm | 7.485 | 3.7425 | 1.2475 |
| Y mm | 7.485 | 3.7425 | 1.2475 |
| Z mm | 0.7485 | 0.62375 | 0.311875 |
| 俯仰 ° | 0.2994 | 0.1996 | 0.0998 |
| 偏航 ° | 0.2994 | 0.1996 | 0.0998 |

以典型0.1s延迟估算X/Y快档为3+7.485=10.485mm；0.5s通信租约为15+7.485=22.485mm。此为公式估算，未测现场网络总停止距离，不替代现场≤15mm验收。

T11逐场景最大值：T2=0.0220900813472；T3=0.00163682273663；T4=0.010797537345；T5=0.00941577431344；T6=0.00512692366522；T7=0.00156628546473；T8=0.00164917241166；T9=0.0993343925211；T9反向=1.48628374195；T10=0.00224999699882。单位按该周期所涉轴为mm/s或°/s。

额外起点诊断：机械 q2=q3=q4=q5=0，q1=100mm，X+5mm/s连续2s，path_speed=0、boundary=TRUE、q3=0，详见同一测试日志末尾。此诊断未纳入T1–T12通过计数，但直接限制“任一构型按住连续运动”的完成结论。

### 6.4 三端回归

ADS mock验证四字段同一SumWrite、alive续发、换模式/方向/速度不发曲线、speedScale完全忽略且不改关节参数、500ms租约、松开后只写一次零请求、归零互斥、复位/断使能/不健康门控/退出清除。模拟快照读失败期间仍每周期写点动保活；租约观察值见 `arm_ads_tests.log`（523ms，受50ms工作周期与主机调度影响）。测试替换传输实现，没有连接真实ADS/PLC。

协议C++黄金样本/严格解码81项通过，C#协议/服务116项通过；四个新字段的签名、序列化、反序列化和RemoteRobotLink映射检查通过。使用 `-SkipNetwork`，未运行真实Zenoh router跨进程网络场景。

WPF182项检查通过，包含末端按住后状态年龄达到原StatusTimeout时两按钮不禁用、保活不发停止且继续发同一请求、3001/3002/3003/NC错误优先显示、limited仅在state2/3显示、状态消失后清空及属性通知。已有单轴松开、归零入口和折叠/关窗停止回归通过。

### 6.5 ST自查、Scope与TcUnit

静态自查逐项记录在 `st_static_audit.log`：

1. 改动的GVL/POU/plcproj均为有效XML；IF/FOR/WHILE/CASE嵌套配对通过，未引入新PLC库。
2. 输入/输出类型为工程已有BOOL/INT/UDINT/DINT/LREAL及固定数组；ADS映射为bool/int16/uint32/double。
3. 只使用已有Tc2_MC2的Enable/Feed/Disable/Stop，轴引用、PositionType、Deceleration/Jerk实参与现有用法一致。
4. 纯Gen不访问G/轴I/O；FB中Feed仅owned且阶段10/25，零速方向保持。
5. MAIN:95–96 中ArmCartJog紧跟ArmCartesian；PlcTask.TcTTO任务1000µs；两个新POU已注册。
6. 新旧drive_ready相同；归零数学语句和阶段10/30/40/50/60在删除排队字段后相同；阶段20只留下原归零分支。
7. 复位等待cjog所有权释放，单轴allow_jog排除cjog；归零占用拒绝不影响点动请求。
8. 原文件UTF-8/BOM和换行风格按改动前副本核对，保留。

本机有运行中的TcSysSrv，并且仓库有历史Scope工程；但没有可用XAE编译器，也没有已加载本次新FB的NC仿真配置。**未做**T2“X、中档、有效肘角90°”的Scope导出及≤1e−6比较，未切换现有NC轴模式，未改运行中的PLC。仓库PLC源码无TcUnit，按要求未引入，因此没有新增PLC单元测试。ST转译比对不覆盖MC2状态机实际握手/故障行为。

### 6.6 删除项静态检查

全仓库源码搜索以下旧引用均为0：G.arm_curve_buffered；ArmCartesian内can_queue/queued/start_velocity；`values[16] == 1`；cartesian_acceleration_override_；kKnownGoodZSpeedMmPerSec；点动错误码2009/2010。`plan_arm_curve` 仅保留mode0。日志/本报告中保留这些名字用于说明删除证据，不是产品引用。

没有增加新的PLC关节/工作空间软限位、轴5±60°点动检查、跟随误差检查、NC参数运行时读取、内部NaN分支、额外超时、重试或旧接口回退。ArmCartesian原归零校验和已有跟随检查保留，NC轴配置未修改。测试程序中的有限值检查、断言和有界等待仅验证模型/测试基础设施，不写入产品控制逻辑。

## 7. 可复现命令与证据目录

在仓库“介入机器人”目录的PowerShell执行：

```powershell
chcp 65001 > $null
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
python 'B数据端/工程/PositioningArmKinematics/tests/run_cart_jog_acceptance.py'
```

脚本通过vswhere选择VS，调用v142环境，编译到被忽略的 `PositioningArmKinematics/build-cart-jog`，日志写入本报告同级 `acceptance_cart_jog`；不会连接PLC，不重写归零基线。当前预期退出码1，因为T10/T11及Z负号诊断明确失败；`results.json` 分别列出各步骤退出码。

```powershell
& 'D:/Work_software/VS2022/MSBuild/Current/Bin/MSBuild.exe' `
  'B数据端/工程/ADS项目/ADS.sln' /t:Rebuild `
  /p:Configuration=Release /p:Platform=x64 /m:2 /v:normal /nologo
$dotnet = & './A主端/工程/tools/Get-ViDotnet.ps1'
& $dotnet build 'A主端/工程/master/MasterConsole.sln' -c Release -p:Platform=x64 -m:2 -v:minimal
& $dotnet build 'A主端/工程/master/VI.Control.Tests/VI.Control.Tests.csproj' -c Release -p:Platform=x64 -m:2 -v:minimal
& './A主端/工程/build/Release/bin/VI.Control.Tests/VI.Control.Tests.exe' `
  'B数据端/工程/PositioningArmKinematics/build-cart-jog/UiChecks'
& './B数据端/工程/ADS项目/tests/run-zenoh-tests.ps1' -SkipNetwork
```

证据清单：`ads_final.log`、`wpf_final.log`、`wpf_tests_build.log`、`wpf_tests.log`、`home_capture.log`、`home_regression.log`、`home_z_negative.log`、`arm_cart_jog_tests.log`、`st_translation.log`、`arm_cart_jog_equivalence.log`、`arm_ads_tests.log`、`kinematics_tests.log`、`zenoh_final.log`、`st_static_audit.log`、`removed_references.log`、`environment.log`、`results.json`。旧捕获编译器输出也保留，方便追溯实际前态。

## 8. 现场验收清单（用户执行）

当前未满足离线全部通过这一前提；以下保留需求指定清单供问题解决后执行。**PLC修改需要用户重新下载并激活**，同时部署匹配的数据端和上位机；仅更新可执行文件不会使新PLC符号生效。

- [ ] 下载并激活 PLC，在线确认 G.arm_cjog_if_version = 1，数据端启动日志打印相同的版本号。
- [ ] 五个模式 × 三档：按住 10 s 连续运动，约 1 s 达到设定速度；松开后平滑停止，X/Y 停止距离不超过 15 mm。
- [ ] 按住 X 时切换速度档：平滑变速，不停车。
- [ ] 让两连杆接近伸直后按住 X 或 Y：自动降速并显示提示，无错误码。
- [ ] 按住时关闭上位机或断开网络：约 1 s 内停止，G.arm_cjog_error = 0。
- [ ] 按住时关闭总使能：故障停机，界面显示 3002。
- [ ] 归零功能、单轴点动功能与修改前一致。
