"""从 R1 已确认清单种子导出 CSV；仅写本调研目录，保留逐项映射。"""
from pathlib import Path
import copy, csv, json, re
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'docs/arch_survey_r2'
rows=json.loads((OUT/'scratch/timeouts_seed.json').read_text(encoding='utf-8'))
header, rows=rows[0],rows[1:]
# 保留原编号作为种子映射，最后用稳定的 T 序号输出。
for r in rows:r[0]=str(r[0])
def at(name):
    found=[r for r in rows if r[3]==name]
    assert len(found)==1,(name,len(found))
    return found[0]
def patch(name,**kw):
    r=at(name)
    for k,v in kw.items():r[int(k)]=v
    return r

def clone(name,newname,val=None,clock=None,action=None,recovery=None,source=None,obj=None):
    r=copy.deepcopy(at(name));r[3]=newname
    for k,v in [(4,val),(5,clock),(7,action),(8,recovery),(2,source),(6,obj)]:
        if v is not None:r[k]=v
    rows.insert(rows.index(at(name))+1,r)
    return r
# 无界/非时间事实保留，数值列只放数值毫秒。
patch('PLC快照时钟停滞判定',**{'3':'PLC快照时钟停滞判定（连续3快照，非ms）'})
patch('低频ADS失败重试次数',**{'3':'低频ADS失败重试次数（2次，非ms）'})
patch('回退故障自动续做次数',**{'3':'回退故障自动续做次数（最多1次，非ms）'})
patch('屈曲恢复速度帽',**{'3':'屈曲恢复速度帽（40mm/s，非ms）','6':'轴3/5/6共同推进量'})
patch('力取零等待次数',**{'3':'力取零等待次数（3次，非ms）'})
patch('力保持释放起始移动距离',**{'3':'力保持释放起始移动距离（2mm，非ms）'})
patch('Zenoh命令FIFO容量',**{'3':'Zenoh命令FIFO容量（256条，非ms）'})
patch('Zenoh router启动探测次数',**{'3':'Zenoh router启动探测次数（40次，非ms）'})
patch('handle Enable错误重试次数',**{'3':'handle Enable错误重试次数（最多3次，非ms）','7':'重试达到3次后不再产生新的重试脉冲；2s阶段超时可转_err'})
patch('return state31 Enable错误重试次数',**{'3':'return state31 Enable错误重试次数（最多3次，非ms）','7':'达到次数后不再产生新重试脉冲；2s阶段超时可转_err'})
patch('err限位退离重试次数',**{'3':'err限位退离重试次数（最多3次，非ms）'})
patch('ArmCartesian到位确认周期',**{'3':'ArmCartesian到位确认（至少2扫描周期，非独立ms阈值）'})
# 低频API对传入timeout直接忽略：提供的数值与实际无界等待分别列出。
patch('低频ADS调用者等待',**{'3':'低频ADS调用者等待（无总上限；timeout参数被忽略）','4':'','5':'condition_variable::wait；无timeout出口'})
for n in ['50','100','250']:
    clone('低频ADS调用者等待（无总上限；timeout参数被忽略）',f'低频ADS调用者timeout参数{n}ms（被忽略，非实际上限）',n,'API形参；不用于wait')
# 数据端同一计时器在不同表行出现不重复列同一缓存年龄。
rows.remove(at('手柄缓存失效龄期'))
clone('ADS硬失联观察窗','ADS快通道失败后重连间隔','1000','QPC',
      '连接尝试失败后不早于1s再尝试','到达QPC next_reconnect_attempt且可连接',
      'D/ads_communication.cpp:16-20,825-858')
clone('手柄最大缓存龄期','手柄init重试间隔','1000','GetTickCount64',
      '到达重试点再次init','新有效样本及静置/重基准完成','D/main.cpp:1079-1128')
clone('计划回退提交保护/ACK','计划回退提交保护输出等待','250','GetTickCount',
      '提交保护输出或清旧状态未满足则retry/fault','guard generation已应用且PLC Busy/Done/Error已清',
      'D/main.cpp:2806-2837')
clone('计划回退提交保护/ACK','计划回退清旧状态/ACK等待','250','GetTickCount',
      '未看到新ACK则retry/fault','新Busy=true或Done=false事件，或满足新Done到位规则',
      'D/main.cpp:2247-2340,2839-2882')
patch('计划回退提交保护/ACK',**{'3':'计划回退提交保护/ACK配置阈值（共用值）'})
clone('回退故障恢复清Req间隔','回退故障恢复总期限（无）','','无独立计时器',
      '故障保持，反复Clear并检查资格','Clear已应用且更新快照/控制资格满足后sync成功')
clone('标准启动夹爪阶段','标准启动运动到位等待总期限（无）','','无独立计时器',
      '持续等待运动到位，或由其它门控中断','实际运动目标满足容差')
clone('寻参记录更新周期','寻参记录年龄expiry（未设置）','','无独立年龄计时器',
      '不按记录年龄单独判过期','配置、app名、位置容差和PLC restore ACK条件满足时可复用')
patch('记录对齐手柄历史',**{'3':'记录手柄历史留存窗口','5':'QPC','7':'移除窗口外旧手柄记录；不是匹配容差','8':'保留100ms以内手柄记录'})
clone('记录手柄历史留存窗口','记录运动行最近手柄匹配容差','10','QPC',
      '与ADS记录快照的QPC差大于10ms时handle_valid不被置真','存在10ms内最近手柄样本',
      'D/main.cpp:4610-4611,4659-4676')
clone('记录写线程空队列退避','记录写线程fflush阈值（64行，非ms）','','行计数',
      '每64行fflush；写盘失败停止记录','磁盘写入正常/新会话重启','D/async_csv_writer.h:187-195')
# 网络任务与router的完整分项。
for r in rows:
    if r[3].startswith('Zenoh ') and '任务' in r[3]:r[5]='GetTickCount64；仅publish_state时检查'
patch('Zenoh命令排队新鲜度',**{'5':'GetTickCount64；入队本地时间'})
for name in ['Zenoh router首探测','Zenoh router单次探测']:
    patch(name,**{'5':'Winsock select 的 timeval 相对超时'})
patch('Zenoh router首探测',**{'2':'D/zenoh_service.cpp:96-128,270-275'})
patch('Zenoh router探测间隔',**{'5':'Sleep(100)'})
clone('Zenoh router单次探测','Zenoh router最后端点探测','250','Winsock select 的 timeval 相对超时',
      '最后探测仍失败则停止自管router并返回false','端点探测成功','D/zenoh_service.cpp:297-307')
clone('Zenoh router单次探测','Zenoh router停止进程等待','2000','WaitForSingleObject',
      'TerminateProcess后最多等待2000ms，然后关闭句柄','进程退出/等待返回','D/zenoh_service.cpp:311-321')
clone('Zenoh ForceZero任务','Zenoh ForceZero成功后ForceSet新窗口','3000','GetTickCount64；仅publish_state时检查',
      'ForceZero成功且需Enable时转换ForceSet并重置deadline；期限到拒绝',
      'state对应侧ff开关达到目标','D/zenoh_service.cpp:398-421')
# 主端使用TimeProvider；命令Task.Delay由运行时计时，不是可替换NowMs。
for r in rows:
    if r[1]=='主端' and r[3] in ['主端认证后状态等待','主端状态帧新鲜度','主端重认证触发','主端重建最小间隔','主端force帧新鲜度','主端Session重建节流']:
        r[5]='TimeProvider.GetElapsedTime（默认System单调时间）'
        r[2]+='; RemoteRobotLink.cs:67-85'
    if r[1]=='主端' and r[5]=='Task.Delay/单调任务':r[5]='Task.Delay(timeoutMs)；.NET运行时计时器'
patch('主端重建最小间隔',**{'3':'主端重认证握手最小间隔','6':'Hello/认证重发','7':'距上次握手不足3s时不再发新Hello','8':'达到3s且状态过期/首次握手'})
patch('主端认证后状态等待',**{'7':'ConnectAsync抛TimeoutException；调用方处理连接失败'})
patch('HandleHost采样deadline',**{'7':'每拍deadline+=Stopwatch.Frequency/200；没有落后时跳到now+5ms分支；SDK异常退出'})
patch('应用关闭等待',**{'5':'HostApplicationOptions.StopTimeout（宿主库内计时）',
      '7':'8s传给宿主框架；面板Stop后断link/stop服务；本段未证明所有阻塞收尾有8s硬上限',
      '8':'关闭流程完成；真实收尾时长不由参数值单独保证'})
clone('手柄宿主监管心跳','手柄宿主重启节流','1000','Environment.TickCount64',
      '进程空且距上次启动满1s才重新启动','新宿主启动成功','HandleService.cs:112-149')
clone('手柄宿主监管心跳','手柄宿主Stop后退出等待','1000','Process.WaitForExit(timeout)',
      'USB重启/服务关闭先置memory.Stop；超时Kill后另有无界WaitForExit','宿主进程实际退出','HandleService.cs:126-135,168-172')
clone('手柄宿主监管心跳','手柄宿主管理循环休眠','5','Thread.Sleep',
      '每轮休眠5ms后再次监管；不是硬周期','监管线程继续运行','HandleService.cs:151-163')
clone('HandleHost力帧新鲜度','HandleHost零力重复发送间隔','100','Environment.TickCount64',
      '无有效力时至少每100ms送零；原先非零时立即清零','收到有效Force后改为输出有效力','VI.HandleHost/Program.cs:124-139')
# PLC专用时钟和合并阈值拆分。
patch('PLC主机心跳watchdog',**{'3':'PLC MAIN主机心跳watchdog','2':'P/MAIN.TcPOU:68-104'})
clone('PLC MAIN主机心跳watchdog','PLC handle主机心跳watchdog','2000','TON（1ms PlcTask）',
      '锁存host_comm_timeout，固定位置保持并停止独立运动','心跳正常、停稳且recover握手后重初始化','P/handle.TcPOU:108-136,321-466')
patch('axis4 state10/30/40',**{'3':'axis4 state10禁用等待'})
clone('axis4 state10禁用等待','axis4 state30停止等待','2000','TON',
      '超时转40错误收尾','Stop Done/低速或进入错误收尾')
clone('axis4 state10禁用等待','axis4 state40恢复Enable等待','2000','TON',
      '超时转_err并estop_hold','Enable Done且无错误')
clone('err普通恢复','err Reset脉冲','200','TON',
      '200ms后拉低Reset，开始2s重试等待','脉冲到期','P/err.TcPOU:179-197')
clone('err Power未就绪恢复','err Power-not-ready脉冲','200','TON',
      '200ms后拉低Reset并恢复Power，启动3s重试等待','脉冲到期','P/err.TcPOU:215-236')
patch('ArmCartesian heartbeat租约',**{'5':'heartbeat_age += cycle_seconds；PLC任务周期累计',
      '6':'定位臂程序归零曲线保活','7':'heartbeat_age>=0.5时allowed=false；活动阶段转40/错误2003','8':'新heartbeat重置age；停止/释放完成后才可新请求'})
patch('ArmCartesian enable/arrival等待',**{'5':'phase_elapsed += cycle_seconds；PLC任务周期累计',
      '6':'程序归零stage10使能/stage30到位等待','7':'超过2s置错误2007并转stage40取消释放','8':'在限期内完成Enable或到位条件'})
patch('ArmCartJog alive租约',**{'5':'alive_age += cycle_seconds；PLC任务周期累计'})
patch('ArmCartJog enable等待',**{'5':'enable_age += cycle_seconds；PLC任务周期累计'})
patch('ArmCartesian曲线允许时长',**{'3':'ArmCartesian曲线最小时长（输入校验）','4':'20','5':'curve[15]秒值；输入校验',
      '7':'时长<0.02s时拒绝请求，错误2002','8':'时长在0.02至120s范围内且其他校验通过'})
clone('ArmCartesian曲线最小时长（输入校验）','ArmCartesian曲线最大时长（输入校验）','120000','curve[15]秒值；输入校验',
      '时长>120s时拒绝请求，错误2002','时长在0.02至120s范围内且其他校验通过')
patch('PLC运行状态检查周期',**{'3':'PLC运行状态检查（每10快拍，目标100ms）','5':'快循环计数；100ms是10ms目标拍长的换算'})
patch('回退故障恢复清Req间隔',**{'5':'GetTickCount64（uint64毫秒传入）'})
patch('return state10 Disable',**{'2':'P/handle.TcPOU:599-627; G:181'})
patch('return state20 MoveAbsolute',**{'2':'P/handle.TcPOU:599-618,629-643; G:181'})
patch('return state31重新Enable',**{'2':'P/handle.TcPOU:599-607,670-703; G:181-182'})
patch('SelfCheck Power pulse',**{'2':'P/SelfCheck.TcPOU:285-306'})
patch('SelfCheck Power wait',**{'2':'P/SelfCheck.TcPOU:285-306'})
patch('PLC MAIN主机心跳watchdog',**{'7':'设置G.host_comm_timeout；_handle时由handle锁存并停止/清请求','8':'非_handle时心跳正常并收到host_recover_req；_handle时由handle完成停稳/恢复握手'})
clone('主端普通按需命令','主端ForceHold命令','5000','Task.Delay(timeoutMs)；.NET运行时计时器',
      'Task超时并移除pending；需重新发送','数据端ForceHold反馈达到目标或重新发送',
      'RemoteRobotLink.cs:266-267,326-345')
# 将路径别名展开为仓库相对实际文件名，校验每段行号没有超出当前文件。
D='介入机器人/B数据端/工程/ADS项目/'
P='介入机器人/C从端/250902/Untitled2/POUs/'
M='介入机器人/A主端/工程/master/'
G='介入机器人/C从端/250902/Untitled2/GVLs/G.TcGVL'
master={'RemoteRobotLink.cs':'VI.Service.Robot/Services/RemoteRobotLink.cs','HandleService.cs':'VI.Service.Robot/Services/HandleService.cs',
        'HandleDeviceNotifications.cs':'VI.Service.Robot/Services/HandleDeviceNotifications.cs','MainWindow.xaml.cs':'VI.Control/MainWindow.xaml.cs'}
for idx,r in enumerate(rows,1):
    r[0]=f'T{idx:03d}'
    r[3]='已确认：'+r[3]
    refs=[]
    for ref in r[2].split('; '):
        name,nums=ref.split(':',1)
        if name.startswith('D/'):name=D+name[2:]
        elif name.startswith('P/'):name=P+name[2:]
        elif name=='G':name=G
        elif not name.startswith('介入机器人/'):
            name=M+master.get(name,name)
        path=ROOT/name
        assert path.is_file(),str(path)
        length=len(path.read_text(encoding='utf-8-sig').splitlines())
        assert all(int(x)<=length for x in re.findall(r'\d+',nums)),(name,nums,length)
        refs.append(name+':'+nums)
    r[2]='; '.join(refs)
    assert not r[4] or re.fullmatch(r'\d+(?:\.\d+)?',r[4]),(r[3],r[4])
    assert len(r)==9
with (OUT/'timeouts.csv').open('w',encoding='utf-8-sig',newline='') as f:
    writer=csv.writer(f);writer.writerow(header);writer.writerows(rows)
print('rows',len(rows),'non_time_or_unbounded',sum(not r[4] for r in rows))
# 明确97个R1原始表行到导出条目的对应（不把D3关系表重复计入）。
keys={}
sec=''
for lineno,line in enumerate((ROOT/'docs/arch_survey_r1/D_超时与恢复.md').read_text(encoding='utf-8').splitlines(),1):
    if line.startswith('## D.'):sec=line.split()[1]
    if sec in ['D.1','D.2','D.4','D.5'] and line.startswith('| ') and not line.startswith('| 项目') and not line.startswith('| 对象'):
        cols=[x.strip() for x in line.strip('|').split('|')]
        keys[(sec,cols[0])]=lineno
assert len(keys)==97
# 用下方关键词映射而非依赖最终行号，便于父代理复核；每个表行至少命中一个导出名称。
patterns={
'D.1':['主循环等待','ADS连接阶段','ADS运行阶段','ADS硬失联|ADS快通道失败','PLC运行状态检查','PLC快照时钟停滞','ADS快周期','低频ADS调用者','低频ADS底层|低频ADS失败','手柄最大缓存|手柄init','手柄速度','手柄恢复新样本|手柄恢复静置','手柄断流后','单/双柄初始','connection hold','计划回退：','计划回退异步','计划回退提交保护|计划回退清旧状态','计划回退执行观察','计划回退RetryClear','计划回退无总期限','回退故障恢复清Req|回退故障恢复总','回退故障自动续做','晚到回退','急停恢复','屈曲恢复ClampWait','屈曲恢复dt|屈曲恢复速度帽','屈曲恢复ExitSync','标准启动','恢复启动','直接启动/紧急','轴4/注射器方向','力取零','力反馈保持样本|力保持释放','tracking夹爪','tracking PI','tracking启用命令','旧PLC回退','寻参记录','记录手柄历史|记录运动行|记录写线程'],
'D.2':['Zenoh认证挑战','Zenoh命令排队|Zenoh命令FIFO','控制域状态','Zenoh控制权','Zenoh方向续租','Zenoh Prepare','Zenoh StartControl','Zenoh ForceZero|Zenoh ForceSet|Zenoh ForceHold|Zenoh Cylinder|Zenoh YValve','Zenoh force发布','Zenoh router','定位臂快照|定位臂轴方向','定位臂末端点动','定位臂程序归零用户','定位臂程序归零状态','定位臂程序归零请求','定位臂ADS'],
'D.4':['Zenoh原生建连','主端认证后','主端状态帧','主端重认证','主端force帧','主端Session','主端普通按需|主端ForceHold','主端StartControl','主端ForceFeedback/Zero','主端PreparePosition','主端Cylinder|主端YValve','手柄宿主','HandleHost力帧|HandleHost零力','HandleHost采样','手柄USB','主端UI状态','定位臂UI','应用关闭'],
'D.5':['PLC MAIN主机|PLC handle主机','handle初始化|handle Enable错误','return state10','return state20','return state30','return state31','axis4 state','PLC init上电','PLC reset清错','clear_err稳定','err普通恢复|err Power未就绪|err Reset脉冲|err Power-not-ready','err限位退离','SelfCheck 轴','SelfCheck堵转','SelfCheck限位稳定','SelfCheck Power','SelfCheck回退','Axis15 reset','ArmCartesian heartbeat|ArmCartesian enable','ArmCartesian曲线|ArmCartesian到位','ArmCartJog alive|ArmCartJog enable','ArmCartJogGen','ArmManual/InjectorManual']}
notes=['# timeouts.csv 导出与完整性说明','',f'**已确认**：UTF-8 BOM、9列、{len(rows)}条数据。字段顺序与用户列清单一致；数值列仅含毫秒数字或空值。空值表示无独立时间上限/未设年龄期限/次数或距离等非ms阈值；具体阈值写入名称。来源为第一轮 D.1、D.2、D.4、D.5 共97个原始表行，D.3监视关系不重复计入（第一轮D_超时与恢复.md:13-73,112-159）。','',
'**已确认**：复合时间分项拆开；同一手柄5s缓存年龄不重复列两次；计数/容量/距离保留空ms并显式写单位。调用者忽略的50/100/250ms参数单列且不称实际deadline。','',
'**已确认**：按当前源码校正第一轮的表述：RemoteRobotLink.NowMs取TimeProvider.GetElapsedTime而非Environment.TickCount64（介入机器人/A主端/工程/master/VI.Service.Robot/Services/RemoteRobotLink.cs:67-85）；HandleHost没有落后时跳到now+5ms的分支（介入机器人/A主端/工程/master/VI.HandleHost/Program.cs:88-93）；记录手柄历史100ms与匹配容差10ms是两个阈值（介入机器人/B数据端/工程/ADS项目/main.cpp:1479-1484,4610-4611,4672-4676）；ArmCartesian租约/阶段超时用任务cycle_seconds累加，阶段10/30超过2s错误2007（介入机器人/C从端/250902/Untitled2/POUs/ArmCartesian.TcPOU:47,56-70,167-184）。','',
'**已确认**：导出脚本检查全部出处文件存在、行号不超范围、ms列可数值解析、每条原始表行至少有映射。它不重新证明第一轮结论；行为/现场边界仍沿用原附件及R2-5。','',
'## 97个原始表行到CSV的映射','', '| 第一轮章节/表行出处 | 对象 | CSV编号 |','|---|---|---|']
counts={}
for sec in ['D.1','D.2','D.4','D.5']:
    items=[(name,line) for (s,name),line in keys.items() if s==sec]
    assert len(items)==len(patterns[sec]),(sec,len(items),len(patterns[sec]))
    counts[sec]=len(items)
    for (name,line),pattern in zip(items,patterns[sec]):
        hits=[r[0] for r in rows if re.search(pattern,r[3])]
        assert hits,(sec,name,pattern)
        notes.append(f'| {sec}；docs/arch_survey_r1/D_超时与恢复.md:{line} | {name} | '+','.join(hits)+' |')
notes+=['',f'原始表行统计：{counts}；合计{sum(counts.values())}。','',
'未进行PLC/主端/ADS现场运行；CSV中的数值来自源码清单，不是实测响应时长。']
(OUT/'scratch/timeouts_export_notes.md').write_text('\n'.join(notes)+'\n',encoding='utf-8')
