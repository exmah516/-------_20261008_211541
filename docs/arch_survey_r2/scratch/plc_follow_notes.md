# PLC Follow 计算核对（供 R2-2 合并）

别名：P=`介入机器人/C从端/250902/Untitled2/POUs/`，G=`介入机器人/C从端/250902/Untitled2/GVLs/G.TcGVL`。所有公式是当前ST直接翻译（已确认），不含现场验证。第一轮C.4已给参数清单，本轮新增依赖图及真正在feed中的分支关系。

## 1. 每扫描顺序及公式

令 `r=G.refer[j]`（相对init），`s=G.ref_slow[j]`，`h=G.act_h[j]`，`z=traj_pos[j]`，`v=traj_vel[j]`，`a=traj_acc[j]`。下标old指此前扫描持久状态，dt硬编码.001，不读取任务实际elapsed。

| 顺序 | 当前计算 | 上一拍状态 | 证据/确定度 |
|---|---|---|---|
| 坐标 | `Act_pos=ActPos-I; act_pos_from_left=ActPos-L; refer_from_left=r+I-L` | I、L来自自检/复用，但本式不作动态积分 | 已确认：P/handle.TcPOU:489-492 |
| 回退冻结 | 对return_enable轴1/3/5/6，`state>=30 OR Done OR Error`时：`r=z_old; refer_from_left=z_old+I-L; CONTINUE`。下面斜坡/均值/二阶全部跳过。 | z及Done/Error/state | 已确认：P/handle.TcPOU:65,494-500 |
| 旁路与均值窗 | `b=(j==1 && (fast1||Busy1))||(j==6 &&(fast6||Busy6)); b: counter=0,N=1; !b: counter=min(counter_old+1,20),N=max(1,counter)` | counter | 已确认：P/handle.TcPOU:13,503-512 |
| 缓存移位 | 对k2..20：`hh[22-k]=hh[21-k]`，即hh[20]←hh[19]…hh[2]←hh[1] | hh[1..19] | 已确认：P/handle.TcPOU:514-516 |
| 右软限位拦截 | 只对1/3/5/6：`if ActPos>=right_slimit && r>s_old: r=s_old`。不是对r或z的min/max位置裁剪；没有此块对应的左软限位。 | s_old；right_slimit在初始化时缓存 | 已确认：P/handle.TcPOU:58-59,298-300,518-522；G:180 |
| 斜坡 | `b: s=r; !b: s=s_old+clamp(r-s_old,-G.v_limit[j],+G.v_limit[j])` | s_old | 已确认：P/handle.TcPOU:502,524-532 |
| 均值 | `b: hh[1..20]=s; h=s; !b: hh[1]=s; h=sum(hh[1..N])/N` | hh/counter | 已确认：P/handle.TcPOU:534-545 |
| 二阶支路 | 若traj_enable且!b且!startup_bypass：`a=clamp(60²*(r-z_old)-120*v_old,-amax,+amax); v=clamp(v_old+.001*a,-vmax,+vmax); z=z_old+.001*v`；若abs(r-z)<.001且abs(v)<.01，令z=r,v=a=0。 | z_old、v_old | 已确认：P/handle.TcPOU:95-103,547-566 |
| 直接轨迹支路 | 否则 `z=h; v=0; a=0`。 | h含均值历史，但本式无额外积分 | 已确认：P/handle.TcPOU:567-570 |
| 最终feed门 | `ext_enabled OR state==31 OR spg_enable_exec`；默认traj_enable且!startup_bypass时发送`Position=z+I,Velocity=v,Acceleration=a`；否则发送`Position=h+I,Velocity=(h-h_prev)/.001,Acceleration=0`。之后`act_hf=h; h_prev=h`。未feed时act_hf=实际相对位置。 | ext_enabled/state、h_prev | 已确认：P/handle.TcPOU:893-919 |
| FB执行 | 先feed，再设置enable/disable Execute和调用ReturnMove；MAIN在handle返回后才调用SETPOSITIONGEN动作，后者图形FB读取各自Execute/Position/PositionType字段。 | FB上次扫描输出 | 已确认：P/handle.TcPOU:922-938；P/MAIN.TcPOU:198-199,3948-4031 |

## 2. 参数与实际串并关系

| 参数/事实 | 值、意义 | 证据/确定度 |
|---|---|---|
| traj_enable | `[true,false,true,false,true,true,false]`，1/3/5/6使用二阶 | 已确认：P/handle.TcPOU:103 |
| wn | 60；临界阻尼式系数2wn=120；硬编码dt=.001秒 | 已确认：P/handle.TcPOU:100,548,555,561 |
| vmax/amax | 1/3/5/6均80 mm/s、2000 mm/s²；其余0但二阶未启用 | 已确认：P/handle.TcPOU:101-103,549-560 |
| G.v_limit默认 | `[7.5,7.5,1.5,1.5,1.5,7.5,1.5]`，斜坡每扫描最大位置步长；代码没有乘.001。单位转换的效果是7.5/1.5 mm或deg每扫描，不应把数值本身写成mm/s。 | 已确认：G:72-74；P/handle.TcPOU:502,526-529 |
| 右软限位 | 初始化缓存`leftlimit + [99,0,666,0,688,688,0]`；1/3/5/6启用 | 已确认：P/handle.TcPOU:58-59,298-300；G:180 |
| 均值窗口 | 1至20扫描逐拍增长；旁路重置counter=0且填满缓存为当拍参考；随后恢复时从N1重长 | 已确认：P/handle.TcPOU:503-545 |
| 均值与二阶的关系 | 二阶输入是`r`而不是`s/h`。默认traj轴最终feed不使用斜坡和均值输出，尽管它们仍被计算。不能把默认Follow画成“斜坡→均值→二阶”的单链。 | 已确认：P/handle.TcPOU:524-568,897-913 |
| 软限位与二阶关系 | 右限位条件在二阶前修改r，仍会影响二阶；但r从s_old取得，二阶z也没有最终位置clamp，因此不等于最终给定被严格钳在软限位内。 | 已确认（赋值路径）/推断（数值可能超过）：P/handle.TcPOU:518-522,548-566 |
| fast效果 | fast/Busy令z=h=r，v=a=0，默认traj轴最终仍选traj feed支路，位置直接而速度报0，不走legacy差分速度。 | 已确认：P/handle.TcPOU:503-505,524-538,567-570,897-904 |
| startup_bypass效果 | 所有轴跳过二阶并选择legacy h位置+差分速度，但斜坡和均值仍按b决定是否旁路。它不能被解释为同时跳过全部平滑。 | 已确认：P/handle.TcPOU:503-545,547,567-570,897-913 |

依赖图（事实图）：

```text
refer --右软限位条件修正--> r --> 斜坡s --> 均值h ----> legacy feed(h,h差分)
                              └--> 二阶(z,v,a) -----> 默认1/3/5/6 feed
fast/Busy: s=h=z=r; v=a=0
startup_bypass/非traj轴: z=h; 最终选legacy
return30/31/Done/Error: r=z_old并冻结全部前处理；默认feed仍取z_old
```

均值与二阶并行这一事实也影响 R2-5：未执行30的snap不会自动由慢参考补到实际；30/31先continue，冻结的是旧z。精确收尾公式见 R2-5.3–R2-5.5。
