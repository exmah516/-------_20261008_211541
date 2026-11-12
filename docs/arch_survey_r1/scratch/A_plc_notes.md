# A：PLC 工程全貌（供主报告合并）

路径约定：T=介入机器人/C从端/250902/250902.tsproj；U=介入机器人/C从端/250902/Untitled2/；P=U/POUs/；G=U/GVLs/G.TcGVL。以下引用均按此前缀展开；以当前磁盘为准。

## 目录、工程与构建

| 项目/目录 | 当前用途与内容 | 确定程度与出处 |
|---|---|---|
| 介入机器人/C从端/250902.sln | TcXaeShell Solution，VisualStudioVersion=15.0.35116.124；显式列系统工程250902和测量工程TwinCAT Measurement Project3 | 已确认；250902.sln:2-9 |
| 250902/250902.tsproj | TwinCAT系统配置，固定TcVersion=3.1.4024.67；系统、NC、PLC、EtherCAT与轴链接 | 已确认；T:2,1441-1465,3594-3598 |
| 250902/Untitled2 | 主PLC应用，AmsPort851，系统仅实例化该PLC项目；TMC路径Untitled2/Untitled2.tmc | 已确认；T:3594-3598 |
| Untitled2/DUTs | state.TcDUT；ST_AxisPlannedReturnCmd.TcDUT | 已确认；U/Untitled2.plcproj:20-26 |
| Untitled2/GVLs | G.TcGVL，150个变量/常量 | 已确认；U/Untitled2.plcproj:27-29；G:5-189；全量索引见scratch/C_GVL全量索引.md |
| Untitled2/POUs | MAIN、init、SelfCheck、handle、err、clear_err、reset、ArmManual、ArmCartesian、ArmCartJog、ArmCartJogGen、Axis2Sync、InjectorManual，共13个编译项 | 已确认；U/Untitled2.plcproj:33-70 |
| Untitled2/PlcTask.TcTTO | 1ms、优先级20、入口MAIN | 已确认；U/PlcTask.TcTTO:3-9 |
| Untitled2/_Libraries | Beckhoff依赖库；项目引用Tc2_MC2、Tc2_NC、Tc2_Standard、Tc2_System、Tc3_MC2_AdvancedHoming、Tc3_Module | 已确认；U/Untitled2.plcproj:79-103 |
| Untitled2/_CompileInfo、Untitled2.tmc | 保存的编译信息和PLC符号/类型输出 | 已确认文件存在；U/Untitled2.plcproj:104-107；T:3595-3596；与当前源文件一致性未知 |
| Untitled1 | 独立旧PLC项目，只有MAIN和任务；任务10ms；当前T的PLC节点未引用 | 已确认；介入机器人/C从端/250902/Untitled1/Untitled1.plcproj:20-27、PlcTask.TcTTO:3-9；T:3594-3598 |
| Untitled3 | TwinSAFE安全PLC工程，目标EL6900、图形编辑器、Release输出bin/Release；不是T中主PLC实例 | 已确认；介入机器人/C从端/250902/Untitled3/Untitled3.splcproj:7-24；T:3594-3598 |
| 250902/_Boot/TwinCAT RT (x64)/Plc | 已有Port_851.app、Port_851_boot.tizip、Port_851.autostart、Port_851.cid/crc/oc*；CurrentConfig目录有Untitled2.tpzip | 已确认目录清单（2026-11-12）；现场是否加载这些文件未知 |
| TwinCAT Measurement Project1/2/3 | .tcmproj/.tcscopex/.svdx Scope测量材料；Solution只显式引用Project3 | 已确认目录清单；250902.sln:8-9 |

handle(备份).TcPOU和move_edge.TcPOU在目录中存在，但不在当前Compile Include中：U/Untitled2.plcproj:20-70（已确认）。C从端README称没有PLC源码，与当前文件直接矛盾：介入机器人/C从端/README.md:3-5（已确认）。

| 构建项 | 事实 | 证据 |
|---|---|---|
| 构建入口 | .sln指向250902/250902.tsproj；Untitled2.plcproj DefaultTargets=Build、ProgramVersion=3.1.4024.0 | 已确认；250902.sln:6；U/Untitled2.plcproj:2,11-18 |
| Solution配置 | Debug/Release，各含TwinCAT CE7 ARMV7、OS ARMT2/x64、RT x64/x86 | 已确认；250902.sln:11-21 |
| 输出 | 系统引用U/Untitled2.tmc；仓库Boot已有Port851应用和归档；不能据此确认现场当前二进制 | 已确认文件/配置；T:3595-3598；现场一致性未知 |
| 启动脚本 | C从端递归检索.bat/.cmd/.ps1/.sh未发现；有Boot .autostart文件 | 已确认检索结果；现场启动配置未知 |
| 本轮构建 | 未运行TwinCAT；维护文档记载此前devenv.com构建遭许可证限制，本轮未复现 | 已确认文档陈述；通讯逻辑改造实施指南.md:439-446；本机最新工具状态未知 |

## 任务、调用顺序和交换方式

| 任务 | 周期 | 优先级/端口 | 确定程度与出处 |
|---|---|---|---|
| PlcTask | 1000微秒=1ms（任务文件明确单位） | 20；System AmsPort350，PLC AmsPort851 | 已确认；U/PlcTask.TcTTO:3-9；T:1443-1446,3595 |
| NC-Task 1 SAF | XML原值10000，字段未写单位 | 4；AmsPort501；IoAtBegin=true | 已确认原值；T:1451-1462；按100ns换算为1ms是推断，需工程属性确认 |
| NC SVB | XML原值50000，字段未写单位 | 8；AmsPort511 | 已确认原值；T:1463-1465；按100ns换算为5ms是推断，需工程属性确认 |

PlcTask只调用MAIN（U/PlcTask.TcTTO:7-9）。MAIN源码顺序：轴引用刷新→FirstCycle初始化→主机watchdog→ArmManual→ArmCartesian→ArmCartJog→Axis2Sync→InjectorManual→轴6许可/POWERED/非_handle SETPOSITIONGEN/RESETED→gen_state CASE。_handle内handle1_返回后再执行SETPOSITIONGEN，使本拍Execute生效（P/MAIN.TcPOU:35-112,120-204，均已确认）。

普通变量在单PLC任务内直接共享；未见队列/锁/额外用户线程。NC/I/O采用tsproj链接，真实任务调度顺序和抖动未知（U/PlcTask.TcTTO:3-9；T:7616-7688）。ArmCartesian与ArmCartJog把_TaskInfo[1].CycleTime乘0.0000001转秒，且要求周期精确为0.001s：P/ArmCartesian.TcPOU:47,63-64；P/ArmCartJog.TcPOU:29,51-52。

## 主要文件规模（XML物理行数）

| 文件 | 行数 | 证据 |
|---|---:|---|
| MAIN.TcPOU | 7076 | P/MAIN.TcPOU:1-7076；含FBD XML，不等于7076行ST业务代码 |
| handle.TcPOU | 966 | P/handle.TcPOU:1-966 |
| SelfCheck.TcPOU | 619 | P/SelfCheck.TcPOU:1-619 |
| ArmCartesian.TcPOU | 272 | P/ArmCartesian.TcPOU:1-272 |
| err.TcPOU | 271 | P/err.TcPOU:1-271 |
| ArmManual.TcPOU | 257 | P/ArmManual.TcPOU:1-257 |
| ArmCartJogGen.TcPOU | 219 | P/ArmCartJogGen.TcPOU:1-219 |
| ArmCartJog.TcPOU | 149 | P/ArmCartJog.TcPOU:1-149 |
| InjectorManual.TcPOU | 112 | P/InjectorManual.TcPOU:1-112 |
| init.TcPOU | 100 | P/init.TcPOU:1-100 |
| Axis2Sync.TcPOU | 96 | P/Axis2Sync.TcPOU:1-96 |
| clear_err.TcPOU | 95 | P/clear_err.TcPOU:1-95 |
| reset.TcPOU | 87 | P/reset.TcPOU:1-87 |
| G.TcGVL | 191 | G:1-191 |

## 关键PLC默认值与常量

| 参数 | 默认值/范围 | 使用位置、确定程度与出处 |
|---|---|---|
| gen_state | _init | 顶层调度；已确认G:17、P/MAIN.TcPOU:120-203 |
| init_pos | [30,9,0,0,0,30,13] | 相对基准，会被自检/恢复刷新；已确认G:31、P/SelfCheck.TcPOU:532-567 |
| selfcheck_target_from_left | [96,0,500,0,530,580,0] | 自检目标；已确认G:41 |
| v_limit | [7.5,7.5,1.5,1.5,1.5,7.5,1.5] | 每PLC扫描refer增量限幅；已确认G:73、P/handle.TcPOU:502-545 |
| rslimit_distance | [99,0,666,0,688,688,0] | 右软限位距左限位；已确认G:180、P/handle.TcPOU:518-522 |
| recovery_wait_timeout / retries | 2s / 3次 | Enable/Disable/Move阶段；已确认G:181-182、P/handle.TcPOU:255-317,579-703 |
| err backoff | 2距离、5速度、50加速度 | 压限退离；已确认G:183-185、P/err.TcPOU:67-139 |
| Axis15 reset | 200ms脉冲、2s间隔 | 同步轴复位；已确认G:186-187、P/Axis2Sync.TcPOU:19-44 |
| arm jog velocity/acc/dec/jerk | 每轴5/50/50/500 | 定位臂手动/曲线；已确认G:92-95 |
| axis4 manual | 速度60，acc/dec200，jerk500 | 手动轴4；已确认P/handle.TcPOU:85-91 |
| traj_wn/vmax/amax | 60；[80,0,80,0,80,80,0]；[2000,0,2000,0,2000,2000,0] | handle二阶轨迹；已确认P/handle.TcPOU:95-103 |
| SelfCheck speed | scan/back=40/40 | 已确认P/SelfCheck.TcPOU:16-18 |
| SelfCheck stagger | scan=[0,0,300,0,600,900,0]ms；return=[3,0,2,0,1,0,0]s | 已确认P/SelfCheck.TcPOU:20-23 |
| ArmCartJogGen geometry | L1=200，L2=180，offset=456.3，tool length=554.5mm | 已确认P/ArmCartJogGen.TcPOU:22-28 |
| ArmCartJogGen profile | T_ACC=1s，T_DEC=.5s，branch=1，bisection=12，joint caps=[24,30,30,30,30] | 已确认P/ArmCartJogGen.TcPOU:29-33 |
| arm cjog interface | version1；mode1..5，dir±1，0<speed<=100，alive<.5s | 已确认G:117-126；P/ArmCartJog.TcPOU:38-52 |
| Y阀 | press TRUE输出500，FALSE2000 | 已确认G:143-145；P/handle.TcPOU:475-479 |
| 已废弃PID | Kp=.14，Ki=.016，Kd=.1，i_limit=100各轴 | 仅声明，无现用POU读取；已确认G:76-83；全量引用索引 |

## PLC/NC/I/O接口

| 组 | 映射 | 证据 |
|---|---|---|
| 定位臂 | NC1..5→G.arm_axis[1..5] | 已确认T:7630-7633,7658-7673 |
| 本体 | NC6..12→G.axis[1..7] | 已确认T:7634-7645,7674-7688 |
| 注射器 | NC13/14→G.inject_axis[1..2] | 已确认T:7646-7653 |
| 同步 | NC15→G.axis2_sync_axis | 已确认T:7654-7657 |
| Box7 | Data3=ft2，Data4=fn2；Value1/2=cyl3/4 | 已确认T:7617-7622 |
| Box8 | Data3=ft1，Data4=fn1；Value1/2/3=cyl1/2/5 | 已确认T:7623-7628 |

GVL注明cyl1固定支撑，cyl2轴1、cyl3轴5、cyl4轴6夹爪，cyl5轴3的Y阀（G:138-145）。实际电气端子、现场接线、下载应用版本和实时任务统计未知，需要在线配置与版本指纹。完整POU职责、状态和GVL读写接口在C_plc_notes.md及C_GVL全量索引.md；完整计时在D_plc_notes.md。
