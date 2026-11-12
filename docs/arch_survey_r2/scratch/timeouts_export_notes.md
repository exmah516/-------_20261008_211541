# timeouts.csv 导出与完整性说明

**已确认**：UTF-8 BOM、9列、166条数据。字段顺序与用户列清单一致；数值列仅含毫秒数字或空值。空值表示无独立时间上限/未设年龄期限/次数或距离等非ms阈值；具体阈值写入名称。来源为第一轮 D.1、D.2、D.4、D.5 共97个原始表行，D.3监视关系不重复计入（第一轮D_超时与恢复.md:13-73,112-159）。

**已确认**：复合时间分项拆开；同一手柄5s缓存年龄不重复列两次；计数/容量/距离保留空ms并显式写单位。调用者忽略的50/100/250ms参数单列且不称实际deadline。

**已确认**：按当前源码校正第一轮的表述：RemoteRobotLink.NowMs取TimeProvider.GetElapsedTime而非Environment.TickCount64（介入机器人/A主端/工程/master/VI.Service.Robot/Services/RemoteRobotLink.cs:67-85）；HandleHost没有落后时跳到now+5ms的分支（介入机器人/A主端/工程/master/VI.HandleHost/Program.cs:88-93）；记录手柄历史100ms与匹配容差10ms是两个阈值（介入机器人/B数据端/工程/ADS项目/main.cpp:1479-1484,4610-4611,4672-4676）；ArmCartesian租约/阶段超时用任务cycle_seconds累加，阶段10/30超过2s错误2007（介入机器人/C从端/250902/Untitled2/POUs/ArmCartesian.TcPOU:47,56-70,167-184）。

**已确认**：导出脚本检查全部出处文件存在、行号不超范围、ms列可数值解析、每条原始表行至少有映射。它不重新证明第一轮结论；行为/现场边界仍沿用原附件及R2-5。

## 97个原始表行到CSV的映射

| 第一轮章节/表行出处 | 对象 | CSV编号 |
|---|---|---|
| D.1；docs/arch_survey_r1/D_超时与恢复.md:13 | 主循环等下一 ADS 快照 | T001 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:14 | ADS 单次连接阶段调用 | T002 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:15 | ADS 单次运行调用 | T003 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:16 | ADS硬失联观察窗 | T004,T005 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:17 | PLC运行状态检查 | T006 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:18 | PLC快照时钟停滞 | T007 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:19 | ADS快周期deadline | T008 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:20 | 低频ADS调用者等待 | T009,T010,T011,T012 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:21 | 低频ADS底层与重连 | T013,T014,T015 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:22 | 手柄最大缓存龄期 | T016,T017 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:23 | 手柄速度新鲜度 | T018 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:24 | 手柄恢复静置连续性 | T019,T020 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:25 | 手柄断流后重基准门槛 | T021 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:26 | 单/双模式初始宽限 | T022 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:27 | 总connection hold | T023 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:28 | 计划回退夹爪等待 | T024,T025,T026,T027 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:29 | 计划回退异步Prepare/Commit/Clear | T028 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:30 | 计划回退提交保护/清旧状态/ACK | T029,T030,T031 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:31 | 计划回退执行观察 | T032 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:32 | 计划回退RetryClear后等待 | T033 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:33 | 计划回退无总期限阶段 | T034 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:34 | 回退故障恢复清Req | T035,T036 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:35 | 回退故障自动续做 | T037 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:36 | 晚到的回退ErrorId | T038 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:37 | 急停恢复请求节流 | T039 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:38 | 屈曲恢复ClampWait | T040 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:39 | 屈曲恢复运动步长 | T041,T042 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:40 | 屈曲恢复ExitSync | T043,T044 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:41 | 标准启动夹爪阶段 | T045,T046 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:42 | 恢复启动错开 | T047,T048 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:43 | 直接启动/紧急回撤 | T049 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:44 | 轴4/注射器UI方向租约 | T050 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:45 | 力取零 | T051,T052 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:46 | 力反馈保持/平滑 | T053,T054,T055,T056 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:47 | tracking夹爪命令稳定 | T057 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:48 | tracking PI dt | T058 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:49 | tracking内部启用命令的力新鲜度 | T059 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:50 | 旧PLC回退状态缓存 | T060 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:51 | 寻参记录 | T061,T062 |
| D.1；docs/arch_survey_r1/D_超时与恢复.md:52 | 记录对齐与写线程 | T063,T064,T065,T066 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:58 | Zenoh挑战 | T067 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:59 | 命令排队新鲜度 | T068,T069 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:60 | 控制域状态新鲜度 | T070 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:61 | 控制权 | T071 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:62 | 持续方向续租 | T072 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:63 | Prepare任务 | T073 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:64 | StartControl任务 | T074 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:65 | ForceZero/Set/Hold、Cylinder、Y阀任务 | T075,T076,T077,T078,T079,T080 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:66 | Force发布 | T081 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:67 | router探测/启动 | T082,T083,T084,T085,T086,T087 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:68 | 定位臂快照周期/轴方向租约 | T088,T089 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:69 | 定位臂末端点动租约 | T090 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:70 | 程序归零用户保活 | T091 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:71 | 程序归零状态年龄 | T092 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:72 | 程序归零请求ACK | T093 |
| D.2；docs/arch_survey_r1/D_超时与恢复.md:73 | 定位臂ADS参数 | T094,T095 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:112 | Zenoh原生建连 | T096 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:113 | 主端认证等待 | T097 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:114 | 主端状态新鲜度 | T098 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:115 | 已连接但重认证阈值 | T099,T100 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:116 | 主端force帧新鲜度 | T101 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:117 | Session重建节流 | T102 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:118 | 普通按需命令 | T103,T104 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:119 | StartControl命令 | T105 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:120 | ForceFeedback/Zero | T106 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:121 | PreparePosition | T107 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:122 | 电缸/Y阀按需命令 | T108,T109 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:123 | HandleService heartbeat | T110,T111,T112,T113 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:124 | HandleHost force tick | T114,T115 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:125 | HandleHost采样 | T116 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:126 | USB通知 | T117 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:127 | UI状态刷新 | T118 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:128 | 定位臂轴/末端/归零保活 | T119 |
| D.4；docs/arch_survey_r1/D_超时与恢复.md:129 | 应用关闭 | T120 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:137 | MAIN/handle主机watchdog | T121,T122 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:138 | handle初始化/Enable | T123,T124 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:139 | return state10 Disable | T125 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:140 | return state20 MoveAbsolute | T126 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:141 | return state30停稳 | T127 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:142 | return state31重新Enable | T128,T129 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:143 | axis4 state10/30/40 | T130,T131,T132,T133 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:144 | init上电 | T134 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:145 | reset清错 | T135 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:146 | clear_err稳定 | T136 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:147 | err普通恢复 | T137,T138,T139,T140 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:148 | err限位退离 | T141 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:149 | SelfCheck错开扫描 | T142,T143,T144,T145 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:150 | SelfCheck堵转后备 | T146 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:151 | SelfCheck限位稳定 | T147 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:152 | SelfCheck Power pulse/wait | T148,T149 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:153 | SelfCheck回退错开 | T150,T151,T152,T153,T154 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:154 | Axis15 reset/GearIn | T155,T156 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:155 | ArmCartesian心跳/enable | T157,T158 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:156 | ArmCartesian曲线/到位 | T159,T160,T161 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:157 | ArmCartJog保活/enable | T162,T163 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:158 | ArmCartJogGen | T164,T165 |
| D.5；docs/arch_survey_r1/D_超时与恢复.md:159 | ArmManual/InjectorManual | T166 |

原始表行统计：{'D.1': 40, 'D.2': 16, 'D.4': 18, 'D.5': 23}；合计97。

未进行PLC/主端/ADS现场运行；CSV中的数值来自源码清单，不是实测响应时长。
