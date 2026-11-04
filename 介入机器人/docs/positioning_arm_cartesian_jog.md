# 定位臂 PLC 末端点动接口

更新：2026-11-05，仅修订 T11 跨过渡路径加速度判据，运动算法与接口保持原样。全量复验结果、T7交界周期与零位四方向实测见[本轮交付报告](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/positioning_arm_cartesian_jog_round2_report.md>)。本机可执行的离线项目全部通过；PLC 未构建，Scope 未做。

旧《定位臂项目修改与接口说明》docx 已过时。归零继续使用原 17 个 LREAL 单段曲线且 `[16]=0`；末端点动由下述 PLC 接口独立生成，协议命令格式不变。

## 本轮符号与计算约定

运动学模型保留 `z=+q1` 和现有右手计算基底，不反转 X、Y 或旋转定义。**运动学模型的 z 轴与现场竖直方向相反；末端点动 Z+ 在 PLC 中直接定义为轴 1 减小，与运动学模型无关。** `base_offset_mm` 默认 `(0,0,0)` 保留，只加平移。正解的 q1、Z 增量逆解和偏移分别见 [kinematics.cpp:84](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:84>)、[225](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:225>)、[100](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:100>)，默认偏移见 [kinematics.h:25](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/include/positioning_arm/kinematics.h:25>)。

由 [kinematics.cpp:86](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:86>) 的两连杆正解，PLC 单位为 mm/°，三角函数内先换算为 rad：

```text
Wx = 200 sin(q2) + 180 sin(q2+q3+π)
Wy = −200 cos(q2) − 180 cos(q2+q3+π)
α = q3+π
```

机械 q3=0 完全折叠，q3=±180° 完全伸直。`INITIAL_BRANCH=+1` 取 `α=+acos(cos α)`，从折叠位沿可达方向离开时 α 从 π 减小，因此机械轴 3 向负方向弯；无需反转该常量。折叠、伸直及其多圈等价值不能判定分支时保留上次分支。实现见 [ArmCartJogGen:74](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:74>)。

姿态仍为 `Rz(q2+q3+q4−90°)·Rx(q5)`（[kinematics.cpp:95](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:95>)）。偏航+对应轴 4 增大；俯仰+对应轴 5 减小。X/Y 保持轴 1、5，轴 4 用 `anchor4+(anchor2+anchor3)−(q2+q3)` 保持偏航。俯仰/偏航只转各自关节。

### Z定义及偏移的使用点

| 使用点 | 保留关系与影响 |
|---|---|
| [kinematics.cpp:84](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:84>) | FK升降量为+q1；98–100加入工具Z分量与base_offset_mm/1000。 |
| [kinematics.cpp:113](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:113>) | Jacobian升降列为+UnitZ；115–117以+q1和同一平移建立轴原点。 |
| [kinematics.cpp:120](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:120>) | 120–136的平面/俯仰Jacobian消费平移后的原点，位置差中抵消平移；旋转手性不变。 |
| [kinematics.cpp:153](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:153>) | 153–160、254–259、362–366的原逆解/奇异度消费上述Jacobian；PLC末端点动不再调用。 |
| [kinematics.cpp:223](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:223>) | 223–238增量逆解保留q1+=Δz；该模型API的Z+与PLC现场Z+是分别定义的方向，本轮按用户决定保留。 |
| [kinematics.cpp:274](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:274>) | 274–275从绝对腕点目标减x/y偏移；Z增量式逆解的平移自然抵消。 |
| [kinematics.cpp:376](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/kinematics.cpp:376>) | 原solveJog的Z入口仍转发到模型Z增量逆解，未用于新点动链路。 |
| [program_zero.cpp:99](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/src/program_zero.cpp:99>) | 99、112–119、208–209的归零代价/梯度/位移界读取FK/Jacobian。本轮符号与默认偏移不变，逐位回归0 mismatch。 |
| [kinematics_test.cpp:124](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/kinematics_test.cpp:124>) | 124–138、171、210–226覆盖原FK Z与旧增量API；T2/T10分别验证Z保持与PLC轴1负向。 |

生产运动学调用者仅归零规划 `arm_cartesian_motion.h`；默认偏移为0。非零偏移 `(20,−30,40) mm` 的FK、Jacobian和逆解一致性已在OFFSET用例验证。

## 本轮发生器规则

1. 三次速度曲线仍从当前名义速度平滑过渡，增速 1 s、减速 0.5 s。每周期先保存名义 `v_prev`，更新曲线得到 `v`，期望路径增量 `Δσ_des=0.5·(v_prev+v)·dt`。同向重按从当前名义速度重新加速；换模式/换向仍在名义速度到零时锚定新请求，不离开阶段 25。
2. 单轴模式的步长为 `MIN(Δσ_des,JOINT_VCAP[j]·dt)`，再依上述 Z/俯仰/偏航符号更新位置。
3. X/Y 先试完整期望步长，闭式逆解并展开到上一周期最近的 360° 等价值。候选腕点必须位于精确环带 `[L1−L2,L1+L2]=[20,380] mm`，轴 2/3/4 每轴增量必须不超过 `JOINT_VCAP[i]·dt`。完整步长不通过时固定二分 12 次，保留已验收的最大正步长；没有正步长通过就保持位置。`FOR i=0..12` 中 i=0 为完整试探，只有该试探成功才提前 EXIT，其余执行全部 12 次二分。
4. ACOS 前仅把 `cos α` 限制到 `[−1,1]`；方位角用 ATAN 与象限分解求 atan2，不增加第二个 ACOS 夹紧。原 `Q3_MARGIN`、单位路径雅可比和 k 均已删除。
5. 前馈速度严格等于位置差分/dt，前馈加速度严格等于前馈速度差分/dt。实际 `path_speed=Δσ/dt`；`limited=(Δσ<Δσ_des)`。限速和可达边界均不写错误码。

| 常量 | ST / C++参考模型同步值 | 来源 |
|---|---|---|
| PI / RAD | 3.14159265358979323846 / PI÷180 | 角度换算 |
| L1 / L2 | 200 / 180 mm | kinematics.h 的 0.200 / 0.180 m |
| T_ACC / T_DEC | 1.0 / 0.5 s | 速度过渡规格 |
| INITIAL_BRANCH | +1 | 零位起步轴 3 向负方向弯 |
| BISECTION_COUNT | 12 | 本轮固定二分次数 |
| JOINT_VCAP[1] | 24 mm/s | NC Axis1 Maximum=30 的 80%，250902.tsproj:1470 |
| JOINT_VCAP[2..5] | 各 10 °/s | 工程未给 Maximum，用户本轮确认暂用 10 |

ST 常量集中在 [ArmCartJogGen:22](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:22>)，参考模型集中在 [arm_cart_jog_reference.h:8](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:8>)。速度档仍以 `MainViewModel.GetArmCartesianJogSpeed` 为唯一生产来源；租约 500 ms、PLC 保活 0.5 s、使能 2 s 保持原定义。

## PLC 与状态接口

### GVL G

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
| G.arm_cjog_limited | BOOL | PLC→数据端 | 本周期 Δσ < Δσ_des |
| G.arm_cjog_path_speed | LREAL | PLC→数据端 | 本周期实际步长/dt，非负路径速度，mm/s 或 °/s |
| G.arm_cjog_if_version | UDINT | PLC→数据端 | 初值1 |

四个请求字段在同一个 `write_sum` 中提交（`arm_manual_ads_service.cpp`:629–633）；此处只保证使用一次 ADS SumWrite，不宣称 ADS 子项与 PLC 扫描之间具有事务原子性。

### 请求与阶段机

[ArmCartJog:39–48](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:39>)：alive 变化时 age=0，否则加实际周期。合法请求且 alive_age<0.5 s 为 held。`mode=0` 正常松开，不产生3001。合法请求在单一 ADS 边界判定；Gen 接受已判定的 held，不再次校验。

| 阶段 | 动作 | 转移 |
|---|---|---|
| 0 空闲 | mode非零且非法报3001；held且归零无所有权、request=ack时检查 drive_ready；就绪则以 G.arm_act_pos 锚定、速度/加速度0、feed方向1，取得所有权 | 就绪→10；未就绪报3002并保持0 |
| 10 使能 | 五轴 MC_ExtSetPointGenEnable，绝对位置；owned期间每周期 Feed(p,0,0) | 全部 Enabled→25；使能超过2s报3003→40 |
| 25 运行 | 每周期 Gen 与 Feed；模式/方向变化先减到0再重新锚定，阶段和所有权保持 | 无held且实际路径速度0连续2周期→40；驱动未就绪报3002→40 |
| 40 停用 | 五轴 MC_ExtSetPointGenDisable | 全停用：正常→60，故障→50 |
| 50 停止 | MC_Stop，使用原 G.arm_jog_dec/jerk | 全Done→60 |
| 60 释放 | 等待 MC_Stop不再Busy | owned=FALSE→0 |

Enable/Disable/Stop 任一 Error 保存 ErrorID；阶段<40时进入40，阶段≥40保持该阶段。Fault 路径包含 MC_Stop，正常减速结束不再额外调用 MC_Stop。Feed 仅在 owned 且阶段10/25。速度非零更新方向符号，零速保持上一值。[ArmCartJog:123–143](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:123>) 映射为对外0–4状态；25且target_speed=0为减速状态3。

drive_ready 在 [ArmCartJog:51–57](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJog.TcPOU:51>) 与原 ArmCartesian:63–69 相同：总使能、主机通信/急停、1ms周期、五轴上电Done、使能请求、轴故障、复位及单轴点动请求。归零起步额外拒绝 cjog_owned，报2001但不写点动请求；ArmManual 在复位和 allow_jog 两处排除 cjog_owned。

### 时间与状态传播

| 事件 | 行为 |
|---|---|
| 正常松开/失去鼠标捕获 | 原 ArmStop；数据端立即标脏写0/0/0/alive；PLC用0.5s减速 |
| 上位机→数据端中断 | 数据端500ms租约到期，写一次零请求后停止写入 |
| 数据端→PLC中断 | alive连续0.5s不变，PLC按松开减速，无错误码 |
| 驱动失去就绪 | 3002，停用→MC_Stop→释放 |
| 换高档/同向重新按下 | 从当前名义曲线速度用1s三次曲线过渡 |
| 换低档/松开/换模式/换向 | 用0.5s减速；换模式/换向到零后在阶段25锚定新请求 |
| 关节限速/边界 | limited=TRUE，不写错误码；无可接受正步长时保持前一指令点 |

snapshot→main.cpp 的 vs→Zenoh StateFrame→RemoteRobotLink→StatusFrame 贯通 `state/error/limited/path_speed` 四字段。[MainViewModel:391–400](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/A主端/工程/master/VI.Service.Robot/ViewModels/MainViewModel.cs:391>)：错误优先于降速提示；limited且state2/3时显示“接近奇异位形或关节限速，已自动降速”，其余为空。保留该规格原文，没有另加边界提示规则。

| 错误 | 中文显示 |
|---|---|
| 3001 | 末端点动参数无效（错误码 3001） |
| 3002 | 驱动未就绪（总使能/上电/轴故障/复位/单轴点动/通信超时/急停）（错误码 3002） |
| 3003 | 外部设定点发生器使能超时（错误码 3003） |
| 其它非零 | NC 错误 0x{ErrorID:X}，同时显示十进制错误码 |

协议沿用仓库此前扩展状态字段时的 Version=1 及 v1 topic。StateFrame 51→55项：索引50为uint32 State、51为uint32 Error、52为bool Limited、53为float64 PathSpeed、54为AuthTag。两端严格长度/类型校验和黄金样本同步更新；主端、数据端须同步部署，旧51项帧会被拒绝，没有兼容回退。ArmCartesianJog 命令格式未变；speedScale 对新末端点动完全忽略，仍供原单轴点动使用。

## 现场验收清单（用户执行）

T1–T13、归零回归、ST↔C++比对及主端/数据端构建回归均已通过；PLC编译和Scope轨迹对照仍待工程机完成。**PLC修改需要用户重新下载并激活**，同时部署匹配的数据端和上位机；仅更新可执行文件不会使新PLC符号生效。

- [ ] 下载并激活 PLC，在线确认 G.arm_cjog_if_version = 1，数据端启动日志打印相同的版本号。
- [ ] 五个模式 × 三档：按住 10 s 连续运动，约 1 s 达到设定速度；松开后平滑停止，X/Y 停止距离不超过 15 mm。
- [ ] 按住 X 时切换速度档：平滑变速，不停车。
- [ ] 让两连杆接近伸直后按住 X 或 Y：自动降速并显示提示，无错误码。
- [ ] 按住时关闭上位机或断开网络：约 1 s 内停止，G.arm_cjog_error = 0。
- [ ] 按住时关闭总使能：故障停机，界面显示 3002。
- [ ] 归零功能、单轴点动功能与修改前一致。
