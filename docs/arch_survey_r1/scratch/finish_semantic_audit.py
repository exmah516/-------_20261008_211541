"""Apply source-checked survey corrections; write only the survey reports."""
from pathlib import Path

OUT=Path(__file__).resolve().parents[1]
def update(name, replacements):
    p=OUT/name
    text=p.read_text(encoding='utf-8-sig')
    for old,new in replacements:
        assert old in text, (name,old)
        text=text.replace(old,new)
    p.write_text(text,encoding='utf-8')

update('00_总览.md',[
    ('P/ArmCartJog.TcPOU:24,96-98','P/ArmCartJog.TcPOU:23,87-89'),
    ('其余引用使用根目录相对路径；行号含空行。','`scratch/`及附件名相对于本报告目录，其余引用相对于调研根；行号含空行。'),
    ('基线及机械引用校验结果','基线及引用完整性校验结果'),
])
update('99_未决问题.md',[
    ('2^32msuptime','2^32ms的uptime'),
    ('这些问题是待确定的事实，不构成本轮设计、修改或现场操作授权。','这些问题只涉及待确定的事实。'),
])
update('D_超时与恢复.md',[
    ('平滑时间常数100ms；交接后移动2mm或400ms开始300ms blend','换手锁存样本平滑时间常数100ms；交接后移动2mm或400ms开始300ms blend，steady_clock毫秒'),
    ('force/position安全门控仍优先清力；DWORD tick被转成int64','force/position安全门控仍优先清力；main传steady_clock的int64毫秒，未使用DWORD tick'),
    ('`D/force_feedback.cpp:21,61-107`；`D/control_types.h:573-591`','`D/force_feedback.cpp:21,61-107`；`D/control_types.h:573-591`；`D/main.cpp:4584-4596`'),
    ('主端FF/Zero等15s、普通命令5s','主端FF/Zero等15s、ForceHold5s、Cylinder/Y阀10s'),
    ('| HandleService heartbeat |','| 电缸/Y阀按需命令 | 各10000ms；主端请求计时 | Task超时并移除pending；下游task3s，与通用5s不同 | 已确认：`RemoteRobotLink.cs:268-271`；下游`D/zenoh_service.cpp:890-916,946-954` |\n| HandleService heartbeat |'),
    ('`A_工程全貌.md:307-315`、`C_时间敏感动作.md:26-28,113`、`F_测试现状.md:124-145`','`99_未决问题.md:7,13,15-19`、`C_时间敏感动作.md:60-68,430-439`、`F_测试现状.md:124-145`'),
])
update('C_时间敏感动作.md',[
    ('ADS单调用250ms（但低频服务等待未使用调用timeout）','ADS调用传250ms参数（低频API忽略调用者期限、底层300ms、队列等待无总上限）'),
])
update('A_工程全貌.md',[
    ('扭矩平滑时间常数100ms','换手锁存用力/扭矩平滑时间常数100ms（实时扭矩直接输出，不经过该平滑）'),
])

p=OUT/'A_工程全貌.md'
t=p.read_text(encoding='utf-8')
marker='## A.19 公共命令参数与界面档位'
if marker in t:
    t=t[:t.index(marker)].rstrip()+'\n\n'
t+='''
## A.19 公共命令参数与界面档位

**已确认**：A.5给出所有字段/类型，下面补齐Param1/2、Value1/2的逐命令意义。未使用参数由当前主端发送0/空，不代表数据端对每个未使用字段都强制0。所有业务命令仍受认证/序号、state新鲜度、lease门控；Release豁免state新鲜度。出处：`RemoteRobotLink.cs:224-271`、`D/zenoh_service.cpp:738-798`。

| 线命令/编号 | 参数与当前实际作用 | 直接出处（均已确认） |
|---|---|---|
| Hello/0 | SessionId=0，NonceClient16字节，其它nonce/proof/tag为空；单独建立挑战，不撤销有效控制权 | `D/zenoh_service.cpp:688-703` |
| Authenticate/1 | 挑战SessionId、两nonce各16字节、Proof32字节；验证后替换会话，清旧队列/控制权 | `D/zenoh_service.cpp:705-735`、`FrameAuth.cs:20-30` |
| Acquire/2 | 无业务参数；消费时持有控制权并回结果，无断流自动expiry | `D/zenoh_service.cpp:771-777,962-980` |
| Release/3 | 无业务参数；释放控制权、清持续方向/力和关联动作 | `D/zenoh_service.cpp:779-784,962-980` |
| PreparePosition/4 | Value1导管距左mm、Value2导丝/Y阀基准mm；范围5..95、10..639；内部转换axis1=Value1、axis3=Value2、axis5=Value2+5、axis6=Value2+5+g，再发自检 | `D/zenoh_service.cpp:816-838`、`RemoteRobotLink.cs:240-241` |
| StartControl/5 | 无业务参数；内部SelectDirectControl，成功在当前位置同步后控制 | `D/zenoh_service.cpp:841-850`、`D/main.cpp:5315-5356` |
| ForceFeedback/6 | Param1=0/1关闭/开启，Param2=0两侧/1导管587/2导丝582；首次未zeroed时先取零 | `D/zenoh_service.cpp:852-879`、`D/main.cpp:4992-4995` |
| Cylinder/7 | Param1=1..4电缸，Param2=1接管手动打开（1/3值2000、2/4值10），0撤手动覆盖回自动；不是“夹紧=true”的机械意义 | `D/zenoh_service.cpp:890-916`、`RemoteRobotLink.cs:268-269` |
| ArmManualEnable/8 | Param1=0/1定位臂总手动禁用/启用 | `D/zenoh_service.cpp:919-921` |
| ArmAxisEnable/9 | Param1=1..5定位臂轴，Param2=0/1单轴使能请求 | `D/zenoh_service.cpp:922-924` |
| ArmAxisReset/10 | Param1=1..5轴，复位脉冲；清末端点动并要求释放曲线所有权 | `D/zenoh_service.cpp:925-927`、`D/arm_manual_ads_service.cpp:187-198` |
| ArmAxisJog/11 | Param1=1..5轴，Param2=-1/0/+1方向，Value1=.5/1/2档；旧主端Value1=0兼容为1；CommandId强制0，不等结果 | `D/zenoh_service.cpp:763-765,928-934`、`RemoteRobotLink.cs:255-259` |
| ArmCartesianJog/12 | Param1=1..5模式，Param2=有符号speed×1000；main除1000，0调用stop；Value1传speedScale但服务忽略，不再参与末端速度计算 | `RemoteRobotLink.cs:249-252`、`D/zenoh_service.cpp:935-937`、`D/main.cpp:5375-5384`、`D/arm_manual_ads_service.cpp:236-251` |
| ArmStop/13 | 无业务参数；清末端/曲线运动并逐轴发送jog=0 | `D/zenoh_service.cpp:938-941` |
| ArmProgramZero/14 | 无业务参数；请求程序归零；后续须ArmCartesianAlive，UI每100ms发 | `D/zenoh_service.cpp:942-945`、`PositioningArmPanel.xaml.cs:160-169` |
| 保留/15 | 旧定位臂参数接口停用；当前枚举无15 | `ZenohFrames.cs:19-22` |
| ArmCartesianAlive/16 | 无业务参数，CommandId=0；延长当前点动/归零租约 | `D/zenoh_service.cpp:763-765,944-945`、`D/arm_manual_ads_service.cpp:279-301` |
| YValve/17 | Param1=1关闭、0打开；内部SetYValveOpen使用反值 | `D/zenoh_service.cpp:946-954`、`RemoteRobotLink.cs:270-271` |
| ForceZero/18 | Param1/2必须0；独立采集装机零点 | `D/zenoh_service.cpp:852-872`、`D/main.cpp:759-804` |
| ForceHold/19 | Param1=0/1关闭/开启换手力保持，清旧锁存；不改变两侧FF开关 | `D/zenoh_service.cpp:882-888`、`D/main.cpp:4997-5004` |

**已确认**：定位臂默认中档（倍率1）；快/中/慢单轴倍率2/1/.5。末端模式1/2速度快/中/慢=30/15/5，模式3=3/2.5/1.25，模式4=1.2/.8/.4，模式5=2.4/1.6/.8；按模式使用mm/s或°/s。UI先按档位计算绝对末端速度，服务忽略额外speedScale。出处：`MainViewModel.cs:58,445-473`、`D/arm_manual_ads_service.cpp:236-251`、`工程/protocol/PROTOCOL.md:132`。PLC模式和几何计算见 `P/ArmCartJogGen.TcPOU:104-164`。
'''
p.write_text(t,encoding='utf-8')
print('Applied clock/timeout corrections and command-parameter inventory.')
