"""Correct survey-only citations and finish explicit recording column lists."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'docs/arch_survey_r1'

doc=OUT/'A_工程全貌.md'
s=doc.read_text(encoding='utf-8-sig')
s=s.replace('B数据端/编译并启动数据端.cmd:1-7','B数据端/编译并启动数据端.cmd:1-6')
s=s.replace('B数据端/工程/build-and-run.ps1:1-39','B数据端/工程/build-and-run.ps1:1-38')
s=s.replace('`D/ads_communication.h:186-193`','`D/ads_communication.h:180`')
s=s.replace('DWORD/ULINT缓存','std::uint32_t/std::int64_t缓存')
s=s.replace('`D/ads_communication.h:44-45`','`D/ads_communication.h:31-33`')
s=s.replace('`P/init.TcPOU:38-42`','`P/init.TcPOU:74-80`')
s=s.replace('`D/main.cpp:5325-5329`','`D/main.cpp:5332-5336`')
s=s.replace('PLC init给默认','GVL声明给默认')
s=s.replace('`D/plc_io.cpp:336-344`、`D/startup_sequence.cpp:138-165,236-248`',
    '`D/plc_io.cpp:336-344`、`D/startup_sequence.cpp:138-165,236-248`、`介入机器人/C从端/250902/Untitled2/GVLs/G.TcGVL:132`')
s=s.replace('worker；读周期50ms，点动租约300ms，笛卡尔点动租约500ms，ADS单操作250ms。',
    'worker；读周期50ms，点动租约300ms，笛卡尔点动租约500ms；调用传250ms，但底层慢通道300ms且调用者等待无总上限。')
s=s.replace('独立环形记录器，读取两个Handle缓存，不参与运动控制。',
    '独立sampler+writer，读remote_handle_bridge两槽缓存，不参与运动控制。')
s=s.replace('`D/experiment_recorder.cpp:399-446`、`B数据端/工程/build-and-run.ps1:27-34`。**未知**',
    '`D/experiment_recorder.cpp:399-446`、`B数据端/工程/build-and-run.ps1:27-34`。**未知**')
if '### A.13.3 CSV完整列名' not in s:
    insert='''### A.13.3 CSV完整列名

以下字段顺序/拼写均为**已确认**，保持CSV原名。schema8的当前头与历史schema6/7不完全相同；不把新列误当历史文件已经记录。实验force/motion使用记录快照QPC换算elapsed_us，手柄独立采样使用GetTickCount64的elapsed_ms；它们不是main整拍周期计时。出处：`D/experiment_recorder.cpp:17-31`、`D/main.cpp:4608-4680`、`D/handle_velocity_recorder.h:22-40,64-94`。

| 文件/位置 | 完整列名 |
|---|---|
| force，1–4 | `sample_index,elapsed_us,sample_valid,calibrated_valid` |
| force，5–10 | `fn_1_raw_v,ft_1_raw_v,fn_1_zero_v,ft_1_zero_v,clean_force_n,clean_handle_torque_nm` |
| force，11–16 | `fn_2_raw_v,ft_2_raw_v,fn_2_zero_v,ft_2_zero_v,clean_force_2_n,clean_handle_torque_2_nm` |
| force，17–22 | `ff_582_out_f,ff_582_out_n,ff_587_out_f,ff_587_out_n,hold_582_active,hold_587_active` |
| motion，1–12 | `sample_index,elapsed_us,position_valid,axis1_from_left_mm,axis2_from_left_mm,axis3_from_left_mm,axis4_from_left_mm,axis5_from_left_mm,axis6_from_left_mm,axis7_from_left_mm,axis1_handle_valid,axis6_handle_valid` |
| events，1–4 | `event_index,elapsed_us,event_code,value` |
| handle_velocity，1–5 | `elapsed_ms,rx582_ms,valid582,rx587_ms,valid587` |
| handle_velocity，6–13 | `sdk582_0,sdk582_1,sdk587_0,sdk587_1,joint582_0_per_s,joint582_1_per_s,joint587_0_per_s,joint587_1_per_s` |

**已确认**：session.json在启动和停止时均可写，只有final才填写结束UTC/stop_reason及完整汇总；表A.13.2中的“会话结束写汇总”不是只在退出才创建JSON。`D/experiment_recorder.cpp:531-544,602-611,884-907`。另有exe旁`ads_fault_diagnostics.log`按故障边沿追加，属于诊断事件日志，无固定Hz、不是上述CSV记录器；路径`D/main.cpp:45-53,1006-1060`。生成物可以因磁盘/路径失败而不可用，main只报错继续；`D/main.cpp:953-964`。

'''
    s=s.replace('## A.14 ',insert+'## A.14 ',1)
doc.write_text(s,encoding='utf-8')

doc=OUT/'D_超时与恢复.md'
s=doc.read_text(encoding='utf-8-sig')
s=s.replace('| 项目 | 数值/时钟 | 到期动作/恢复 | 关系与证据（均已确认） |\n|---|---|---|---|',
    '| 项目 | 数值/时钟 | 到期动作/恢复 | 上下游关系（均已确认） | 出处 |\n|---|---|---|---|---|')
s=s.replace('| 超期Authenticate拒绝；新Hello建立新挑战 | `D/zenoh_service.cpp:688-728`；主端认证也等10s |',
    '| 超期Authenticate拒绝；新Hello建立新挑战 | 主端认证也等10s | `D/zenoh_service.cpp:688-728` |')
s=s.replace('| 消费时拒绝，累计expired；FIFO256满时丢最旧 | `D/zenoh_service.cpp:25-30,491-507,744-756` |',
    '| 消费时拒绝，累计expired；FIFO256满时丢最旧 | 主端命令等待期限另见D.4 | `D/zenoh_service.cpp:25-30,491-507,744-756` |')
s=s.replace('| 持有到显式Release或新认证会话替换；断流不撤认证租约 | `D/zenoh_service.cpp:722-733,773-785,962-980,1005` |',
    '| 持有到显式Release或新认证会话替换；断流不撤认证租约 | main另以handle hold门控动作 | `D/zenoh_service.cpp:722-733,773-785,962-980,1005` |')
s=s.replace('worker自身33ms运行不等于控制域状态更新','worker的33ms状态发送deadline不等于控制域状态更新')
doc.write_text(s,encoding='utf-8')

doc=OUT/'E_功能核查.md'
s=doc.read_text(encoding='utf-8-sig').replace('D/return_recovery.h:24-134','D/return_recovery.h:17-62')
doc.write_text(s,encoding='utf-8')

doc=OUT/'C_时间敏感动作.md'
s=doc.read_text(encoding='utf-8-sig')
s=s.replace('均在U/Untitled2.plcproj:33-70','均在 `U/Untitled2.plcproj:33-70`')
s=s.replace('不在U/Untitled2.plcproj:33-70','不在 `U/Untitled2.plcproj:33-70`')
s=s.replace('`existing_records_stats.json:1`','`docs/arch_survey_r1/scratch/existing_records_stats.json:1`')
sessions=['x64/Release/records/20261102/165217_auto','records/20261108/092851_auto','records/20261108/120537_auto','records/20261110/084418_auto']
for directory in sessions:
    # Expand each table row's same-directory CSV evidence without changing the directory label.
    rows=s.splitlines()
    for i,row in enumerate(rows):
        if row.startswith('| R') and (f'`{directory}/`' in row):
            row=row.replace(f'`{directory}/session.json:',f'`D/{directory}/session.json:')
            row=row.replace('`force.csv:',f'`D/{directory}/force.csv:')
            row=row.replace('`motion.csv:',f'`D/{directory}/motion.csv:')
            rows[i]=row
    s='\n'.join(rows)+'\n'
all_sess=lambda nums:'、'.join(f'`D/{p}/session.json:{nums}`' for p in sessions)
s=s.replace('各 `session.json:6`',all_sess('6'))
s=s.replace('各 `session.json:9`、`:10`',all_sess('9-10'))
s=s.replace('见各 `motion.csv:1`、`force.csv:1`',
    '见 `D/records/20261110/084418_auto/motion.csv:1`、`D/records/20261110/084418_auto/force.csv:1`（其余三套文件头见C.7.1）')
s=s.replace('各session的session.json:29（R1为session.json:28）',
    '`D/x64/Release/records/20261102/165217_auto/session.json:28`、`D/records/20261108/092851_auto/session.json:29`、`D/records/20261108/120537_auto/session.json:29`、`D/records/20261110/084418_auto/session.json:29`')
s=s.replace('R2/R3/R4 `events.csv:1` 后没有数据',
    'R2/R3/R4仅有events表头（`D/records/20261108/092851_auto/events.csv:1`、`D/records/20261108/120537_auto/events.csv:1`、`D/records/20261110/084418_auto/events.csv:1`）')
doc.write_text(s,encoding='utf-8')
print('A/C/D/E survey details corrected')
