# R2-2 Follow 路径的逐拍计算

本附件将当前赋值翻译为公式，不定义新算法。第一轮的模式、crawl转移和配置全集分别见 B §B.3、§B.5、A §A.15；本轮只给从关节量到最终给定的计算次序。入口安全门见本轮 R2-4；触边之后的事务见 R2-1。

路径别名：`D/`=`介入机器人/B数据端/工程/ADS项目/`；`P/`=`介入机器人/C从端/250902/Untitled2/POUs/`；`G`=`介入机器人/C从端/250902/Untitled2/GVLs/G.TcGVL`。以下“已确认”指赋值路径或公式，不表示实机动态已验证。

## 2.1 共同输入、方向和坐标

令 `A_i=plc_act_pos[i-1]+I_i`、`I_i=init_pos[i-1]`、`L_i=leftlimit[i-1]`，`C_i`为数据端保存的绝对Follow目标，`r_i=C_i-I_i`为下发refer。位置减小为递送方向。令 `u0/u1` 为HandleFrame的线性/旋转关节量。下表均为**已确认**。

| 次序 | 公式/行为 | 跨拍状态 | 证据 |
|---|---|---|---|
| 物理柄分配 | 双柄轴1输入=587、轴6输入=582；单柄两个输入指针指向同一现有柄 | 拓扑、single_handle_mode | `D/main.cpp:130-148` |
| 原始量 | `fJoints2[i]=sample.joints[i]`；没有由vels积分生成joints | 最新远程样本 | `D/手柄/手柄.cpp:91-103` |
| 低通 | 首次直接reset；其后`f0←f0+0.25(u0-f0)`，`f1←f1+0.20(u1-f1)`；handle软保持时不update | 每侧filter.inited、f0/f1 | `D/control_types.h:52-75,176-177`；`D/main.cpp:1458-1470` |
| 线性差分 | `d_raw=(f0-prev_f0)×750×(-1)`；`abs(d_raw)<0.02mm`则`d=0`，否则`d=d_raw`。被死区去掉的量不会另存待偿还 | prev_linear_filtered | `D/control_types.h:81-84,156-157`；`D/main.cpp:2099-2104,2120-2125,4601-4605` |
| 方向门 | `gate(d,R)=R?max(d,0):min(d,0)`；单柄尚未入窗时使用gate，已入窗允许窗口内正负修正；双柄Follow直接用d而非directional量 | window_active、方向 | `D/main.cpp:470-481,2105-2108,2126-2129,3755-3758` |
| 方向来源 | 双柄任一B6=撤出，planned/spacing期间冻结；单柄取内部override或物理B6；独立导丝无override时取导丝B6 | dual_reverse_active、override、guidewire_mode | `D/main.cpp:1523-1547` |
| 双柄换向 | 先把两侧f0和prev_f0置当前raw，丢弃旧滤波尾量，本拍差分不沿用旧方向 | filter、prev | `D/main.cpp:1651-1662` |
| 旋转 | `r2=rot_base1+(f1_1-rot_ref1)×(-Rad)`；`r7=rot_base6+(f1_6-rot_ref6)×(-Rad)`。是相对基准的绝对映射，不是线性差分积分 | rot_base/rot_ref，axis2/7_hold | `D/main.cpp:3078-3089,3841-3846,4116-4118`；`D/control_types.h:84` |
| 拍末 | 无论本拍是否进入运动树，`prev_f0/prev_f1←当前滤波值`；连接保持/同步还会主动重建基准 | 两侧prev值 | `D/main.cpp:4601-4605,1250-1257` |

**已确认**：窗口 `W1=[L1+5,L1+30]`，`W6(A5)=[L6+(A5-L5)+1,L6+(A5-L5)+33]`；实际按当前cfg读值。双柄约束使用命令轴5推算的g，单柄普通导管的轴6窗口按当拍**实际A5**重算。出处：`D/motion_sync.cpp:14-65`；`D/main.cpp:3780-3786,3934-3947`。

## 2.2 双手柄：名义量、间距裁剪、先行、镜像

以下公式按 `D/main.cpp:3723-3926` 的先后顺序；每行均为**已确认**。`B1=axis1_crawl.base_rel`、`B3/B5=axis3/5_base_rel`，`P/E`为先行名义进度/已加量，`M=10mm`、`lead=2mm`、`lead_limit=5mm`为默认值。

| 步骤 | 当拍公式/输出 | 证据 |
|---|---|---|
| 1 输入冻结 | `inc1/6=dual_handoff.active()?0:d1/6`；轴3实际距左限位`<=20+0.2mm`时，只把负inc1清零，撤出仍可走 | `D/main.cpp:2115-2117,3755-3758` |
| 2 轴4额外输入 | 轴4点动且无pending意图时，首拍仅建立GetTickCount64基准；后续`inc6 += sign×7.8016217564×elapsed_s`，前进sign=-1、后退+1。elapsed无50/100ms裁剪 | `D/main.cpp:61,3759-3778` |
| 3 本拍g起点 | `C5=I5+B5+(C1-I1-B1)`；`g=(C6-L6)-(C5-L5)` | `D/main.cpp:3780-3786` |
| 4 先行预留 | `k=active&&!R?clip(lead,0,5)/M:0`；`remain=k>0?max(0,M-P):0`；向递送端的可用room=`max(C1-lo1,0)`，预留`e_room=min(remain,room/(1+k))×k` | `D/main.cpp:3788-3801` |
| 5 轴自身裁剪 | `d1>0:min(inc1,max(hi1-C1,0))`；`d1<=0:max(inc1,-(room-e_room))`。`d6>0:min(inc6,max(L6+660-C6,0))`；负d6此处不另裁剪 | `D/main.cpp:3794-3804` |
| 6 g裁剪 | 见下段伪代码；只缩造成越界的运动分量，不修改使g回到窗口的分量 | `D/main.cpp:3805-3824` |
| 7 先行增量 | `p=min(max(-d1,0),remain)`；`e=p×k`；`P+=p,E+=e`；P到M关闭active；`B3+=e,B5+=e` | `D/main.cpp:3825-3833` |
| 8 累加目标 | `C1←C1+d1-e`；`C6←C6+d6`；`g_after=g+d6-d1`。g公式使用名义d1，先行e由镜像基准吸收 | `D/main.cpp:3834-3839` |
| 9 镜像/旋转 | `δ=C1-I1-B1`；`r3=B3+δ,r5=B5+δ`；r1=C1-I1、r6=C6-I6；r2/r7按旋转式 | `D/main.cpp:3840-3846`；`D/motion_sync.cpp:513-521` |
| 10 输出及触边 | 缸1/2/3/4=`450/1000/550/1000`；按裁剪前方向辨认g触边意图；意图锁存后后续inc归零，等实际到位/低速才begin事务 | `D/main.cpp:3847-3891`；`D/control_types.h:208-215` |

g约束的无迭代公式（**已确认**；`D/main.cpp:3807-3824`）：

```text
q=d6-d1; q_hi=max(gmax-g,0); q_lo=min(gmin-g,0)
if q>q_hi:
    closing=max(d6,0)+max(-d1,0)
    s=max(0,1-(q-q_hi)/closing)
    if d6>0: d6*=s
    if d1<0: d1*=s
else if q<q_lo:
    closing=max(-d6,0)+max(d1,0)
    s=max(0,1-(q_lo-q)/closing)
    if d6<0: d6*=s
    if d1>0: d1*=s
```

**已确认**：上述修正允许已经在g窗外时保持或向窗内运动，不因改窗直接跳变；双柄轴1/3/5镜像在e吸收后是名义d1，轴1多走e。双柄普通Follow不具备tracking资格：资格要求`single_handle_mode && guidewire_mode==None`（轴1）或`guidewire_mode==Independent`（轴6）。出处：`D/main.cpp:2149-2152,3808-3810,3831-3844`。

## 2.3 单手柄普通导管

**已确认**：此路径的C1不是每拍按`handle_ref`直接算绝对位移，而是保存的上拍命令加本拍增量，再依次约束、tracking和先行。镜像仍从最终C1计算。出处：`D/main.cpp:3992-4121`。

定义当拍约束函数F（以下为原式等价伪代码，**已确认**；`D/main.cpp:3993-4031`）：

```text
follow_enabled=R || !delivery_stop_latched
F(candidate):
    out=C1_old
    if directional_increment_active && follow_enabled:
        out=window_active?clip(candidate,lo1,hi1):candidate
    if window_active: out=clip(out,lo1,hi1)
    if !R && out<lo1: out=lo1
    if delivery_stop_latched && !R: out=lo1
    return out
```

| 次序 | 公式/行为 | 状态、证据（均已确认） |
|---|---|---|
| 1 入窗/停止锁 | 首次实际进入W1时capture基准并置window_active；停止锁只在撤出且正增量时解除；到达轴3停止位的递送触边会锁住，不建立换手 | `D/main.cpp:3948-3953,3994-4006,4174-4183` |
| 2 名义目标 | `nom=F(C1_old+d_directional)`；`n=(!R && nom<C1_old)?C1_old-nom:0` | C1、window_active、stop_latch；`D/main.cpp:4033-4039` |
| 3 tracking | n>0时`req=tracking(n)`，`cmd=F(C1_old-req)`；否则cmd=nom；裁剪信息送commit | `D/main.cpp:4041-4052` |
| 4 先行 | 若active且!R且n>0：`P_new=min(M,clip(P,0,M)+n)`；`E_target=clip(lead×P_new/M,0,5)`；`e_req=max(0,E_target-E)`；`cmd_new=F(cmd-e_req)`；`e_applied=max(0,cmd-cmd_new)`；保存P/E并`B3/B5+=e_applied`；P到M关active | `D/main.cpp:4053-4090` |
| 5 tracking记账 | 给tracking的有效量为`min(max(0,C1_old-cmd_new),req)`，不把先行额外量算作PI偿还 | `D/main.cpp:4092-4111` |
| 6 refer | r1=cmd-I1；保存C1=cmd；r2旋转；r3/r5用最终cmd镜像 | `D/main.cpp:4113-4121`；`D/motion_sync.cpp:513-521` |
| 7 轴6普通保持 | 无轴4联动时r6=axis6_mirror_base_rel，轴7保持；缸3为`cyl3_follow_release=400`、缸4=1000；不是双柄的550 | `D/main.cpp:3087-3091,4122-4134`；`D/control_types.h:212-215` |
| 8 触边 | 用实际位置过边/已在边+同向有效输入触发；撤出目标lo1、递送目标hi1；轴6反向等位移目标经当拍W6裁剪及660检查，之后进入双腿事务 | `D/main.cpp:4137-4218` |

**已确认**：先行按裁剪前的**名义n**积累P，而非已含tracking/先行的最终位移；清映射只清active/P/E，不回滚已吸收进B3/B5的偏置。发生连接保持时剩余先行消失，临时夹具在本轮 R2-8 §8.4证明此消费者行为。出处：`D/main.cpp:337-357,4053-4086,1579-1596`。

单柄轴4/6联动（均为**已确认**）：每拍先以实际A5更新W6；点动首拍C6=A6且记录钟，后续`Δ6=±7.8016217564×elapsed_s`，传入axis6 crawl。crawl先把C6+Δ裁到W6，再做660越限检查；正/反向触边时建立GuidewireDelivery/Retraction单腿事务。正向联动后普通导管覆写缸3/4为400/1000，反向联动保留crawl输出。PLC轴4本身走点动flags/MoveVelocity，不由r4差分产生；r4本拍起始取实际值。出处：`D/main.cpp:2086,3934-3988,4122-4134,3121-3209,4501-4506`；轴4 PLC分段执行见第一轮C §C.4.1。

## 2.4 单手柄独立导丝

**已确认**：当前公共界面无进入此模式的入口，详R2-3；以下是内部模式若已进入时的行为。轴1/2/3/5固定在independent hold，r7使用导丝旋转。出处：`D/main.cpp:3672-3679`。

```text
nom=clip(C6_old+d6_directional, min(W6), max(W6))  # 有效输入时
n=(!R6 && nom<C6_old)?C6_old-nom:0
req=n>0?tracking_axis6(n):0
raw=n>0?C6_old-req:C6_old+d6_directional
increment=n>0?-req:d6_directional
run_axis6_crawl_state(raw,increment,R6)
effective=(!R6)?max(0,C6_old-C6_final):0
tracking.commit(n,req,effective,有效量不等请求量)
```

出处与确定度：**已确认**，`D/main.cpp:3681-3721`。crawl只有输入非零才重算目标，先clip到已保存W6；若`C6-L6>660+1e-6`则冻结轴1/2/3/5/6/7的实际位置并用450/1000/550/1000夹持组合；正向/撤出到边后取相反窗端建立轴6回退。窗口在进入/重基准时可锁定，不像普通导管每拍重算。`D/main.cpp:3095-3114,3125-3209`；`D/motion_sync.cpp:68-79,251-338`。

## 2.5 tracking 的状态和算式

资格、开关入口与换手欠账生命周期在第一轮B §B.7、本轮R2-1 §1.4、R2-3；此处只补数值递推。**已确认**：默认kp=.5、ki=2、max_gain=2、max_error=20mm；积分上限20mm·s，每拍补偿上限.25mm；普通Follow的夹爪**命令**稳定150ms后才active，没有夹爪到位反馈。出处：`D/delivery_tracking.h:42-49,170-172`；`D/delivery_tracking.cpp:128-169`。

```text
if !enabled || !segment_active || nominal<=eps: return max(0,nominal)
available=max(0,pending_handover_error-comp_outstanding)
if available<=eps: integral*=.75; return nominal
dt=last_pi_tick?clip((now-last_pi_tick)/1000,0,.1):0
e=min(available,max_error)
integral=clip(integral+e*dt,0,20)
extra_gain=clip(kp*e+ki*integral,0,max_gain-1)
extra=min(nominal*extra_gain,.25,available)
return nominal+extra
```

**已确认**：出处 `D/delivery_tracking.cpp:223-280`。commit把名义量计入nominal_outstanding；输出裁剪会回滚本拍积分，清PI时钟；仅把有效额外量计入comp_outstanding。实际向前量先冲nominal_outstanding，再冲comp_outstanding，只有后者才扣pending_handover_error。无名义输入不会自主偿还。`D/delivery_tracking.cpp:283-331,431-458`。

## 2.6 PLC 收到refer后的实际计算图

令`r=G.refer[j]`、`s=G.ref_slow[j]`、`h=G.act_h[j]`、`z/v/a=traj_pos/vel/acc[j]`，old为上扫描值。**已确认**：r为相对I坐标，默认1/3/5/6最终feed选择二阶结果；斜坡/均值和二阶是并行分支，不能写成“斜坡→均值→二阶”串联。出处：`P/handle.TcPOU:524-570,897-913`。

```text
refer --右软限位条件修正--> r --> 斜坡s --> 均值h --> legacy feed
                              └--> 二阶(z,v,a) --> 默认1/3/5/6 feed
fast/Busy: s=h=z=r，v=a=0
startup_bypass/非traj轴: z=h，最终选legacy
return30/31/Done/Error: r=z_old，跳过上述全部前处理
```

| 顺序 | 公式/参数 | 证据（均已确认，除注明推断） |
|---|---|---|
| 0 回退冻结 | return_enable轴1/3/5/6且state>=30/Done/Error时：r=z_old、refer_from_left=z_old+I-L，然后CONTINUE | `P/handle.TcPOU:65,489-500`；效果另见R2-5 |
| 1 旁路标志b | 仅轴1 `(fast1\|\|Busy1)` 或轴6 `(fast6\|\|Busy6)`；b时均值counter=0、N=1；否则counter=min(old+1,20)，N=max(1,counter) | `P/handle.TcPOU:503-512` |
| 2 右软限位 | 启用轴1/3/5/6：若`ActAbs>=right && r>s_old`则r=s_old；right初始化缓存为`L+[99,0,666,0,688,688,0]`。此块无对应左限位 | `P/handle.TcPOU:58-59,298-300,518-522`；`G:180` |
| 3 斜坡 | b时s=r；否则`s=s_old+clip(r-s_old,-v_step,+v_step)`。G.v_limit默认`[7.5,7.5,1.5,1.5,1.5,7.5,1.5]`，为每扫描位置步长，式中没有乘.001 | `P/handle.TcPOU:502,524-532`；`G:72-74` |
| 4 均值 | 先移位20槽；b时全部缓存=s、h=s；否则hh[1]=s，h为最近N槽平均 | `P/handle.TcPOU:514-516,534-545` |
| 5 二阶 | traj_enable且!b且!startup_bypass：`a=clip(3600(r-z_old)-120v_old,±2000)`；`v=clip(v_old+.001a,±80)`；`z=z_old+.001v`；接近r<.001且abs(v)<.01时z=r,v=a=0 | `P/handle.TcPOU:95-103,547-566` |
| 6 直接分支 | 其它情况z=h、v=a=0；b使s/h/z全等r | `P/handle.TcPOU:567-570` |
| 7 feed选择 | ext_enabled或state31或spg_enable_exec时，traj_enable且!startup_bypass发`(z+I,v,a)`，否则发`(h+I,(h-h_prev)/.001,0)`；更新h_prev。未feed时act_hf=实际相对位置 | `P/handle.TcPOU:893-919` |
| 8 FB与调用顺序 | feed之后调用ReturnMove/设置enable-disble；MAIN在handle之后执行SETPOSITIONGEN动作 | `P/handle.TcPOU:922-938`；`P/MAIN.TcPOU:198-199,3948-4031` |

**已确认**：traj_enable为`[true,false,true,false,true,true,false]`；startup_smoothing_bypass绕过二阶并改选legacy，但不自动绕过斜坡/均值。dt为硬编码.001秒，不读取任务实际elapsed。**推断**：右软限位用s_old替换r而非对最终z作位置clip，不能单凭此块证明最终给定严格不超右限位；是否发生需现场给定/实际曲线。出处：`P/handle.TcPOU:103,518-570,897-913`。

## 2.7 上一拍状态依赖总表

下表“无状态”仅指该算子本身；其输入仍可能由上游有状态算子产生。每行均为**已确认**。

| 算子 | 依赖上一拍/长驻状态 | 无状态部分 | 证据 |
|---|---|---|---|
| Handle读取 | 最新样本缓存、epoch/静置 | joints拷贝 | `D/手柄/手柄.cpp:45-103` |
| 低通/差分 | f、inited、prev_f | 比例与.02死区 | `D/control_types.h:52-75`；`D/main.cpp:2099-2129` |
| 方向与入窗 | dual_reverse、override、window_active | gate的min/max | `D/main.cpp:1523-1547,3747-3758,3948-3953` |
| Follow累加 | C1/C6、stop_latch、基准 | F的clip、g分量缩放 | `D/main.cpp:3780-3839,3992-4031` |
| 先行 | active、P、E、B3/B5 | 比例及当拍e公式 | `D/main.cpp:3788-3833,4057-4090` |
| 镜像/旋转 | B1/B3/B5、rot_ref/base、hold | 最终线性坐标变换 | `D/motion_sync.cpp:513-521`；`D/main.cpp:3841-3846` |
| 轴4耦合 | active_prev、last_ms、C6 | speed×elapsed | `D/main.cpp:3759-3778,3960-3988` |
| tracking | active、欠账、两类outstanding、积分、last_tick、夹持计时 | 当拍kp/ki与限幅 | `D/delivery_tracking.cpp:128-169,223-331,431-458` |
| 换手意图/crawl | dual_handoff、prev_abs/valid、方向保护、planned | 到边条件 | `D/main.cpp:3852-3891,3131-3209,4137-4173` |
| 上位软限位 | reason/hold、取消状态 | `target-L6>660+eps` | `D/main.cpp:709-749,1491-1495,3095-3114` |
| PLC右限位 | s_old、初始化right缓存 | 条件比较 | `P/handle.TcPOU:298-300,518-522` |
| PLC斜坡/均值 | s_old、counter、20槽hh | 当拍clamp/average | `P/handle.TcPOU:503-545` |
| PLC二阶/legacy速度 | z_old/v_old、h_prev | 当拍限速/限加速度 | `P/handle.TcPOU:547-570,897-915` |
| PLC回退/使能 | return_state、FB输出、Done/Error | feed坐标+I | `P/handle.TcPOU:494-500,574-703,893-938` |
