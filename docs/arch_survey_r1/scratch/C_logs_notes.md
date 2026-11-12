# C.4 现有日志可证明的时序事实

调查日期：2026-11-12；路径相对仓库根。只读取既有记录，没有启动数据端或连接 PLC。统计脚本：`docs/arch_survey_r1/scratch/analyze_existing_records.py:1`；完整统计：`docs/arch_survey_r1/scratch/existing_records_stats.json:1`。分位数采用 nearest-rank，median 为常规定义；单位在表内列明。

## 1. 四套历史 session

**已确认**：仓库当前运行工程的 records 目录共找到以下四套 session；全部 `utc_end=null`、`stop_reason=null`、`metrics_available=false`、`baseline_available=false`。它们没有可用的会话结束 ADS 统计，JSON 中的 null 不能解释为 0。当前记录器只在 final 写结束时间、结束原因，RTT 和计数也有可用性条件；因此“这些文件没有正常写出最终汇总”是**推断**，“为什么没有写完”是**未知**。证据：各 `session.json:6`、下表所列 ADS 元数据行；`介入机器人/B数据端/工程/ADS项目/experiment_recorder.cpp:884`、`:907`、`:934`、`:947`、`:951`、`:1018`。

| 编号 | session 目录（前缀均为 `介入机器人/B数据端/工程/ADS项目/`） | schema、UTC 开始 | 力/运动行数 | 数据有效性 | 原始证据 |
|---|---|---|---:|---|---|
| R1 | `x64/Release/records/20261102/165217_auto/` | 6；2026-11-02 08:52:17.015Z | 261 / 130 | 所有 sample/calibrated/position/两手柄 valid 均0 | `x64/Release/records/20261102/165217_auto/session.json:2`、`:5`、`:23`；同目录 `force.csv:1`、`:262`，`motion.csv:1`、`:131` |
| R2 | `records/20261108/092851_auto/` | 7；2026-11-08 01:28:51.091Z | 476,992 / 238,464 | 所有 sample/calibrated/position valid 均0；两个 handle_valid 各54,905行1、183,559行0 | `records/20261108/092851_auto/session.json:2`、`:5`、`:24`；同目录 `force.csv:1`、`:476993`，`motion.csv:1`、`:238465` |
| R3 | `records/20261108/120537_auto/` | 7；2026-11-08 04:05:37.984Z | 169 / 85 | 所有 sample/calibrated/position/两手柄 valid 均0 | `records/20261108/120537_auto/session.json:2`、`:5`、`:24`；同目录 `force.csv:1`、`:170`，`motion.csv:1`、`:86` |
| R4 | `records/20261110/084418_auto/` | 8；2026-11-10 00:44:18.291Z | 2,203 / 1,101 | 所有 sample/calibrated/position/两手柄 valid 均0；新增4列 ff 输出全0 | `records/20261110/084418_auto/session.json:2`、`:5`、`:24`；同目录 `force.csv:1`、`:2204`，`motion.csv:1`、`:1102` |

表内原始证据路径为避免超宽省略同一工程前缀；完整路径同时保存在 `existing_records_stats.json:1`。**已确认**：脚本扫描了全部 CSV 行而非只看文件头；四套所有力原始/清零/清洁力/扭矩列及七轴位置列都没有有限数值（NaN）；R4 的四列反馈输出为有限0。证据：`docs/arch_survey_r1/scratch/analyze_existing_records.py:43`（完整 reader）、`:45`（有效位计数）、`:50`（逐列数值扫描）；各原始 CSV 引用见上表。

## 2. 可统计的记录间隔（不是 main 周期）

**已确认**：以下数值是 CSV 相邻 `elapsed_us` 相减后转换为 ms 的分布。R1–R4 都声明力100Hz、运动50Hz；当前代码逐 ADS 记录快照写 force，仅选择偶数 attempt_sequence 写 motion，并将 `record_snapshot.qpc_ticks` 传给记录器。故它们至多可用于描述文件内记录的时间间隔；不能当作主循环 wait→publish 整拍耗时，也不能将间隔中的约1s空洞直接定性为线程丢拍。证据：各 `session.json:9`、`:10`；`介入机器人/B数据端/工程/ADS项目/main.cpp:4608`、`:4615`、`:4616`、`:4650`、`:4653`；`介入机器人/B数据端/工程/ADS项目/experiment_recorder.cpp:711`、`:739`。历史 schema 对应旧版源代码的确切构建哈希未记录，故旧文件具体采样实现是**未知**。

| 会话/文件 | 相邻间隔数 | 最小ms | 中位数ms | p95 ms | p99 ms | 最大ms | 均值ms | 原始时间覆盖（首/末elapsed，s） |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| R1 force | 260 | 0.006 | 10 | 10 | 10.053 | 1012.126 | 17.562 | 0.789781 / 5.355776 |
| R1 motion | 129 | 15.979 | 20 | 20 | 1010.873 | 1012.132 | 35.318 | 0.789797 / 5.345776 |
| R2 force | 476,991 | 0.007 | 10 | 10.258 | 13.610 | 1894.202 | 19.710 | 0.809929 / 9402.325303 |
| R2 motion | 238,463 | 0.241 | 20 | 20.280 | 1013.594 | 1894.225 | 39.423 | 0.809952 / 9401.685303 |
| R3 force | 168 | 0.014 | 10 | 10.004 | 10.059 | 1015.527 | 15.893 | 0.002088 / 2.672088 |
| R3 motion | 84 | 14.459 | 20 | 20.028 | 1015.541 | 1015.541 | 31.786 | 0.002088 / 2.672088 |
| R4 force | 2,202 | 0.011 | 10 | 10.003 | 10.594 | 1021.647 | 19.446 | 0.409329 / 43.229240 |
| R4 motion | 1,100 | 10.771 | 20 | 20.003 | 1015.819 | 1021.665 | 38.918 | 0.409355 / 43.219238 |

统计依据是上表对应全部 CSV 行；计算实现 `docs/arch_survey_r1/scratch/analyze_existing_records.py:23`、`:44`、`:64`，机器可读统计 `docs/arch_survey_r1/scratch/existing_records_stats.json:1`。不报告主循环周期分布，原因不是“没有任何日志”，而是所找到日志不含每轮主循环的起止时间，也未保存可识别的成功 ADS 快照序列。

## 3. 三份既有 ADS 诊断日志

| 文件 | 总行数/CONNECT行数 | 具体失败 | 有 RTT 字段 | 确定程度/证据 |
|---|---:|---|---:|---|
| `介入机器人/B数据端/工程/ADS项目/x64/Release/ads_fault_diagnostics.log` | 3 / 3 | 每行 local192.168.0.160.1.1:851 read-state错误6；fallback169.254.119.135.1.1:851错误1861；timeout1000ms | 0 | **已确认**：该文件`:1`、`:3`，扫描脚本`:84` |
| `介入机器人/B数据端/工程/ADS项目/x64/Debug/ads_fault_diagnostics.log` | 4,687 / 4,687 | 每行同上两项错误与1000ms | 0 | **已确认**：该文件`:1`、`:4687`，扫描脚本`:84` |
| `ADS项目/ADS项目/x64/Debug/ads_fault_diagnostics.log` | 40 / 40 | 前8行同上；后32行包含 AdsPortOpenEx failed | 0 | **已确认**：该文件`:1`、`:8`、`:9`、`:40`，扫描脚本`:84` |

**已确认**：这里的 `tick_ms` / `failure_tick_ms` / `seq` 是失败事件序列；日志里没有正常成功通信周期的 RTT 序列，不能将它的约2s重试间隔称为 ADS 往返耗时。`timeout_ms=1000` 是请求配置值，也不是实测 RTT。各文件原始行见上表；完整文件哈希和分类计数保存在 `existing_records_stats.json:1`。

## 4. 对 C.4 四项问题的结论

| 要求 | 本轮结果 | 确定程度与缺失材料 |
|---|---|---|
| 主循环周期分布 | 未取得；上表仅记录文件间隔 | **未知**：需要包含每拍主循环起止时刻或等价测量的日志；当前CSV头没有这些字段，见各 `motion.csv:1`、`force.csv:1` |
| 正常 ADS 往返时间分布 | 未取得 | **未知**：session RTT 为null，三份diagnostic只有CONNECT失败；需要成功快通道的RTT样本；各 session行29（R1行28）、各diagnostic上述行 |
| 快照缺失次数 | 未取得；不能把所有 position_valid=0 直接算成缺失次数 | **未知**：没有最终 ADS 计数/基线；session failed_cycles和snapshot_queue字段为null；例如 `介入机器人/B数据端/工程/ADS项目/records/20261110/084418_auto/session.json:30`、`:37` |
| 一次换手各阶段实际耗时 | 未取得 | **未知**：R2/R3/R4 `events.csv:1` 后没有数据，R1无events文件，位置全无效；需要带planned_return/dual_handoff阶段时间和对应PLC反馈的完整实际记录 |

**已确认**：仓库 `介入机器人/docs/`、`验证记录/` 还有大量明确为 `target=offline` 的注入故障日志，本轮离线测试也可生成虚拟时钟换手周期数。它们证明模型在指定输入下的行为，不是上述四项现场时序实测；例如 `docs/arch_survey_r1/scratch/tests_native/ads_diagnostics_test.log:1`、`docs/arch_survey_r1/scratch/tests/v5_st/results.log:2`。未将这些数值混入实际时序分布。
