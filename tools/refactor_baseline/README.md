# 阶段 1 离线行为基线

在仓库根目录运行（Python 3、Node.js、VS C++ 工具链）：

```powershell
python tools/refactor_baseline/run.py --events
python tools/refactor_baseline/run_behavior_probes.py
python tools/handoff_tests/run_st.py
python tools/handoff_tests/compare_terminal.py
python tools/handoff_tests/run_robust.py
node '介入机器人/B数据端/工程/ADS项目/tools/run_offline_tests.cjs' all 'docs/refactor_stage1/logs/offline_final'
```

`run.py` 把当前生产 main.cpp 完整主循环和控制模块编译为离线程序，并翻译当前 Handoff、handle 全文，由内存 NC 功能块推进 PLC 的 1 ms 扫描。时钟、ADS、Zenoh、定位臂和记录文件边界使用替身；不启动 ADS.exe，不加载设备 SDK，也不连接 PLC。每拍末输出 7 轴 refer、4 电缸、租约和 49 个主要状态量。历史 CSV 中已删除的 fast、请求/清除掩码列填零，保留原列序；double 使用无损十六进制文本。

`golden/` 是阶段 0 删除前提交 `f3612a1` 固定的 34 个场景、27,197 拍预期值；源哈希在 `golden/manifest.json`。原黄金未修改。阶段 1 按实际首个换手触发拍划分比较范围：9 个未换手场景共 7,200 拍整段每列文本相等（仅忽略Git检出的行尾转换）；其余 25 个场景比较同一触发拍之前的每列，共同覆盖 8,143 拍。`verify_baseline.py` 遇到范围内差异立即失败，`verify_coverage.py` 检查输入确实命中所需阶段。禁止用当前输出更新已有黄金；`--record` 会拒绝覆盖。

`compare_terminal.py` 从阶段 0 固定提交 `d2bb5fc` 重建旧主循环，在两套程序中于首个事务触发后保持手柄关节量不变，其它输入规则不变。比较 25 场景接管后的 40 项状态：位置容差 0.01 mm，布尔和电缸精确相等。它不宣称原连续推动输入在换手后的同拍输出相同。阶段 0 漏收的三个历史头文件按当时 manifest 的 SHA256 恢复到 `tools/handoff_tests/stage0_support/`，该目录内的 manifest 保存原始哈希，仅用于旧测试重建。

| 任务范围 | 场景/断言来源 |
|---|---|
| 普通递送/撤出、窗口/g/镜像/旋转 | `follow_delivery`、`follow_retraction`；dual_control_test 保留断言 |
| 先行武装/10 mm 多走2 mm/中断/仅轴6换手 | `lead_hold`、`lead_axis6`、`handoff_merge`；R2 行为探针 |
| 轴4联动 | `axis4`；dual_control_test |
| 换手四种意图、到位/低速等待 | `handoff_axis1/active/passive/merge`；dual_control_test |
| Dual 轴1/轴6/双腿，完成/重试/可能已启动/失败续做 | `return_axis1/axis6/both`，`return_retry/possible/fault` 各三种腿组合；`return_estop` |
| 屈曲恢复、保持后重新按键、ExitSync | `spacing`、`spacing_hold`；behavior_probes 的47条保留断言 |
| ADS/手柄三档/500 ms静置/epoch、急停/寻参 | `ads_hold`、`handle_gap_short/rebase/disconnect`、`device_epoch`、`restart/accept/reject` |
| 公共StartControl、电缸/Y阀/注射器/轴4租约 | 每场景初始公共命令，`manual`；标准离线cylinder/recovery测试 |
| 力低通/换手保持/2 mm或400 ms/300 ms混合/取零 | `feedback`；recording_feedback_test 的标定/双向保持/取零断言 |

`--events` 额外执行三个只用于事件的输入。37 个场景的真实主循环事件由 `verify_events.py` 核对：顺序、原因、快照序号、全部 39 个事件码、四组状态的逐拍重建，共 316 项检查；这些额外输入不进入旧黄金集。真实 CSV writer 的 schema10/列/入队/drain/错误另由 recording_feedback_test 验证。

`run_behavior_probes.py` 沿用 R2-8 的生产片段抽取，原51条中4条仅检查已删除的单手柄分支，已随第3.1.1组删除；其余47条保留。正式黄金从未使用单手柄初始化，无需调整输入。

`run_st.py` 检查三种腿组合的毫秒等待、失败/中断收尾、对齐预送、参考冻结、无跳变释放和握手。`run_robust.py` 运行 10 个数据端与 ST 闭环场景、78 项检查，包含主循环暂停 2 s、写失败同号重发、保持、急停和进程重启。

边界：ST 使用生产时序，NC 运动功能块由内存替身提供 Done/Error/CommandAborted、位置及速度；通知/代次/错误由夹具控制。没有模拟真实伺服轨迹、通知并发、网络或夹爪机械时间。暂停主循环时 PLC 继续扫描；全机断网触发看门狗属于另一条中断路径，已单列 ST 检查。离线结果不能替代 PLC 编译或现场验收。`run_baseline.cjs` 是早期标准套件清单入口，不是逐拍黄金生成器。
