# 阶段 0 离线行为基线

在仓库根目录运行（Python 3、Node.js、VS C++ 工具链）：

```powershell
python tools/refactor_baseline/run.py --events --output docs/refactor_stage0/logs/timeline_final_check
python tools/refactor_baseline/run_behavior_probes.py
node '介入机器人/B数据端/工程/ADS项目/tools/run_offline_tests.cjs' all 'docs/refactor_stage0/logs/all_final_check'
```

`run.py` 把当前生产 main.cpp 完整主循环和控制模块编译为离线程序。时钟、ADS、Zenoh、定位臂和记录文件边界使用本目录替身；不启动 ADS.exe，不加载设备 SDK，也不连接 PLC。每拍末输出 7 轴 refer、4 电缸、fast、请求/清除掩码、租约和 49 个主要状态量。double 使用无损十六进制文本。

`golden/` 是删除前提交 `26623db` 固定的 34 个场景、27,197 拍预期值；源哈希在 `golden/manifest.json`。每次从当前源码重新构建并逐字节比较，`verify_coverage.py` 同时检查输入确实命中所需阶段。禁止用删除后的输出更新黄金。`--record` 只允许生成不存在的预期文件，已有文件会报错。

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

`--events` 额外执行两个只用于事件的输入。36 个场景的真实主循环事件由 `verify_events.py` 核对：顺序、原因、快照序号、全部34个事件码、五组状态的逐拍重建；这些额外输入不进入旧黄金集。真实 CSV writer 的 schema9/列/入队/drain/错误另由 recording_feedback_test 验证。

`run_behavior_probes.py` 沿用 R2-8 的生产片段抽取，原51条中4条仅检查已删除的单手柄分支，已随第3.1.1组删除；其余47条保留。正式黄金从未使用单手柄初始化，无需调整输入。

边界：内存PLC按100 Hz的离散模型推进，通知/代次/错误由夹具控制；没有模拟 TwinCAT 1 ms FB轨迹或真实通知并发、网络延迟、机械动作时间。输出相等证明这些确定输入下的数据端行为相等；不能替代 PLC 编译或现场验收。`run_baseline.cjs` 是早期标准套件清单入口，不是逐拍黄金生成器。
