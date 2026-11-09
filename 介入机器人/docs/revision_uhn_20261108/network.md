# N：网络结构、复现和根因

## 源码确认的拓扑

- 数据端`zenoh_service.h::ZenohServiceConfig`默认`tcp/192.168.60.1:7447`；主端RemoteRobotLink按配置Host/Endpoint生成`tcp/<Host>:7447`。这是主端到数据端的Zenoh链路。
- `AdsCommunicationService::ensure_connection`先尝试`OpenCommInsideReadOnly()`，失败才尝试`OpenCommReadOnly()`；低频通道顺序相同。
- `ADSComm.cpp::OpenCommInsideReadOnly`调用`OpenCommLocalMode`：AdsPortOpenEx→AdsGetLocalAddressEx→端口851。本机AMS NetId来自运行机Router，不是源码固定值。
- `OpenCommReadOnly`调用`OpenCommRemoteMode`：目标AMS NetId硬编码`169.254.119.135.1.1`，端口851，经本机Router访问远端。
- 两种Open只确认状态可读，不切换RUN；服务另要求`ADSSTATE_RUN`，否则记录`CONNECT_PLC_STATE`或`PLC_STATE`并沿原规则恢复。

结论为“本机优先，远程回退”，不能由仓库确定现场那一次选中了哪条路径。新增[ADS-STATE]日志带目标NetId、操作及原因，供现场确认。main的`hardcoded_ads_netid`用于寻参记录身份；真正远程地址在ADSComm.cpp。

192.168.60.x和169.254.x.x是不同地址段，不能据此认定物理网卡/网线不同。仓库没有现场网卡绑定、AMS路由表或接线记录，故“同一根网线是否同时承载Zenoh和ADS”**未确认**。本轮未连接现场PLC，未读取或修改TwinCAT运行状态。

## 重复“连接保持结束”的根因

修改前公共恢复分支包含`connection_hold_active_prev && initial_sync_done`，外层却没有排除`handle_soft_hold_active`。手柄失效时内部跳过滤波和motion_sync，但`baseline_rebuilt`默认true，仍清ADS保持并打印“连接保持结束：实际位置与手柄基准已重建”。下一拍保持继续由手柄失效维持，再次进入该分支。

这并非真实保持每拍退出再进入，而是**手柄保持持续期间错误执行完成分支**。HEAD分支在10 s只断Zenoh回放中重现547次过早完成报告，真实保持只进入一次。见`network/baseline/zenoh_10.log`末尾`REPRODUCED N old_premature_rebases=547`。

修复仅在外层增加`!handle_soft_hold_active`。所需手柄有效且恢复静置完成后，才调用现有同步函数重建。保持入口/出口仍按原边沿运行，并输出来源；ADS状态统一通过set_connection_state只在变化时打印原因；删除原每秒hold_update。未增加重试、超时、RUN切换或恢复绕过。

## 六种离线复现

生产ADS通信线程+已有ADS mock；真实ZenohService::on_handle认证接收边界+生产Handle；main抽取保持/重建/自动恢复分支。真实等待10/30 s。Zenoh中断停止投递认证手柄帧；ADS中断使Open、状态读取和Sum Read/Write返回通信失败。

这不是完整网络驱动层或现场AMS Router模拟。真实zenohd及C#↔C++互通另由原Zenoh181/84项回归覆盖。

| 中断 | 时长 | 恢复到ADS Running且保持退出 | 进入/退出/基准重建日志 | 控制 |
|---|---|---|---|---|
| 只断Zenoh | 10 s | 515 ms | 1/1/1 | 自动恢复 |
| 只断Zenoh | 30 s | 516 ms | 1/1/1 | 自动恢复 |
| 只断ADS | 10 s | 109 ms | 1/1/1 | 自动恢复 |
| 只断ADS | 30 s | 297 ms | 1/1/1 | 自动恢复 |
| 两者同时断 | 10 s | 500 ms | 1/1/1 | 自动恢复 |
| 两者同时断 | 30 s | 516 ms | 1/1/1 | 自动恢复 |

ADS恢复本身不晚于表中时刻，六项均在现有1 s重连周期加初始化时间以内。之后继续稳定运行600 ms，断言一次进入/退出、一次基准重建和控制恢复。状态日志审计见`network/log_audit.json`；没有相同状态自转换，相邻转换首尾连续。

## ADS持续非Running：现场根因未解决

六项mock未复现持续非Running。原条件仍为PLC设备RUN、初始化成功、完整ADS读写成功；Zenoh已连接并有控制权不会保证这些条件成立。**没有证据证明重复完成分支导致ADS线程永久非Running，不能把两个现象合并宣布修复。**

缺少现场ads_fault_diagnostics.log、[ADS-STATE]转换/错误码、实际目标NetId和TwinCAT路由状态，无法判断来自路由恢复、PLC状态或具体ADS调用失败。按用户约束，该根因修复停止在证据边界；未强制PLC RUN，未添加绕过，也没有证据将其归因于TwinCAT路由。
