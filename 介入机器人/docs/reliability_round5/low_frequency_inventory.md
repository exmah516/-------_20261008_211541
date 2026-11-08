# R2 搬迁前清单

基线：806036a8f0b65a2d6557b3f7d47bcf5f65990acf。以下清单在修改通信线程之前完成。

| 调用方 | 顺序依赖核对 | 去留 |
|---|---|---|
| ArmManualAdsService：状态批读、dirty 批写、曲线数据/提交/取消/心跳 | 调用方串行等待返回后再提交下一请求；不依赖本体某一快拍，只要求定位臂自身写入先后 | 全部迁入独立端口；调用方及250 ms API参数不变 |
| plc_io read/write_v_limit；startup_sequence、紧急撤出 | 先读备份再写比例/恢复，函数返回后才推进业务 | 低频；保持同步返回的先后关系 |
| 自检目标写入、selfcheck_start_req | main先等待目标写成功，再写启动请求 | 低频；同一调用线程仍保持先目标后请求 |
| main 电缸手动初值读取、PLC AppName | 独立查询，无快拍相对顺序 | 低频 |
| 本轮新增 R9：PLC 应用名、轴健康/NC绝对位置、两组恢复数组/status、最后置req、回执status/reason | 健康/位置只用于复用判定；数组及req由同一调用线程同步先后提交，回执必须在提交后读取，不要求同一快线程 | 全部低频；应用名在复用待判定时可于_self_check等待态读取，普通启动仍沿原门控 |
| refresh_coordinates / request_coordinate_refresh | 快照本身同批读取init_pos/leftlimit；显式刷新等待结果，后台刷新只更新缓存 | 显式/后台刷新进入低频；首次连接握手所需缓存初始化保留在快线程开始运行之前 |
| plc_io write_refer：motion_sync、guidewire_mode、startup_sequence | 同步调用返回后才确认rebase/进入下一业务阶段；原实现也在已发布快拍之后独立写，不与核心Sum事务绑定 | 进入低频；等待返回的语义保持 |
| plc_io write_cylinder_values、startup_loading_ready、startup_smoothing_bypass、clear_plc_reinit_req | 同步返回决定下一业务阶段，不依赖具体快拍 | 低频 |
| plc_io 旧按名回退读/Prepare/Commit/Clear（含guidewire/motion_sync） | 按函数次序读写，等待结果；现行协调器不用此路径提交专用任务 | 低频；新协调器专用队列不搬迁 |
| 快照/位置、力读取 | 运动和记录必须消费同一快照；运行期不走read/read_sum | 保留快线程ByHandle批读与缓存消费 |
| 常规输出、夹爪交接代次、心跳/会话、watchdog恢复 | 交接要求refer、Req清除、夹爪输出同一核心写代次 | 保留快线程核心SumWrite |
| 计划回退专用Prepare/Commit/Clear队列 | Clear屏障及possibly_started判定与核心写结果绑定 | 保留快线程 |

检查入口：`rg -n 'read_sum|write_sum|refresh_coordinates|ads_communication\.read|ads_communication\.write' B数据端/工程/ADS项目`，以及`plc_io.cpp`四个按名转发函数的全部调用。没有低频调用方要求在指定快拍后由同一线程执行；以上核心事务明确保留。
