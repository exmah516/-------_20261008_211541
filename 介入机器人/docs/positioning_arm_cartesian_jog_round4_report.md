# 第四轮交付：故障释放已修复，奇异制动按规格停止

日期：2026-11-07。修改基准：`713d8830aaf04cd23b1547c3f0da03528f5c5d94`。**本轮整体未通过。修复A完成离线验证；修复B因试探条件不单调，按第四轮§3.3的停止条款未实施。S1、S2、S4未通过。** 第三轮偏航保持TCP的实现已存在，本轮没有重做第三轮。

## 1. 修改与范围

生产代码只修改两个功能块：

- [ArmCartJog.TcPOU:108](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:108>)：错误轴不执行MC_Stop；Disable/Stop的Done、Error或NC轴错误均算该轴完成；阶段60只等待Stop.Busy清除；错误码保留第一个，下一次成功起步清零。
- [ArmCartesian.TcPOU:228](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartesian.TcPOU:228>)：归零存在相同死锁，按同样规则修改停止和释放；归零曲线的位置、速度、加速度计算语句不变。

新增S5阶段机参考模型、脚本FB接口和逐周期比较；新增S1–S4诊断与非单调反例；现有验收器增加回归采样及全周期加速度统计。没有修改ArmCartJogGen、其C++参考模型、ArmManual、数据端生产代码、上位机、协议、GVL或MAIN。源码不变核对见[scope_and_build_summary.json](acceptance_cart_jog/round4/scope_and_build_summary.json)。

源码差异见[本轮diff](positioning_arm_cartesian_jog_round4.diff)，新基线文件单独保存在tests内，不把二进制嵌入文本diff。没有新增超时、重试、内部数值检查、限幅或替代算法。测试中的NaN/Inf检查只用于离线验收。

## 2. 修复A：停止与复位通路

| 阶段 | 修改后的每轴判据 | 转移 |
|---|---|---|
| 40 | Disable.Done OR Disable.Error OR axis.Status.Error | 五轴完成后，正常→60，故障→50 |
| 50 | 错误轴Execute=FALSE；其余轴MC_Stop；Stop.Done OR Stop.Error OR axis.Status.Error | 五轴完成→60 |
| 60 | NOT Stop.Busy | 五轴均释放→owned=FALSE、阶段0 |

ArmCartJog的上述代码分别见[108](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:108>)、[112](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:112>)、[114](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:114>)、[125](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:125>)。FB错误仅在error=0时写入，轴循环及Enable/Disable/Stop顺序决定同周期首个错误；驱动就绪检测在FB循环之前，因此该检测先触发时保留3002。后续Stop的0x4650不会覆盖3002。成功起步清零见[66](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:66>)。保留`stage<40`用于从运行进入停止；阶段40/50随后按Done/Error/轴错误完成判据继续转移，不再把FB错误当作驻留条件。

**ArmCartesian确有同类死锁。** 修改前阶段50要求`Stop.Done AND NOT Stop.Error`，阶段60还要求`NOT Stop.Error`；错误轴无法满足，所有权一直不释放。修复后的停止调用、完成条件和释放见[228](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartesian.TcPOU:228>)、[232](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartesian.TcPOU:232>)、[254](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartesian.TcPOU:254>)。归零根因码也只在当前为0时写入，下一条成功归零清零。

ArmManual未改：[73](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmManual.TcPOU:73>)的复位请求在两种owned均为FALSE后设置reset_exec；[86](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmManual.TcPOU:86>)在axis_state=0时执行MC_Reset；其Stop.Error分支在[136](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmManual.TcPOU:136>)已将axis_state归0。因此所有权释放后，仍保持的复位请求能沿原逻辑执行MC_Reset。实际驱动是否能复位仍取决于驱动故障消除情况。

### S5：脚本驱动阶段机

FB的Done/Error/Busy/Enabled与NC轴错误由脚本直接提供，不模拟NC内部。测试代码见[arm_cart_jog_stage_tests.cpp:67](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_stage_tests.cpp:67>)。

| 场景 | 实测 | 判定 |
|---|---|---|
| S5.1 阶段25轴2错误，随后Stop也报0x4650 | 错误轴Stop.Execute始终FALSE；其余轴第5周期Done；第6周期Busy仍为TRUE时保持owned；第7周期释放并回到0，距Done为2周期；error保持3002 | PASS |
| S5.2 阶段40轴3Disable报0x1234，随后另一轴Stop报0x5678 | 第7周期释放，距其余轴Done为2周期；error保持首个0x1234（4660） | PASS |
| S5.3 正常松开 | 3200周期、2,387,200字节，与修改前ST基线、修改后ST转译和手写参考模型逐位一致 | PASS |
| 首错保持与再次起步 | 原0x4650在非法请求期间保留；下一次合法且驱动就绪的成功起步清零 | PASS |

累计3232个脚本周期，数值最大差0，序列化字段逐位一致，阶段完成标志逐周期一致。[arm_cart_jog_stage_tests.log](acceptance_cart_jog/round4/arm_cart_jog_stage_tests.log)。

阶段机转译新增时，测试工具曾把ST布尔赋值右侧的`=`误译为C++赋值，已修正并加入断言。正常松开的有效基线是用**上述固定提交中的原始ST**重新转译生成，源码SHA256为`ec5d861a8c24f5faf8251090e1ecd1da1d6091ecf37b2cd54e879b457f068920`，没有用修复后ST生成基线。基线SHA256和提交号已写入[results.json](acceptance_cart_jog/round4/results.json)。

## 3. 修复B停止的可复现证据

第四轮§3.3声称试探条件随步长单调变严，且要求发现不单调时停止。该声称在`δ_now<0`时不成立。

源码腕点公式位于[ArmCartJogGen.TcPOU:86](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:86>)，X/Y环带、闭式逆解和关节步长条件位于[165](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:165>)、[183](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:183>)、[190](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:190>)。取：

```text
(q1,q2,q3,q4,q5)=(100,-31.568598207665932,-4,0,0)
α_now=176°，δ_now=-1°
W=(-0.00190000000000623,-23.987254484051618) mm
方向X+，dt=0.001 s，Δσ_des=0.005 mm（慢档5 mm/s）
```

| 试探步长mm | δ_try (°) | 旧环带与三关节步长 | 新条件δ_try≥δ_now |
|---:|---:|:---:|:---:|
| 0.001 | -1.0000000319422497 | 通过 | 拒绝 |
| 0.0025 | -1.0000000370756368 | 通过 | 拒绝 |
| 0.004 | -0.9999999908736470 | 通过 | 接受 |
| 0.005 | -0.9999999315526793 | 关节步长拒绝 | 完整条件拒绝 |

0.004mm试探的三关节增量分别约`0.009554314828° / -9.1264e-9° / -0.009554305702°`，均小于0.01°，是真实可接受的正步长。完整步长0.005mm的轴2增量约0.011942545330°，被旧限速拒绝；二分第一次试0.0025mm又被新δ条件拒绝，于是排除了含0.004mm的上半区。固定12次二分最终接受步长为0，却存在0.004mm可接受步长。

这不是舍入误判：`r(s)²=r0²+2·Wx·s+s²`，Wx<0时半径先减小、越过最近点后增大。在折叠侧奇异区中，δ随半径增大而增加，所以δ先降后升。接受集合不是从零开始的连续前缀；二分的单调前提失败。可复现程序见[arm_cart_jog_round4_diagnostics.cpp:91](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_round4_diagnostics.cpp:91>)，实测见[arm_cart_jog_round4_diagnostics.log](acceptance_cart_jog/round4/arm_cart_jog_round4_diagnostics.log)中的B_NONMONOTONE、B_TRIAL、B_BISECTION。

另一个独立限制：零位Y−的首周期使`δ=-5→-4.999834656422536°`，符合`δ_now<0`分支“只允许离开”的条件，但现有闭式起步前馈加速度为轴2 `-1488.092196896°/s²`、轴3 `-165.343577477°/s²`、轴4 `1653.435774373°/s²`。该分支本身没有加速度约束，不能据此保证S4。没有排除这个周期，也没有为其增加未授权逻辑。

因此模式1/2/5的修复B整体停止；**ArmCartJogGen及参考模型保持原样，没有加入SING_MARGIN或ELBOW_BRAKE_ACC生产常量**。下面S1–S4为当前第三轮发生器的诊断，不是修复B的通过证明。

## 4. 回归与全部测试结果

### 修改前基线

| 场景 | 个数 | 周期 |
|---|---:|---:|
| T2（X/Y×3档×3构型） | 18 | 63018 |
| T3、T5、T6、T7、T8 | 5 | 18601 |
| T10 Z、俯仰 | 2 | 4000 |
| Y1（9构型×两方向） | 18 | 63000 |
| Y2、Y6 | 2 | 11501 |
| 合计 | 45 | 160120 |

修改后五轴p/v/a、path_speed和limited **0字节差异**。基线文件`tests/arm_cart_jog_round4_baseline.bin`，20,655,848字节，SHA256=`a0673d668432f991c276f6a662fec4af5a8dd6ab918a9637a1f63c1c07cfb877`。既有第二轮基线30场景、99,619周期也通过。归零100组、1700个double及error/already_home为**0 mismatch**。见[arm_cart_jog_round4_regression.log](acceptance_cart_jog/round4/arm_cart_jog_round4_regression.log)、[arm_cart_jog_regression.log](acceptance_cart_jog/round4/arm_cart_jog_regression.log)、[home_regression.log](acceptance_cart_jog/round4/home_regression.log)。

### S1：现场位姿，四方向×慢快档，各5s

q1取100mm，其余轴按用户现场值。下表加速度为轴2/3/4所有周期的最大绝对值；周期从1计。全部有限、关节速度不超限，直线最大偏离`6.662e-13 mm`。

| 方向 | 档位mm/s | TCP位移mm | 最终α° | 最小δ° | 最大关节加速度°/s² | 峰值周期/轴 | 判定 |
|---|---|---:|---:|---:|---:|---|---|
| X+ | 慢5 | 22.5 | -23.2652171 | 7.4924 | 5.29302864 | 450/3 | PASS |
| X+ | 快30 | 131.042027 | -33.0512363 | 7.4924 | 25.9251773 | 344/3 | PASS |
| X− | 慢5 | 7.93305054 | -0.0037832111 | -4.99621679 | 7471.53941 | 2340/3 | FAIL |
| X− | 快30 | 7.93304983 | -0.00536106439 | -4.99463894 | 7963.81978 | 1530/3 | FAIL |
| Y+ | 慢5 | 22.5 | -40.8616447 | 7.4924 | 16.7703134 | 393/3 | PASS |
| Y+ | 快30 | 52.5179933 | -61.0115183 | 7.4924 | 72.0919093 | 229/3 | PASS |
| Y− | 慢5 | 2.33847838 | -0.00462401529 | -4.99537598 | 8947.52494 | 1630/3 | FAIL |
| Y− | 快30 | 2.33846726 | -0.0276211759 | -4.97237882 | 7753.17806 | 1397/3 | FAIL |

X−、Y−进入裕量以内并在边界产生速度跳变，S1整体FAIL。最终路径速度均为0且limited=TRUE，不能据此称为平滑停止。**本轮无法离线确认这些指令加速度是否导致现场轴2的0x4650；故障根因以驱动器自身记录为准。**

### S2：有效肘角10°，径向伸直5s后反向3s

| 档位mm/s | 阶段 | 距初始TCP位移mm | 最终α° | 末路径速度mm/s | 累计最大关节加速度°/s² | 峰值周期/轴 |
|---|---|---:|---:|---:|---:|---|
| 慢5 | 向外5s | 1.44200033 | 0.0090650695 | 0 | 3849.05888 | 1324/3 |
| 慢5 | 再反向3s（累计） | 6.4864715 | 23.4822404 | 5 | 8775.05904 | 5052/3 |
| 快30 | 向外5s | 1.44199435 | 0.0222796332 | 0 | 9405.96991 | 1130/3 |
| 快30 | 再反向3s（累计） | 7.47302893 | 24.9058857 | 7.12646484 | 9998.65112 | 5189/3 |

两档均停在接近0°而非≥5°，加速度超200，S2 FAIL。反向后分支保持并离开边界；慢档末速5mm/s，快档仍被关节限速，末速7.12646484375mm/s。

### S3：零位四方向，与第二轮T13对比

| 方向 | 2s位移mm | 末路径速度mm/s | 最大关节加速度°/s² | 最终limited | 与第二轮T13 |
|---|---:|---:|---:|:---:|---|
| X+ | 7.5 | 5 | 11.3921624 | 0 | 数值相同 |
| X− | 2.99535409 | 1.73583984 | 42.0108319 | 1 | 数值相同 |
| Y+ | 0 | 0 | 0 | 1 | 数值相同 |
| Y− | 1.13264188 | 1.18652344 | 1653.43577 | 1 | 数值相同 |

X±、Y−产生位移；Y+朝向轴2转轴，不动且limited。方向、有限性等S3条款PASS；Y−首周期加速度超过S4阈值，未豁免。

### S4：全部原有运动用例的每周期统计

下表覆盖受限、到边界、起步和反向周期。周期号在同名用例的所有构型中累计；T13累计6001即零位Y−第1周期，Y5累计2001即负偏航第1周期。Z与俯仰不属于模式1/2/5，无平面加速度条目。

| 用例 | 最大加速度°/s² | 累计周期 / 轴 | 上周期→本周期速度°/s | limited | ≤200 |
|---|---:|---|---|:---:|:---:|
| T1 | 0.209892306 | 573 / 3 | 0 → -0.000209892306 | 0 | PASS |
| T13 | 1653.43577 | 6001 / 4 | 0 → 1.65343577 | 0 | FAIL |
| T2 | 43.44254 | 10244 / 2 | 7.53723582 → 7.49379328 | 0 | PASS |
| T3 | 3.27772733 | 2252 / 2 | 0.545338405 → 0.542060677 | 0 | PASS |
| T4 | 30.7284659 | 32266 / 4 | 5.07218505 → 5.04145658 | 0 | PASS |
| T5 | 18.9346639 | 4253 / 2 | 4.4140677 → 4.39513304 | 0 | PASS |
| T6 | 10.2848331 | 2253 / 2 | 1.70395587 → 1.69367104 | 0 | PASS |
| T7 | 3.13684279 | 2200 / 2 | 0.71328069 → 0.710143847 | 0 | PASS |
| T8 | 3.30233968 | 2751 / 2 | 0.549413523 → 0.546111183 | 0 | PASS |
| T9 | 3849.05888 | 1324 / 3 | -3.84905888 → 0 | 1 | FAIL |
| T9 reverse | 8775.05904 | 52 / 3 | 0 → -8.77505904 | 1 | FAIL |
| Y1 | 20.6122052 | 45251 / 4 | 3.44298225 → 3.42237005 | 0 | PASS |
| Y2 | 9.35789282 | 2253 / 4 | 1.54993897 → 1.54058108 | 0 | PASS |
| Y3 | 30.4081196 | 54502 / 4 | 9.9393974 → 9.96980552 | 0 | PASS |
| Y4 | 1895.39407 | 1218 / 3 | -9.97566877 → -8.0802747 | 1 | FAIL |
| Y4 reverse | 8819.79353 | 254 / 3 | 0 → -8.81979353 | 1 | FAIL |
| Y5 | 2057.47736 | 2001 / 4 | 0 → 2.05747736 | 0 | FAIL |
| Y6 | 9.47097186 | 5752 / 4 | 1.57794811 → 1.56847714 | 0 | PASS |

新增S1–S3的峰值与具体周期见上表及原始日志，S2快档反向累计第5189周期为`0→-9.99865111748°/s`，加速度`9998.65111748°/s²`。S4整体FAIL。已有T9、Y4在几何边界附近，位置差分速度由接近限速突然降到零或反向重启；零位T13/Y5的闭式解在奇异起步时也产生首周期峰值。这些周期均计入。

### 受影响旧用例的新旧对比

B未实施，发生器源码及以下数值均未变化；35条原始结果行与第三轮日志逐行相同，见[新旧结果对比](acceptance_cart_jog/round4/affected_cases_comparison.json)：

| 用例 | 第二/第三轮结果 → 本轮结果 | 本轮判断 |
|---|---|---|
| T9 | 向外运动α=0.009065069502°、末速0；反向2.287s达到5mm/s → 相同 | 原T9几何判据PASS；替代验收S2/S4 FAIL |
| T13 | X+7.5mm、X−2.99535408713mm、Y+0、Y−1.13264187977mm → 相同 | S3方向PASS；S4 FAIL |
| Y3 | 27项降速结果均与第三轮日志一致；本轮最大关节速度约10°/s，最大加速度30.408119585°/s² | 原Y3与S4均PASS |
| Y4 | 向外第1218周期停住；反向3s末速0.5658203125°/s → 相同 | 原几何判据PASS；S4 FAIL |
| Y5 | 正偏航2s为0、末速0；负偏航−0.161867881708°、末速0.2015625°/s → 相同 | 原Y5几何判据PASS；S4 FAIL |

T1–T13及Y1–Y6的原判据全部重跑PASS（本轮S2/S4阈值仍FAIL）：T1腕点误差`1.25056e-12mm`、逆解`4.61853e-14rad`；T2直线`4.9960e-13mm`；T3达速1.001s、停止0.501s、停止1.25mm；T4停止表不变；T5过渡1.001/0.501s；T6零速换模式0.5s；T7最低3.24000263824mm/s；T8第500周期开始减速；T10连续积分误差≤8.527e-14；T12 Z/俯仰速度表通过。Y1最大TCP误差`1.43729e-12mm`，Y2停止0.2°，Y6锚定周期2500/6000、TCP误差`1.06054e-12mm`。完整每项实测值见[arm_cart_jog_tests.log](acceptance_cart_jog/round4/arm_cart_jog_tests.log)。

T11保持第二轮判据：T7交界第2201周期路径加速度`7.19132176mm/s²`，对应B值`15/15/2.64mm/s²`、允许15.15；比值0.479421450667。全部用例最大`加速度/max(B_j)=0.999998666667`，前馈速度和加速度的差分一致性最大差均0。新增S1–S3的64000周期也按同一T11判据逐周期检查，最大比值`0.99999866666667314`、两项差分误差均0，PASS。

## 5. ST与C++对照及常量

| ArmCartJog ST | 手写参考模型 | 内容 |
|---|---|---|
| [29](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:29>) | [14](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_stage_reference.h:14>) | 周期与FirstCycle初始化 |
| [38](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:38>) | [17](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_stage_reference.h:17>) | alive、请求边界、held、首个参数错误 |
| [51](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:51>) | [23](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_stage_reference.h:23>) | drive_ready五轴条件 |
| [58](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:58>) | [27](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_stage_reference.h:27>) | 成功起步、锚定、清错、取得所有权 |
| [74](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:74>) | [34](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_stage_reference.h:34>) | 失去就绪、使能超时、正常零速退出 |
| [98](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:98>) | [45](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_stage_reference.h:45>) | Feed方向、三个FB调用 |
| [111](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:111>) | [52](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_stage_reference.h:52>) | Done/Error/轴错误完成判据、Busy释放、首错保持 |
| [125](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:125>) | [65](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_stage_reference.h:65>) | 10/40/50/60阶段转移 |
| [137](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:137>) | [71](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_stage_reference.h:71>) | 状态映射与诊断字段 |

Gen未改，逐段对照延续[第三轮报告](positioning_arm_cartesian_jog_round3_report.md)。本轮在原T/Y测试的共同tick中直接比较ST转译和参考模型，覆盖实际T1–T13、Y1–Y6的**301853周期**；S1–S3另比较**64000周期**；既有混合/边界等价套件**761218周期**。三组数值最大差均0，limited、branch、mode、dir逐周期一致。S5另3232周期、数值差0。

| 常量 | ST | C++ | 本轮状态 |
|---|---|---|---|
| ALIVE_TIMEOUT | ArmCartJog:6，0.5s | JogStageReference:6，0.5s | 未变，static_assert对照 |
| ENABLE_TIMEOUT | ArmCartJog:7，2.0s | JogStageReference:6，2.0s | 未变，static_assert对照 |
| SING_MARGIN | 未加入生产发生器 | 仅诊断判据用5° | B停止；不声称实现 |
| ELBOW_BRAKE_ACC | 未加入 | 未加入 | B停止；不声称实现 |

T_ACC/T_DEC、JOINT_VCAP、INITIAL_BRANCH、工具参数等Gen常量全未变，既有逐项static_assert和数组比较继续通过。

静态自查全部通过：XML可解析；IF/FOR/WHILE/CASE嵌套平衡；MAIN调用顺序与1ms任务不变；驱动就绪条件一致；纯Gen无轴I/O；仅使用工程已有Tc2_MC2 FB/函数及已有的`G.arm_axis[i].Status.Error`；40/50按Error完成，60仅等待Busy；ArmManual源码未改；不存在TcUnit，未引入。ST转译消费Gen 1735个token、ArmCartJog 1109个token，无未处理语句。静态自查不能代替TwinCAT类型检查和NC执行。见[st_static_audit_round4.log](acceptance_cart_jog/round4/st_static_audit_round4.log)。

## 6. 单周期最坏计算量（当前第三轮Gen未改）

按ST源码调用次数给出上界，未假设编译器会合并相同SIN/COS；三角函数包括ACOS、ATAN。完整试探1次+二分12次=**最多13次闭式逆解**；落在环带外的试探会跳过逆解，所以实际可能更少。

| 模式 | 常规周期最多逆解 | SIN | COS | ACOS | ATAN | 三角调用总数 | SQRT |
|---|---:|---:|---:|---:|---:|---:|---:|
| X/Y | 13 | 13 | 13 | 13 | 26 | 65 | 13 |
| 偏航保持TCP | 13 | 39 | 39 | 13 | 26 | 117 | 13 |
| Z/俯仰 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

重新锚定最多另加4次SIN、5次COS（9次三角调用），因此含重锚定的源码上界为X/Y **74次**、偏航 **126次**。start=TRUE锚定后直接RETURN，仅9次、无逆解。依据：锚定[86](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:86>)，偏航试探[100](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:100>)，X/Y试探[162](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:162>)。角度展开的WHILE次数随多圈角值变化，函数调用计数不能换算成PLC最坏执行时间；本机没有测量1ms任务运行时间。

## 7. 构建、复现与未执行项

| 项目 | 结果 | 证据 |
|---|---|---|
| MasterConsole Release/x64 | 0警告、0错误 | [masterconsole_build.log](acceptance_cart_jog/round4/masterconsole_build.log) |
| VI.Control.Tests.slnx Release/x64，warnaserror | 0警告、0错误 | [wpf_build.log](acceptance_cart_jog/round4/wpf_build.log) |
| VI.Control.Tests WPF回归 | 全部通过 | [wpf_tests.log](acceptance_cart_jog/round4/wpf_tests.log) |
| ADS.sln Release/x64 Rebuild | 退出0，无编译/链接警告或错误 | [ads_build.log](acceptance_cart_jog/round4/ads_build.log) |
| C++17离线程序 | 编译均通过；诊断与S4按实际失败返回1 | [results.json](acceptance_cart_jog/round4/results.json) |
| ADS mock/运动学 | PASS；运动学20/20 | [arm_ads_tests.log](acceptance_cart_jog/round4/arm_ads_tests.log)、[kinematics_tests.log](acceptance_cart_jog/round4/kinematics_tests.log) |
| PLC | **未构建** | [environment.log](acceptance_cart_jog/round4/environment.log) |
| NC仿真/Scope/真机 | 未执行 | 本机无XAE编译器、未连接现场PLC |

本机`D:/Work_software/Twincat3_ADS/3.1/Components/Plc`只有Managed Libraries，没有PLC编译器/targets；未发现VS TwinCAT XAE扩展。未下载、未激活、未运行ADS生产程序。用户需在工程机编译、下载并激活PLC，才能让修复A实际生效。没有把桌面C++逐周期相同表述为TwinCAT运行时一致性已验证。

可复现命令（仓库根目录，正常验收不要传capture参数）：

```powershell
python 'B数据端/工程/PositioningArmKinematics/tests/run_cart_jog_acceptance.py' --round4
$taskDotnet = & './A主端/工程/tools/Get-ViDotnet.ps1'
& $taskDotnet build 'A主端/工程/master/MasterConsole.sln' -c Release -p:Platform=x64 -warnaserror -m:2 -v:minimal
& $taskDotnet build 'A主端/工程/master/VI.Control.Tests.slnx' -c Release -p:Platform=x64 -warnaserror -m:2 -v:minimal
& './A主端/工程/build/Release/bin/VI.Control.Tests/VI.Control.Tests.exe' 'B数据端/工程/PositioningArmKinematics/build-cart-jog/UiChecks'
& 'D:/Work_software/VS2022/MSBuild/Current/Bin/MSBuild.exe' 'B数据端/工程/ADS项目/ADS.sln' /t:Rebuild /p:Configuration=Release /p:Platform=x64 /nologo /verbosity:minimal
```

总验收脚本预期退出1，因为S1/S2/S4确实失败；results.json逐项保留退出码，没有豁免失败项。生产源码之外的父目录造影机器人子模块已有改动未触碰。

## 8. 现场验证步骤（用户执行）

**当前交付只有A修复，B仍未实现且S1/S4失败；以下事故位姿运动步骤待B规格与验收完成后执行，不能把本轮作为奇异区域真机运动已通过的版本。**

- [ ] 工程机Build PLC，确认0错误且不新增警告；下载并激活修复A，确认接口版本1。
- [ ] 先在NC仿真或设备允许的静止条件下制造驱动故障/断使能，记录首个error；确认40→50→60→0、owned释放；故障轴的Stop.Execute=FALSE，健康轴照常Stop，60等待Busy结束。
- [ ] 故障消除后发复位，确认MC_Reset执行、单轴点动与末端点动可恢复；对归零中同类故障也检查arm_curve_owned释放和复位恢复。恢复能力仍须工程机实际验证。
- [ ] B完成后，在用户给定轴2~5位姿分别复现X+、X−、Y+、Y−，慢/快档各5s；Scope记录p/v/a、有效α、δ、path_speed、limited，逐项核对S1，不省略边界与反向周期。
- [ ] B完成后覆盖偏航模式5、零位起步与S2反向；核对所有轴2/3/4前馈加速度≤200°/s²及PLC任务执行时间。

未验证：TwinCAT编译、运行时/Scope、真实驱动故障恢复及0x4650物理根因。剩余风险：奇异区与边界急停问题仍在，不能认定本轮全部完成。
