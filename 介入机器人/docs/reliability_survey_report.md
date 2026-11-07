# 介入机器人停机、锁存与恢复路径：只读调查报告

调查日期：2026-11-07（Asia/Shanghai）。对象为本机当前磁盘源码；Git 基点 `713d8830aaf04cd23b1547c3f0da03528f5c5d94`。未启动主端、数据端、机器人程序、ADS 探针或 PLC，未进行运动、上电、PLC 读写、编译工程或故障注入。唯一新增文件为本报告。

**证据口径**：**已确认**＝源码/留存日志直接证明；**推断**＝基于已确认结构的条件推演，尚未现场验证；**未知**＝证据不足。静态可达性不代表现场发生率；“没有源码出口”也不等于建议重启。摘要按造成恢复困难、诱使重启或丢失寻参资格的可能性排序，不是日志统计排名。

路径均相对 `D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\介入机器人`，以下简称用于所有“文件:行号”引用：

|简称|完整相对路径|
|---|---|
|`B/`|`B数据端/工程/ADS项目/`|
|`A/`|`A主端/工程/master/`|
|`R/`|`A主端/工程/master/VI.Service.Robot/`|
|`P/`|`C从端/250902/Untitled2/POUs/`|
|`G`|`C从端/250902/Untitled2/GVLs/G.TcGVL`|
|`TC`|`C从端/250902/250902.tsproj`|
|`SDK/`|包根目录（本报告根目录的父目录）`zenoh/sdk/zenoh-c-1.10.1-msvc/include/`|
|`PKG/`|包根目录 `D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\`|
|`TEMP/`|`C:/Users/Sui/AppData/Local/Temp/`（仅用于历史离线测试出处）|

工作区原有未提交修改包括 `ArmCartesian.TcPOU`、`ArmCartJog.TcPOU` 及定位臂测试/文档；本报告读的是这些修改后的磁盘内容。实际工程引用 `Untitled2`（`TC:3595`），正式编译表含 `handle.TcPOU`，不含 `handle(备份).TcPOU`（`C从端/250902/Untitled2/Untitled2.plcproj:20–72`）。主端使用说明指向 .NET 8 WPF `VI.Control.slnx`（`A主端/使用说明.md:3–5`），不是旁边保留的旧 C++ USB 主程序。**未知**：三端已部署二进制、PLC 下载内容是否与上述版本一致。

PLC行号均为 `.TcPOU/.TcGVL/.tsproj` 的XML物理行号，含声明与CDATA所在行；不是TwinCAT编辑器独立ST窗格的行号。证据片段前的简称均由上表展开。

## 1. 摘要

1. **[F01：数据端存在无清除入口的回退故障锁存](#t1-fault)**。【已确认】`return_ads_fault_hold` 只在启动初始化为假，失败函数置真后没有清零；“开始控制”、ADS 重连和 PLC 自检完成均不能解除它（`B/main.cpp:497,1414–1417,2281–2303,5077–5081`）。解除该进程内状态需要重建数据端进程，源码并不要求同时重启主端或 PLC。
2. **[F02：PLC 重启必丢寻参资格](#t7-reuse)**。【已确认】FirstCycle 强制 `_init`、`self_check_done=FALSE`，init 再次清标志；现有应用变量没有 RETAIN/PERSISTENT 声明（`P/MAIN.TcPOU:38–46`、`P/init.TcPOU:36`、`G:5–35`）。普通 err/clear_err 保留资格，三端一起重启反而会确定触发重新寻参。
3. **[F03：普通 err 不会主动退离电子限位](#t2-limit)**。【已确认】它只复位/尝试重新使能，不包含 SelfCheck 的停稳、专门下降沿及退离动作；错误持续时可无限重试（`P/err.TcPOU:22–185`、`P/SelfCheck.TcPOU:183–198,215–237,285–306,333–440`）。【推断】压限位持续造成 Error/Power 未就绪时，可能一直无法恢复；仅 DriveLimitActive 持续并不足以断定必定死锁。
4. **[F04：PLC 若干阶段只等 Done，没有错误/超时出口](#t2-waits)**。【已确认】handle 初始化、回退禁给定/重使能、轴4切换都有这种结构；初始化的 RETURN 还挡住后续错误与主机恢复处理（`P/handle.TcPOU:168–214,559–627,656–754`）。【推断】对应 FB 单次失败可形成条件性死等。
5. **[F05：主端保留已关闭的 Zenoh 会话时，当前 UI 没有重建出口](#t3-session)**。【已确认】会话对象非空就跳过 Connect，而状态不鲜时按钮又走不到 Disconnect（`R/Services/RemoteRobotLink.cs:86,122,285`、`R/ViewModels/MainViewModel.cs:126–135`）。【推断】native 会话永久 closed 后可能只能重启主端；普通网络中断是否会造成这种关闭，仍未知。
6. **[F06：主端 USB 采样未实现已打开设备的断开判定](#t3-handle)**。【已确认】四个 void SDK 读取之后无条件 `Valid=true`、`return true`，常规失败关闭分支不可达（`R/Services/HandleService.cs:158,192–211`）。【未知】拔出时 SDK 是阻塞、返回旧值还是自行恢复；不能把初次打开重试当作拔插自动恢复保证。
7. **[F07：定位臂和本体共享 ADS 线程，低频超时不是执行时限](#t5-arm-time)**。【已确认】低频请求同步执行，开始后一直等请求完成；运行 ADS 单次调用 timeout 设为 300 ms（`B/ads_communication.cpp:622–624,1043–1045,1114,2077–2163`）。【推断】冷句柄或慢调用可拖延本体快照/心跳，扩大为保持甚至 PLC 2 s 看门狗事件。
8. **[F08：Axis15 错误可能卡住已经进入的 err](#t2-other-axes)**。【已确认】err 将 Axis15 错误纳入退出条件，Axis15 的 Reset 却只跟随本体2轴 Reset（`P/err.TcPOU:57–60,103–107`、`P/Axis2Sync.TcPOU:17–20`）。【推断】仅剩 Axis15 错而主2轴无错时可能缺复位脉冲；它没有直接使 `_handle→_err` 的判断。
9. **[F09：断流恢复与 UI 解除能力存在分层差异](#t4-lease)**。【已确认】数据端租约无断流 TTL，主端按2 s状态新鲜度门控、3 s重认证；当前UI没有本体急停解除/清错命令（`B/zenoh_service.cpp:1003`、`R/Services/RemoteRobotLink.cs:86–97,494–513`、`A/MasterConsole.Protocol/ZenohFrames.cs:14–23`）。[UI门控详T3](#t3-ui)，恢复连接不会清所有锁或续做所有动作。
10. **[F10：留存日志不足以统计现场前三类停机](#t6-logs)**。【已确认】本机诊断仅找到 51 行、去重 43 行连接阶段失败；没有当前版 PLC-ESTOP、计划回退失败或重启链事件（详见 T6 文件逐行统计）。旧版 3 条“ADS 快照已恢复”及离线测试不并入现场排名；该恢复文案本身也不唯一指向 ADS 故障（`B/main.cpp:1265–1301`）。

## 2. 停机、保持、锁存总表

“是”严格指当前可达实现没有源码/当前 UI 清除入口；“条件性等待”不等于已证明只能重启。每行的详细触发、恢复后动作及片段见对应 T 节；同一标志的不同写点在正文穷举。普通位置 hold 值本身是参考基准，其上层激活状态列入表。

|编号|所在层|名称|位置|触发|影响范围|出口|无出口或需重启（是/否）|确定程度|备注|
|---|---|---|---|---|---|---|---|---|---|
|B01|数据端|`return_ads_fault_hold`|B/main.cpp:497,2281|回退最终失败，详 T1 失败表|本体参考输出、力反馈、Start；取消本地回退，PLC撤Req异步|没有清零；重建数据端进程才初始化|是|已确认|主端/PLC重启本身不清该进程变量；PLC撤Req未必成功|
|B02|数据端|`control_active=false`|B/main.cpp:360,397,1051,1338,1369,1416,1650,1807,1843,2282,3269,5066|初连、PLC重启、自检失效、estop、回退故障、同步失败或旧启动接管|常规控制；不等于所有独立设备停止|startup已完成且健康时1867–1876自动sync；初连需StartControl→SelectDirectControl:5072|否|已确认|B01仍真时不能恢复；完整写点见T1.7|
|B03|数据端|`!initial_sync_done`|B/main.cpp:780,1330–1345|进程初始，无有效PLC/手柄同步|连接保持、常规输出|有效ADS、所需手柄、模式已锁定后sync_all(30)|否|已确认|同步后待用户开始|
|B04|数据端|`ads_soft_hold_active`|B/main.cpp:916,1204–1296|本拍非有效运动快照或重建失败|本拍参考/力反馈门控|有效快照；回退中先放行原任务，否则当前位置rebase|否|已确认|不直接取消回退|
|B05|数据端|`handle_soft_hold_active`|B/main.cpp:1197–1199|单柄模式活动柄失败；双柄模式任一失败|本体控制和力输出|远程有效采样；1s重试打开；当前位置重建|否|已确认|双柄不自动降成单柄|
|B06|数据端|`connection_hold_active`|B/main.cpp:1305–1327|B03/B04/B05任一|停常规输出、清力、关补偿、退出大部分屈曲恢复|各原因消失；不恢复历史参考；回退任务保留|否|已确认|不是总急停|
|B07|数据端|`estop_hold_active`|B/main.cpp:1645–1677|PLC通知estop_req变化为真|本体停控、取消回退、清力|PLC清req；数据端每秒请求watchdog恢复；已启动时再自动sync|否|已确认|当前UI无本体解除命令|
|B08|数据端|`handle_startup_locked` / `single_handle_mode`|B/main.cpp:101–135,1057–1085|首次可用柄集合；等待路径单柄宽限3s|锁定输入映射，双柄缺一仍保持|本进程没有解锁/重新选单双柄入口|是|已确认|这是模式锁，不必然停机；新增第二柄不升级|
|B09|数据端|`axis6_soft_limit_hold`|B/main.cpp:688–723,1408–1412|预测目标超过距左660mm|保持1/2/3/5/6/7，停轴4相关联动、取消回退|非ActualPosition原因下一拍清除重评|否|已确认|ActualPosition保留枚举没有置位调用|
|B10|数据端|`axis1_delivery_stop_latched`|B/main.cpp:3960–3966|单柄正向到触发边且axis3距左≤20mm|拒继续正向Follow/换手|反向有效正增量3784–3789；或屈曲退出axis3>22mm时4110–4116|否|已确认|双柄另有直接增量门控3541|
|B11|数据端|双柄递送停止条件|B/main.cpp:3540–3541|axis3到递送停止位且inc1<0|导管继续递送增量为0|撤出方向/位置离开停止条件|否|已确认|非持久锁存|
|B12|数据端|`planned_return.active()`|B/main.cpp:2313–2352,2507–2893|窗口边缘触发合法换手|冻结相关链路、拒模式切换与重复动作|夹持→提交→ACK→Done→新快照→交接→rebase；失败B01|否|已确认|保持中保留，明确接管才取消|
|B13|数据端|`RetryClear` / `retry_count`|B/main.cpp:2465–2485,2735–2820|尚未开始且首次握手失败|清Req并重试一次|状态复位、新快照、目标安全；否则B01|否|已确认|“可能已开始”也禁止重发|
|B14|数据端|`AwaitFreshSnapshot`|B/main.cpp:2824–2835|PLC Done后等待新快照/rebase|尚不交回Follow|新有效快照且rebase成功，逐拍重试|否|已确认|该阶段无独立总时长上限|
|B15|数据端|`ClampSettle` / `AwaitHandoffApplied` / `PostHandoffClampSettle`|B/main.cpp:2507–2534,2837–2893|等待实际写入代次、夹爪稳定、最后重建|换手占有，模式命令拒绝|相应输出代次成功及定时结束|否|已确认|部分等待无独立超时，持续I/O失败可一直等|
|B16|数据端|`dual_handoff.active()`|B/main.cpp:3525–3529,3647–3705|单双链窗口边界换手意图已登记|增量冻结，等实际到位和低速|四直线轴到位且速度≤0.5；方向改变/reset/取消|否|已确认|无本地等待超时|
|B17|数据端|reverse-switch guards|B/main.cpp:1591–1625,3926–3957|在触发边附近切方向|抑制立刻重复触发换手|离边2mm或有效同向边界重触发；sync重建|否|已确认|不冻结所有轴|
|B18|数据端|spacing `requested/ClampWait/Active`|B/main.cpp:1425–1491,3059–3209|587低三位键，或内部恢复请求|固定1/2/7；3/5/6只允许正向共同移动|松键/退出或模式请求→ExitSync；安全事件reset|否|已确认|无行程时保持，松键可退出|
|B19|数据端|spacing `ExitSync`|B/main.cpp:3050–3056,4084–4127|松键/退出/模式切换|等停稳与基准重建|≥100ms且到位，或≥2s按当前位置；sync失败下拍重试|否|已确认|通信恢复后可重试，不设永久fault|
|B20|数据端|startup `WaitForEnter/!completed`|B/main.cpp:1338–1341,5072–5119|初连或PLC重启恢复准备|常规手柄控制未启动|当前UI StartControl在当前位置sync，成功Done|否|已确认|不必重新寻参，取决PLC资格|
|B21|数据端|startup活动phase|B/startup_sequence.cpp:69–233；B/main.cpp:3211–3446|旧ExecuteStartup内部入口|暂时接管轴与夹爪，拒重复开始|到位/阶段定时→Done；部分位置等待无超时|否|已确认|当前WPF/Zenoh无ExecuteStartup入口|
|B22|数据端|`emergency_retract_active`|B/main.cpp:4948–4985,3118–3134|旧内部EmergencyRetractDevice|锁1/2/7，3/5/6到目标，关补偿和力|到位4373–4390；无超时/专门取消入口|否（有到位出口）|已确认|当前WPF/Zenoh无此入口；持续不到位可长期等待|
|B23|数据端|tracking禁用/invalid/gain冻结|B/main.cpp:1037,1313,1969–2067,4070–4080|失联、非Follow、夹持不稳、误差/参数/样本不合格|位移补偿/PI；不是本体总停机|有效段/参数恢复；禁用后需内部SetTrackingCompensation重开|否|已确认|当前界面可达性见T1|
|B24|数据端|手动电缸覆盖门控|B/main.cpp:4143–4156,4684–4724|PLC不健康或自动流程占有、参数无效|拒手动目标并清旧覆盖|条件解除后重新发目标/ResetCylinderManual|否|已确认|电缸不完全依赖手柄/主控资格|
|B25|数据端|轴4/注射器500ms deadman|B/main.cpp:490–493,1526–1543,4923–4945|方向未续期、冲突/上层不允许|归零该独立方向|新持续方向帧，必须重新操作|否|已确认|不等于全系统租约|
|B26|数据端|`user_disabled_manual_enable_` / arm请求清理|B/arm_manual_ads_service.cpp:147–157,424–475|用户关定位臂或ADS不健康|定位臂去使能、停点动/曲线|用户重新开；健康自动恢复总开关但不补旧轴使能/点动|否|已确认|局部状态|
|B27|数据端|arm `cartesian_blocked_`|B/arm_manual_ads_service.cpp:265–296,314–345|曲线取消失败/各种abort|拒曲线keepalive，清待办|ArmStop或新ProgramZero清，动作重新请求|否|已确认|不直接清/设本体状态|
|B28|数据端|arm jog/cjog过期、忙/owned拒绝|B/arm_manual_ads_service.cpp:238–251,268,478–500|300ms轴点动、500ms末端续期过期，或已有动作占有|仅定位臂方向/新动作|新请求；占有释放后再操作|否|已确认|阻塞线程时到期检测也会迟到|
|B29|数据端|辅助服务启动失败|B/main.cpp:78–95；B/arm_manual_ads_service.cpp:108–122|线程/资源/网络启动失败|相应服务不可用|main没有再次调用start，当前UI无重启服务入口|是（当前入口）|已确认|不同于运行期ADS断线自动重连|
|B30|数据端|sync/rebase失败|B/motion_sync.cpp:113–155,410–458；B/main.cpp:1296,1867–1875|读写失败、零个有效手柄样本或窗口无效|基准未确认、保持当前恢复阶段|主循环按所在阶段重试；Start失败可再次请求|否|已确认|函数本身不设永久fault；回退失败升级另见B01|
|B31|数据端|导丝模式切换被拒|B/guidewire_mode.cpp:19–42,72–112；B/main.cpp:1690,1799|回退忙/错、非Running、同步或窗口失败|不能进入请求的新模式|条件恢复后产生新的模式请求边沿|否|已确认/推断|失败不会逐拍重试；退出失败main仍清独立模式|
|B32|数据端|力反馈enabled/zeroed/样本门控|B/force_feedback.cpp:21–25,103–139；B/main.cpp:150–156|未归零、关闭或输入无效/控制保持|常规力停止；样本无效时可能保留B33换手力，不直接停NC运动|满足条件/归零开启；安全清力会撤全部力锁|否|已确认|不能把所有sample.invalid都说成力必为0|
|B33|数据端|`ff.hold_582/hold_587.active`|B/control_types.h:550–578；B/main.cpp:2347–2349|启用对应力通道及clamp_hold时计划回退捕获力|只冻结对应手柄的力/扭矩|换手完成后沿当前方向移动2mm，再100ms融合；关闭保持/该侧力或安全clear_force清|否|已确认|样本无效可延长保留；不是轴运动锁|
|B34|数据端|`selfcheck_start_pending/selfcheck_command_error`|B/main.cpp:994–999,4656–4680|自检命令不完整/不就绪/写失败；写成功等待PLC接受|拒重复自检、门控手动电缸，发布等待/失败状态|status变0/2/3或ADS非Running清pending；新参数/运行/完成清error|否（有条件出口）|已确认|pending无独立本地确认超时；Zenoh任务超时不直接清它|
|B35|数据端|力归零同步等待|B/main.cpp:733–777|当前UI ForceZero或首次开启前归零|main连续等3份force有效快照，每份500ms；不是永久fault|成功更新零点；失败返回、计fail，可重新请求|否|已确认|极端名义等待3×500ms，可能形成poll消费间隔|
|B36|数据端|重力补偿/参数拒绝|B/main.cpp:5121–5135；B/force_calibration.h:48–49|gravity_comp_validated假、其它参数越界|仅拒该补偿/参数，不停止普通控制|满足校准资格/合法参数后重发；当前WPF无该旧补偿入口|否|已确认|不是可清除的运行fault|
|D01|ADS|非Running/SoftHold|B/ads_communication.cpp:942–1050|设备状态/快照/核心写失败|无效快照；失败排队低频/回退请求|下一完整成功周期恢复；累计失败龄期300ms转重连|否|已确认|单拍也能令业务队列失败|
|D02|ADS|PlcRestarted/Reconnecting|B/ads_communication.cpp:919–939,988–1059,2060–2074|PLC非RUN、周期/时钟回退、持续I/O失败|清缓存/句柄/待办；重新连接|1s重试并重建通知/句柄；PLC非RUN不自动切RUN|否|已确认|不是主动重启PLC|
|D03|ADS|低频排队超时|B/ads_communication.cpp:554–624|未开始等待超过调用方timeout|请求取消、不执行；已开始则等完成|调用返回后上层决定重试|否|已确认|250ms不是正在执行请求的硬期限|
|P01|PLC|`self_check_done=FALSE`|G:28；P/MAIN.TcPOU:40；P/init.TcPOU:36|初值/FirstCycle/init|不能直接回已寻参handle|仅SelfCheck全部完成置TRUE|否|已确认|每次PLC启动均重新失去资格|
|P02|PLC|`startup_loading_ready`清除|G:32；P/init.TcPOU:37；P/SelfCheck.TcPOU:69|初始化/自检reset/主机消费|装载位资格|SelfCheck:547设TRUE|否|已确认|不是所有控制恢复都要装载流程|
|P03|PLC|`host_comm_timeout/host_timeout_latched`|P/MAIN.TcPOU:63–89；P/handle.TcPOU:96–124|无session、2s无心跳或非首次session变化|本体冻结并停回退，注射器停，定位臂去使能|显式host_recover_req且心跳正常、已停稳；handle重建|否|已确认|旧动作取消，不自动续做|
|P04|PLC|`estop_hold_req`|P/handle.TcPOU:258；P/err.TcPOU:62；P/clear_err.TcPOU:43|本体错误/上电异常/错误恢复|本体保持；影响主机与定位臂运动门控|正常handle:433清；FirstCycle:45清|否|已确认|注射器POU本身不检查它|
|P05|PLC|handle `!init_done`|P/handle.TcPOU:126–214|初始化/重建外部给定|7轴未全Enable.Done则RETURN|全Done；无FB.Error/超时处理|否（条件性死等）|已确认/推断|失败阶段可能挡住恢复处理|
|P06|PLC|回退state10|P/handle.TcPOU:555–565|接受Req后禁外部给定|Busy，等Disable.Done或已Disabled|条件满足进20；host/reinit可清|否（条件性死等）|已确认/推断|无本地超时|
|P07|PLC|回退state20/30/31|P/handle.TcPOU:568–627|运动→停稳→重新使能|Busy、冻结Follow|FB终态/低速/Enable.Done，或host/reinit|否（条件性死等）|已确认/推断|31不处理Enable.Error|
|P08|PLC|回退Done/Error且Req未清|P/handle.TcPOU:456–459,541–547|回退终态等待撤请求|拒新回退，保持traj_pos|Req归零或handle重建|否|已确认|需要B正确撤Req|
|P09|PLC|轴4结果Error|P/handle.TcPOU:638–653,742–753|手动FB错误且方向仍请求|不再起新轴4动作|方向0或handle重建清|否|已确认|非全局永久latch|
|P10|PLC|轴4state10/30/40|P/handle.TcPOU:656–755|禁给定/停止/重新使能|轴4等待|Done/速度/重建；部分无Error/超时|否（条件性死等）|已确认/推断|不应直接归为只能重启|
|P11|PLC|Axis15 `gear_failed`|P/Axis2Sync.TcPOU:29–60|GearIn.Error/Aborted|停止重试同步|使能/主从Power条件失效时清，再静止重入|否|已确认|没有专用重试命令|
|P12|PLC|注射器超时/双向冲突|P/InjectorManual.TcPOU:24–42|host超时/同轴推拉同时为真|该注射器方向0/停|新合法单方向请求|否|已确认|未见该POU复位错误路径|
|P13|PLC|定位臂allow_jog=FALSE|P/ArmManual.TcPOU:35–42,105–143|去使能、owned、Reset、冲突、轴/Power错|定位臂拒点动|条件消失/单轴Reset成功，新请求|否|已确认|不直接影响gen_state|
|P14|PLC|`arm_motion_error`|P/ArmManual.TcPOU:129–143,241–249|定位臂局部FB/轴错误|局部状态/门控|Reset成功93–96或新合法点动159–161清|否|已确认|结果标志不等于硬件错已清|
|P15|PLC|`arm_curve_owned/stage/error/cancel`|P/ArmCartesian.TcPOU:63–177,249–269|曲线占有、参数/就绪/0.5s心跳错误|互斥其他定位臂动作，停止释放|40→50→60释放；新合法请求清error|否（条件性死等）|已确认/推断|停止释放部分无总超时|
|P16|PLC|`arm_cjog_owned/error`|P/ArmCartJog.TcPOU:45–146|末端点动、0.5s保活、2s使能超时/FB错|局部停止、拒抢owned|40→50/60→0；合法新动作清error|否（条件性死等）|已确认/推断|40/50/60无总超时|
|P17|PLC|`arm_cjog_limited`|P/ArmCartJogGen.TcPOU:94,212|本拍可用步长不足|局部限速/零步长|每拍重算|否|已确认|不是锁存|
|P18|PLC|SelfCheck各等待/4↔5/55↔56|P/SelfCheck.TcPOU:140–506|未触限/复位或Power不成功|一直_self_check|各阶段成功后推进，全部stu=10完成|否（无总超时/取消）|已确认|没有当前UI取消入口|
|M01|主端|状态失鲜|R/Services/RemoteRobotLink.cs:86–97|认证有效state龄期≥2s|UI灰、持续方向归零、关触觉、停Handle发送|有效新状态自动恢复门控|否|已确认|方向不补发|
|M02|主端|未持控制权|R/ViewModels/MainViewModel.cs:114–119,168–175|LeaseHeld假/Acquire失败|运动区灰|断开再连Acquire；重认证后满足controlRequested时自动申请|否|已确认|没有单独Acquire按钮|
|M03|主端|`_sdkMissing`/采样线程退出|R/Services/HandleService.cs:164–173|DLL缺失/位数/入口异常|USB采样服务退出|当前UI无Start服务入口；需重启主端|是（当前UI）|已确认|不是普通USB拔出必然发生|
|M04|主端|SDK调用未返回|R/Services/HandleService.cs:147–161,192–211|封闭SDK阻塞（是否发生未知）|同一串行线程两柄采样可能同时过100ms|调用返回可恢复；源码无取消阻塞机制|未知|推断/未知|需厂商行为和线程栈|
|M05|主端|松开/捕获丢失/面板关闭|A/VI.Module.Intervention/InterventionPanel.xaml.cs:22–49；A/VI.Module.PositioningArm/PositioningArmPanel.xaml.cs:30–37|持续按钮结束/Stop命令|该面板注射器/轴4或定位臂停|用户重新按下|否|已确认|主窗口失焦不全局停|
|M06|主端|pending超时/认证失效|R/Services/RemoteRobotLink.cs:182–196,494–513|回执超时/会话失效|请求失败，旧动作不补发|重新请求/重新认证|否|已确认|不说明已下发PLC动作必取消|
|M07|主端|残留非空且已closed的Zenoh session|R/Services/RemoteRobotLink.cs:86,122,285,494–513；R/ViewModels/MainViewModel.cs:126–135|native session不可用，而_zenoh仍非null|UI一直失联；Connect不重建，Acquire拒绝|当前UI无出口；外部Disconnect/Dispose或重启主端|是（当前UI）|已确认/推断|进入该条件已证明；普通掉线是否导致closed未知|
|M08|主端|力SDK持锁未返回|R/Services/HandleService.cs:216–244；R/Services/RemoteRobotLink.cs:350–358,401|native sendForce阻塞（是否发生未知）|可连带阻塞GetSample、Handle发送、状态回调和UI取状态|调用返回可恢复；源码无取消native调用|未知|推断/未知|比单纯串行采样停顿影响范围更广|
|Z01|Zenoh|未认证/未Acquire|B/zenoh_service.cpp:639,736,783|会话/控制权不成立|丢Handle、拒业务命令|Hello/Auth/Acquire|否|已确认|无断流撤租TTL|
|Z02|Zenoh|session/HMAC/sequence拒绝|B/zenoh_service.cpp:636–644,736–740|旧会话、非增序、tag不符|丢该包|后续合法包|否|已确认|不产生永久fault|
|Z03|Zenoh|state龄期>500ms|B/zenoh_service.cpp:765–767,1025–1026|main不更新state|拒除Release外命令；state停发|main恢复更新|否|已确认|可连带主端2s门控|
|Z04|Zenoh|FIFO满/命令过期/epoch变化|B/zenoh_service.cpp:495–505,744–754|256满、排队>2s、会话变化|命令失败、不补做|新请求|否|已确认|最旧请求可被丢弃|
|Z05|Zenoh|pending容量/重复|B/zenoh_service.cpp:785–790,816–818,840–842|≥8待办或同类未完|拒新任务|完成/超时/撤租，新请求|否|已确认|不是永久锁存|
|Z06|远程桥|样本invalid/龄期>3s|B/手柄/remote_handle_bridge.h:69–78；B/手柄/手柄.cpp:45–49|有效位假立即；到达龄期超3s|Handle轮询失败→B05|有效采样及每秒init重试|否|已确认|双柄按既定模式门控|
|Z07|远程桥|`rebase_requested_`|B/手柄/手柄.cpp:50–52；B/手柄/Handle.h:29|两次消费rx时间差>250ms|当前位置重建基准|take后清；重建失败走B04重试|否|已确认|不唯一指网络丢帧|
|Z08|Zenoh|Prepare/Start任务失败态|B/zenoh_service.cpp:450–451,966–967|deadline/拒绝/新认证撤租|结果失败，UI任务结束|新任务置state=1|否|已确认|软件任务失败≠PLC动作已撤销|
|Z09|Zenoh|业务就绪/参数拒绝|B/zenoh_service.cpp:799–848,855–955|ADS/PLC/手柄基准未就绪，prepare状态或参数不合法，业务互斥|拒对应Start/Prepare/电缸等，不另设永久latch|消除原条件后重新请求；B01本身仍无清除入口|否|已确认|T4.1补表区分具体业务门|

### 2.1 无出口、只能重启与条件性死等的单独清单

**已确认无当前入口，解除需相应进程重建或外部API**：B01 回退故障锁；B08 单/双柄启动模式锁；M03 SDK载入异常后的线程；B29 辅助服务启动失败；M07 非空但closed会话。B08是模式永久性，不是“断柄必重启”；M07当前UI无出口是代码事实，现场是否进入该条件尚未知。证据为各表行及T1/T3/T4的调用位置；没有发现这些路径要求同时重启三端。

**保留但当前 UI 无入口的无专门取消路径**：B22 极简撤出只在实际到位时结束；活动 startup、SelfCheck 也无当前 UI 总取消。它们仍可因到位或阶段成功自然退出，不等同“触发后必需重启”（`B/main.cpp:4373–4390,4948–4985`、`P/MAIN.TcPOU:116–138`）。

**条件性等待，不能确认只能重启**：P05–P07/P10/P15–P16 的 Done/停稳/释放等待；持续硬件错误中的 err；仅剩 Axis15 错误；M04 native阻塞。代码中部分存在 host恢复/reinit/去使能等其它路径，能否实际走到依赖当时阶段；本调查未用机器人验证，详 T2/T8。

## 3. T1～T8 详细结论

<a id="t1-fault"></a>
### T1. 数据端：停机、保持、锁存与恢复

#### T1.1 回退最终失败锁：确实没有清零，重启主端或PLC不会清数据端内存

**已确认**：对目标数据端全部业务源码检索 `return_ads_fault_hold`，只有初始化假（497）、失败函数真（2281），其余是读取。每拍真时强制 `control_active=false`、清力；主运动总出口还单独要求 `!return_ads_fault_hold`（4323–4325）。`SelectDirectControl` 明确拒绝它（5077–5081）；PLC重启分支（1361–1373）、自检完成分支（1813–1844）都未清它。即使有分支短暂把 control_active 设真，独立门控仍阻止参考，下一拍又置假。**推断**：只恢复主端连接或只重启 PLC 不能解除已有数据端进程中的该锁；重启数据端后可重新同步，PLC若保留自检资格并不因此必须重新寻左限位。

**证据 `B/main.cpp:2276–2283`：**

```cpp
2276: 			auto fail_planned_return = [&](const char* reason,
2277: 				const PlannedReturnLeg* failed_leg,
2278: 				bool attempt_request_clear)
2279: 			{
2280: 				(void)attempt_request_clear;
2281: 				return_ads_fault_hold = true;
2282: 				control_active = false;
2283: 				clear_force_output();
```
**证据 `B/main.cpp:1414–1418`：**

```cpp
1414: 		if (return_ads_fault_hold)
1415: 		{
1416: 			control_active = false;
1417: 			clear_force_output();
1418: 		}
```
**证据 `B/main.cpp:5074–5081`：**

```cpp
5074: 						if (control_active || planned_return.active() || spacing_recovery.active()) break;
5075: 						// 自检完成后点击“开始控制”：不再走到准备位置，直接在当前位置开始手柄控制。
5076: 						pending_selfcheck_axes = 0;
5077: 						if (!startup.is_active() &&
5078: 							!return_ads_fault_hold && !handle_soft_hold_active && initial_sync_done &&
5079: 							!estop_hold_active &&
5080: 							!ads_soft_hold_active &&
5081: 							has_self_check_flag && self_check_done)
```

**已确认**：失败函数忽略 `attempt_request_clear` 参数，所有调用都走相同锁存；清力、异步清Req并立即 reset 本地 planned_return（2280–2303、638–676）。清Req入队失败的返回值被多处 `(void)` 忽略，不能从“已取消本地任务”推定 PLC 一定收到取消。

触发分类如下，每类均最终调用上述同一个失败函数；“第一次”不是所有条件都可重试：若任一腿 `started` **或** `possibly_started`，立即失败而不重发（2178–2185、2471–2474）。

|触发|准确条件/期限|调用位置（B/main.cpp）|
|---|---|---|
|提交前已有Busy|准备提交时PLC已Busy且当前非Commit待回执|2550–2556|
|参数阶段失败|入队失败、批写失败、等待≥250ms、未知通信purpose|2562–2595→2465–2485；已重试1次或可能已开始则锁|
|请求提交失败|Commit失败/≥250ms未完成；提交保护输出未确认；入队失败；PLC旧状态未清|2600–2688→2465–2485；Commit超时先把全部腿标possibly_started(2621–2627)|
|ACK失败|新PLC错误或ACK等待≥250ms|2693–2707→2465–2485|
|执行失败|AwaitDone观察到本次错误；或首次ACK后执行计时≥5000ms|2710–2732；ACK计时设于2140–2143|
|重试清理时已开始|RetryClear期间发现任何started/possibly_started|2735–2739|
|清Req失败|重试清Req入队失败、回执失败、等待≥250ms|2743–2772|
|重试前复核失败|清理后新快照与PLC无Busy/Error，但目标不安全；或状态复位≥500ms|2778–2820|

**已确认**：配置为 ACK 250ms、重试清状态500ms、执行5000ms（`B/control_types.h:172–175`）。安全重试只一次：清Req成功并见更新快照、Busy/Error均假、目标仍在窗口才 `++retry_count` 并重新提交（`B/main.cpp:2778–2814`）；已确认/可能开始不会整组重发。250ms业务期限短于运行ADS一次300ms，且业务判断依赖main能执行到该阶段；两者不能当作同一超时。

**证据 `B/main.cpp:2465–2475`：**

```cpp
2465: 			auto enter_planned_return_retry = [&](const char* reason)
2466: 			{
2467: 				for (int leg_index = 0; leg_index < planned_return.leg_count; ++leg_index)
2468: 				{
2469: 					refresh_planned_return_leg(planned_return.legs[leg_index]);
2470: 				}
2471: 				if (planned_return.retry_count >= 1 || planned_return_any_started())
2472: 				{
2473: 					fail_planned_return(reason, planned_return_has_error(), true);
2474: 					return;
2475: 				}
```

#### T1.2 普通保持与重建：保留模式/任务，但不是所有功能都自动恢复

**已确认**：`ads_motion_cycle_valid` 同时要求本拍新快照、位置有效、ADS Running、自检完成、handle_reinit_done，以及快照/通知均无host超时（`B/main.cpp:1041–1044`）。单纯力数据无效不使运动条件失效（1040）。缺少任一条件可进入 ads_soft_hold；这不一定是ADS调用错误，诊断有时会显示 `No ADS call failure recorded`（1234–1236）。

**证据 `B/main.cpp:1040–1044`：**

```cpp
1040: 		// 运动控制只依赖位置快照；力数据质量单独由 force_valid 传给力反馈和记录器。
1041: 		const bool ads_motion_cycle_valid = has_new_ads_snapshot && ads_snapshot.position_valid &&
1042: 			ads_stats.state == AdsConnectionState::Running &&
1043: 			self_check_done && ads_events.handle_reinit_done &&
1044: 			!ads_snapshot.host_comm_timeout && !ads_events.host_comm_timeout;
```
**证据 `B/main.cpp:1305–1316`：**

```cpp
1305: 		const bool connection_hold_active = !initial_sync_done || ads_soft_hold_active || handle_soft_hold_active;
1306: 		const bool connection_hold_enter_edge = connection_hold_active && !connection_hold_active_prev;
1307: 		const bool connection_hold_exit_edge = !connection_hold_active && connection_hold_active_prev;
1308: 		connection_hold_active_prev = connection_hold_active;
1309: 
1310: 		if (connection_hold_enter_edge)
1311: 		{
1312: 			clear_cylinder_manual_overrides();
1313: 			tracking_controller.disable_compensation();
1314: 			if (spacing_recovery.phase != SpacingRecoveryPhase::ExitSync) spacing_recovery.reset();
1315: 			clear_force_output();
1316: 			std::cout << "连接保持激活：保留回退任务，等待有效快照与手柄采样，清零力反馈。" << std::endl;
```

**已确认**：普通保持不把 control_active 改假；按原稳定模式恢复。快照恢复且 planned_return仍活动时，只清ads_soft_hold、继续原任务确认，不先rebase或重新提交（1260–1263）。无回退时加载当前位置、重建两柄/单柄对应基准；成功清hold（1265–1301）。连接恢复时旋转保持值更新为实际位置，**不追赶中断前目标**（1318–1327）。补偿在连接进入或ADS重连时已关闭（1037、1313），不会随连接恢复自动重新启用。

**已确认**：手柄断开会使 `handle_soft_hold_active` 为真，双柄模式必须两柄poll都成功；每个关闭柄每1000ms再init（1093–1111），首次有效帧重置滤波（1115–1191）。有效位假立即失败，3s只用于仍valid样本的到达龄期（见T4）。短暂保持保留回退；estop、自检失效、PLC重启等明确接管会取消回退（1516–1522、1651、1806）。被取消的任务不会恢复原阶段，普通连接保持中的原任务可继续。

#### T1.3 “开始控制”、自检变化、启动流程的不同入口

**已确认**：初次 `sync_all(30)` 成功只进入 `WaitForEnter` 且 `startup.completed=false/control_active=false`（1330–1345）；当前 WPF `StartControl` 映射 `SelectDirectControl`，在已自检、健康、手柄就绪、无回退/恢复/启动活动时 `sync_all(20)`，直接以当前位置设Done与active（5072–5119）。失败可再次点击；没有执行“重新寻参”的调用。主端重启后若B和PLC状态仍健康，能否直接继续由这些现存状态决定。

**已确认**：PLC自检完成从假变真时，B刷新坐标、取消旧回退、清spacing/独立导丝窗口，把startup设Done；健康且sync成功会自动active（1802–1847）。首次读到“自检本来已真”不是这个边沿，而是初次待开始路径。已完成startup但active假时，1867–1876逐拍自动sync恢复；因此estop清除后通常无需再点开始，但B01锁仍优先拦住。

**证据 `B/main.cpp:1864–1876`：**

```cpp
1864: 		// 8) 当启动已完成但控制未激活时，通过全量重同步恢复。
1865: 		const bool motion_startup_active = startup.is_active();
1866: 		startup_smoothing_bypass = motion_startup_active || emergency_retract_active;
1867: 		if (!control_active && !return_ads_fault_hold && !motion_startup_active &&
1868: 			!planned_return.active() && !spacing_recovery.active() &&
1869: 			ads_motion_cycle_valid && initial_sync_done && !connection_hold_active &&
1870: 			!handle_soft_hold_active && self_check_done &&
1871: 			!estop_hold_active && !ads_soft_hold_active && startup.completed)
1872: 		{
1873: 			if (sync_all(20))
1874: 			{
1875: 				control_active = true;
1876: 			}
```

**已确认**：保留的 `ExecuteStartup` 只在 `WaitForEnter/!completed`、已自检、无ADS/estop保持时运行（5034–5068）；`begin_direct_positioning` 仅定义未被调用（361–399）。当前 Zenoh `PreparePosition` 是设置自检目标再 `StartSelfCheck`，`StartControl` 是当前位置控制（`B/zenoh_service.cpp:814–848`），都没有进入旧ExecuteStartup。旧位置等待阶段缺总超时、极简撤出缺取消的事实应以“保留路径”理解，不能称为当前按钮必经停机。

#### T1.4 axis6 限位、投送停止位、双柄边界与模式锁

**已确认**：axis6默认距左上界660mm；预测检查 `target > 660+1e-6`（683–686）。三个有效置位调用分别是单柄导丝目标（2961–2967）、计划回退（3009–3015）、单柄导管联动目标（3982–3988）。保持时本体1/2/3/5/6/7参考采当前位置、夹爪用稳定组合（2916–2935），取消现有回退（709–713）。非ActualPosition原因在下一拍清除再判（1408–1412）。

**证据 `B/main.cpp:1406–1412`：**

```cpp
1406: 		// 预测目标造成的阻断只覆盖当前危险动作；下一拍重新读取位置和手柄输入。
1407: 		// 实际越限的保持仍由原有分支处理，不把预测目标拒绝锁存成永久故障。
1408: 		if (axis6_soft_limit_hold &&
1409: 			axis6_soft_limit_reason != Axis6SoftLimitReason::ActualPosition)
1410: 		{
1411: 			axis6_soft_limit_hold = false;
1412: 			axis6_soft_limit_reason = Axis6SoftLimitReason::None;
```

**已确认**：`ActualPosition` 在本文件只有枚举、比较，没有实际置位调用；`plc_restart_recovery_latched` 与 `axis1_push_rearm_after_hold` 只有false赋值，没有true。不能把这些残留名字列为已发生的永久锁。双柄路径直接限d6和目标到660、约束g，不用上述预测锁来停整条链（3578–3607、3689–3690）。

**已确认**：单柄导管递送停止锁在axis3≤20mm且递送换手触发时置位，反向有效增量清；屈曲恢复退出后axis3>22mm也清（3960–3966、3784–3789、4110–4116）。双柄是 `axis3_delivery_stop_active && inc1<0` 直接置零（3540–3541），不是重启锁。反向切换guard只抑制重复换手，离边2mm或合法边界重触发可清（1591–1625、3926–3957）。

**已确认**：`lock_handle_mode` 一旦 `handle_startup_locked` 即return，无解锁入口；若程序启动当下已有一柄init成功，97–135直接锁单柄，**这条快路径没有3s宽限**。只有开始时两柄均无效、进入1057等待路径后，单柄才等3s（1073–1076）。双柄锁定后不因掉一柄自动降级；单柄锁定后另一柄后到也不升级。该事实与USB是否能重连是两件事。

**证据 `B/main.cpp:110–118`：**

```cpp
110: 	auto lock_handle_mode = [&](bool axis1_ok, bool axis6_ok)
111: 	{
112: 		if (handle_startup_locked) return;
113: 		if (axis1_ok && axis6_ok)
114: 		{
115: 			single_handle_mode = false;
116: 			axis1_input_handle = &handle_axis1;
117: 			axis6_input_handle = &handle_axis6;
118: 			handle_startup_locked = true;
```
**证据 `B/main.cpp:120–129`：**

```cpp
120: 		else if (axis1_ok || axis6_ok)
121: 		{
122: 			single_handle_mode = true;
123: 			Handle* single_handle = axis1_ok ? &handle_axis1 : &handle_axis6;
124: 			const DWORD single_serial = axis1_ok ? serial_axis1_handle : serial_axis6_handle;
125: 			axis1_input_handle = single_handle;
126: 			axis6_input_handle = single_handle;
127: 			single_handle_requested_mode = GuidewireMode::None;
128: 			handle_startup_locked = true;
129: 			std::cout << "单手柄模式已锁定（序列号: " << single_serial << "）。" << std::endl;
```

#### T1.5 计划回退的非故障等待、屈曲恢复与独立输出

**已确认**：回退活动时拒新回退（2313–2314）、拒模式/方向切换（883–888、4775–4778）；普通动作互斥不是故障。`ClampSettle` 等输出generation真正applied后才开始夹爪等待（2507–2533），Done后等新快照与rebase、交接代次成功、两级夹爪恢复后再次rebase才reset（2824–2893）。这些交接/rebase等待没有独立总截止；持续同步失败可等待，下一有效拍会继续尝试，不能一概归为永久fault。

**已确认**：双柄 `dual_handoff.active` 时inc1/inc6先为0（3539–3542），必须相关四直线轴到位且速度≤0.5才建回退（3665–3675）；方向不符可reset（3525–3529），同步/取消也reset（630–641）。这一路没有本地超时，可能表现为“在边缘等待”，现场需看实际误差和速度。

**已确认**：屈曲恢复由物理587低三位同时按下触发（1422–1485、配置`0x07`在`B/control_types.h:152–153`）。Active固定1/2/7，只让3/5/6同向共同移动，速度40mm/s，剩余取三轴上界最小值，axis6再取660（3036–3047、3150–3209）。源码日志称“582手柄”，实际累计使用 `axis1_linear_increment_mm`（3170–3172）；不能按日志文字断定真实控制柄就是582，单双柄映射见110–129。达到行程上限只清pending_delta并提示，松键仍可退出。

**已确认**：松键、退出或模式请求进入ExitSync；100ms后已到位，或2s后按实际位置sync，成功恢复active/原模式；sync单次失败保留ExitSync下拍重试（4084–4127）。连接保持会reset非ExitSync的恢复，estop/回退故障也会reset（1314、1680–1686）；这些被中止的恢复不续做，需要再次按键。

**证据 `B/main.cpp:4090–4099`：**

```cpp
4090: 		const bool spacing_recovery_exit_ready =
4091: 			spacing_recovery_exit_elapsed_ms >= cfg.spacing_recovery_exit_settle_ms &&
4092: 			(spacing_recovery_targets_settled ||
4093: 				spacing_recovery_exit_elapsed_ms >= cfg.spacing_recovery_exit_timeout_ms);
4094: 		if (spacing_recovery.phase == SpacingRecoveryPhase::ExitSync && spacing_recovery_exit_ready &&
4095: 			ads_motion_cycle_valid && !ads_soft_hold_active && !handle_soft_hold_active &&
4096: 			!estop_hold_active && !return_ads_fault_hold)
4097: 		{
4098: 			const double recovered_mm = spacing_recovery.moved_mm;
4099: 			if (sync_all(20))
```
**证据 `B/main.cpp:4118–4127`：**

```cpp
4118: 				axis1_push_rearm_after_hold = false;
4119: 				spacing_recovery.reset();
4120: 				control_active = true;
4121: 				if (single_handle_mode) apply_mode_selection(restore_selection);
4122: 				else vis_reverse_override_active = false;
4123: 				std::cout << "屈曲恢复退出后，双手柄方向按当前B6状态生效。" << std::endl;
4124: 				std::cout << "屈曲恢复已退出并完成重同步，本次共同移动 "
4125: 					<< recovered_mm << " mm。" << std::endl;
4126: 			}
4127: 			// 单次失败保留 ExitSync，下一份有效快照重试，不设置永久故障标志。
```

**已确认**：手动电缸、轴4、注射器不能直接等同“本体全停”。电缸门控不要求control_active或手柄连接，只要求PLC健康且无自动流程占有；不可用时清覆盖（4142–4156）。轴4条件见1544–1552，注射器/轴4方向500ms到期归零（1526–1543）；B01禁常规参考不代表所有独立执行机构的命令都同样被禁。PLC注射器与本体estop的差异另见T2。

#### T1.6 motion_sync、startup、tracking、力反馈及记录器的覆盖边界

**已确认**：`sync_all` 和各rebase并不自设全局fault，返回失败由main决定后续状态。全同步失败点和平均采样的阻塞详补充证据；rebase重建实际位置/滤波/窗口，不续追历史旋转目标。各调用的处理是：初始同步1335失败继续等待；普通恢复1296保留ads_hold；自检完成1834–1843保持active假并走后续自动重试；回退2829/2878保留阶段；屈曲4099保留ExitSync；Start5093失败提示后可再请求。不是每个false都进入B01。

**已确认**：tracking在连接变化、非有效控制段被disable（1037、1313、2063–2067、4077–4080），参数/夹持/力数据不合格拒开启（4788–4847）；作用是PI补偿、增益或实验段有效性，不直接向PLC发急停。力反馈 `ff.enabled`、zeroed、样本门控只影响力输出；样本无效但control仍active、已归零且无estop时，会保留active的换手锁力，不能笼统说必清零（`B/force_feedback.cpp:21–25,103–139`）。`clear_force_output`才明确清夹持保持并发零（`B/main.cpp:150–156`）。记录器写入错误只走 `stop_async` 停录（`B/experiment_recorder.cpp:622–638`），未见设置control_active/estop的路径，不能把录制Error统计为本体停止。

**已确认**：`sync_axis1` / `sync_axis6` 会尝试清对应回退Req，但忽略清除bool（`B/motion_sync.cpp:113,350`）；后续快照读、平均手柄采样、窗口或refer写失败只返回false（115–155、352–401）。`sync_all` 也先尝试清两组Req及轴4点动，忽略三个返回值（410–412）；两次状态读/写、平均样本零有效才返回false（414–458）。成功后axis1.enabled=true；axis6窗口暂压到当前位置、enabled=false、lock=false（470–493），属于退出独立导丝基准；再次进入导丝模式会重新enabled（`B/guidewire_mode.cpp:101`）。

**证据 `B/motion_sync.cpp:410–422`：**

```cpp
410: 		plc_io::clear_axis1_group_return_requests(ctx);
411: 		plc_io::clear_axis_return_request(ctx, AdsSymbol::axis6_return);
412: 		plc_io::write_axis4_manual_requests(ctx, false, false);
413: 
414: 		if (!plc_io::read_plc_state(ctx))
415: 		{
416: 			return false;
417: 		}
418: 		// 使用“当前轴实际位置”重置旋转保持基准，避免复用陈旧 hold 值导致意外回零。
419: 		preserved_axis2_hold_rel = ctx.plc_act_pos[1];
420: 		preserved_axis7_hold_rel = ctx.plc_act_pos[6];
421: 
422: 		plc_io::load_pos_from_actual(ctx);
```

**已确认**：平均函数每个样本都 `Sleep(10)`，20/30样本包含约200/300ms主动等待，实际更久取决调度；只需有效数大于0，不要求每次读取不同序号（`B/control_types.cpp:36–51,70–88`）。因此同步可暂停main，但ADS独立线程仍可发心跳；不能把每条250ms rebase都解释成网络长间隔。回退后的rebase只用当前内存快照/滤波，不再poll、Sleep或同步ADS读（`B/motion_sync.cpp:160–174,227–280,323–325`）。

**证据 `B/control_types.cpp:70–83`：**

```cpp
70: 	for (int i = 0; i < samples; ++i)
71: 	{
72: 		const bool ok_a = handle_a.poll();
73: 		const bool ok_b = handle_b.poll();
74: 		if (ok_a && ok_b)
75: 		{
76: 			a0_sum += handle_a.fJoints2[0];
77: 			a1_sum += handle_a.fJoints2[1];
78: 			b0_sum += handle_b.fJoints2[0];
79: 			b1_sum += handle_b.fJoints2[1];
80: 			++valid_samples;
81: 		}
82: 		Sleep(10);
83: 	}
```

**已确认**：导丝模式入口明确拒清Req失败、非Running、轴6回退Busy/Error（`B/guidewire_mode.cpp:19–42`），兼容直连ADS分支只检查Busy（48–56）。后续读状态、平均、窗口、写refer也能失败（72–112）。只有main确认成功才改Independent（`B/main.cpp:1772–1776`），但无论成功失败都会更新requested_mode_prev（1799）；**推断**：失败后应有新的模式请求边沿才再尝试。退出helper本来可能返回失败保留模式（`B/guidewire_mode.cpp:133–144`），main失败分支却仍强制mode=None、axis6.enabled=false、解锁窗口（`B/main.cpp:1707–1712`），最终行为以main为准。`axis6_window_locked`只是相对窗口锁定，无独立永久故障；sync_all/退出会解（`B/motion_sync.cpp:68–108,488`、`B/guidewire_mode.cpp:146–148`）。

**已确认**：旧startup入口拒非有限/越±360°角度、读PLC/ready失败、速度上限读写失败、夹持恢复失败、ready消费失败、旁路或首refer写失败（`B/startup_sequence.cpp:69–207`）。恢复速度写失败时保留 `v_limit_scaled=true` 供后续再恢复（236–249）；消费ready写PLC的 `startup_loading_ready=false`（53–66）。装载姿态不符或ready假进入recovery_mode（122–124），并不直接要求重新SelfCheck。活动定义只看phase（`B/control_types.h:682–685`）；main位置阶段只等到位、无总超时（3298–3446）。**推断**：位置永远不到时可无限活动并拒Start；这是当前WPF没有旧入口的保留路径。

**证据 `B/main.cpp:3298–3310`：**

```cpp
3298: 					if (startup.phase == StartupPhase::PositionAxis156)
3299: 					{
3300: 						pos[0] = from_left_to_rel(0, startup.final_axis1_from_left_mm);
3301: 						pos[1] = 0.0;
3302: 						pos[4] = from_left_to_rel(4, startup.direct_axis5_stage_from_left_mm);
3303: 						pos[5] = from_left_to_rel(5, startup.direct_axis6_stage_from_left_mm);
3304: 						if (axis156_reached)
3305: 						{
3306: 							startup.phase = StartupPhase::PositionAxis356;
3307: 							std::cout << "开始控制：轴1与轴5/6相对位置已就绪，轴3/5/6开始共同到位。" << std::endl;
3308: 						}
3309: 					}
3310: 					else
```

**已确认**：tracking的 `state.active=false` 指实验跟踪段。base不成立或夹持未成立会end_segment，条件恢复且夹持连续150ms后自动begin_segment（`B/delivery_tracking.cpp:137–162`、`B/delivery_tracking.h:92`）。end_segment清PI/未确认运动，除CrawlReturn/GripSettling之外清欠账（382–408、467–470）。`compensation_enabled=false`只旁路补偿，仍返回名义正向手柄位移（233–235）；`tracking_error_limited`是欠账限幅标记（215–220），输出夹取会回滚积分（312–319）。因此这些不是新增本体停机锁；`AdsReturnFault`等invalid reason是外部安全条件标签，不另算独立原因。关闭补偿后可用内部 `SetTrackingCompensation`（`B/main.cpp:4788–4835`）重开，但当前WPF/Zenoh命令枚举没有该入口（`A/MasterConsole.Protocol/ZenohFrames.cs:14–23`、`B/zenoh_service.cpp:812–958`）。

**证据 `B/delivery_tracking.cpp:149–162`：**

```cpp
149: 	if (state.grip_command_since_ms == 0)
150: 	{
151: 		state.grip_command_since_ms = now_ms;
152: 	}
153: 	if ((now_ms - state.grip_command_since_ms) < kGripSettleMs)
154: 	{
155: 		end_segment(state, TrackingInvalidReason::GripSettling, true);
156: 		return;
157: 	}
158: 
159: 	if (!state.active)
160: 	{
161: 		begin_segment(state, now_ms, actual_abs_mm);
162: 	}
```
**证据 `B/delivery_tracking.cpp:229–239`：**

```cpp
229: 	state.snapshot.nominal_forward_increment_mm = std::max(0.0, nominal_forward_increment_mm);
230: 	state.snapshot.compensated_requested_forward_increment_mm = state.snapshot.nominal_forward_increment_mm;
231: 	state.snapshot.compensation_gain = 1.0;
232: 
233: 	if (!compensation_enabled_ || !state.active || nominal_forward_increment_mm <= kIncrementEpsilonMm)
234: 	{
235: 		return std::max(0.0, nominal_forward_increment_mm);
236: 	}
237: 
238: 	refresh_error(state);
239: 	const DeliveryTrackingParameters& params = parameters(axis);
```

**已确认**：服务线程本身启动失败与PLC连接失败不同。main对ADS、定位臂、Zenoh各只调用一次start，失败告警后继续（`B/main.cpp:78–95`）；ADS资源/线程构造失败（`B/ads_communication.cpp:178–181,213–220`）、定位臂线程构造失败（`B/arm_manual_ads_service.cpp:108–122`）无再次启动入口。ADS线程若已启动，则首次连接失败仍在run中每秒重试（`B/ads_communication.cpp:860–900`），不能列成只能重启。

#### T1.7 补充写点：控制开关、自检待确认、锁力与局部拒绝

**已确认**：`control_active=false` 的生产main写点穷举如下，不能仅靠该bool区分每一种恢复需求。其它独立总门包括最终 `ads_output.motion_enabled`（`B/main.cpp:4323–4333`）、手动电缸门（4143–4156）、轴4/注射器门（1526–1552），因此bool假不等于所有设备断使能。

|main写点|具体原因|重新控制条件|确定程度|
|---|---|---|---|
|360|进程变量初始化|有效初始同步后用户Start|已确认|
|397|保留begin_direct_positioning接管；当前没有调用点|旧启动流程完成并sync|已确认|
|1051|第一份有效快照确认自检未完成|PLC完成自检后边沿恢复或后续Start|已确认|
|1338|初次sync_all成功后进入待开始|StartControl→SelectDirectControl|已确认|
|1369|检测PLC重启/应用重载等价事件|资格与快照重建；若PLCFirstCycle则先重新SelfCheck|已确认|
|1416|已有return_ads_fault_hold每拍强制假|B01无当前清锁入口|已确认|
|1650|PLC estop上升沿|PLC错误/保持清除，startup已完成时自动sync|已确认|
|1807|已确认的自检true→false|自检再次完成/初次同步规则|已确认|
|1843|自检完成边沿的sync失败|健康条件满足后1867–1875逐拍再sync|已确认|
|2282|回退最终失败|B01无当前清锁入口|已确认|
|3269|旧startup完成，但最后sync失败|startup已标completed，后续健康拍自动sync|已确认|
|5066|旧ExecuteStartup入口成功接管|旧startup完成并sync；当前WPF无入口|已确认|

**已确认**：`selfcheck_start_pending`在两笔自检ADS写都成功后置真（`B/main.cpp:4671–4680`），在PLC status=0/2/3或ADS非Running时清（994–997）。本地没有独立确认deadline；它为真会拒重复StartSelfCheck、阻止手动电缸（4656–4658、4148）。参数不全/不就绪或写失败只置selfcheck_command_error，新的SetSelfCheckAxisPos、PLC运行自检/完成可清error（4639、994–999）；不会停普通控制或直接进B01。**推断**：PLC长期保留status=1又没有连接状态变化时，pending可一直等；Zenoh层5分钟Prepare任务超时没有直接清main这个bool，需要用实际PLC状态/通知判断是哪一端未确认。

**证据 `B/main.cpp:994–999`：**

```cpp
994: 		if (ads_events.selfcheck_status == 2 || ads_events.selfcheck_status == 0 ||
995: 			ads_events.selfcheck_status == 3 ||
996: 			ads_stats.state != AdsConnectionState::Running)
997: 			selfcheck_start_pending = false;
998: 		if (ads_events.selfcheck_status == 2 || self_check_done)
999: 			selfcheck_command_error = false;
```
**证据 `B/main.cpp:4655–4662`：**

```cpp
4655: 					pending_selfcheck_axes = 0;
4656: 					if (!valid || selfcheck_start_pending ||
4657: 						(ads_events.selfcheck_status != 1 && ads_events.selfcheck_status != 3) ||
4658: 						ads_stats.state != AdsConnectionState::Running)
4659: 					{
4660: 						selfcheck_command_error = true;
4661: 						std::cout << "自检启动被拒绝：参数不完整、越界或 PLC 未就绪。" << std::endl;
4662: 						break;
```

**已确认**：换手力锁 `ff.hold_582/hold_587.active`在计划回退创建时捕获对应力、扭矩和恢复方向（`B/main.cpp:2347–2349`、`B/control_types.h:550–560`）；回退完成标记handoff_complete和起点（main2883–2885、control_types563–568）。沿恢复方向实际移动≥2mm才开始100ms融合，融合完成清该hold（control_types571–578、`B/force_feedback.cpp:89–96`）。关闭对应力通道、UI ForceHold任一切换、安全clear_force_output均撤hold（control_types495–511、main4754–4756、150–156）。sample无效且控制条件仍成立会保留hold、若正在融合则把融合起时延后（force_feedback103–115）；这是只冻结触觉的状态，不锁轴，不要求重启。

**证据 `B/control_types.h:571–578`：**

```cpp
571: 	void observe_clamp_hold_motion(int channel, double axis_abs, bool position_valid, ULONGLONG now_ms)
572: 	{
573: 		HandoffHold& hold = channel == 2 ? hold_582 : hold_587;
574: 		if (!hold.active || !hold.handoff_complete || hold.blending || !position_valid) return;
575: 		if ((axis_abs - hold.resume_origin_abs) * hold.resume_direction >= 2.0)
576: 		{
577: 			hold.blending = true;
578: 			hold.blend_start_ms = now_ms;
```
**证据 `B/force_feedback.cpp:103–115`：**

```cpp
103: 	else if (!estop_hold_active && control_active && cal_state.zeroed)
104: 	{
105: 		ff.reset_force_filters();
106: 		auto retain_handoff_hold = [&](ForceFeedbackState::HandoffHold& hold,
107: 			double& output_force, double& output_torque)
108: 		{
109: 			if (!ff.clamp_hold_enabled || !hold.active) return;
110: 			if (hold.blending) hold.blend_start_ms = now_ms;
111: 			output_force = hold.force;
112: 			output_torque = hold.torque;
113: 		};
114: 		retain_handoff_hold(ff.hold_582, out_cmd.force_582_f, out_cmd.force_582_n);
115: 		retain_handoff_hold(ff.hold_587, out_cmd.force_587_f, out_cmd.force_587_n);
```

**已确认**：UI ForceZero或首次启用力反馈时，main同步等待连续3份force有效快照，每份wait_for_snapshot参数500ms（`B/main.cpp:733–759`、`B/zenoh_service.cpp:865–875`）。成功更新零点并清力，失败只返回、增加fail计数，允许再次请求（main760–777）；名义累计等待预算最高3×500ms，调度开销另计且任一失败即提前退出。**推断**：这也能造成main消费手柄的250ms间隔，而ADS线程心跳继续；没有证据表明现场恢复文案均来自该路径。

**证据 `B/main.cpp:738–749`：**

```cpp
738: 		for (int i = 0; i < 3; ++i)
739: 		{
740: 			AdsFastSnapshot snapshot{};
741: 			if (!ads_communication.wait_for_snapshot(after_sequence, 500, snapshot) || !snapshot.force_valid)
742: 			{
743: 				std::cout << source << "零点采集失败：ADS 连续 3 帧数据不完整。" << std::endl;
744: 				return false;
745: 			}
746: 			after_sequence = snapshot.attempt_sequence;
747: 			sums[0] += static_cast<double>(snapshot.fn_1_value) / 1000.0;
748: 			sums[1] += static_cast<double>(snapshot.ft_1_value) / 1000.0;
749: 			sums[2] += static_cast<double>(snapshot.fn_2_value) / 1000.0;
```

**已确认**：`gravity_comp_validated=false`只拒绝启用重力补偿（`B/force_calibration.h:48–49`、`B/main.cpp:5121–5135`），不会改control_active或PLC state。记录器/净距离跟踪的active/valid/stop标志只决定数据记录或实验区段（`B/experiment_recorder.cpp:622–638`、`B/handle_net_distance.h:5–35`），未发现写本体运动总门；它们不与运行故障锁混算。

**已确认**：main尾部保留shutdown_output（motion_enabled/cylinder_valid/轴4请求均清）、清力、停服务的收尾代码（`B/main.cpp:5161–5178`）；主循环却为while(true)（944），当前命令switch没有退出命令（4630–5157）。**推断**：不能把这段尾部收尾当作关闭控制台/强制结束进程时一定执行的保证；B进程消失后PLC主要由2s主机看门狗处理，定位臂/注射器另见T2。是否存在外部优雅终止机制，交付源码未见。

#### T1.8 定位臂服务的拒绝、abort与重新请求

**已确认**：局部命令还有下列门，均不直接置本体return_ads_fault_hold或gen_state。新合法请求不会补做旧点动；ArmStop/新ProgramZero清cartesian_blocked的事实不能保证硬件错误也已清。

|条件/故障|局部后果、出口|证据（B/arm_manual_ads_service.cpp）|确定程度|
|---|---|---|---|
|轴号/方向/倍率/速度或参数非法|直接忽略/false；合法输入后重新请求|164、189、203–212、238–239、405–416|已确认|
|曲线/归零/末端点动互斥|拒非零轴点动；有home/curve拒cjog；新归零拒curve/cjog/owned/home|207–208、242、268；占有结束再请求|已确认|
|关单轴或请求Reset|清cjog、递增epoch、请求停曲线，关该轴点动；需重新启用/动作|173–181、192–196|已确认|
|ADS非Running或host超时|clear_requests，清五轴使能/Reset/方向/曲线；健康可自动开总使能，旧轴使能不补|424–475|已确认|
|曲线快照无效/年龄>200ms、总使能未就绪|abort(1004)，取消曲线并置blocked|329–331；ArmStop或新ProgramZero清blocked269/287|已确认|
|曲线连续续期过期（300ms）|abort(1005)，与cjog500ms/轴点动300ms独立|333、274/296、14–17|已确认|
|任一轴未Power.Done、Power/Error、未请求使能、正在Reset或轴点动|abort(1006)|336–340；清原条件、Stop/新ProgramZero后重新动作|已确认|
|心跳/曲线数据/提交/取消ADS失败|abort(1003)，取消写也失败则cartesian_error=1003且blocked=true|305–321、343–345、378–395|已确认|
|提交后curve_sequence两秒仍不匹配|abort(1007)|348–351|已确认|
|PLC curve_state=4或3|分别abort(PLC curve_error)或abort(0)；取消成功且error0不blocked|353–354、314–321|已确认|
|速度参数还没在PLC快照匹配、curve_owned或某轴motion_busy|直接等，下拍再查；非独立永久latch|335、362–367；续期等其它abort门仍存在|已确认|
|规划器报错|abort(plan.error)；已在零位则status=3完成|370–375；需清错误条件再新请求|已确认|
|curve/cjog/轴点动的继续请求未续期|到deadline清本地方向/abort；已开始ADS调用阻塞时检查可迟到|478–501、333；重新操作，不补旧方向|结构已确认，迟到推断|

**证据 `B/arm_manual_ads_service.cpp:314–321`：**

```cpp
314: 	auto abort = [&](int error) {
315: 		const bool cancel = true;
316: 		const bool stopped = write("G.arm_curve_cancel", sizeof(cancel), &cancel);
317: 		home_requested_ = false;
318: 		curve_pending_ = false;
319: 		cartesian_blocked_ = error != 0 || !stopped;
320: 		snapshot_.cartesian_status = error == 0 && stopped ? 4 : 5;
321: 		snapshot_.cartesian_error = stopped ? error : 1003;
```
**证据 `B/arm_manual_ads_service.cpp:328–333`：**

```cpp
328: 	if (!curve_pending_ && !home_requested_) return;
329: 	if (!snapshot_.valid || now - snapshot_at_ > std::chrono::milliseconds(200)
330: 		|| !desired_manual_enable_ || !snapshot_.manual_enable) {
331: 		abort(1004); return;
332: 	}
333: 	if (now > cartesian_deadline_) { abort(1005); return; }
```

<a id="t2-state"></a>
### T2. PLC：状态转移、锁存与限位恢复

#### T2.1 `G.gen_state` 的完整写点与进入条件

**已确认**：正式编译POU共有14处运行期赋值，另有 `G:7` 初值 `_init`。下表穷举；各动作在MAIN的CASE分派（`P/MAIN.TcPOU:114–144`）。未发现写 `_reset` 的正式PLC语句；主/数据端也未发现ADS写 `G.gen_state`，只有读取/本地通知缓存（`B/plc_io.cpp:49`、`B/ads_communication.cpp:714,1182–1193,1952`）。**未知**：现场第三方ADS工具、手工在线写或其它未交付程序是否进入_reset。

|目标状态|来源与全部赋值位置|准确条件|确定程度|
|---|---|---|---|
|`_init`|`P/MAIN.TcPOU:39`|Task1.FirstCycle，无条件覆盖原state|已确认|
|`_init`|`P/clear_err.TcPOU:66`|clear_err step1，无本体Status/Power/Reset错，但self_check_done假|已确认|
|`_init`|`P/reset.TcPOU:75`|Reset完成或3s到期后，本体Status/Power/Reset均无错|已确认|
|`_self_check`|`P/init.TcPOU:81`|7轴Power.Done均真，并且未先命中错误/5s超时|已确认|
|`_err`|`P/init.TcPOU:52`|任一本体轴Status.Error或Power.Error|已确认|
|`_err`|`P/init.TcPOU:70`|上电等待5s超时|已确认|
|`_err`|`P/handle.TcPOU:277`|初始化完成后，7轴任一Power.Done假或Status/Power/Reset.Error真（216–232）|已确认|
|`_err`|`P/clear_err.TcPOU:59`|step1任一本体轴Status/Power/Reset.Error|已确认|
|`_err`|`P/reset.TcPOU:71`|Reset完成/超时后仍有上述任一错|已确认|
|`_clear_err`|`P/err.TcPOU:82,128,171`|step1/3/11：无has_err且7轴全Power.Done；has_err另包含Axis15|已确认|
|`_handle`|`P/clear_err.TcPOU:83`|已自检、无本体错误、全Power.Done连续200ms；先置handle_reinit_req真|已确认|
|`_handle`|`P/SelfCheck.TcPOU:566`|所有自检stu到10并完成参考与装载资格更新|已确认|
|`_reset`|未找到赋值；仅分派`P/MAIN.TcPOU:143`|本次磁盘源码无进入来源|已确认（搜索范围内）；现场来源未知|

**证据 `P/MAIN.TcPOU:38–46`：**

```iecst
38: IF TwinCAT_SystemInfoVarList._TaskInfo[1].FirstCycle THEN
39: 	G.gen_state := _init;
40: 	G.self_check_done := FALSE;
41: 	G.selfcheck_start_req := FALSE;
42: 	G.selfcheck_status := 0;
43: 	G.handle_reinit_req := FALSE;
44: 	G.handle_reinit_done := FALSE;
45: 	G.estop_hold_req := FALSE;
46: 	G.selfcheck_reset_req := TRUE;
```
**证据 `P/clear_err.TcPOU:63–67`：**

```iecst
63: 	IF NOT G.self_check_done THEN
64: 		settle_timer(IN:=FALSE);
65: 		step := 0;
66: 		G.gen_state := _init;
67: 		RETURN;
```
**证据 `P/clear_err.TcPOU:78–84`：**

```iecst
78: 	settle_timer(IN:=TRUE, PT:=T#200MS);
79: 	IF settle_timer.Q THEN
80: 		settle_timer(IN:=FALSE);
81: 		step := 0;
82: 		G.handle_reinit_req := TRUE;
83: 		G.gen_state := _handle;
84: 	END_IF
```

**已确认**：进入_self_check并不马上寻限位。MAIN等待 `selfcheck_start_req`；目标1须5..96mm、3须10..650mm、5/6须10..670mm，并要求1≤3≤5≤6（`P/MAIN.TcPOU:116–137`）。不合格status=3；合法后把目标锁定，status=2才逐拍调用SelfCheck。已开始的SelfCheck路径中没有基于 `host_comm_timeout/estop_hold_req` 的取消检查，也没有进入_err的赋值（`P/SelfCheck.TcPOU:140–566`）。**推断**：主机掉线不能据此保证正在寻参的动作立即退出；真实硬件保护仍取决于驱动与NC。

#### T2.2 PLC锁存的置位、清除和恢复后动作

|量/状态|置位条件与位置|清除条件与位置|恢复后的行为|确定程度|
|---|---|---|---|---|
|`G.host_comm_timeout`|session为0、心跳2s不变、非首次session变化；`P/MAIN.TcPOU:63–84`|非_handle：心跳有效且host_recover_req，85–90；_handle：由下行锁存统一清|继续原状态机；handle走重新建基准|已确认|
|handle `host_timeout_latched`|上述条件或G.host_comm_timeout；`P/handle.TcPOU:100–124`|host_recover_req、session非零、当前未超时且所有捕获动作已受控停止，416–421|清超时，请求handle_reinit；旧回退/轴4不续跑|已确认|
|`estop_hold_req`|本体错误边沿258；err62；clear_err43|健康handle在非超时分支433清；FirstCycle45清|B见下降沿后可按当前位置重建；不是执行旧运动|已确认|
|`handle_reinit_req / !handle_reinit_done`|FirstCycle、自检结束、clear_err或host恢复；`P/MAIN.TcPOU:43–44`、`P/SelfCheck.TcPOU:548`、`P/clear_err.TcPOU:82`、`P/handle.TcPOU:421`|消费reinit请求126–165；全Enable.Done后211按NOT host_timeout确认|当前实际值刷新refer/滤波/轨迹/软界，清回退与轴4任务|已确认|
|return Busy/Done/Error与局部return_state|接受Req后进入状态，终态等撤Req；`P/handle.TcPOU:541–627`|Req归零（541–547）、host处理303–318、reinit135–147|新的Req才能新动作，已取消动作无恢复阶段|已确认|
|axis4手动busy/error/state|`P/handle.TcPOU:638–755`|方向0清Error641–643，reinit152–165，host_stop381–413|新方向才再起动|已确认|
|Axis15 `gear_failed`|GearIn.Error/Aborted；`P/Axis2Sync.TcPOU:57–60`|使能/主从Power不满足时29–33清，再满足且静止后重试|重建齿轮同步，不直接改变gen_state|已确认|
|SelfCheck `stu/step/selfcheck_started`|MAIN128–130及SelfCheck各阶段|离开_self_check MAIN108–112清started；selfcheck_reset_req在SelfCheck初始清局部阶段；`P/SelfCheck.TcPOU:55–80`|重新做该流程；没有单独UI取消|已确认|
|Arm局部 owned/error/limited|Arm三个POU；详T2.6/T8|停机释放或新合法请求/局部Reset；limited逐拍重算|只作用定位臂，无本体state赋值|已确认|

**证据 `P/MAIN.TcPOU:78–90`：**

```iecst
78: host_heartbeat_seen := G.host_session_id <> 0;
79: host_watchdog_timer(
80: 	IN:=host_heartbeat_seen AND NOT host_heartbeat_changed,
81: 	PT:=T#2S
82: );
83: IF (NOT host_heartbeat_seen) OR host_watchdog_timer.Q THEN
84: 	G.host_comm_timeout := TRUE;
85: ELSIF G.gen_state <> _handle THEN
86: 	// 自检前等待阶段：收到上位机恢复请求且心跳正常时解除超时
87: 	IF G.host_recover_req THEN
88: 		G.host_comm_timeout := FALSE;
89: 		G.host_recover_req := FALSE;
90: 	END_IF
```
**证据 `P/handle.TcPOU:415–425`：**

```iecst
415: 	// 清请求并解除超时即为 PLC 对恢复请求的确认；随后复用既有重初始化流程。
416: 	IF G.host_recover_req AND host_heartbeat_seen AND
417: 		(NOT host_watchdog_timer.Q) AND host_all_stopped THEN
418: 		host_timeout_latched := FALSE;
419: 		G.host_comm_timeout := FALSE;
420: 		G.host_recover_req := FALSE;
421: 		G.handle_reinit_req := TRUE;
422: 	END_IF
423: 
424: 	host_timeout_prev := G.host_comm_timeout;
425: 	RETURN;
```

**已确认**：handle通信保持第一次只捕获一次绝对hold位置（284–290），之后持续给该固定位置、速度0（352–358），而非逐拍把Actual灌回目标。只对超时前已执行的回退/轴4激活MC_Stop（293–301、335–413），持续撤Req/fast标志/启动旁路（303–318），恢复条件要求这些停止完成；MC_Stop.Error会拉低重试，没有总超时。数据端每秒请求watchdog恢复（`B/main.cpp:1669–1677`），不会直接清PLC任意错误。**推断**：恢复顺利时重新以当前位置建立控制，通信中断的独立动作不续做；如果Stop一直Error，保持本身也可无限等待。

<a id="t2-limit"></a>
#### T2.3 直线轴持续压电子限位：err与SelfCheck的处理不同

**已确认**：`err` 的has_err是Status.Error、DriveDeviceError、Power.Error、Reset.Error以及Axis15错误；没有 `DriveLimitActive` 条件（`P/err.TcPOU:35–60`）。`DriveLimitActive` 是 `ST_AxisStatus.StateDWord` bit18的聚合标志，不是分别传入的左/右开关；SelfCheck因正在做负向MoveVelocity（`P/SelfCheck.TcPOU:511–518`）而将它视为左端触发（183–185）。位含义核对Beckhoff Infosys《ST_AxisStatus》，文档70161291，访问2026-11-07；本工程实际接线/驱动映射仍未知。

**证据 `P/err.TcPOU:35–45`：**

```iecst
35: FOR i:=1 TO 7 DO
36: 	IF NOT G.power_output[i].Done THEN
37: 		all_powered := FALSE;
38: 	END_IF
39: 
40: 	IF G.axis[i].Status.Error OR G.axis[i].Status.DriveDeviceError OR G.power_output[i].Error OR G.reset_output[i].Error THEN
41: 		has_err := TRUE;
42: 
43: 		IF (G.power_output[i].Error AND (G.power_output[i].ErrorID = power_not_ready_errid))
44: 		   OR (G.axis[i].Status.Error AND (G.axis[i].Status.ErrorID = power_not_ready_errid)) THEN
45: 			has_power_not_ready := TRUE;
```
**证据 `P/err.TcPOU:57–69`：**

```iecst
57: IF G.axis2_sync_axis.Status.Error OR G.axis2_sync_axis.Status.DriveDeviceError THEN
58: 	has_err := TRUE;
59: 	has_other_error := TRUE;
60: END_IF
61: 
62: G.estop_hold_req := TRUE;
63: 
64: CASE step OF
65: 0:
66: 	FOR i:=1 TO 7 DO
67: 		G.SetPointGenEnable[i].Execute := FALSE;
68: 		G.SetPointGenDisable[i].Execute := TRUE;
69: 		G.axis_Reset[i].Execute := FALSE;
```

**已确认**：普通err先禁外部给定并置estop；无错误且全部Power.Done时进_clear_err。普通错误走200ms Reset脉冲→最多等2s→回step1，无次数上限（103–135）。仅4650且无其它错误，或无Error但Power未就绪时走Power恢复路径（86–100）；step10只在该轴仍报4650/DriveDeviceError时令Power.Enable=FALSE，否则当拍又TRUE（137–147），200ms后统一TRUE并等3s/再次重试（150–184）。故“有200ms timer”不能证明每轴Enable一定保持FALSE满200ms。整个err没有MC_Stop、速度停稳判断或正向退离位置命令（22–185）。

**证据 `P/err.TcPOU:103–115`：**

```iecst
103: 	FOR i:=1 TO 7 DO
104: 		IF G.axis[i].Status.Error OR G.axis[i].Status.DriveDeviceError OR G.power_output[i].Error OR G.reset_output[i].Error THEN
105: 			G.axis_Reset[i].Execute := TRUE;
106: 		END_IF
107: 	END_FOR
108: 	pulse_timer(IN:=TRUE, PT:=T#200MS);
109: 	step := 2;
110: 
111: 2:
112: 	pulse_timer(IN:=TRUE);
113: 	IF pulse_timer.Q THEN
114: 		FOR i:=1 TO 7 DO
115: 			G.axis_Reset[i].Execute := FALSE;
```
**证据 `P/err.TcPOU:137–151`：**

```iecst
137: 10:
138: 	FOR i:=1 TO 7 DO
139: 		IF (G.power_output[i].Error AND (G.power_output[i].ErrorID = power_not_ready_errid))
140: 		   OR (G.axis[i].Status.Error AND (G.axis[i].Status.ErrorID = power_not_ready_errid))
141: 		   OR G.axis[i].Status.DriveDeviceError THEN
142: 			G.power_[i].Enable := FALSE;
143: 			G.axis_Reset[i].Execute := TRUE;
144: 		ELSE
145: 			G.power_[i].Enable := TRUE;
146: 			G.axis_Reset[i].Execute := FALSE;
147: 		END_IF
148: 	END_FOR
149: 
150: 	pulse_timer(IN:=TRUE, PT:=T#200MS);
151: 	IF pulse_timer.Q THEN
```

|压限位时实际反馈|err静态路径|可否回_handle|确定程度|
|---|---|---|---|
|DriveLimitActive仍真，但Status/Drive/Power/Reset错误均清且7轴Power.Done真|进入_clear_err；已自检且200ms稳定后_handle|代码允许；不主动移开轴|已确认（条件路径）|
|限位持续导致任一被检查错误一直为真|Reset或Power恢复循环反复，不满足err出口|持续条件下无法退出；何种错误需现场确认|推断|
|错误消失但某轴Power一直Done假|进入/重复Power恢复，不满足all_powered|持续条件下无法退出|推断|
|err暂清又因原条件重现|clear_err可再回_err；handle后也会检查Power/Error|可能循环；实际错误码/时序未知|推断|

**已确认**：SelfCheck触限后记录leftlimit并MC_Stop（183–198），等待实际速度<1、持续300ms（215–237）；其55阶段明确对目标轴Power.Enable=FALSE并保持到200ms结束，再重启并等3s（285–306），不是按Error是否仍在来决定每拍FALSE。之后SetPosition用当前ActPos、Mode=FALSE（333–338），退离目标为leftlimit+用户正距离（360–369），经MoveAbsolute离开（388–440）。轴6还有“自检触限只禁止负向、保留正向”的MAIN特例（100–101）。这些动作在普通err中不存在。

**证据 `P/SelfCheck.TcPOU:183–195`：**

```iecst
183:             IF G.axis[i].Status.DriveLimitActive THEN
184:                 // 记录左限位（绝对坐标）
185:                 G.leftlimit[i] := G.axis[i].NcToPlc.ActPos;
186: 
187:                 // 立即撤销 MoveVelocity，并在轴状态完全失效前发送受控停止。
188:                 // 现场确认 16#4650 来自 MoveVelocity/MC_Power 就绪链路，而不是 MC_Stop。
189:                 mv_vel_exec[i] := FALSE;
190:                 drive_limit_stop[i] := TRUE;
191:                 wait_timer[i](IN:=FALSE);
192:                 fb_stop[i](
193:                     Axis:=G.axis[i],
194:                     Deceleration:=500,
195:                     Jerk:=1000,
```
**证据 `P/SelfCheck.TcPOU:285–299`：**

```iecst
285:         55: // RepowerPulse: 关闭 MC_Power Enable 一个脉冲，让轴重新初始化
286:             G.power_[i].Enable := FALSE;
287:             power_pulse_timer[i](IN:=TRUE, PT:=T#200ms);
288:             IF power_pulse_timer[i].Q THEN
289:                 power_pulse_timer[i](IN:=FALSE);
290:                 G.power_[i].Enable := TRUE;
291:                 stu[i] := 56;
292:             END_IF
293: 
294:         56: // RepowerWait: 等待 MC_Power 重新就绪
295:             G.power_[i].Enable := TRUE;
296:             power_wait_timer[i](IN:=TRUE, PT:=T#3s);
297:             IF G.power_output[i].Done AND NOT G.power_output[i].Error THEN
298:                 power_wait_timer[i](IN:=FALSE);
299:                 drive_limit_stop[i] := FALSE;
```
**证据 `P/SelfCheck.TcPOU:360–369`：**

```iecst
360:                 // 启动绝对回退（目标=左限位绝对坐标 + 回退距离）
361:                 debug_limit_pos[i] := G.leftlimit[i];
362:                 debug_init_target_pos[i] := G.leftlimit[i] + init_target_from_left[i];
363:                 
364:                 mv_abs_pos[i] := debug_init_target_pos[i];
365:                 mv_abs_vel[i] := vel_back;
366:                 mv_abs_exec[i] := FALSE;
367:                 wait_timer[i](IN:=FALSE);
368:                 
369:                 stu[i] := 72;
```

**未知**：现场“可清除限位错”究竟是NC错误、驱动内部限位、Power 4650、DriveDeviceError还是机械接触导致的其它故障；缺少触发当拍的PLC-ESTOP全量快照、6041/60FD/错误对象与接线定义。源码足以确认不主动退离，不能证明每次压开关都必需PLC重启。

<a id="t2-waits"></a>
#### T2.4 其它可无限等待的阶段与一个不可达的回退重建分支

|阶段|本地退出门槛/遗漏|外部可用出口及限制|证据|程度|
|---|---|---|---|---|
|handle初始化|七个ExtSetPointGenEnable.Done；不查该FB.Error、不设timeout|reinit能重置局部状态，但仍可能重遇同错；本段RETURN早于错误和host恢复处理|`P/handle.TcPOU:168–214,216–281`|结构已确认；死等推断|
|回退10|Disable.Done或轴已非外给定；不查Disable.Error/timeout|host超时/reinit清任务；B业务可能先锁B01|`P/handle.TcPOU:559–565`|同上|
|回退20/30|运动Done/Error/Aborted，随后速度≤0.5；运动无PLC总执行时限|host停止/reinit；B的5s总限独立存在|`P/handle.TcPOU:568–608`|同上|
|回退31|重新Enable.Done；不查Enable.Error/timeout|host/reinit|`P/handle.TcPOU:610–627`|同上|
|轴4 10/30/40|Disable.Done、停稳、Enable.Done；部分无FB.Error/总timeout|host/reinit；终态Error还要方向0|`P/handle.TcPOU:638–755`|同上|
|SelfCheck|Move/Reset/Power阶段逐项推进；4↔5、55↔56可重试；无总时长/总行程/主机取消|各阶段自然成功；当前UI无终止命令|`P/SelfCheck.TcPOU:140–506`、`P/MAIN.TcPOU:116–138`|结构已确认；长等待推断|
|Arm曲线/末端点动停止释放|等待停稳、Disable/Enable释放；部分阶段无总timeout|受局部动作/驱动状态支配；不直接改本体state|`P/ArmCartesian.TcPOU:249–269`、`P/ArmCartJog.TcPOU:45–146`|结构已确认；长等待推断|

**证据 `P/handle.TcPOU:168–181`：**

```iecst
168: IF NOT init_done THEN
169: 	all_enabled := TRUE;
170: 	FOR i:=1 TO 7 DO
171: 		G.SetPointGenEnable[i].Execute := TRUE;
172: 		IF NOT G.SetPointGenEnable[i].Done THEN
173: 			all_enabled := FALSE;
174: 		END_IF
175: 	END_FOR
176: 
177: 	IF all_enabled THEN
178: 		FOR j:=1 TO 7 DO
179: 			G.Act_pos[j] := G.axis[j].NcToPlc.ActPos - G.init_pos[j];
180: 			G.act_pos_from_left[j] := G.axis[j].NcToPlc.ActPos - G.leftlimit[j];
181: 			G.ref_slow[j] := G.Act_pos[j];
```
**证据 `P/handle.TcPOU:209–214`：**

```iecst
209: 		init_done := TRUE;
210: 		// 通信超时未解除时不提前确认恢复；正式握手后会再次进入本初始化流程。
211: 		G.handle_reinit_done := NOT G.host_comm_timeout;
212: 	END_IF
213: 	RETURN;
214: END_IF
```
**证据 `P/handle.TcPOU:610–623`：**

```iecst
610: 		G.return_cmd[i].Busy := TRUE;
611: 		spg_enable_exec[i] := TRUE;
612: 		IF G.SetPointGenEnable[i].Done THEN
613: 			ext_setpoint_enabled[i] := TRUE;
614: 			G.return_cmd[i].Busy := FALSE;
615: 			IF return_finish_error[i] THEN
616: 				G.return_cmd[i].Done := FALSE;
617: 				G.return_cmd[i].Error := TRUE;
618: 				G.return_cmd[i].ErrorId := return_error_id_pending[i];
619: 			ELSE
620: 				G.return_cmd[i].Done := TRUE;
621: 				G.return_cmd[i].Error := FALSE;
622: 				G.return_cmd[i].ErrorId := 0;
623: 			END_IF
```

**已确认**：回退CASE 30内部又判断 `IF return_state[i] <> 30 THEN`（591），条件在该分支恒假，因此592–602的参考、滤波、轨迹重建不会执行；之后仍进入31。在状态≥30时前面的总逻辑还把refer冻结到traj_pos（456–459）。**推断**：回退结束重新接外部给定时可能沿用旧轨迹基准、引起目标修正；是否造成实际跳动/报错，需记录外部给定与ActPos验证，不能据静态分支直接声称已发生。

**证据 `P/handle.TcPOU:584–598`：**

```iecst
584: 	30:
585: 		G.return_cmd[i].Busy := TRUE;
586: 		IF ABS(G.axis[i].NcToPlc.ActVelo) <= 0.5 THEN
587: 			G.Act_pos[i] := G.axis[i].NcToPlc.ActPos - G.init_pos[i];
588: 			G.act_pos_from_left[i] := G.axis[i].NcToPlc.ActPos - G.leftlimit[i];
589: 			// 状态 30 只锁存位置一次，不再更新 refer/ref_slow/act_h
590: 			// 防止在禁用→重启外部给定期间参考继续变化
591: 			IF return_state[i] <> 30 THEN
592: 				G.refer[i] := G.Act_pos[i];
593: 				G.ref_slow[i] := G.Act_pos[i];
594: 				G.act_h[i] := G.Act_pos[i];
595: 				G.act_hf[i] := G.Act_pos[i];
596: 				G.refer_from_left[i] := G.act_pos_from_left[i];
597: 				FOR k:=1 TO 30 DO
598: 					act_hh[i][k] := G.Act_pos[i];
```

#### T2.5 自检资格的全部清零点和左右软限位

**已确认**：正式源中 `G.self_check_done` 变FALSE只有三处：`G:28` 声明初值，`P/MAIN.TcPOU:40` FirstCycle，`P/init.TcPOU:36` init step0。唯一TRUE是 `P/SelfCheck.TcPOU:546`（541为all_done条件）。err/clear_err、通信保持、Axis15/注射器/定位臂POU均不主动清它；reset若有外部入口，成功后进_init间接清。故“普通错后必须重新寻左”不是所有err分支的必然结果；PLC重启/应用重新FirstCycle则必丢资格。

**已确认**：PLC handle的 `slimit_enable` 仅用于直线轴1/3/5/6，右界=leftlimit+99/666/688/688mm（`P/handle.TcPOU:45–47,195–197`）。这是实际位置已经≥右界且refer还要大于ref_slow时，将refer保持为ref_slow（478–482）；不是预先把所有目标截在右界，也未证明能拦截独立MoveAbsolute。没有与之对称的左界判断。NC6..15的AxisPara/EncPara中未显式配置EnableSoftPos/Neg及界值，`TC`里“SoftLimit”命中仅类型字段（793、799、925、931）；不能从文件省略推定在线启用状态或默认界值。

**证据 `P/handle.TcPOU:478–482`：**

```iecst
478: 	IF slimit_enable[j] THEN
479: 		IF (G.axis[j].NcToPlc.ActPos >= right_slimit[j]) AND (G.refer[j] > G.ref_slow[j]) THEN
480: 			G.refer[j] := G.ref_slow[j];
481: 		END_IF
482: 	END_IF
```

|左侧相关机制|覆盖与未覆盖|证据|确定程度|
|---|---|---|---|
|axis1业务窗口|默认leftlimit1+5..30mm；Follow双端夹取，递送即使窗口inactive也有下界|`B/control_types.h:102–103`、`B/motion_sync.cpp:26–34`、`B/main.cpp:3793–3812`|已确认|
|axis6独立导丝窗口|leftlimit6+(axis5实际距左)+1..33mm；这是相对axis5的窗口，非统一绝对左0保护|`B/control_types.h:106–109`、`B/motion_sync.cpp:51–65`、`B/main.cpp:3465–3472`|已确认|
|axis3/5镜像|直接base+axis1_delta，没有各自独立左界夹取；axis6被纳入镜像时也base+delta|`B/motion_sync.cpp:513–521`、`B/main.cpp:3905`|已确认|
|axis3距左≤20mm递送止动|特定递送/换手门控；不是全模式全路径左保护|`B/main.cpp:1951–1953,3541,3960–3966`|已确认|
|startup/自检正目标范围|只约束输入和目标，不等于所有运行路径左软限位|`P/MAIN.TcPOU:121–127`、`B/main.cpp:5037–5040`|已确认|
|电子开关/驱动内部限位|有DriveLimitActive反馈，但未找到左右分立PLC接线逻辑；硬件是否限制单向、如何清错未知|`P/SelfCheck.TcPOU:183–185`、`TC:6468,7525–7540`|代码已确认；硬件未知|

**证据 `B/motion_sync.cpp:513–521`：**

```cpp
513: 	void apply_axis1_mirror_from_abs(AppContext& ctx, double axis1_abs_cmd, bool include_axis6)
514: 	{
515: 		const double axis1_delta_rel = axis1_abs_cmd - ctx.plc_init_pos[0] - ctx.axis1_crawl->base_rel;
516: 		ctx.pos[2] = *ctx.axis3_base_rel + axis1_delta_rel;
517: 		ctx.pos[4] = *ctx.axis5_base_rel + axis1_delta_rel;
518: 		if (include_axis6)
519: 		{
520: 			ctx.pos[5] = *ctx.axis6_mirror_base_rel + axis1_delta_rel;
521: 		}
```

<a id="t2-other-axes"></a>
#### T2.6 注射器、Axis15与定位臂是否传播到本体gen_state

**已确认**：Axis15本身未写gen_state，handle进入_err的检查只遍历本体1..7（`P/handle.TcPOU:216–232`）。但是一旦因其它原因进入err，Axis15的Status.Error/DriveDeviceError会计入has_err、阻止退出（`P/err.TcPOU:57–60`）。Axis15 Reset.Execute只跟随 `G.axis_Reset[2].Execute`（`P/Axis2Sync.TcPOU:17–20`），err普通Reset只对本体有错轴置位（103–107）。**推断**：仅剩Axis15错误而本体2无错时可能一直缺Reset脉冲；轴间耦合会不会把错进一步传到主轴，需要NC/驱动实证。

**证据 `P/Axis2Sync.TcPOU:17–20`：**

```iecst
17: fb_reset(
18: 	Axis:=G.axis2_sync_axis,
19: 	Execute:=G.axis_Reset[2].Execute
20: );
```
**证据 `P/Axis2Sync.TcPOU:29–33`：**

```iecst
29: IF NOT (G.power_[2].Enable AND G.power_output[2].Done AND fb_power.Status) THEN
30: 	gear_execute := FALSE;
31: 	gear_active := FALSE;
32: 	gear_done := FALSE;
33: 	gear_failed := FALSE;
```

**已确认**：InjectorManual始终请求两注射器Power=TRUE，host_comm_timeout或同轴双向冲突则方向归零（`P/InjectorManual.TcPOU:24–42`）。该POU没有estop_hold_req判断，没有MC_Reset或gen_state写入。**未知**：注射器故障在真实NC/总线层是否扩散；就应用代码而言不直接使本体_err。仅本体estop时不能据此断言注射器POU也收到停止条件。

**证据 `P/InjectorManual.TcPOU:24–38`：**

```iecst
24: 	IF G.host_comm_timeout THEN
25: 		G.inject_push_req[i] := FALSE;
26: 		G.inject_pull_req[i] := FALSE;
27: 	END_IF
28: 
29: 	fb_power[i](
30: 		Axis:=G.inject_axis[i],
31: 		Enable:=TRUE,
32: 		Enable_Positive:=TRUE,
33: 		Enable_Negative:=TRUE
34: 	);
35: 
36: 	IF G.inject_push_req[i] AND NOT G.inject_pull_req[i] THEN
37: 		req_dir := 1;
38: 	ELSIF G.inject_pull_req[i] AND NOT G.inject_push_req[i] THEN
```

**已确认**：ArmManual关总使能或host超时清请求（35–42）；allow_jog受owned、Power、Reset、冲突、轴错误门控（105–143），局部error在Reset成功或新合法点动时清（93–96、159–161）。ArmCartesian的drive_ready包含estop和host条件（63–70）；参数/就绪/0.5s保活失败走局部停止释放，新合法请求清error（72–177、249–269）。ArmCartJog有0.5s续期和2s使能超时（45–146），`ArmCartJogGen` limited每拍重算（94、212）。三个POU均没有gen_state/self_check_done赋值；反向干扰的共享线程/PLC任务路径见T8。

### T3. 主端：控制权、停止操作、USB与恢复

#### T3.1 控制权申请、失效与恢复：主端缓存不等于PLC会话

**已确认**：连接按钮先ConnectAsync，再AcquireControlAsync（`R/ViewModels/MainViewModel.cs:123–151`）。Connect创建Zenoh会话、两publisher及state/force订阅，发Hello，最多等10s认证状态（`R/Services/RemoteRobotLink.cs:117–176`）；Acquire请求回执期限5s，成功才设置 `_controlRequested`（182–191）。真正 `_hasControl` 来自通过session/HMAC/增序校验的state里的LeaseHeld标志（414–428），不是收到Acquire回执就立即开放UI。

**证据 `R/Services/RemoteRobotLink.cs:182–191`：**

```csharp
182:         public async Task<CommandResult> AcquireControlAsync()
183:         {
184:             int generation;
185:             lock (_lock) generation = ++_controlRequestGeneration;
186:             var result = await RequestAsync(ZenohCommand.Acquire, 0, 0, 0, 0, 5000).ConfigureAwait(false);
187:             lock (_lock)
188:             {
189:                 if (generation == _controlRequestGeneration && _acceptingCallbacks) _controlRequested = result.Ok;
190:             }
191:             return result;
```

|时点/条件|主端动作|数据端/PLC含义|证据与程度|
|---|---|---|---|
|正常持有控制权|100Hz持续送HandleFrame；无独立续租定时命令|B租约无TTL，不需要周期Acquire；PLC另看B ADS线程session/heartbeat|`R/Services/RemoteRobotLink.cs:337–377`、`B/zenoh_service.cpp:769–780,1003`、T5.3；已确认|
|认证state龄期≥2000ms，或session不可用|清本地hasControl、注射器/轴4持续方向、触觉；停发整个HandleFrame|不直接发送本体急停，也不等于B租约已撤销|`R/Services/RemoteRobotLink.cs:86–97,357`、`A/MasterConsole.Protocol/ProtocolConstants.cs:9–11`；已确认|
|state距上次>3000ms且距上次握手≥3000ms|清认证/session key/序号缓存，发新Hello；拒pending，“命令不补发”|成功新认证才撤旧B租约；Hello本身不撤|`R/Services/RemoteRobotLink.cs:494–513`、`B/zenoh_service.cpp:686–731`；已确认|
|重新认证后第一份有效state|若此前_controlRequested为真且未持有，则自动发一次Acquire(id=0)|恢复先前控制权意图；旧运动命令和持续方向不重放|`R/Services/RemoteRobotLink.cs:418–428`；已确认|
|同一认证会话只丢LeaseHeld|更新hasControl为假；上述自动Acquire条件不成立|当前UI提示断开再连；未见独立自动重试Acquire定时器|`R/Services/RemoteRobotLink.cs:418–428`、`R/ViewModels/MainViewModel.cs:168–175`；已确认|
|用户主动断开|Shutdown清会话、控制意图、方向、力并Dispose|当前Disconnect未调用ReleaseControlAsync；B会靠采样失效或下次新认证处理|`R/Services/RemoteRobotLink.cs:179–196,534–560`；已确认|

**证据 `R/Services/RemoteRobotLink.cs:86–97`：**

```csharp
86:         private bool IsFreshLocked(long now) => _acceptingCallbacks && _authenticated && _sessionAvailable() &&
87:             _lastStateMs >= 0 && now - _lastStateMs >= 0 && now - _lastStateMs < StateTimeoutMs;
88: 
89:         private void ExpireStateLocked(long now)
90:         {
91:             if (IsFreshLocked(now)) return;
92:             _hasControl = false;
93:             _inj1 = _inj2 = _axis4 = 0;
94:             if (!_forceGateOpen) return;
95:             _forceGateOpen = false;
96:             _handles?.ClearHaptic();
97:         }
```
**证据 `R/Services/RemoteRobotLink.cs:418–428`：**

```csharp
418:                 bool restoreControl = !_authenticated && _controlRequested;
419:                 _lastStateSequence = f.Sequence;
420:                 _hasStateSequence = _authenticated = true;
421:                 _lastStateMs = receivedMs;
422:                 _hasControl = (f.Flags & (uint)StatusFlags.LeaseHeld) != 0;
423:                 if (!_forceGateOpen) _forceGateOpenedAtOrdinal = callbackOrdinal;
424:                 _forceGateOpen = true;
425:                 ExpireStateLocked(now);
426:                 // 自动重认证只恢复先前的控制权申请，不补发运动指令或旧输入。
427:                 if (restoreControl && !_hasControl && IsFreshLocked(now))
428:                     SendCommand(ZenohCommand.Acquire, 0, 0, 0, 0, "", null, null, null, 0);
```

**已确认**：这四种“会话/存活”相互独立：主端本地state新鲜度、Zenoh认证/租约、B远程手柄样本年龄、PLC的B ADS session/heartbeat（上述代码、T4/T5）。主端重启只改变Zenoh客户端，不会重建B的host_session_id；数据端进程重启才产生新的ADS主机会话（`B/ads_communication.cpp:158–160`）。因此不能把主端断开直接描述为PLC看门狗2s超时。

<a id="t3-session"></a>
#### T3.2 已关闭会话残留：源码可确认的当前UI恢复缺口

**已确认**：`ZenohSession.IsAvailable` 依赖native `z_session_is_closed`（`A/VI.Service.Net/ZenohSession.cs:26–27`）；它为假时RemoteRobotLink.IsConnected必假（86–87）。但ConnectAsync只要 `_zenoh != null` 就直接return（122），不重建closed对象。UI仅在IsConnected真时调用Disconnect（`R/ViewModels/MainViewModel.cs:126`）；此时点击“连接”会得到Connect立即返回、Acquire因未连接而拒（`R/Services/RemoteRobotLink.cs:285`），UI只记警告不Disconnect（VM133–135）。CheckConnection只重认证，不new原生会话（494–513），closed publisher又使SendCommand返回false（314）。

**证据 `A/VI.Service.Net/ZenohSession.cs:26–27`：**

```csharp
26:         public bool IsAvailable { get { lock (_publishLock) return AvailableLocked; } }
27:         private bool AvailableLocked => !_disposing && _loanedSession != IntPtr.Zero && !Native.z_session_is_closed(_loanedSession);
```
**证据 `R/Services/RemoteRobotLink.cs:117–124`：**

```csharp
117:         public async Task ConnectAsync()
118:         {
119:             ZenohSession session;
120:             lock (_lock)
121:             {
122:                 if (_zenoh != null) return;
123:                 if (_cfg.Token == null || _cfg.Token.Length < 16) throw new InvalidOperationException("连接认证密钥缺失或长度不足。");
124:                 string endpoint = string.IsNullOrWhiteSpace(_cfg.Endpoint) ? "tcp/" + _cfg.Host + ":7447" : _cfg.Endpoint;
```
**证据 `R/ViewModels/MainViewModel.cs:123–135`：**

```csharp
123:         private async void ToggleConnect()
124:         {
125:             if (_connecting) return;
126:             if (IsConnected) { _link.Disconnect(); _hasStatus = false; RaiseConnectionProps(); return; }
127:             _connecting = true;
128:             RaiseConnectionProps();
129:             try
130:             {
131:                 await _link.ConnectAsync();
132:                 var result = await _link.AcquireControlAsync();
133:                 if (!result.Ok)
134:                 {
135:                     AddLog("warn", "自动获取控制权失败：" + result.Reason + (IsConnected ? " 链路仍保持连接。" : " 链路未连接。"));
```

**推断**：只要进入“非空session永久closed”条件，当前UI没有释放重建路径，需要外部调用Disconnect/Dispose或重启主端；这不是数据端/PLC重启要求。首次Connect过程中抛异常则VM148会Disconnect，仍有恢复入口，不能混在一起。**未知**：普通断网、路由器重启或native错误是否使会话永久closed，需运行版本的Zenoh日志/线程栈；本次没有故障注入。

<a id="t3-ui"></a>
#### T3.3 所有停止/急停入口与按钮门控

**已确认**：当前WPF命令枚举与IRobotLink没有本体急停、解除急停、清错或StopControl命令；介入面板也没有相应按钮（`A/MasterConsole.Protocol/ZenohFrames.cs:14–23`、`R/Services/IRobotLink.cs:56–92`、`A/VI.Module.Intervention/InterventionPanel.xaml:37–97`）。EstopHold只读展示（`R/ViewModels/MainViewModel.cs:181–192`），B的estop来自PLC通知；错误自动恢复并非主端“解除急停”按钮驱动（T1/T2）。

**证据 `A/MasterConsole.Protocol/ZenohFrames.cs:14–23`：**

```csharp
14:     public enum ZenohCommand : byte
15:     {
16:         Hello = 0, Authenticate = 1, Acquire = 2, Release = 3,
17:         PreparePosition = 4, StartControl = 5, ForceFeedback = 6,
18:         Cylinder = 7, ArmManualEnable = 8, ArmAxisEnable = 9,
19:         ArmAxisReset = 10, ArmAxisJog = 11, ArmCartesianJog = 12,
20:         ArmStop = 13, ArmProgramZero = 14,
21:         // 命令号15的旧定位臂参数接口已停用，其他编号不变。
22:         ArmCartesianAlive = 16, YValve = 17, ForceZero = 18, ForceHold = 19
23:     }
```

|界面操作|实际发送/处理|停止范围、解除方式|证据/确定程度|
|---|---|---|---|
|定位臂“停止”|依次ArmAxisJog(1..5,0)，再ArmStop；B清局部待办并发曲线取消/零方向|仅定位臂；新合法动作重新请求。不会直接清B01或本体estop|`A/VI.Module.PositioningArm/PositioningArmPanel.xaml:24`及`.xaml.cs:30–37,155`、`R/Services/RemoteRobotLink.cs:211–216`、`B/zenoh_service.cpp:905–915`、`B/arm_manual_ads_service.cpp:278–296`；已确认|
|定位臂总使能关闭|ArmManualEnable(false)，B记user_disabled并清使能/方向；PLC清请求|局部去使能；用户重新开，旧点动不自动恢复|`R/Services/RemoteRobotLink.cs:201–202`、`B/arm_manual_ads_service.cpp:147–157`、`P/ArmManual.TcPOU:35–42`；已确认|
|注射器/轴4鼠标抬起或LostMouseCapture|本地持续方向归0，下个100Hz HandleFrame携带0|仅该方向；重新按下；网络断流还有B500ms门控|`A/VI.Module.Intervention/InterventionPanel.xaml:84–95`及`.xaml.cs:38–49`、`R/ViewModels/MainViewModel.cs:345–357`、`R/Services/RemoteRobotLink.cs:373–374`；已确认|
|定位臂轴点动松开/捕获丢失|SetArmAxisJog(axis,0)|该定位臂轴停；重新按下|`A/VI.Module.PositioningArm/PositioningArmPanel.xaml:31–35`、`A/VI.Module.PositioningArm/PositioningArmPanel.xaml.cs:99–105`；已确认|
|定位臂末端点动松开/捕获丢失|StopArmAsync，清末端模式/速度|停止该定位臂动作；新请求重新进入|`A/VI.Module.PositioningArm/PositioningArmPanel.xaml.cs:136–142`；已确认|
|主窗口关闭/面板折叠|ApplicationCommands.Stop分派给面板StopHoldActions|持续动作归0；主端退出后另由B样本龄期门控|`A/VI.Control/MainWindow.xaml.cs:28–46`、`A/VI.Module.Intervention/InterventionPanel.xaml.cs:22–34`、`A/VI.Module.PositioningArm/PositioningArmPanel.xaml.cs:30–37`；已确认|
|主窗口失焦|没有Deactivated全局停止调用；VM的StopAllHoldActions只有定义|不能按该函数注释推定失焦自动全停|`A/VI.Control/MainWindow.xaml.cs:10–46`、`R/ViewModels/MainViewModel.cs:359–367`；已确认（当前源码搜索范围）|

**已确认**：对当前A业务 `.cs/.xaml`（排除bin/obj及旧C++程序）搜索“重启/restart”，未发现面向用户的重启提示。确实存在“控制权已失效：请断开后重新连接从端”（VM173）、“10秒内未收到通过认证的ADS状态帧…”（RemoteRobotLink174）和“手柄线程未在2秒内停止，不能再次启动设备”（HandleService81）；这些分别是重连建议/握手超时/停止异常，不是PLC重启要求。**未知**：第三方SDK弹窗、native库或现场不同版本的文案。

|灰掉/拒绝条件|相关控件与退出|证据与程度|
|---|---|---|
|CanOperate=IsConnected && HasControl && StatusFresh|介入动作区整体、定位臂使能/复位/归零/点动；有效状态及LeaseHeld恢复后自动恢复，M07例外见上|`R/ViewModels/MainViewModel.cs:114–119`、`A/VI.Module.Intervention/InterventionPanel.xaml:43`、`A/VI.Module.PositioningArm/PositioningArmPanel.xaml:6,23,31–35`；已确认|
|CanStartControl=CanOperate && !ControlActive|开始控制；active时灰。没有额外检查selfcheck、AdsHealthy或EstopHold|`R/ViewModels/MainViewModel.cs:194–202`、`A/VI.Module.Intervention/InterventionPanel.xaml:54`；已确认|
|_connecting|重复连接直接返回，finally清false；不是持久锁存|`R/ViewModels/MainViewModel.cs:125,127,151`；已确认|
|FfZeroing|力反馈开启/归零函数直接return，控件未因此永久禁用；待B归零结束|`R/ViewModels/MainViewModel.cs:311–322`；已确认|
|SDKmissing/手柄offline|是状态展示；不直接纳入CanOperate，可能UI仍可点而B保持/拒绝|`R/Services/RemoteRobotLink.cs:276–278`、`R/ViewModels/MainViewModel.cs:114–119`；结构已确认，体验推断|

**推断**：按钮可点而B按B01/estop/selfcheck/回退互斥拒绝时，PhaseText主要显示“待开始控制/待机”，没有细分这些原因（VM196–202）；用户可能重复操作而误以为必须重启。现有代码没有把“请求失败”写成永久按钮锁，但M07和SDK线程退出可让相关功能长期不可用。

<a id="t3-handle"></a>
#### T3.4 USB手柄：两柄同线程，打开重试与拔插重连须分开

**已确认**：HandleService槽0=SN582，槽1=SN587（`R/Services/HandleService.cs:15–16,48`），对应B默认导丝/导管映射（`B/main.cpp:50–51`）。同一个AboveNormal后台线程顺序处理两设备，每轮尾Sleep(5)，名义约200Hz，非UI定时器（HandleService66、140–162）。仅Id<0才每1000msTryOpen；成功后Id保留（149–155、176–190）。两柄用相同读写/重试逻辑，区别是序号、先后顺序、B业务映射；SDK伺服回调固定deviceStatus(0)（23–26），是否代表全设备状态需厂商说明。

**证据 `R/Services/HandleService.cs:147–161`：**

```csharp
147:                     foreach (var d in _dev)
148:                     {
149:                         if (d.Id < 0)
150:                         {
151:                             if (now >= d.NextOpenMs)
152:                             {
153:                                 d.NextOpenMs = now + OpenRetryMs;
154:                                 TryOpen(d);
155:                             }
156:                             continue;
157:                         }
158:                         if (!PollOne(d, now)) CloseDevice(d);
159:                         else ApplyForce(d, now);
160:                     }
161:                     Thread.Sleep(LoopPeriodMs);
```

**已确认**：PollOne四个SDK读取均为void PInvoke（`R/Services/FlCatheterNative.cs:42–52`），随后无条件Valid=true、return true（`R/Services/HandleService.cs:192–211`）。因此Run里的“Poll失败→CloseDevice”常规返回路径不可达；没有单独轮询设备在线状态。SDK读取如果迅速返回旧值，代码也会更新SampleMs并标有效；如果阻塞，GetSample在样本年龄>100ms时标无效（93–101）。**未知**：拔出后实际返回值/阻塞/异常，以及重新插入同设备是否在原Id下自行恢复；缺少SDK实现与拔插记录。

**证据 `R/Services/HandleService.cs:197–211`：**

```csharp
197:             FlCatheterNative.getEncVel(vel, d.Id);
198:             FlCatheterNative.getJoints(joints, d.Id);
199:             FlCatheterNative.getEncoders(enc, d.Id);
200:             FlCatheterNative.getSwitch(out byte buttons, d.Id);
201: 
202:             var s = new HandleSample
203:             {
204:                 Buttons = buttons,
205:                 Valid = true,
206:                 Encoder0 = enc[0], Encoder1 = enc[1],
207:                 Joint0 = (float)joints[0], Joint1 = (float)joints[1],
208:                 Vel0 = (float)vel[0], Vel1 = (float)vel[1],
209:             };
210:             lock (_lock) { d.Sample = s; d.SampleMs = now; }
211:             return true;
```
**证据 `R/Services/HandleService.cs:93–101`：**

```csharp
93:         public HandleSample GetSample(int slot)
94:         {
95:             var d = _dev[slot];
96:             lock (_lock)
97:             {
98:                 var s = d.Sample;
99:                 if (_stop || d.SampleMs < 0 || _clock.ElapsedMilliseconds - d.SampleMs > SampleMaxAgeMs) s.Valid = false;
100:                 return s;
101:             }
```

**已确认**：另一个独立后台线程“Zenoh-Handle-100Hz”按10ms单调时钟推进，超拍跳过积压（`R/Services/RemoteRobotLink.cs:20,160–161,337–348`）。在认证state仍鲜时，它持续发含两柄valid字段的同一HandleFrame；某柄无样本/过100ms仍发valid=false，不是停发该柄通道（364–377）。整帧停发只在state/会话门控失效等条件（357）。B对valid=false立即失败，不等3s（T4）。

**已确认**：DLL不存在、位数不符、入口缺失三类异常设置_sdkMissing并退出采样线程（HandleService164–173）；当前唯一业务Start在 `A/VI.Control/App.xaml.cs:53–63` 应用启动时，连接/断开不重启HandleService。类本身有Start API，但当前UI无入口；修复DLL后重建主端进程才能从当前业务入口重新启动采样。其它native异常后果未见通用恢复处理，不能推断所有异常都被安全吞掉。

**证据 `R/Services/HandleService.cs:164–173`：**

```csharp
164:             catch (DllNotFoundException) { _sdkMissing = true; }
165:             catch (BadImageFormatException) { _sdkMissing = true; }
166:             catch (EntryPointNotFoundException) { _sdkMissing = true; }
167:             finally
168:             {
169:                 foreach (var d in _dev)
170:                 {
171:                     try { if (d.Id >= 0) { ZeroForce(d); CloseDevice(d); } } catch { }
172:                 }
173:             }
```

**已确认**：力回包有session/序号/新鲜度门控（`R/Services/RemoteRobotLink.cs:464–481`）；HandleService要求收到的触觉龄期<3000ms才输出，过期清零并每100ms重复发零（HandleService214–244；常量见ProtocolConstants9–11）。state失鲜2s会先ClearHaptic。**推断**：native sendForce在HandleService._lock内执行（216–235），GetSample也拿该锁（93–101）；网络线程持RemoteRobotLink._lock取GetSample（350–358、249–259），state回调也拿RemoteRobotLink._lock（401）。若sendForce卡住，可能连带阻塞采样发送、state接收处理和UI getter；SDK四个读本身在锁外，阻塞它们的影响路径有所不同。源码没有取消native调用的机制，是否出现该阻塞仍未知。

**证据 `R/Services/HandleService.cs:214–226`：**

```csharp
214:         private void ApplyForce(Device d, long now)
215:         {
216:             lock (_lock)
217:             {
218:                 // 清零与原生写力共用锁，避免清零返回后又写入旧采样。
219:                 bool fresh = !_stop && IsHapticFresh(now, d.HapMs);
220:                 if (d.HapEnable && fresh)
221:                 {
222:                     var vec = new double[3];
223:                     vec[d.HapAxis] = d.HapForce;
224:                     FlCatheterNative.sendForce(vec, d.HapTorque, d.Id);
225:                     d.ForceNonZero = true;
226:                     return;
```

#### T3.5 主端重启/崩溃后的分层状态

|保留下来的进程/阶段|主端消失后|重新连接后|证据与程度|
|---|---|---|---|
|B和PLC仍正常，原样本valid且不再到达|桥龄期>3s后Handle失败、B按锁定模式保持；B ADS心跳仍可继续，PLC不一定超时|有效采样后当前位置rebase；原control_active/稳定模式通常保留，不必再Start|`B/手柄/remote_handle_bridge.h:69–78`、`B/main.cpp:1197–1301,1305–1327`、T5.3；已确认条件路径|
|B收到主端valid=false|立刻Handle失败；B同样保持|恢复有效采样后重建|同上；已确认|
|PLC回退已实际开始|B保留本地任务；Handle保持期间main不推进常规构帧分支，但PLC独立FB不因Zenoh失联自动撤销|恢复后确认原任务；也可能已结束/超时，需要观察实际FB状态|`B/main.cpp:1310–1316,1919–1920,3449–3454`、`P/handle.TcPOU:568–627`；代码已确认，现场进展未知|
|B已有return_ads_fault_hold|一直停普通控制|新认证、Acquire、Start也不清锁；需重建B进程|T1.1；已确认|
|B也被重启，PLC未重启|新ADS session触发PLC恢复握手；B初次同步到WaitForEnter|PLC自检资格仍真且健康时可当前位置Start；不必仅因B重启再寻参|`P/MAIN.TcPOU:63–89`、`P/handle.TcPOU:100–124,416–421`、`B/main.cpp:1330–1345,5072–5119`；已确认条件路径|
|PLC也被重启/FirstCycle|self_check_done强制假|当前实现必经新的SelfCheck；“驱动器没断电”尚无复用分支|T2.5/T7；已确认|

**已确认**：主端崩溃不会执行优雅Shutdown；B租约并无断流TTL，3s远程样本龄期不是即时总急停。新主端认证成功会撤旧B租约和软件待办（T4），已执行的PLC任务是否停止须另查其状态机，不能用“主端重连成功”代替任务/坐标/自检资格确认。

<a id="t4-lease"></a>
### T4. 数据端Zenoh：鉴权、断流、采样间隔与状态发布

#### T4.1 “租约”的真实寿命与校验位置

**已确认**：B消费Acquire后置 `lease_held_=true`；worker明确“断流由手柄采样龄期处理，不撤销已认证会话的控制权”（`B/zenoh_service.cpp:769–780,1003`）。`last_handle_rx_ms_`没有租约到期判断，只用于接收记录及持续点动续推（636–679、983–1016）。真正撤租发生在显式Release、成功新认证或服务停止；断流多久“判失联”不是一个统一TTL，而是下表的多层期限。

**证据 `B/zenoh_service.cpp:769–780`：**

```cpp
769:     if (type == vi_protocol::Command::Acquire)
770:     {
771:         lease_held_ = true;
772:         last_handle_rx_ms_ = GetTickCount64();
773:         add_result_locked(frame.command_id, 0, "");
774:         std::cout << "Zenoh: remote control lease acquired." << std::endl;
775:         return;
776:     }
777:     if (type == vi_protocol::Command::Release)
778:     {
779:         release_lease_locked("Control lease was released.");
780:         add_result_locked(frame.command_id, 0, "");
```
**证据 `B/zenoh_service.cpp:1000–1008`：**

```cpp
1000:         const auto expired = expired_commands_.exchange(0);
1001:         if (expired != 0)
1002:             std::cout << "Zenoh: dropped " << expired << " commands older than 2 seconds in the ADS queue." << std::endl;
1003:         // 断流由手柄采样龄期处理，不撤销已认证会话的控制权；新会话或显式释放仍撤销。
1004:         const auto publish_tick = Clock::now();
1005:         if (publish_tick >= next_state)
1006:         {
1007:             publish_state_frame(); next_state += state_period;
1008:             if (next_state <= publish_tick) next_state = publish_tick + state_period;
```

|层次|门槛|结果|证据/程度|
|---|---|---|---|
|主端自有USB样本|年龄>100ms|仍可发送整帧，但该柄valid=false|`R/Services/HandleService.cs:99`；已确认|
|B桥有效位|valid=false|Handle::poll立即失败，不等3s|`B/手柄/remote_handle_bridge.h:75–76`、`B/手柄/手柄.cpp:45–49`；已确认|
|B本地接收龄期|>3000ms|桥返回失败，B关闭该逻辑柄、每1s重试；按锁定单双柄保持|同上、`B/main.cpp:1093–1111,1141–1146,1197–1199`；已确认|
|B本地样本龄期/消费间隔|龄期>250ms速度置0；前后消费rx差>250ms请求rebase|当前位置重建，不等于USB或ADS已断|`B/手柄/手柄.cpp:50–55`；已确认|
|Zenoh会话/租约|无断流TTL|未再收到Handle不自动Release；新认证/显式Release撤旧租约|`B/zenoh_service.cpp:720,769–780,960–981,1003`；已确认|
|PLC主机看门狗|ADS heartbeat不变2s|PLC锁存host保持；不是直接监测A端或Zenoh|`P/MAIN.TcPOU:63–89`、T5.3；已确认|

**已确认**：Hello只生成挑战，不撤旧会话；Authenticate要求challenge/session、双nonce、sequence、32字节proof、空auth_tag及10s内有效，HMAC比较成功才换session/key（`B/zenoh_service.cpp:686–731`）。成功新认证先release_lease，清命令队列/结果/软件prepare/start任务；普通包按协议结构、session、HMAC与严格增序验证。没有逐包检查IP或Zenoh publisher身份/来源metadata；subscriber默认设置（348–356）。这说明当前校验依赖共享密钥与会话，并非“来自某台IP就信任”。

**证据 `B/zenoh_service.cpp:705–714`：**

```cpp
705:         if (frame.session_id == 0 || frame.session_id != challenge_session_id_ ||
706:             frame.nonce_client != challenge_client_nonce_ || frame.nonce_server != challenge_server_nonce_ ||
707:             frame.proof.size() != 32 || !frame.auth_tag.empty() || frame.sequence <= challenge_sequence_ ||
708:             GetTickCount64() - challenge_tick_ms_ > 10000) return;
709:         std::vector<std::uint8_t> message = bytes_of("auth");
710:         message.insert(message.end(), challenge_client_nonce_.begin(), challenge_client_nonce_.end());
711:         message.insert(message.end(), challenge_server_nonce_.begin(), challenge_server_nonce_.end());
712:         std::uint8_t expected[32]{};
713:         if (!hmac_sha256(token_, message.data(), message.size(), expected) ||
714:             !constant_time_equal(expected, frame.proof.data(), 32)) return;
```
**证据 `B/zenoh_service.cpp:636–644`：**

```cpp
636: void ZenohService::on_handle(const vi_protocol::HandleFrame& frame)
637: {
638:     std::lock_guard<std::mutex> lock(state_mutex_);
639:     if (!authenticated_ || !lease_held_ || frame.session_id != session_id_ || !verify_tag(frame)) return;
640:     if (have_handle_sequence_ && frame.sequence <= last_handle_sequence_) return;
641:     last_handle_sequence_ = frame.sequence;
642:     have_handle_sequence_ = true;
643:     echo_main_monotonic_ms_ = frame.monotonic_ms;
644:     last_handle_rx_ms_ = GetTickCount64();
```
**证据 `B/zenoh_service.cpp:736–740`：**

```cpp
736:     if (!authenticated_ || frame.session_id != session_id_ ||
737:         (have_command_sequence_ && frame.sequence <= last_command_sequence_) || !verify_tag(frame)) return;
738:     last_command_sequence_ = frame.sequence;
739:     have_command_sequence_ = true;
740:     echo_main_monotonic_ms_ = frame.monotonic_ms;
```

|校验/拒绝点|具体条件|结果/解除|证据与程度|
|---|---|---|---|
|协议解码|字段类型/数目/范围、整buffer、≤65536字节、无尾随|丢该包，合法包可继续；native入口先允许≤1MiB分配|`B/protocol/cpp/zenoh_validation.h:37–65,106–117`、`B/zenoh_service.cpp:25,549–569`；已确认|
|Handle验证|authenticated、lease、相同session、tag正确、sequence递增|不满足直接return；不设永久fault|`B/zenoh_service.cpp:624–644`；已确认|
|Command验证|authenticated、相同session、tag正确、sequence递增|同上；业务消费再查lease|`B/zenoh_service.cpp:736–740,783–784`；已确认|
|FIFO容量|最多256个，满丢最旧；部分连续jog/alive不回单独失败|命令失败或丢弃；需新请求|`B/zenoh_service.cpp:25–30,742–754`；已确认|
|消费时二次验证|session/auth或lease_epoch已变；Acquire例外只免epoch判定|拒该命令，不补发|`B/zenoh_service.cpp:492–505`；已确认|
|排队龄期|`GetTickCount64()-queued_ms > 2000`|Command expired；新命令可继续|`B/zenoh_service.cpp:501–504`；已确认|
|B业务状态失鲜|state年龄>500ms，Release例外|拒命令ADS state is stale；main刷新恢复|`B/zenoh_service.cpp:765–767`；已确认|
|pending容量/防重|pending≥8、同类Prepare/Start/Force/Cylinder等已在进行|拒重复；任务终态/超时/撤租后可再请求|`B/zenoh_service.cpp:785–790,816–818,840–842,857–859,882–895,946–948`；已确认|
|任务deadline|Prepare=300000ms；Start/Force/Cylinder/YValve等通常3000ms|失败态，可新任务；检查在main publish_state中|`B/zenoh_service.cpp:376–456,836,848,869–875,885,913–914,951–952`；已确认|

**已确认**：通过会话校验后，各业务仍有额外门控，不是持租约即可执行所有命令：

|命令|具体拒绝/等候条件|条件消失后的出口|出处（B/zenoh_service.cpp）|程度|
|---|---|---|---|---|
|PreparePosition|导管不在5..95、wire不在10..639、导管>wire；ADS非运行、已经selfcheck_done或status非1/3；推导的axis6目标>660|输入合法且PLC在待自检状态后重新请求；已自检时该命令本来不用于重复启动旧startup|820–829|已确认|
|StartControl|ADS非运行、PLC estop/host握手、自检未完、B01、手柄hold、初同步未完、ads_hold；同类pending拒绝|清原条件后重新发Start；control已active则直接成功；B01没有现有解除入口|799–809、839–848|已确认|
|ForceFeedback/ForceZero|布尔/通道参数非法；同类力任务pending；未归零首次开启先排Zero|参数正确/待任务完成后新请求；归零结果由计数确认，不是仅收到命令即成功|853–876、376–456|已确认|
|ForceHold|布尔非法、同类pending|合法新请求；此命令清两路旧锁力|880–886；对应B/main.cpp:4754–4756|已确认|
|Cylinder|编号/布尔非法、同电缸已有pending、开启时manual_allowed假|PLC/自动流程占有解除后重新请求；关闭是ResetCylinderManual|890–914|已确认|
|ArmManualEnable/AxisEnable/AxisReset/AxisJog|布尔、轴号、方向/倍率非法；其它就绪/owned门交给Arm服务/PLC|合法请求并满足局部门；回执不等于运动已发生|917–935；T1.8/T2.6|已确认|
|ArmStop/ProgramZero/Alive|仍受通用会话、状态新鲜度与lease门；ArmStop排StopCartesian及五轴方向0|通用门恢复后新请求，持续动作不补发|765–790、936–943|已确认|
|YValve|布尔非法/同类pending；状态已达目标则直接成功|新合法请求|944–952|已确认|
|未知command|Unsupported command|改为协议支持的命令；无持久fault|954–955|已确认|

**证据 `B/zenoh_service.cpp:799–809`：**

```cpp
799:     const auto control_unavailable_reason = [&]() -> std::string
800:     {
801:         if (state_.ads_state != 2) return u8"ADS 尚未处于运行态";
802:         if (state_.estop_hold) return u8"PLC 保持请求尚未解除，请查看数据端 PLC 故障提示";
803:         if (state_.host_comm_timeout) return u8"PLC 通信恢复握手尚未完成";
804:         if (!state_.self_check_done) return u8"PLC 器械准备尚未完成";
805:         if (extra_.return_ads_fault_hold) return u8"回退任务故障尚未解除，请查看数据端回退错误";
806:         if (extra_.handle_soft_hold) return u8"手柄有效采样尚未就绪";
807:         if (!extra_.initial_sync_done) return u8"初始坐标同步尚未完成";
808:         if (extra_.ads_soft_hold) return u8"ADS 位置快照或恢复重同步尚未就绪";
809:         return {};
```

**推断**：main若卡住，pending超时判断也推迟，3s并非worker独立执行的硬期限。源 `monotonic_ms` 只回显，没有用于拒绝同会话迟到Handle（`B/zenoh_service.cpp:643–644,739–740`）；桥以本地到达时刻刷新rx。若旧帧积压后按序到达，可能被当作新鲜；有session/序号保护不等于做过源样本年龄校验。

#### T4.2 撤租后究竟取消什么

**已确认**：release_lease清桥的采样/力输出，把pending Prepare/Start标失败，清pending/control_actions；给两个注射器和轴4安排方向0（`B/zenoh_service.cpp:960–981`）。这里没有发送本体回退取消；B连接保持也明确保留planned_return（`B/main.cpp:1310–1316`）。**推断**：主端新认证时，UI任务可以失败而已下发的PLC运动仍运行；需区分“待办结果”与“物理动作”。后者只由本体状态机、错误或host停止等路径决定。

**证据 `B/zenoh_service.cpp:960–970`：**

```cpp
960: void ZenohService::release_lease_locked(const std::string& reason)
961: {
962:     lease_held_ = false; ++lease_epoch_; force_zeroing_ = false;
963:     remote_handle_bridge::clear_samples(); remote_handle_bridge::clear_outputs();
964:     for (const auto& task : pending_tasks_)
965:     {
966:         if (task.kind == PendingKind::Prepare) prepare_task_state_ = 3;
967:         if (task.kind == PendingKind::StartControl) start_task_state_ = 3;
968:         add_result_locked(task.command_id, 1, reason);
969:     }
970:     pending_tasks_.clear();
```
**证据 `B/zenoh_service.cpp:971–981`：**

```cpp
971:     std::lock_guard<std::mutex> queue_lock(command_mutex_);
972:     control_actions_.clear();
973:     for (int i = 0; i < 3; ++i)
974:     {
975:         VisCommand command;
976:         if (i < 2) { command.type = VisCommandType::SetInjectorManualJog; command.param1 = i + 1; command.param2 = 0; }
977:         else { command.type = VisCommandType::SetAxis4ManualJog; command.param1 = 0; }
978:         hold_actions_[i] = ControlAction{ command, 0, false, session_id_, lease_epoch_ };
979:         hold_dirty_[i] = true; hold_direction_[i] = 0;
980:     }
981: }
```

#### T4.3 两柄采样链与250ms重建文案

**已确认**：两柄走同一 `vi/ads/handle/v1` topic，同一HandleFrame含582/587及持续方向；B回调顺序写两个槽（`B/protocol/cpp/zenoh_protocol.h:11,45–61`、`B/zenoh_service.cpp:646–679`）。bridge没有FIFO，只保留最近一份，put_sample按GetTickCount64设rx，get_sample拒invalid或龄期>3000ms（`B/手柄/remote_handle_bridge.h:59–78`）。单柄valid假可单独失效；共同传输延迟不证明USB状态相同。

**证据 `B/手柄/remote_handle_bridge.h:59–66`：**

```cpp
59: 	inline void put_sample(int slot, Sample s)
60: 	{
61: 		if (slot < 0 || slot > 1) return;
62: 		s.rx_ms = GetTickCount64();
63: 		auto& st = detail::state();
64: 		std::lock_guard<std::mutex> lock(st.m);
65: 		st.samples[slot] = s;
66: 	}
```
**证据 `B/手柄/remote_handle_bridge.h:69–78`：**

```cpp
69: 	inline bool get_sample(int slot, Sample& out, ULONGLONG max_age_ms)
70: 	{
71: 		if (slot < 0 || slot > 1) return false;
72: 		auto& st = detail::state();
73: 		std::lock_guard<std::mutex> lock(st.m);
74: 		const Sample& s = st.samples[slot];
75: 		if (!s.valid) return false;
76: 		if (GetTickCount64() - s.rx_ms > max_age_ms) return false;
77: 		out = s;
78: 		return true;
```

**已确认**：Handle::poll比较的是“本次消费到的最新接收时间戳”和“上次消费的接收时间戳”。主循环停顿>250ms，即使回调持续更新桥，也可能跨过门槛；不是只统计网络相邻两包的间隔。take_rebase_request取走即清（`B/手柄/Handle.h:29`），main把它与ads_soft_hold、普通连接恢复共用基准重建分支，统一打印“ADS快照已恢复”（`B/main.cpp:1201–1203,1265–1301`）。只看到这条文案，不能证实此前有ADS故障。

**证据 `B/手柄/手柄.cpp:45–55`：**

```cpp
45: 	remote_handle_bridge::Sample sample;
46: 	if (!remote_handle_bridge::get_sample(remote_handle_bridge::slot_of(serial_number_), sample, kRemoteMaxAgeMs))
47: 	{
48: 		return false;
49: 	}
50: 	if (last_sample_rx_ms_ != 0 && sample.rx_ms > last_sample_rx_ms_ &&
51: 		sample.rx_ms - last_sample_rx_ms_ > 250) rebase_requested_ = true;
52: 	last_sample_rx_ms_ = sample.rx_ms;
53: 	for (int i = 0; i < 2; ++i)
54: 	{
55: 		fVels2[i] = GetTickCount64() - sample.rx_ms > 250 ? 0.0 : static_cast<double>(sample.vels[i]);
```
**证据 `B/main.cpp:1265–1268`：**

```cpp
1265: 		else if ((ads_soft_hold_active || handle_rebase_requested || (connection_hold_active_prev && initial_sync_done)) &&
1266: 			!planned_return.active())
1267: 		{
1268: 			if (!handle_soft_hold_active)
```
**证据 `B/main.cpp:1296–1301`：**

```cpp
1296: 			ads_soft_hold_active = !baseline_rebuilt;
1297: 			if (baseline_rebuilt)
1298: 			{
1299: 				axis1_fast_return = false;
1300: 				axis6_fast_retract = false;
1301: 				std::cout << "ADS 快照已恢复：实际位置、手柄基准与当前稳定模式已重建。" << std::endl;
```

**推断**：T1的sync_all(30)自身就含约300ms采样等待，主循环其它同步等待也可能形成消费rx差值；需要主循环、网络接收、poll三套时间戳才能区分。不是每次取样都要求新序号，因此平均采样等待也不证明期间有30个独立物理样本（`B/control_types.cpp:70–88`）。

#### T4.4 QoS、队列与回调阻塞：存在路径，未定位现场原因

|已确认配置/同步点|可能影响（推断）|证据|
|---|---|---|
|B单个TCP client session，默认tcp/192.168.60.1:7447|网络拥塞/路由器排队共同影响两柄；实际延迟未知|`B/zenoh_service.h:19`、`B/zenoh_service.cpp:324–332`|
|B state/force默认publisher options；包内SDK默认congestion Drop、priority Data|拥塞允许丢弃，未单独设实时优先/express；不能据此证明发生250ms长间隔|`B/zenoh_service.cpp:343–346`、`SDK/zenoh_commons.h:53–62,89–94`、`B/ADS.vcxproj:73–75`|
|A publisher明确Drop+express，native Publish持_publishLock|仍是同步native调用；若该调用不返回，可拖住Handle发送|`A/VI.Service.Net/ZenohSession.cs:63–77,120–133`|
|B state_mutex为Handle/Command回调、main更新/消费、发布帧制作共用，HMAC在锁中|锁等待/大包处理可能推迟回调或main；未测竞争持续时间|`B/zenoh_service.cpp:378,464,638,685,1025,1113`|
|B回调只解码/验证/桥与队列操作，没有直接同步ADS I/O|ADS慢调用不直接占回调函数，但能暂停main消费或状态更新|`B/zenoh_service.cpp:572–585,636–756`|
|A写力native与GetSample共享锁，网络线程又持link锁取样|SDK卡住可能连带发送、state回调、UI；详T3.4|`R/Services/HandleService.cs:93–101,216–244`、`R/Services/RemoteRobotLink.cs:249–259,350–358,401`|

**已确认**：SDK头文件对reliability说明“不会因此在链路上重传”（`SDK/zenoh_commons.h:105–120`）；不能把应用层Reliable/BestEffort选项直接当TCP重传策略。**未知**：当前native SDK的callback线程调度、路由器/TCP缓冲、实际丢包量与最长锁等待；没有同步抓包或线程跟踪，不能把Zenoh/QoS认定为现场已证实的主要原因。

#### T4.5 状态/力发布频率及失败处理

**已确认**：main按30Hz计划提供VisState（`B/main.cpp:55,4457–4467,4621`）；worker state周期33ms（约30.3Hz）、force周期10ms（100Hz），超拍跳过积压，循环Sleep(1)（`B/zenoh_service.cpp:991–1015`）。state没有数据或main提供的状态年龄>500ms时停发（1025–1026）；force不做同一个500ms门槛（1113–1137）。这两个频率是计划值，非现场测得保证值。

**已确认**：编码/HMAC/分配失败直接return；state最终 `z_publisher_put` 返回值未检查，force相同（1095–1105、1129–1137）。没有该失败的重试、计数、日志或native session重建分支；SDK签名规定put返回0成功、负数失败（`SDK/zenoh_commons.h:4993–5008`）。**推断**：持续发布失败可能最终表现为A的状态过期，而B本地没有对应失败原因；是否发生需增加外部观测，现有日志无法证明。

**证据 `B/zenoh_service.cpp:1095–1105`：**

```cpp
1095:     if (authed)
1096:     {
1097:         auto clean = frame; clean.auth_tag.clear(); const auto data = pack(clean); std::uint8_t mac[32]{};
1098:         if (!hmac_sha256(key, data.data(), data.size(), mac)) return;
1099:         frame.auth_tag.assign(mac, mac + 16);
1100:     }
1101:     auto bytes = pack(frame);
1102:     z_owned_bytes_t payload{};
1103:     if (z_bytes_copy_from_buf(&payload, bytes.data(), bytes.size()) < 0) return;
1104:     z_publisher_put_options_t options{}; z_publisher_put_options_default(&options);
1105:     z_publisher_put(z_loan(state_publisher_), z_move(payload), &options);
```

<a id="t5-ads"></a>
### T5. ADS 链路、阻塞预算和重连状态

#### T5.1 CADSComm 超时和按名访问：有成功句柄缓存

**已确认**：构造默认20ms（`B/ADSComm.cpp:10,101–105`），但生产通信服务连接前 `SetTimeout(1000)`、初始化完成后 `SetTimeout(300)`（`B/ads_communication.cpp:1069–1119`）。SetTimeout在端口已开时调用 `AdsSyncSetTimeoutEx`，成功才更新成员（`B/ADSComm.cpp:765–786`）；OpenComm内部也将当前成员应用于新端口（841–844、894–897）。不能用构造默认20ms估算运行阻塞。

**证据 `B/ads_communication.cpp:1069–1078`：**

```cpp
1069: bool AdsCommunicationService::ensure_connection(bool reconnecting)
1070: {
1071: 	// 服务只被动建立路由，禁止在后台把人工 STOP/故障 STOP 自动切回 RUN。
1072: 	// 握手连接阶段使用 1000 ms 宽容超时
1073: 	if (!ads_.SetTimeout(1000))
1074: 	{
1075: 		record_failure("SET_TIMEOUT", ads_.GetLastErrorCopy());
1076: 		return false;
1077: 	}
1078: 	if (!ads_.IsCommOpen() && !ads_.OpenCommInsideReadOnly())
```
**证据 `B/ads_communication.cpp:1113–1122`：**

```cpp
1113: 	// 运行期允许单次 ADS 调用等待 300 ms，短时调度抖动不直接升级为断线。
1114: 	if (!ads_.SetTimeout(300))
1115: 	{
1116: 		record_failure("SET_TIMEOUT", ads_.GetLastErrorCopy());
1117: 		clear_runtime_connection_state();
1118: 		ads_.CloseComm();
1119: 		return false;
1120: 	}
1121: 	std::cout << "[ADS-DIAG] connected " << ads_.ConnectionDescription() << std::endl;
1122: 	return true;
```

**已确认**：按名 `ADSReadSum/ADSWriteSum` 每次都会调用ADSGetAddr，但ADSGetAddr先查`m_symbolHandles`；命中直接返回，不产生HNDBYNAME网络请求（`B/ADSComm.cpp:291–305,452–466,623–642`）。成功解析入缓存，CloseComm才清并逐个释放（654–656、964–982）；未找到/异常句柄不缓存，也不会在同一调用内循环重试（643–651）。一个符号缺失使该Sum在解析阶段立即失败，不执行最终Sum；日志保留符号名（645）。命中缓存但PLC符号已失效时，此函数也不会自行删缓存重解析，需断线重建句柄路径。

**证据 `B/ADSComm.cpp:623–637`：**

```cpp
623: 	const auto existing = m_symbolHandles.find(key);
624: 	if (existing != m_symbolHandles.end())
625: 	{
626: 		ClearLastErrorLocked();
627: 		return existing->second;
628: 	}
629: 
630: 	unsigned long handle = 0;
631: 	unsigned long cbReturn = 0;
632: 	const unsigned long nameLength = static_cast<unsigned long>(key.size() + 1U);
633: 	const long nErr = AdsSyncReadWriteReqEx2(
634: 		m_adsPort,
635: 		m_PAmsAddr,
636: 		ADSIGRP_SYM_HNDBYNAME,
637: 		0,
```

|调用情况|ADS往返数|运行300ms配置下的逐调用预算（推断，不是OS硬实时保证）|
|---|---|---|
|按名单读/单写，句柄已缓存|1|约≤300ms加调度/锁等待|
|按名单读/单写，首次成功解析|2|2×300ms；连接阶段用1000ms|
|n项Sum，u个不同未缓存符号均成功|u+1|`(u+1)×300ms`；全缓存只有一次Sum|
|第k个未缓存符号解析失败|到失败点已做的解析次数，无最终Sum|失败响应可很快；丢包/无响应可吃满该次300ms；后续调用还会再次解析失败符号|
|直接ByHandle的快照/输出|1次Sum|约≤300ms加调度/锁等待；完整快拍至少读和写两次|

证据：单调用转发`B/ADSComm.cpp:180–200`；Sum解析291–305/452–466；最终单次 `AdsSyncReadWriteReqEx2` 在376–385、540–554。**未知**：现场典型RTT、ADS路由排队与厂商DLL额外阻塞；未做真实ADS请求。服务配置20项通知（`B/ads_communication.cpp:1941–1960`），清理路径还逐个注销注册项并释放已缓存符号（`B/ADSComm.cpp:953–980`），故“检测300ms硬超时”不保证300ms内已经完成断线或下一次连接。

#### T5.2 单拍SoftHold、300ms累计失败和队列失败

**已确认**：100Hz服务并非只在硬超时才失败业务队列。以下路径都调用fail_queued：连接尚未初始化（888–891）、PLC状态非RUN（919–925）、读设备状态失败（942–946）、快拍未正式进入完整Running（1043–1050）、线程停止（1063–1065）；运行源码为`B/ads_communication.cpp`。fail_queued回退逐项结果Failed（1915–1928）；低频只标尚未开始请求失败（2166–2181）。不是把已经在PLC执行的运动通过队列标失败就直接停住。

**证据 `B/ads_communication.cpp:1033–1045`：**

```cpp
1033: 		// 只有完整读写成功且本拍正式进入 Running 才发布可用于运动控制的测量；
1034: 		// 力数据的首尾 PLC 周期跨度只保留作诊断，不参与有效性和运动保持判定。
1035: 		if (!process_low_frequency)
1036: 		{
1037: 			snapshot.position_valid = false;
1038: 			snapshot.force_valid = false;
1039: 			snapshot.valid = false;
1040: 		}
1041: 		// 先提交状态再唤醒快照消费者，避免消费者看到“新快照 + 旧 Running 状态”。
1042: 		publish_snapshot(snapshot);
1043: 		if (process_low_frequency)
1044: 		{
1045: 			if (!planned_return_processed) process_one_low_frequency_request();
```
**证据 `B/ads_communication.cpp:1047–1059`：**

```cpp
1047: 		else
1048: 		{
1049: 			fail_queued_planned_return_commands();
1050: 			fail_queued_low_frequency_requests();
1051: 		}
1052: 		if (disconnect)
1053: 		{
1054: 			clear_runtime_connection_state();
1055: 			ads_.CloseComm();
1056: 			connection_initialized = false;
1057: 			handles_fresh = false;
1058: 			reconnect_backoff_index = 0;
1059: 			next_reconnect_attempt_qpc = snapshot.qpc_ticks;
```

**已确认**：设备状态/快读/核心快写任一失败，且距最后完整成功快照不足300ms，状态SoftHold；到300ms或PLC重启待重连则清运行状态、CloseComm再连（951–966、988–1059）。本拍快照位置与力均置无效（1035–1039），B主循环进入保持。下一完整成功可回Running；不主动将人工STOP的PLC切RUN（1071）。重新连接失败每1000ms尝试（883–885），不是PLC或进程自动重启。

**已确认**：核心写定义为常规输出+输出携带清Req+会话/心跳；专用回退字段的子项错误可独立失败其命令而核心保持健康（1789–1843）。transport失败或核心子项失败则核心快拍失败。被Clear屏障覆盖而尚未开始的Prepare/Commit也直接Failed（1620–1651），属于主动取消语义，不应全统计为网络故障。

**推断**：SoftHold让排队回退结果失败，可被业务RetryClear或B01升级为停控；是否升级取决该腿是否已开始/possibly_started及重试次数（T1）。300ms只是循环执行到判定点后的失败龄期判断；线程阻塞于某次ADS/清理/低频调用期间，没有另一个线程在300ms强行打断它。快照时间采用读前后中点（1498–1501），也不是独立硬看门狗。

#### T5.3 session、心跳与main卡顿

**已确认**：host_session_id在AdsCommunicationService构造中由QPC与PID生成非零32位值（158–160）；main构造该服务（`B/main.cpp:76`），之后不随Zenoh会话改变。heartbeat在ADS工作线程`write_output_cycle`每次递增（1654），与session放同一次SumWrite（1792–1793）。main卡住但ADS线程仍正常读写时，心跳仍能递增、继续使用最后desired_output（1595–1601）；没有desired_output年龄门槛。PLC看门狗观测的是ADS主机线程健康，不是main推进或主端存活。

**证据 `B/ads_communication.cpp:1652–1655`：**

```cpp
1652: 	}
1653: 
1654: 	++heartbeat_sequence_;
1655: 	// 常规输出最多16项，双腿Prepare最多再增加12项。
```
**证据 `B/ads_communication.cpp:1789–1798`：**

```cpp
1789: 	// 心跳与输出放在同一Sum事务；这里的排列只用于后续逐项结果分类，
1790: 	// 不把SUMUP_WRITE子项顺序误认为PLC扫描级原子提交保证。
1791: 	const std::size_t heartbeat_fields_begin = count;
1792: 	append(fast_write_handles_[kWriteHostSession], sizeof(host_session_id_), &host_session_id_);
1793: 	append(fast_write_handles_[kWriteHeartbeat], sizeof(heartbeat_sequence_), &heartbeat_sequence_);
1794: 	const bool recover = watchdog_recovery_pending_.exchange(false, std::memory_order_acq_rel);
1795: 	if (recover) append(fast_write_handles_[kWriteHostRecover], sizeof(recover), &recover);
1796: 	bool transport_succeeded = false;
1797: 	const bool write_attempted = fast_handles_valid_;
1798: 	const bool all_items_succeeded = write_attempted && ads_.ADSWriteSumByHandle(
```

**推断**：main阻塞时不能据PLC心跳仍正常判断业务控制循环也正常；反之，定位臂慢请求占住同一个ADS线程会同时推迟快照和心跳。PLC运行到handle时，2s心跳不变才走host冻结；已开始SelfCheck不检查该条件，见T2。

<a id="t5-arm-time"></a>
#### T5.4 定位臂50ms低频请求的典型结构与最坏预算

**已确认**：Arm服务独立工作线程，读周期50ms、调用方等待参数250ms（`B/arm_manual_ads_service.cpp:13–17,505–550`），但请求在100Hz线程成功发布快照后同步执行；每个快拍最多处理一个低频请求，回退专用命令优先（`B/ads_communication.cpp:1042–1045,2077–2163`）。250ms只限制尚未开始的排队等待；开始后为保护缓冲区必须等待完成（554–624）。因此不能把“独立低频线程”理解成与快照ADS完全隔离。

**证据 `B/ads_communication.cpp:622–624`：**

```cpp
622: 	// 超时前已经开始的 ADS 请求必须等待完成，避免调用方缓冲区提前失效。
623: 	request->cv.wait(request_lock, [&]() { return request->completed; });
624: 	return request->success;
```
**证据 `B/ads_communication.cpp:2118–2132`：**

```cpp
2118: 			case LowFrequencyOperation::ReadSum:
2119: 			{
2120: 				std::vector<const char*> symbols(request->symbols.size());
2121: 				std::vector<void*> outputs(request->buffers.size());
2122: 				for (std::size_t i = 0; i < request->symbols.size(); ++i)
2123: 				{
2124: 					symbols[i] = request->symbols[i].c_str();
2125: 					outputs[i] = request->buffers[i].data();
2126: 				}
2127: 				success = ads_.ADSReadSum(
2128: 					symbols.data(),
2129: 					request->lengths.data(),
2130: 					outputs.data(),
2131: 					static_cast<unsigned long>(symbols.size()));
2132: 				break;
```

**已确认**：read_snapshot固定25项（673–697），五轴每轴10项Power/Reset输出（698–710），共75个符号；每轮一次ReadSum（712–713）。dirty写只写脏字段，最多手动1+使能1+复位1+参数4+方向2+cjog4＝13项（616–637），不是每50ms固定75次读加75次写。末端点动活动时cjog每50ms再次dirty（517）；程序回零还会额外同步写曲线心跳、曲线数据/取消及提交序号（299–400）。

|场景|调用成本估算|对快照/心跳的含义|
|---|---|---|
|热缓存、闲置读状态|一次ReadSum，执行约1个ADS RTT；排队通常等下个可用10ms快拍|典型毫秒值未知，需实测RTT；不能称75次RTT|
|热缓存且有脏写|一次WriteSum＋一次ReadSum，分别排队；程序回零另加请求|快线程在每次同步ADS调用期间不产下一快拍|
|75个读符号均未缓存|至多75次解析＋1次Sum＝76次ADS往返|按每次300ms串行预算22.8s；这是冷缓存极端预算，不是现场观测或固定延时|
|最多13项写均未缓存|至多13次解析＋1次Sum＝14次往返|按300ms预算4.2s；实际可复用读/初始化缓存，不能简单每轮相加|
|缓存已热、单次请求无响应|该ADS调用可等300ms|可超过main300ms等快照期限，也可能跨250ms手柄消费间隔|
|某符号不存在|解析到该符号即失败，没有最终Sum；失败不缓存|快速错误回包通常较快，具体耗时未知；无响应可等300ms，后续仍可重试|

**推断**：冷解析/慢请求可延迟main的300ms等帧、引发B04；若同线程停写心跳超过PLC2s，触发P03。它不一定令ADS服务的failure计数增加：若慢低频最终成功、下次核心读写也成功，仍可能只是missed_deadlines/主机保持。**未知**：实际75个符号有多少已被初始化缓存、现场单次RTT及请求分布、是否达到这些期限；`ads_rtt_us`只覆盖快速ReadSum（1498–1501），并不测量低频请求占用总时长。没有把上表预算冒称“实际最坏停机时间”。

**推断的预算限定**：22.8s要求前75次句柄解析均成功且各接近300ms，再执行最终Sum；任一解析失败立即结束，绝非“75个符号全部超时后仍继续”。13项冷写同理，且读写之间有句柄重叠，不能把22.8s与4.2s当成每周期必然相加。该推导依据 `B/ADSComm.cpp:291–305,452–466,633–656`；OS调度、互斥等待、端口清理另计，因此也不是硬wall-clock上界。

#### T5.5 main重连与PLC重启清理清单

**已确认**：`reconnect_count`变化只执行以下动作：两柄净距离following=false、排坐标刷新、排watchdog恢复、非ExitSync的spacing.reset、关tracking补偿；日志明确保留原回退任务与方向（`B/main.cpp:1029–1038`）。不清B01、不改变单双柄锁、不把PLC自检直接判假，也不无条件清startup/control_active。

**证据 `B/main.cpp:1029–1038`：**

```cpp
1029: 		if (ads_stats.reconnect_count != handled_reconnect_count)
1030: 		{
1031: 			handled_reconnect_count = ads_stats.reconnect_count;
1032: 			axis1_net_distance.set_following(false);
1033: 			axis6_net_distance.set_following(false);
1034: 			ads_communication.request_coordinate_refresh();
1035: 			ads_communication.request_watchdog_recovery();
1036: 			if (spacing_recovery.phase != SpacingRecoveryPhase::ExitSync) spacing_recovery.reset();
1037: 			tracking_controller.disable_compensation();
1038: 			std::cout << "ADS 重连：保留原回退任务与方向，按 PLC 当前状态继续确认和交接。" << std::endl;
```

**已确认**：ADS服务断线清快句柄/通知、desired/last输出与代次、坐标缓存和事件缓存（`B/ads_communication.cpp:2022–2056`）；事件缓存estop和host假只是缓存失效，不证明PLC已清。首次重新读取自检来自PLC事件，不能把缓存清零当PLC重新自检。main也只用新鲜Running有效快照更新self_check_done（`B/main.cpp:990–993`）。

**已确认**：`plc_restart_count`变化另执行：FF关、tracking关、spacing重置、取消回退、control=false、startup.completed=false/WaitForEnter、清力（`B/main.cpp:1361–1373`）；自检真假边沿另见T1.3。服务将PLC状态非RUN或PLC DC时钟/周期回退计为重启检测（`B/ads_communication.cpp:919–923,1550–1572,2060–2074`），这是“重启/应用重载等价事件”检测，不是证明断过驱动电。连续3份PLC周期和DC时间均不变是task_stalled，作无效快照而非立即增加restart_count（1555–1581）。

**已确认**：另外，重连时发现PLC非RUN（`B/ads_communication.cpp:1098–1104`）和重新初始化时应用名改变（1280–1281）也可mark_plc_restart；mark只更新本地检测状态/坐标缓存和计数（2060–2074），不是执行PLC重启。全搜三端生产代码未发现调用AdsSyncWriteControl切RESET/SHUTDOWN/RECONFIG，也未见以故障处理启动/重启A、B或PLC进程的路径；ADS头文件中的API声明不算调用。`B/zenoh_service.cpp:270–293` 的CreateProcessW启动的是zenohd路由器。**未知**：现场运维脚本、人工操作或系统服务是否触发三端重启，现有日志无法判定。

<a id="t6-logs"></a>
### T6. 历史日志：搜索范围、可用证据与统计边界

#### T6.1 搜索方法与日志路径

**已确认**：诊断路径由exe目录拼 `ads_fault_diagnostics.log`，控制台诊断也尝试附加到此文件（`B/main.cpp:41–49,1223–1247,1654–1657`）。自动记录器则基于**进程当前工作目录**建records，不是exe目录（`B/experiment_recorder.cpp:397–444`，410拼records、441–444列文件）。主端LogReceived进入最多300条内存集合，未发现业务落盘实现（`R/Services/RemoteRobotLink.cs:563`、`R/ViewModels/MainViewModel.cs:48,99,538–548`）；重启主端会丢掉该内存上下文。

**证据 `B/main.cpp:41–49`：**

```cpp
41: 	// 只记录故障边沿；使用 exe 旁的文件，VS 与脚本启动都能找到同一位置。
42: 	wchar_t diagnostic_exe[MAX_PATH] = {};
43: 	GetModuleFileNameW(nullptr, diagnostic_exe, MAX_PATH);
44: 	std::wstring diagnostic_path(diagnostic_exe);
45: 	const auto diagnostic_slash = diagnostic_path.find_last_of(L"\\/");
46: 	diagnostic_path = diagnostic_slash == std::wstring::npos ? L"" : diagnostic_path.substr(0, diagnostic_slash + 1);
47: 	diagnostic_path += L"ads_fault_diagnostics.log";
48: 	std::cout << "[ADS-DIAG] build=" << diagnostic_build << " " << __DATE__ << " " << __TIME__
49: 		<< "；故障记录位于 ADS.exe 同目录的 ads_fault_diagnostics.log" << std::endl;
```
**证据 `R/ViewModels/MainViewModel.cs:538–548`：**

```csharp
538:         private void AddLog(string level, string text, DateTime? time = null)
539:         {
540:             if (level == "warn" || level == "error")
541:             { LastWarning = text; OnPropertyChanged(nameof(LastWarning)); }
542:             Log.Insert(0, new LogItem
543:             {
544:                 Time = (time ?? DateTime.Now).ToString("HH:mm:ss"),
545:                 Level = level,
546:                 Text = text,
547:             });
548:             while (Log.Count > MaxLogItems) Log.RemoveAt(Log.Count - 1);
```

**已确认（本次只读统计）**：对工作区 `D:\Work_files\Vessel intervention Robot` 和用户目录 `C:\Users\Sui` 枚举 `.log/.out/.err/.txt`，按UTF-8/UTF-16LE/GB18030标记字节检索：工作区3891个候选，成功读61,717,997字节；用户目录12691个候选，成功读1,838,814,112字节。排除.git/node_modules/site-packages/.codex，包含Debug/Release、包验证目录、用户Temp。两份Windows WebCache日志占用不可读，Temp及Quark证书缓存各一目录拒绝枚举；没有证据表明它们属于机器人日志。检索命中的源码、文档、AI搜索转录、PASS/fixture不计运行事件；有效事件原始出处为下列L-DBG/L-REL/L-COPY/L-OLD及记录器文件。

**未知**：未挂载现场电脑、用户其它盘或未重定向的控制台记录不在覆盖范围；零命中不等于现场零次。下表是留存记录计数，不是实际故障/停机次数。

|日志别名|完整位置|有效行数|文件属性最后写入时间（Asia/Shanghai；不是每条事件时间）|
|---|---|---:|---|
|`L-DBG`|`B/x64/Debug/ads_fault_diagnostics.log`|8（1–8）|2026-10-16 15:29:56|
|`L-REL`|`B/x64/Release/ads_fault_diagnostics.log`|3（1–3）|2026-11-02 16:52:21|
|`L-COPY`|`D:\Work_files\Vessel intervention Robot\主从分机软件包_20261008_211541\ADS项目\ADS项目\x64\Debug\ads_fault_diagnostics.log`|40（1–40）|2026-11-02 20:33:36|
|`L-OLD`|`D:\Work_files\Vessel intervention Robot\260316\x64_debug_verify2_run.log`|39（1–39）|2026-09-02 21:44:44|

#### T6.2 当前/同类诊断的原因统计与相对时间分布

**已确认**：前三份诊断共51行；L-COPY:1–8与L-DBG:1–8逐字相同，按完整行去重得到43行。全部是hold_update、CONNECT阶段失败；没有墙钟、build或session头。local/fallback错误写在**同一条连接尝试**里，不能把6和1861计成两个停机事件。逐条证据范围如下。

|ADS-DIAG原因|原始行数|去重行数|来源|确定程度|
|---|---:|---:|---|---|
|`AdsPortOpenEx failed`，local和fallback均未开端口|32|32|L-COPY:9–40|已确认|
|`AdsSyncReadStateReqEx: 6`，fallback `1861`|19|11|L-DBG:1–8、L-REL:1–3、L-COPY:1–8|已确认|
|合计|51|43|以上三文件|已确认|

|去重记录段|文件:行号|条数|tick_ms首→尾|跨度|确定程度|
|---|---|---:|---|---:|---|
|ReadState段1|L-DBG:1–7；副本L-COPY:1–7|7|177763812→177775875|12.063s|已确认|
|ReadState段2|L-DBG:8；副本L-COPY:8|1|6478765|单条|已确认|
|PortOpen段1|L-COPY:9–14|6|21148375→21153375|5.000s|已确认|
|PortOpen段2|L-COPY:15–25|11|43770203→43780218|10.015s|已确认|
|PortOpen段3|L-COPY:26–34|9|45050015→45058015|8.000s|已确认|
|PortOpen段4|L-COPY:35–40|6|40771078→40776078|5.000s|已确认|
|ReadState段3|L-REL:1–3|3|470735171→470739187|4.016s|已确认|

**未知**：tick为本机运行计时，不是日期；seq重置只支持分记录段，不能确认多少次程序/PC/PLC重启。日志证明“连接阶段失败”，不能证明当时已经控制机器人、更不能证明三端重启由这些记录导致。文件mtime只能表明最后写文件的时间。

#### T6.3 用户指定事件计数与“恢复”前序分类

|事件|目标/同类三份诊断运行日志|其它可用运行证据|确定程度|
|---|---:|---|---|
|[ADS-DIAG]失败|51原始/43去重|只有上列两类CONNECT原因|已确认|
|[PLC-ESTOP]|0|未找到有效运行样例|已确认（留存范围）|
|计划回退各种失败文案|0|离线测试模拟有6条，不计现场|已确认（留存范围）|
|“轮询失败并已断开”|0|其它命中是源码/转录，不计|已确认（留存范围）|
|“ADS快照已恢复”|0|L-OLD:28,35,38共3条；另有离线测试16条|已确认|
|“连接保持激活”|0|离线测试24条，不计|已确认（留存范围）|
|PLC重启检测|0|无有效运行事件序列|已确认（留存范围）|
|ADS重连|0|离线测试24条，不计|已确认（留存范围）|
|自检状态变化|无可统计序列|文档/PASS文字不算变化事件|未知（缺运行序列）|

**已确认**：当前版没有“ADS快照已恢复”运行记录，因此不能对当前版按前邻ADS-DIAG/连接保持/无标记给出占比。唯一额外运行重定向L-OLD先记录物理USB手柄打开失败、后台重试（1–6），属于旧直连版本；其3条恢复文案都没有ADS-DIAG/“连接保持激活”前序。按“上次恢复后至本次恢复前”的事件窗口：第1条（28）前是“PLC保持开启”（27），没有中断标记；第2/3条（35、38）前有旧版“ADS快照中断”（34、36），第3次还出现“PLC保持开启”（37）。**未知**：第1条是不是250ms手柄rebase；旧版初始化路径与当前版不同，不能强行归因。

#### T6.4 原始日志样例及无法给出第三类的原因

样例A，**已确认**，ReadState双目标连接失败，`L-REL:1–2`：

**证据 `L-REL:1–2`：**

```text
1: [ADS-DIAG] hold_update tick_ms=470735171 failure_tick_ms=470735171 seq=1 state=1 CONNECT: local={target=192.168.0.160.1.1:851 client_port=0 timeout_ms=1000 Error: AdsSyncReadStateReqEx: 6 } fallback={target=169.254.119.135.1.1:851 client_port=0 timeout_ms=1000 Error: AdsSyncReadStateReqEx: 1861 } target=169.254.119.135.1.1:851 client_port=0 timeout_ms=1000
2: [ADS-DIAG] hold_update tick_ms=470737171 failure_tick_ms=470737171 seq=2 state=1 CONNECT: local={target=192.168.0.160.1.1:851 client_port=0 timeout_ms=1000 Error: AdsSyncReadStateReqEx: 6 } fallback={target=169.254.119.135.1.1:851 client_port=0 timeout_ms=1000 Error: AdsSyncReadStateReqEx: 1861 } target=169.254.119.135.1.1:851 client_port=0 timeout_ms=1000
```

样例B，**已确认**，AdsPortOpenEx失败，`L-COPY:9–10`：

**证据 `L-COPY:9–10`：**

```text
9: [ADS-DIAG] hold_update tick_ms=21148375 failure_tick_ms=21147750 seq=1 state=1 CONNECT: local={target=0.0.0.0.0.0:0 client_port=0 timeout_ms=1000 Error: AdsPortOpenEx failed } fallback={target=169.254.119.135.1.1:851 client_port=0 timeout_ms=1000 Error: AdsPortOpenEx failed } target=169.254.119.135.1.1:851 client_port=0 timeout_ms=1000
10: [ADS-DIAG] hold_update tick_ms=21149375 failure_tick_ms=21148765 seq=2 state=1 CONNECT: local={target=169.254.119.135.1.1:851 client_port=0 timeout_ms=1000 Error: AdsPortOpenEx failed } fallback={target=169.254.119.135.1.1:851 client_port=0 timeout_ms=1000 Error: AdsPortOpenEx failed } target=169.254.119.135.1.1:851 client_port=0 timeout_ms=1000
```

样例C，**已确认但属旧版**，`L-OLD:27–39`；只展示恢复与中断文案，不能补为当前第三类停机：

**证据 `L-OLD:27–39`：**

```text
27: PLC 保持：开启。
28: ADS 快照已恢复：实际位置、手柄基准与当前稳定模式已重建。
29: ADS 目标 PLC 应用: Port_851
30: PLC 保持：关闭。
31: 请安装器械。
32: 进入启动姿态请按键盘 S 键。
33: 直接继续控制请按键盘 C 键。
34: ADS 快照中断：保持最后参考并丢弃故障期间手柄增量。
35: ADS 快照已恢复：实际位置、手柄基准与当前稳定模式已重建。
36: ADS 快照中断：保持最后参考并丢弃故障期间手柄增量。
37: PLC 保持：开启。
38: ADS 快照已恢复：实际位置、手柄基准与当前稳定模式已重建。
39: PLC 保持：关闭。
```

**结论〔未知〕**：“现场最常见前三类停机原因”无法从留存资料计算。现有诊断中记录最多的是PortOpen失败32条，其次ReadState失败11条；它们是连接重试记录，不能等同停机事件。没有第三类有效运行原因或样例，不以离线fixture补足。

#### T6.5 自动录制与离线测试的排除依据

**已确认**：唯一目标工程录制在 `B/x64/Release/records/20261102/165217_auto/`。`session.json:5–6`开始为2026-11-02T08:52:17.015Z（北京时间16:52:17.015），结束null；23–36 ADS baseline=false/指标null，54 stop_reason=null。motion.csv:2–131共130行，position_valid和两handle_valid全部0、位置NaN，elapsed_us为789797→5345776；force.csv:2–262共261行，sample/calibrated_valid全0，elapsed_us为789781→5355776。该目录未见events.csv。不能从这些数据重建有效控制的停机链，`utc_end=null`本身也不证明崩溃。

**证据 `B/x64/Release/records/20261102/165217_auto/session.json:5–6`：**

```json
5:   "utc_start": "2026-11-02T08:52:17.015Z",
6:   "utc_end": null,
```
**证据 `B/x64/Release/records/20261102/165217_auto/motion.csv:1–3`：**

```text
1: sample_index,elapsed_us,position_valid,axis1_from_left_mm,axis2_from_left_mm,axis3_from_left_mm,axis4_from_left_mm,axis5_from_left_mm,axis6_from_left_mm,axis7_from_left_mm,axis1_handle_valid,axis6_handle_valid
2: 0,789797,0,NaN,NaN,NaN,NaN,NaN,NaN,NaN,0,0
3: 1,805776,0,NaN,NaN,NaN,NaN,NaN,NaN,NaN,0,0
```

**已确认**：包根 `验证记录/20261010_ADS恢复排查/ads_diagnostics_test.log:27–35` 有PASS及 `connected target=offline`，属于离线验证。用户Temp的 `handoff-offline-*/main_recovery_test.log` 8份含恢复16条；`return_recovery_test.log` 6份含连接保持24、ADS重连24、模拟回退失败6；5份Temp与6份仓库ads_diagnostics_test共11条offline connected，均排除。原始依据样例：

**证据 `PKG/验证记录/20261010_ADS恢复排查/ads_diagnostics_test.log:27–35`：**

```text
27: PASS: both connection failures remain failures
28: PASS: fallback failure cannot erase primary connection error
29: PASS: missing return symbol prevents readiness before first return
30: PASS: missing symbol failure preserves variable name
31: [ADS-DIAG] connected target=offline:851 timeout_ms=300
32: PASS: reconnection reloads actual PLC self-check instead of retaining cleared cache
33: PASS: two-byte PLC state notification is accepted
34: PASS: wrong-size state notification is rejected
35: Offline test artifacts: C:\Users\Sui\AppData\Local\Temp\ads-offline-diagnostics-fqgkor29
```
**证据 `TEMP/handoff-offline-TWlDdS/main_recovery_test.log:1–8`：**

```text
1: main_recovery_test.cpp
2: PASS: repeated start leaves an active return and its baseline untouched
3: PASS: fresh ADS releases the snapshot gate without rebasing an unfinished return
4: PASS: disconnect cache reset does not masquerade as PLC self-check falling edge
5: PASS: a transient snapshot loss does not cancel the selected control mode
6: ADS 快照已恢复：实际位置、手柄基准与当前稳定模式已重建。
7: PASS: dual-handle recovery rebases both handles together
8: PASS: recovered control keeps the selected mode without explicit restart
```

**未知**：主端框架外第三方程序集是否有独立落盘日志，现场启动方式是否重定向stdout，以及是否另有有效records未复制到本机。当前业务主端仅内存日志与上述全无效录制，无法补足历史因果。

<a id="t7-reuse"></a>
### T7. 寻参跨PLC重启复用：静态事实与待证前提

#### T7.1 NC6..15编码器参数、偏置和参考系统

**已确认**：磁盘工程 `TC:2` 声明TwinCAT 3.1.4024.67，POU对象头有3.1.4024.15（如 `P/SelfCheck.TcPOU:2`）；二者都不能替代在线运行版本核对。NC6..15均AxisType=1、Encoder EncType=19、MaxCount=#xffffffff；官方Beckhoff《Encoder Types》文档1255152907把19列为MDP 513/DS402 CoE增量编码器接口。下表是文件中的原始比例，不把它擅自解释成电机一圈/丝杠一圈。

|NC轴|应用用途|Encoder/EncPara位置（TC）|ScaleFactorNumerator / Denominator|方向附加项|所连驱动Box；映射位置（TC）|确定程度|
|---|---|---|---|---|---|---|
|6|本体axis1|2183–2184|65.94 / 425164.8|无显式Inverse|Drive 9，7525–7540|已确认|
|7|本体axis2|2324–2325|360 / 768300|同上|Drive 10，7543–7558|已确认|
|8|本体axis3|2465–2466|65.94 / 425164.8|同上|Drive 11，7561–7576|已确认|
|9|本体axis4|2607–2608|360 / 758988.8|同上|Drive 12，7579–7594|已确认|
|10|本体axis5|2748–2749|65.94 / 425164.8|同上|Drive 13，7353–7368|已确认|
|11|本体axis6|2890–2891|65.94 / 425164.8|Inverse=true|Drive 14，7371–7386|已确认|
|12|本体axis7|3031–3032|360 / 768300|无显式Inverse|Drive 15，7389–7404|已确认|
|13|注射器1|3173–3174|360 / 180224|同上|Drive 16，7407–7422|已确认|
|14|注射器2|3315–3316|360 / 180224|同上|Drive 17，7425–7440|已确认|
|15|axis2同步从轴|3457–3458|360 / 757760|同上|Drive 18，7443–7458|已确认|

**证据 `TC:2177–2184`：**

```xml
2177: 				<Axis Id="6" CreateSymbols="true" AxisType="1" GroupId="3">
2178: 					<Name>Axis 6</Name>
2179: 					<AxisPara>
2180: 						<Velo SlowManual="3" FastManual="10"/>
2181: 						<OtherSettings AllowMotionCmdToSlave="true"/>
2182: 					</AxisPara>
2183: 					<Encoder Name="Enc" EncType="19">
2184: 						<EncPara ScaleFactorNumerator="65.94" ScaleFactorDenominator="425164.8" MaxCount="#xffffffff"/>
```
**证据 `TC:2887–2893`：**

```xml
2887: 						<Velo SlowManual="3" FastManual="10"/>
2888: 						<OtherSettings AllowMotionCmdToSlave="true"/>
2889: 					</AxisPara>
2890: 					<Encoder Name="Enc" EncType="19">
2891: 						<EncPara ScaleFactorNumerator="65.94" ScaleFactorDenominator="425164.8" Inverse="true" MaxCount="#xffffffff"/>
2892: 						<Vars VarGrpType="1">
2893: 							<Name>Inputs</Name>
```

**已确认（磁盘配置范围）**：NC6..15各AxisPara/EncPara未显式写Position Bias/Offset、参考系统选择或retain/persistent字段；相关配置内嵌在250902.tsproj，没有另找到本组轴外置.xti参数。TC1617、1759、1902确有别的轴（NC2/3/4）Offset=196.397/480/144，不能移用于NC6..15。**未知**：字段省略后的在线默认值、运行时MC_SetPosition累积的偏置、NC在PLC重启/配置激活/驱动重连后是否沿用坐标；“没写Offset”不等于已证明在线为0或必保留。

#### T7.2 驱动型号、ESI身份与位置连续性信息

|驱动Box|ESI Type/Desc与身份字段|适用NC轴|证据（TC）|确定程度|
|---|---|---|---|---|
|9/11/13|Micro 4804 MZ-CAT-STO；Vendor 0x1a3、Product 0x01313932、Revision 0x4130324a|6/8/10|6439、6626、6813|已确认|
|14|同型号；Revision 0x4130324b|11|6894|已确认|
|16/17/18|同型号；Revision 0x41303248|13/14/15|7082、7163、7244|已确认|
|10|DEN-XCR-E；Vendor 0x29c、Product 0x03831002、Revision 0x0005000f|7|6520|已确认|
|12|DEN-XCR-E；Vendor 0x29c、Product 0x03831002、Revision 0x0005000e|9|6707|已确认|
|15|CAP-XCR-E；Vendor 0x29c、Product 0x03731001、Revision 0x0005002d|12|6976|已确认|

**已确认**：上述是工程内嵌ESI描述/身份，不代表已取得完整厂家对象字典。Micro列出的发送PDO有6041 Statusword、6064 DINT Position actual、6077 Torque，另有60F4 Following error与60FD Digital inputs（`TC:6453–6470,6827–6844,7258–7275`）；DEN/CAP包含6041、6064、606C、6061（6532–6550、6988–7006）。Micro额外列出的PDO条目是否实际激活要结合SyncMan/映射，不能仅凭出现在XML就认定全部送PLC。NC链接能确认位置与状态输入（映射范围见上表）。

**证据 `TC:6453–6462`：**

```xml
6453: 						<Pdo Name="Transmit PDO 1 mapping parameters" Index="#x1a00" Flags="#x0000" SyncMan="3">
6454: 							<Entry Name="Statusword" Index="#x6041">
6455: 								<Type>UINT</Type>
6456: 							</Entry>
6457: 							<Entry Name="Position actual value" Index="#x6064">
6458: 								<Type>DINT</Type>
6459: 							</Entry>
6460: 							<Entry Name="Torque (current) actual value" Index="#x6077">
6461: 								<Type>INT</Type>
6462: 							</Entry>
```
**证据 `TC:6464–6470`：**

```xml
6464: 						<Pdo Name="Transmit PDO 2 mapping parameters" Index="#x1a01" Flags="#x0000" SyncMan="3">
6465: 							<Entry Name="Following error actual value" Index="#x60f4">
6466: 								<Type>DINT</Type>
6467: 							</Entry>
6468: 							<Entry Name="Digital inputs" Index="#x60fd">
6469: 								<Type>UDINT</Type>
6470: 							</Entry>
```

**已确认**：交付文件未显式映射名为encoder_valid、encoder_position_valid、power_cycle_count或boot_counter的对象，也未发现PLC利用此类标志验证寻参连续性。WcState/InputToggle属于通信观测，不能直接证明驱动电源从未断过（`TC:7353–7594`）。**未知**：6041厂家位、60FD特定位或未映射对象是否含“位置仍有效/驱动重新上电”信息；缺完整ESI/固件对象字典、驱动供电拓扑和断电记录。用户“断电约5秒丢位”作为现场背景保留，代码和现存日志不证明这个5秒阈值。

#### T7.3 现有RETAIN/PERSISTENT与寻参建立的量

**已确认**：正式生产POU/GVL声明未发现应用 `VAR_GLOBAL PERSISTENT/RETAIN` 或 `VAR RETAIN`；G为普通VAR_GLOBAL（`G:5`），init_pos、leftlimit、自检资格和装载资格均为普通变量（21–35）。TMC中库类型的TcIgnorePersistent等元数据不是应用持久化声明；不能据此声称已保存寻参结果。FirstCycle和init主动清self_check_done，即使某些位置恰好在线保留也不能直接绕过资格门（T2.5）。

**证据 `G:21–35`：**

```iecst
21:     init_pos			: ARRAY[1..7] OF LREAL := [30, 9, 0, 0, 0, 30, 13];
22:     refer,Act_pos,act_h,act_hf	: ARRAY[1..7] OF LREAL;
23:     
24: 	// 自检相关变量
25:     leftlimit           : ARRAY[1..7] OF LREAL;
26: 	act_pos_from_left	: ARRAY[1..7] OF LREAL;
27: 	refer_from_left		: ARRAY[1..7] OF LREAL;
28:     self_check_done     : BOOL := FALSE;
29: 	selfcheck_start_req  : BOOL := FALSE;
30: 	selfcheck_status     : DINT := 0; // 0=未就绪，1=等待启动，2=运行中，3=参数拒绝
31: 	selfcheck_target_from_left : ARRAY[1..7] OF LREAL := [96,0,500,0,530,580,0];
32: 	startup_loading_ready : BOOL := FALSE; // TRUE=自检刚完成且仍允许按标准器械装卸姿态启动；上位机开始控制前消费为 FALSE
33: 	handle_reinit_req	: BOOL := FALSE;
34: 	handle_reinit_done	: BOOL := FALSE;
35: 	selfcheck_reset_req : BOOL := FALSE;
```

|量/动作|什么时候建立/改变|源码位置|确定程度|
|---|---|---|---|
|七轴init_pos|init全部上电后采当前ActPos|`P/init.TcPOU:74–81`|已确认|
|直线1/3/5/6 leftlimit|负向寻限，DriveLimitActive或MoveError/停滞检测触发后记录当前ActPos|`P/SelfCheck.TcPOU:166–211`|已确认|
|停止/复位/Power循环/退离|触发后Stop→停稳→必要Reset/Power→SetPosition→移到leftlimit+输入距离|`P/SelfCheck.TcPOU:183–440`|已确认|
|直线init_pos|退离完成后刷新，随后相关位置为当前ActPos减init_pos|`P/SelfCheck.TcPOU:446–460`|已确认|
|旋转2/7回零|MoveAbsolute到NC绝对0，完成后init_pos设0|`P/SelfCheck.TcPOU:450–507,550–556`|已确认|
|轴4参考|不做同样直线寻限，保持当前位置；末尾init_pos=ActPos|`P/SelfCheck.TcPOU:450–507,554–555`|已确认|
|自检/装载/handle重建资格|all stu=10后self_check_done=true、startup_loading_ready=true、handle_reinit_req=true|`P/SelfCheck.TcPOU:532–548`|已确认|
|夹持输出|完成时cyl1/3=1000、cyl2/4=150；流程中cyl5=2000|`P/SelfCheck.TcPOU:113–116,542–545`|已确认|
|参考/滤波/距左|完成时统一刷新Act_pos、act_pos_from_left、refer、ref_slow、act_h/hf|`P/SelfCheck.TcPOU:557–564`|已确认|
|右软界|后续handle初始化以leftlimit加固定距离99/666/688/688重算|`P/handle.TcPOU:45–47,195–197`|已确认|
|装载资格消费|旧startup入口把startup_loading_ready=false，下次可走recovery_mode；不是自动重新SelfCheck|`B/startup_sequence.cpp:53–66,109–124`|已确认；当前入口区别见T1|

**推断**：MoveError或停滞也可触发记录leftlimit，因此文件中的leftlimit不必然每次都是“已经确认接触正确左开关”的物理基准（`P/SelfCheck.TcPOU:166–211`）。能否保证重复性需自检当拍位置/速度/错误及开关记录，不能只凭变量名认定标定质量。

**证据 `P/SelfCheck.TcPOU:541–555`：**

```iecst
541: IF all_done THEN
542: 	G.cylinder1_value := 1000;
543: 	G.cylinder2_value := 150;
544: 	G.cylinder3_value := 1000;
545: 	G.cylinder4_value := 150;
546:     G.self_check_done := TRUE;
547: 	G.startup_loading_ready := TRUE;
548:     G.handle_reinit_req := TRUE;
549:     FOR i:=1 TO 7 DO
550:         // 旋转轴已回到绝对零位，参考坐标必须同样采用绝对零点。
551:         // 不能保留 init 阶段采集的自检前角度，否则相对 0 会重新指向旧位置。
552:         IF NOT target_axes[i] AND i <> 4 THEN
553:             G.init_pos[i] := 0.0;
554:         ELSIF i = 4 THEN
555:             G.init_pos[i] := G.axis[i].NcToPlc.ActPos;
```

#### T7.4 `MC_SetPosition` 是否把左限位置零

**已确认**：调用明确是 `Position := G.axis[i].NcToPlc.ActPos`、`Mode := FALSE`，不是Position=0，也不是Mode=TRUE相对加值；等待Done后运动目标=leftlimit+退离距离（`P/SelfCheck.TcPOU:333–369`）。项目没有在这个调用传ClearPositionLag/ClearPositionOffset选项。官方《MC_SetPosition》（Beckhoff Infosys文档70052491，访问2026-11-07）说明FALSE为绝对设置，默认保留跟随误差；ClearPositionLag才额外清跟随误差，较新库还有读/清位置偏置功能。此处源码只确认“把当前NC实际坐标再设置为该值”，没有把物理左限重新定义成NC零点；业务距左靠减G.leftlimit实现。

**证据 `P/SelfCheck.TcPOU:333–338`：**

```iecst
333: 				fb_set_pos[i](
334: 					Axis:=G.axis[i],
335: 					Position:=G.axis[i].NcToPlc.ActPos,
336: 					Mode:=FALSE,
337: 					Execute:=set_pos_exec[i]
338: 				);
```

**未知**：该调用在现场NC状态下的内部偏置是否严格不变、是否跨配置激活保留，以及驱动6064在EtherCAT重新初始化时的行为；须同时记录原始6064、NC ActPos/SetPos、可用偏置/参考状态，并核对在线TwinCAT/Tc2_MC2版本。静态文件无法证明“驱动器没断电”足以保证这组坐标连续。

#### T7.5 B端对PLC重启和自检变化的处理；涉及的改动点（不实现）

**已确认**：B检测到plc_restart_count变化，会清startup完成/控制、力、spacing、补偿并取消回退（`B/main.cpp:1361–1373`）；自检由真变假清控制/旧动作，假变真则重建坐标与当前位置基准（1802–1847）。ADS服务的非RUN/DC时钟或周期回退与坐标/通知缓存重建，也会参与判定（`B/ads_communication.cpp:919–923,1550–1572,2022–2074`）。这些是当前必经行为，不包含“读取上次寻参然后沿用”的分支。

依T7问题要求，以下仅列将来支持复用时需要审视的**源码/配置位置**，不提供修改方案、不代表已授权实现：

|涉及位置|当前承担的行为/须覆盖的关联点|确定程度|
|---|---|---|
|`G:21–35`|寻参数据、资格、装载与重建标志均普通应用变量|已确认|
|`P/MAIN.TcPOU:38–46,116–138`|FirstCycle资格清除；自检请求/参数与启动门|已确认|
|`P/init.TcPOU:36–38,74–81`|清资格/装载并重采init_pos后转_self_check|已确认|
|`P/SelfCheck.TcPOU:183–211,333–369,446–460,541–566`|leftlimit、NC位置设置、init_pos与完成资格/参考来源|已确认|
|`P/clear_err.TcPOU:63–83`、`P/handle.TcPOU:126–213`|按资格选择_init/handle；重建参考/滤波/右软界及任务状态|已确认|
|本节NC6..15 EncPara/PDO/驱动身份表|应用坐标与原始位置连续性的相关配置/观测入口|已确认；连续性本身未知|
|`B/main.cpp:1029–1044,1330–1345,1361–1373,1802–1847,5072–5119`|重连、重启、自检边沿、初次同步与开始控制门|已确认|
|`B/ads_communication.cpp:1550–1572,2022–2074`|重启识别、事件/坐标缓存失效和重新装载|已确认|

**结论〔已确认/未知〕**：当前实现不复用；纯PLC应用层有确定的资格清零点。驱动不掉电条件下能否可靠恢复原参考，静态调查尚不能保证，缺位置连续性与电源连续性证据。本报告没有实现或提出具体复用算法。

本节及T2使用的Beckhoff官方释义核验页（访问日期2026-11-07；工程事实仍以各文件:行号为准）：

```text
MC_SetPosition： https://infosys.beckhoff.com/content/1033/tcplclib_tc2_mc2/70052491.html
Encoder Types： https://infosys.beckhoff.com/content/1033/tcadscommon/1255152907.html
ST_AxisStatus： https://infosys.beckhoff.com/content/1033/tcplclib_tc2_mc2/70161291.html
```

### T8. 定位臂对本体的直接与间接干扰路径

**已确认**：应用层没有“定位臂局部错误→写本体G.gen_state/self_check_done”的直接语句。MAIN在同一任务依次调用ArmManual、ArmCartesian、ArmCartJog、Axis2Sync、InjectorManual，再执行本体状态分派（`P/MAIN.TcPOU:94–144`）；三个Arm POU各自维护owned/error，不能将其POU内部RETURN误读为退出MAIN或取消本体。定位臂Power/Reset/轴状态也不在handle的本体1..7错误循环（`P/handle.TcPOU:216–232`）。

**证据 `P/MAIN.TcPOU:94–105`：**

```iecst
94: arm_manual_();
95: arm_cartesian_();
96: arm_cart_jog_();
97: axis2_sync_();
98: injector_manual_();
99: 
100: // 轴6自检触发左限位后只关闭负向许可，保留正向脱离限位的能力。
101: axis6_enable_negative := NOT ((G.gen_state = _self_check) AND G.axis[6].Status.DriveLimitActive);
102: POWERED();
103: IF G.gen_state <> _handle THEN
104: 	SETPOSITIONGEN();
105: END_IF
```

|路径|已确认链路|对本体的条件影响|证据与确定程度|
|---|---|---|---|
|定位臂局部拒绝/错误/owned|手动、曲线、末端点动互斥；清本地请求/局部停止释放|没有直接gen_state/selfcheck赋值；局部锁不会按源码直接要求本体重新寻参|`B/arm_manual_ads_service.cpp:265–345,424–475`、`P/ArmManual.TcPOU:105–161`、`P/ArmCartesian.TcPOU:63–177,249–269`、`P/ArmCartJog.TcPOU:45–146`；已确认|
|同一个ADS同步执行线程|Arm低频按名读写在100Hz服务中执行；主循环等快照300ms|慢请求可使本体连接保持；够久还会延迟ADS心跳，可能触发PLC2s host锁存|`B/ads_communication.cpp:1042–1045,1654,2077–2163`、`B/main.cpp:52–53,947–948`、`P/MAIN.TcPOU:79–84`；链路已确认，实际超限推断|
|低频请求开始后无限期等返回|调用方250ms只可撤尚未开始的请求；正在执行继续等|Arm服务工作线程也停在请求上，到期清方向/取消等检查可能迟到；共享ADS快照同时受阻|`B/ads_communication.cpp:574–624`、`B/arm_manual_ads_service.cpp:478–550`；结构已确认，现场时长未知|
|同一PLC 1ms任务|PlcTask周期10000×100ns=1ms，Arm调用在本体之前|Arm计算/FB调用变慢可增加同任务本体周期；是否触发TwinCAT任务超时/STOP取决在线策略|`C从端/250902/Untitled2/PlcTask.TcTTO:4–8`、`P/MAIN.TcPOU:94–98`；共享已确认，故障传播推断/未知|
|角度归一化的无界WHILE|ArmCartJogGen多处反复±360，无迭代上限；未见该处finite门|若角度为±Inf，减/加360仍为Inf，可阻塞本PLC任务；巨大有限数也可耗时。未证明有效现场数据能进入此条件|`P/ArmCartJogGen.TcPOU:81–82,124–127,185–188`；条件推断|
|共享EtherCAT设备/链路|工程本体、同步、注射器及定位臂映射位于共同设备结构|单个臂驱动故障是否使整链路异常/PLC停机，不能仅凭拓扑判定|`TC:6437–6518,6811–6813,7331–7595`；拓扑已确认，传播未知|
|共享主端状态发布/门控|CanOperate统一依赖同一state/lease；B state失鲜停发|Arm引起共享线程延迟时，UI本体区也可能变灰；局部arm错误字段本身不使CanOperate假|`R/ViewModels/MainViewModel.cs:114–119`、`B/zenoh_service.cpp:1025–1026`；条件推断|
|本体安全条件反向传播给臂|host超时清Arm使能，curve/cjog也检查estop|这是本体/主机→Arm门控，不能反推每个Arm错都导致本体_err|`P/ArmManual.TcPOU:35–42`、`P/ArmCartesian.TcPOU:63–70`、`P/ArmCartJog.TcPOU:45–65`；已确认|

**证据 `P/ArmCartJogGen.TcPOU:78–84`：**

```iecst
78: IF reanchor THEN
79:     anchor := p; latched_mode := mode; latched_dir := dir; sigma := 0;
80:     branch_q3 := anchor[3];
81:     WHILE branch_q3 > 180.0 DO branch_q3 := branch_q3-360.0; END_WHILE
82:     WHILE branch_q3 < -180.0 DO branch_q3 := branch_q3+360.0; END_WHILE
83:     IF (branch_q3 <> 0.0) AND (branch_q3 <> 180.0) AND (branch_q3 <> -180.0) THEN
84:         IF branch_q3 < 0 THEN branch := 1; ELSE branch := -1; END_IF
```

**结论〔已确认/推断〕**：静态可确认的主要跨域通道是共享ADS线程、共享PLC任务及共享状态链；未找到定位臂局部错误直接改本体状态或清寻参资格的代码。正常有限角度不会因为上述WHILE自然变成死循环，实际阻塞/任务超时/总线传播仍需要记录验证。

## 4. 历史日志统计与样例索引

完整统计与原文已放在[T6](#t6-logs)，避免把重复副本和离线验证混算。便于交接查阅：

|需要回答的问题|本次可交付结果|原始出处/确定程度|
|---|---|---|
|当前诊断按原因计数|PortOpen失败32；ReadState local6/fallback1861去重11；共43，原始51|L-COPY:9–40，L-DBG:1–8，L-REL:1–3；已确认|
|每类按日期/时间分布|只能还原7个tick段与文件最后写入时间；无法恢复逐事件墙钟|T6.2逐行范围；未知准确日期|
|PLC-ESTOP/回退失败/USB轮询失败|当前有效运行日志0命中|T6.3范围；已确认留存范围，不代表现场0次|
|ADS恢复前序分类|当前版无样本；旧版3次中1次前序保持开启，2次有ADS快照中断|L-OLD:27–39；已确认旧版，当前归因未知|
|前三类停机与每类1–2段日志|不能给现场前三；仅两类连接失败各给1段原文|T6.4；未知第三类，不编造|
|录制能否还原|130条motion和261条force均无有效样本|B/x64/Release/records/20261102/165217_auto/{motion.csv:2–131,force.csv:2–262}；已确认|

## 5. 未能确定的问题与所缺证据

下表是调查边界与验证所需信息；本次未执行所列现场测试，也未修改任何实现。

|编号|问题（未知）|当前已有证据与不能跨越的边界|需要的信息/现场验证|
|---|---|---|---|
|U01|现场二进制是否就是本次源码|磁盘Git基点与工作区修改已记录；TC:2/各POU:2版本头不能证明部署版本|A/B可执行文件版本/hash、构建时间；PLC下载工程/在线版本；启动命令与cwd|
|U02|实际最常见前三类停机|仅43条去重CONNECT失败；没有有效现场停机链，T6|带墙钟/进程或会话身份的完整B控制台与诊断、主端未清内存日志、故障前后PLC快照|
|U03|电子限位实际报哪种错、是否压住就反复Error|err不直接检查DriveLimitActive；SelfCheck将聚合标志作负限触发，P/err.TcPOU:35–60、P/SelfCheck.TcPOU:183–198|触发当拍Status.ErrorID、DriveDeviceError、Power/Reset ErrorID、6041/60FD与接线定义；受控确认清错/退离反馈|
|U04|主端USB拔插后SDK真实行为|四次void读取后无条件有效，R/Services/HandleService.cs:192–211|厂家SDK接口/源码或设备状态文档；拔插的调用耗时、返回数据和线程栈；两个序号分别确认|
|U05|native写力/publish是否阻塞|锁内native调用已确认；R/Services/HandleService.cs:216–244，A/VI.Service.Net/ZenohSession.cs:120–133|SDK调用耗时、失联时线程栈；独立确认采样线程、Handle线程、state回调/UI等待链|
|U06|Zenoh何时永久closed、UI M07路径现场发生过没有|closed→不可用、非空Connect早退与UI分支均确认；T3.2|运行版本native日志与session状态；受控断网/路由器重启后的重连观察；不要将普通2s失鲜等同永久closed|
|U07|250ms rebase主要来自哪一层|poll消费rx差、main同步等待、丢帧/锁路径均存在；T4.3/4.4|两端采样/发送/回调/消费时间戳、同时间基抓包与main/ADS线程周期记录；现有单一恢复文案不足|
|U08|定位臂请求典型/最长耗时|75读/最多13写、缓存与300ms预算已证；B/arm_manual_ads_service.cpp:616–713、B/ADSComm.cpp:623–656|冷/热句柄命中数、每次低频请求及快读/写耗时、排队龄期；ads_rtt_us只测快读，不能替代总占用|
|U09|PLC无Error/timeout等待是否真的发生|P/handle.TcPOU:168–214,559–627等结构已确认|停机时Enable/Disable FB的Busy/Done/Error/ErrorID、return_state、host_recover_req、任务周期；需识别卡住的实际阶段|
|U10|Axis15单独错误能否恢复、是否传播给本体2|Reset只跟随2轴，err纳入15轴；T2.6|Axis15与2轴同步的NC错误/Power/Reset/GearIn状态；耦合配置的在线错误传播策略|
|U11|NC/驱动位置跨PLC重启或配置激活是否连续|EncType19、6064、无应用持久化和资格强清已证；T7|同一驱动未掉电时的重启前后6064、NC ActPos/SetPos/偏置/参考状态；分开验证PLC重启与激活配置；准确在线库版本|
|U12|是否有可信电源/编码器有效证明|交付PDO未显式映射有效位或启动计数；TC:6453–6470等|厂家完整ESI/对象字典、固件版本、6041/60FD位解释、可用boot counter/编码器状态、供电拓扑及掉电行为|
|U13|NC左右软限位在线有效值|NC6..15文件省略相关属性；本体PLC只有右侧refer门控，T2.5|在线NC软限位使能/上下界；电子开关方向与驱动内部限位策略；不能把缺字段当禁用|
|U14|Arm异常计算/总线故障能否使本体PLC停止|共享1ms任务/总线、条件性WHILE风险已证；T8|在线任务超时/异常策略、EtherCAT故障策略、实际臂位置值和任务耗时；未观察到真实卡死|
|U15|注射器错误/本体estop的现场联锁范围|InjectorManual不查estop_hold_req、不写gen_state，P/InjectorManual.TcPOU:24–42|硬接线安全链、驱动组策略与注射器故障记录；应用代码不能证明物理安全链覆盖|
|U16|是否有源码外的gen_state/_reset写入或“重启”提示|生产POU与A/B未找到；T2.1/T3.3|第三方ADS工具、运维脚本、现场其它主端、SDK弹窗与操作规程|
|U17|录制元数据null是否表示崩溃|session.json:6,54及CSV均无有效样本；T6.5|该次进程退出方式、Windows事件与对应版本记录器收尾行为；null本身不足以归因|

## 6. 只读性与交付校验

本报告只写入用户指定的Markdown文档。没有执行机器人/主端/ADS/PLC程序，没有运行运动/上电/清错/PLC变量读写命令，没有提交代码。调查所用Python仅读取源码、XML、日志与文件属性，并生成/校验本报告；已有工作区修改保持原样。

**已确认（交付校验）**：排除bin/obj/x64/.git/.vs/node_modules后，按相对路径排序，对855个源码/配置文件的“相对路径UTF-8＋原始内容”累计SHA-256，调查前后均为 `5e089354ea46019a457ae6dfe25f8dac438a5cddb1591ab36eb53953a3395fff`。覆盖后缀为cpp/h/hpp/cs/xaml/TcPOU/TcGVL/TcDUT/TcTTO/tsproj/xti/plcproj/csproj/json/config/yaml/yml/toml；这确认本次覆盖的源码/配置未变，不冒称对全部磁盘文件做过镜像。

**已确认（报告自检）**：T1～T8齐全；总表74行；99段带文件与行号的源证据，每段最多15行；摘要10项/10个物理行；引用范围、摘要锚点与Markdown表格列数通过只读脚本校验。原有Git修改/未跟踪文件保留，新增交付仅本报告。未跑工程测试，未进行在线ADS读写或机器人验证。
