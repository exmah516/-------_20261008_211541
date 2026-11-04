# 定位臂末端按住点动：T11 判据修订与全量复验

日期：2026-11-05。本次只修改 T11 的路径加速度判据，并重跑全部现有离线验收；ArmCartJogGen、C++参考模型、运动算法和其它测试判据均未修改。完整接口说明见 [positioning_arm_cartesian_jog.md](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/positioning_arm_cartesian_jog.md>)。

## 本次修改

仅修改 `arm_cart_jog_tests.cpp` 的 T11 检查与结果输出。测试侧保存前两个名义速度样本的过渡上限，每周期与当前样本的上限取最大值；新场景开始时清空历史，受限周期也更新历史。未受限周期采用：

```text
B_j = 1.5·|v1_j−v0_j|/T_j
A_k = |path_speed_k−path_speed_{k−1}|/dt
A_k ≤ 1.01·max(B_j)
```

`j` 覆盖 `v_{k−2}`、`v_{k−1}`、`v_k` 所属的全部过渡。下文“比值”统一指 `A_k/max(B_j)`，通过上限为1.01。速度前馈与位置差分、加速度前馈与速度差分的误差阈值仍各为 `1e−9`。源码哈希和测试主体核对结果见末尾证据，确认生产代码、参考模型及其它用例判据保持原样。312个受检源码/配置/测试文件中，只有此测试文件改变；ArmCartJogGen、参考模型及归零基线的SHA-256均与本次修订前相同。

## 零位四方向（T13）

起点每次重置为 `(q1,q2,q3,q4,q5)=(100 mm,0°,0°,0°,0°)`，慢档5 mm/s，按住2 s。腕点为 `(0,−20 mm)`，因此 Y+ 朝轴2转轴（完全折叠内边界），Y− 远离，X± 为切向。

| 方向 | 2 s 末端位移 | 末路径速度 | 最大关节步进速度 | q3 | limited |
|---|---:|---:|---:|---:|---|
| X+ | 7.500000 mm | 5.000000 mm/s | 1.510171 °/s | −2.264962° | false |
| X− | 2.995354 mm | 1.735840 mm/s | 9.999987 °/s | −0.904532° | true |
| Y+（朝轴2转轴） | 0 mm | 0 mm/s | 0 °/s | 0° | true |
| Y− | 1.132642 mm | 1.186523 mm/s | 9.999996 °/s | −2.061268° | true |

四方向均有限，最大直线偏离 `3.89e−13 mm`，关节步长均未超限。Y+在内边界保持不动，limited为true。Gen没有错误码输出；ArmCartJog对限速/边界不写错误码的结论来自静态分支检查，本轮未读取真实PLC状态。

## 新离线验收

执行命令：

```powershell
python 'B数据端/工程/PositioningArmKinematics/tests/run_cart_jog_acceptance.py'
```

归零基线 SHA-256：`6c1af930624100da6a7109eb3b58f655e4d2ca87ef33cb59b4a8b08c6c7e118b`；100组、1700个 double、`error/already_home` 均为 **0 mismatch**。

| 用例 | 实测 | 判定 |
|---|---|---|
| T1 | 腕点 `1.25056e−12 mm`；逆解回代 `4.61853e−14 rad` | PASS |
| T2 | 直线偏差 `4.99601e−13 mm`；姿态/ Z 最大变化 `5.08889e−14` | PASS |
| T3 | 加速 `1.001 s`，停止 `0.501 s`，峰值 `7.49999 mm/s²`，慢档X停止 `1.25 mm` | PASS |
| T4 | X/Y `7.5/3.75/1.25 mm`；Z `0.75/0.625/0.3125 mm`；转动 `0.3/0.2/0.1°` | PASS |
| T5 | 15→30：`1.001 s`；30→5：`0.501 s`；未过零 | PASS |
| T6 | X切Y重锚时间 `0.500 s`，Gen在零速点重锚；ArmCartJog保持阶段25为静态确认 | PASS |
| T7 | 重按期间最低执行路径速度 `3.240003 mm/s`；重按速度 `3.247194 mm/s`；最终5 | PASS |
| T8 | 按PLC的alive_age判据驱动held，500周期超时；停止距离 `1.25 mm`；超时无写错误分支（静态确认） | PASS |
| T9 | 最大关节速度 `9.999996°/s`；半径 `[372.071527,380] mm`；伸直方向速度降至0且limited；反向 `2.287 s` 到5 mm/s | PASS |
| T10 | Z/俯仰/偏航位移 `4.5/1.8/1.8`；积分误差最大 `8.53e−14` | PASS |
| T11 | 速度/加速度前馈差分误差均0；全部用例最大路径加速度比值 `0.999998666667`，低于1.01；T7交界周期比值 `0.479421450667` | PASS |
| T12 | 单轴速度均低于24/10 caps | PASS |
| T13 | 见上表；位移/轨迹/步长/有限性均通过 | PASS |

### T7 交界周期实测

第2201周期重新按下同方向，三个名义速度样本涉及两段过渡：

| 名义样本 | 所属过渡 v0→v1 | T | B_j |
|---|---|---:|---:|
| v2199、v2200 | 5→0 mm/s | 0.5 s | 15 mm/s² |
| v2201 | 3.24→5 mm/s | 1.0 s | 2.64 mm/s² |

实测 `path_speed_2200=3.24719396 mm/s`，`path_speed_2201=3.24000263824 mm/s`，因此绝对路径加速度为 **7.19132176 mm/s²**。三个样本的 B 值依次为 `[15,15,2.64]`，最大值15，允许值 **15.15 mm/s²**，比值为 **0.479421450667**，通过。

全部用例的最大比值为 **0.999998666667**，记录峰值位于 T7 第2701周期：加速度 `2.63999648 mm/s²`，三个 B 均为 `2.64 mm/s²`，允许值 `2.6664 mm/s²`。速度前馈与位置差分、加速度前馈与速度差分的最大误差均为 **0**。T1–T13及额外BRANCH/OFFSET/FINITE检查全部PASS；验收脚本返回0，`results.json`各执行项均为0。

## ST 与 C++ 对照

| ST源码 | C++参考模型 | 对应操作 |
|---|---|---|
| [ST:53](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:53>) | [C++:24](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:24>) | 初始化位置、速度和过渡状态 |
| [ST:60](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:60>) | [C++:31](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:31>) | 保存前周期量；目标速度和三次平滑曲线 |
| [ST:72](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:72>) | [C++:43](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:43>) | 零速换模式/方向；分支与锚点 |
| [ST:87](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:87>) | [C++:58](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:58>) | 名义速度梯形路径增量 |
| [ST:88](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:88>) | [C++:60](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:60>) | 单轴步长上限与位置符号 |
| [ST:103](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:103>) | [C++:72](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:72>) | 完整步长试探，失败后固定12次二分 |
| [ST:110](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:110>) | [C++:79](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:79>) | 可达环带、cos α夹紧、象限方位角、闭式IK |
| [ST:126](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:126>) | [C++:96](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:96>) | 角度展开、偏航补偿和三轴增量验收 |
| [ST:135](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:135>) | [C++:105](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:105>) | 最大可接受步长；无正步长保持位置 |
| [ST:152](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:152>) | [C++:122](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:122>) | sigma累计、执行路径速度与limited |
| [ST:155](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/C从端/250902/Untitled2/POUs/ArmCartJogGen.TcPOU:155>) | [C++:125](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/B数据端/工程/PositioningArmKinematics/tests/arm_cart_jog_reference.h:125>) | 位置差分速度和速度差分加速度 |

ST 与参考模型常量逐项一致：`PI=3.14159265358979323846`、`RAD=PI/180`、`T_ACC=1.0 s`、`T_DEC=0.5 s`、`INITIAL_BRANCH=1`、`BISECTION_COUNT=12`、`JOINT_VCAP=[24,10,10,10,10]`、`L1/L2=200/180 mm`。转译器消费1101 tokens；覆盖多分支、零位、模式/方向/速度和松开状态的600000周期比对，位置、速度、加速度、path状态、内部过渡量最大数值差为 **0**；limited、分支、模式和方向逐周期一致。对应源码行与常量完整索引见 [source_index_round2.txt](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/source_index_round2.txt>)。

静态检查通过：XML、ST嵌套、1 ms任务、MAIN调用顺序、已有 Tc2_MC2 API、stage10/25 Feed 所有权、drive_ready一致性、旧排队/旧错误码/`Q3_MARGIN`/Jacobian `g/k` 删除、无TcUnit时未引入测试框架。

## 构建与未执行项目

- 数据端 ADS Release x64：PASS，0警告、0错误。
- 上位机 MasterConsole Release x64：PASS，0警告、0错误。
- VI.Control.Tests：PASS，0警告、0错误；WPF回归182项通过。
- Zenoh C++协议81项、C#协议/服务116项通过；真实网络未执行（`-SkipNetwork`）。
- PositioningArmKinematics：20/20通过；ADS mock通过，租约观测516 ms，无真实PLC。
- **PLC未构建**：本机有 TcSysSrv 和 TwinCAT runtime，但无 TwinCAT XAE/PLC compiler（`Components/Plc`为空、VS扩展为0）。XML/ST静态自查和ST↔C++差分不能替代工程机编译。
- **Scope未做**：没有装载新FB的NC仿真配置，无法导出指定X、中档、肘角90°轨迹。

用户需在工程机编译、下载并激活PLC，在线确认 `G.arm_cjog_if_version=1` 和数据端版本日志，再执行接口说明末尾的现场清单。按T13表对照零位四方向。本机可执行的离线项目全部通过；PLC编译和Scope轨迹对照仍待工程机完成。

当前证据：[总输出](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/acceptance_round2.log>)、[T1–T13](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/arm_cart_jog_tests.log>)、[归零回归](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/home_regression.log>)、[ST逐周期比对](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/arm_cart_jog_equivalence.log>)、[ST静态自查](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/st_static_audit_round2.log>)、[删除项搜索](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/removed_references_round2.log>)、[编码与源码核对](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/source_checks_round2.log>)、[数据端构建](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/ads_final.log>)、[WPF构建](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/wpf_final.log>)、[WPF回归](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/wpf_tests.log>)、[协议回归](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/zenoh_final.log>)、[结果JSON](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/results.json>)、[本次T11差异](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/t11_criterion.diff>)、[最终核对](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/final_checks_round2.log>)、[工具链环境](<D:/Work_files/Vessel intervention Robot/主从分机软件包_20261008_211541/介入机器人/docs/acceptance_cart_jog/environment.log>)。旧Z反号实验和旧静态自查仅为第一轮历史，不属于本轮验收；运行器未包含该历史实验。

复现三端构建与回归：

```powershell
& 'D:/Work_software/VS2022/MSBuild/Current/Bin/MSBuild.exe' 'B数据端/工程/ADS项目/ADS.sln' /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m:2 /v:normal /nologo
$dotnet = & './A主端/工程/tools/Get-ViDotnet.ps1'
& $dotnet build 'A主端/工程/master/MasterConsole.sln' -c Release -p:Platform=x64 -m:2 -v:minimal
& $dotnet build 'A主端/工程/master/VI.Control.Tests/VI.Control.Tests.csproj' -c Release -p:Platform=x64 -m:2 -v:minimal
& './A主端/工程/build/Release/bin/VI.Control.Tests/VI.Control.Tests.exe' 'B数据端/工程/PositioningArmKinematics/build-cart-jog/UiChecks'
& './B数据端/工程/ADS项目/tests/run-zenoh-tests.ps1' -SkipNetwork
```
