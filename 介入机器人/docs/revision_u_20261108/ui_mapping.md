# 逐控件对应关系（自动断言输出）

本表由 VI.Control.Tests 的冻结基线映射检查输出。IsEnabled 同时比较本地与祖先绑定；电缸命令的祖先由 ItemsControl 改为 UserControl，另在运行时断言仍为同一个 CylinderCommand 与同一个 CylinderItem。

| 原文件 | 原控件 | 新控件 | Command或事件 | Tag | CommandParameter | IsEnabled | ToolTip | 结果 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| InterventionPanel.xaml | ConnectButton | ConnectButton | {Binding ConnectCommand} |  |  |  |  | 通过 |
| InterventionPanel.xaml | CatheterPosition | CatheterPosition |  |  |  | {Binding CanOperate} |  | 通过 |
| InterventionPanel.xaml | WirePosition | WirePosition |  |  |  | {Binding CanOperate} |  | 通过 |
| InterventionPanel.xaml | 进入器械准备位置 | Prepare | {Binding PrepareCommand} |  |  | {Binding CanOperate} |  | 通过 |
| InterventionPanel.xaml | 开始控制（手柄） | StartControl | {Binding StartControlCommand} |  |  | {Binding CanOperate} & {Binding CanStartControl} |  | 通过 |
| InterventionPanel.xaml | CatheterForceFeedback | CatheterForceFeedback | {Binding CatheterForceFeedbackCommand} |  |  | {Binding CanOperate} |  | 通过 |
| InterventionPanel.xaml | GuidewireForceFeedback | GuidewireForceFeedback | {Binding GuidewireForceFeedbackCommand} |  |  | {Binding CanOperate} |  | 通过 |
| InterventionPanel.xaml | ForceZero | ForceZero | {Binding ForceZeroCommand} |  |  | {Binding CanOperate} | 器械保持空载，取当前连续3帧ADS数据的平均值 | 通过 |
| InterventionPanel.xaml | ForceHold | ForceHold | {Binding ForceHoldCommand} |  |  | {Binding CanOperate} | 开启时启用轴向力低通、虚拟阻尼和换手保持；关闭时旁路低通与阻尼。保持期间锁存当前力和扭矩，换手完成后对应轴沿当前方向运动2 mm，再平滑恢复实时反馈 | 通过 |
| InterventionPanel.xaml | {Binding Title} | Cylinder1 | {Binding DataContext.CylinderCommand, RelativeSource={RelativeSource AncestorType=UserControl}} |  | {Binding} | {Binding CanOperate} | {Binding Caption, Mode=OneWay} \|  ·  \| {Binding CurrentText, Mode=OneWay} | 通过 |
| InterventionPanel.xaml | {Binding Title} | Cylinder2 | {Binding DataContext.CylinderCommand, RelativeSource={RelativeSource AncestorType=UserControl}} |  | {Binding} | {Binding CanOperate} | {Binding Caption, Mode=OneWay} \|  ·  \| {Binding CurrentText, Mode=OneWay} | 通过 |
| InterventionPanel.xaml | {Binding Title} | Cylinder3 | {Binding DataContext.CylinderCommand, RelativeSource={RelativeSource AncestorType=UserControl}} |  | {Binding} | {Binding CanOperate} | {Binding Caption, Mode=OneWay} \|  ·  \| {Binding CurrentText, Mode=OneWay} | 通过 |
| InterventionPanel.xaml | {Binding Title} | Cylinder4 | {Binding DataContext.CylinderCommand, RelativeSource={RelativeSource AncestorType=UserControl}} |  | {Binding} | {Binding CanOperate} | {Binding Caption, Mode=OneWay} \|  ·  \| {Binding CurrentText, Mode=OneWay} | 通过 |
| InterventionPanel.xaml | 关闭 Y 阀 | YValve | {Binding YValveCommand} |  |  | {Binding CanOperate} |  | 通过 |
| InterventionPanel.xaml | Axis4Reverse | Axis4Reverse | HoldButton_Down; HoldButton_Up; HoldButton_Up | A4 | -1 | {Binding CanOperate} |  | 通过 |
| InterventionPanel.xaml | Axis4Forward | Axis4Forward | HoldButton_Down; HoldButton_Up; HoldButton_Up | A4 | 1 | {Binding CanOperate} |  | 通过 |
| InterventionPanel.xaml | Injector1Push | Injector1Push | HoldButton_Down; HoldButton_Up; HoldButton_Up | 1 | 1 | {Binding CanOperate} |  | 通过 |
| InterventionPanel.xaml | 拉 | Injector1Pull | HoldButton_Down; HoldButton_Up; HoldButton_Up | 1 | -1 | {Binding CanOperate} |  | 通过 |
| InterventionPanel.xaml | 推 | Injector2Push | HoldButton_Down; HoldButton_Up; HoldButton_Up | 2 | 1 | {Binding CanOperate} |  | 通过 |
| InterventionPanel.xaml | 拉 | Injector2Pull | HoldButton_Down; HoldButton_Up; HoldButton_Up | 2 | -1 | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | 总使能 | ArmManualEnable | ArmManualEnable_Click |  |  | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | ArmSpeedFast | ArmSpeedFast | ArmSpeed_Checked |  |  |  | 平移 30 mm/s，旋转 1.2°/s，Z 轴 3 mm/s | 通过 |
| PositioningArmPanel.xaml | ArmSpeedMedium | ArmSpeedMedium | ArmSpeed_Checked |  |  |  | 平移 15 mm/s，旋转 0.8°/s，Z 轴 2.5 mm/s | 通过 |
| PositioningArmPanel.xaml | ArmSpeedSlow | ArmSpeedSlow | ArmSpeed_Checked |  |  |  | 平移 5 mm/s，旋转 0.4°/s，Z 轴 1.25 mm/s | 通过 |
| PositioningArmPanel.xaml | CartesianReverse | CartesianReverse | ArmCartesian_Down; ArmCartesian_Up; ArmCartesian_Up | 1 | -1 |  |  | 通过 |
| PositioningArmPanel.xaml | CartesianForward | CartesianForward | ArmCartesian_Down; ArmCartesian_Up; ArmCartesian_Up | 1 | 1 |  |  | 通过 |
| PositioningArmPanel.xaml | ProgramHome | ProgramHome | ArmProgramZero_Click |  |  | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | 停止 | ArmStop | ArmStop_Click |  |  |  |  | 通过 |
| PositioningArmPanel.xaml | 上电 | 上电 | ArmAxisEnable_Click | 1 |  | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | 复位 | 复位 | ArmAxisReset_Click | 1 |  | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | − | − | ArmJogButton_Down; ArmJogButton_Up; ArmJogButton_Up | 1 | -1 | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | Axis1Forward | Axis1Forward | ArmJogButton_Down; ArmJogButton_Up; ArmJogButton_Up | 1 | 1 | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | 上电 | 上电 | ArmAxisEnable_Click | 2 |  | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | 复位 | 复位 | ArmAxisReset_Click | 2 |  | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | − | − | ArmJogButton_Down; ArmJogButton_Up; ArmJogButton_Up | 2 | -1 | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | + | + | ArmJogButton_Down; ArmJogButton_Up; ArmJogButton_Up | 2 | 1 | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | 上电 | 上电 | ArmAxisEnable_Click | 3 |  | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | 复位 | 复位 | ArmAxisReset_Click | 3 |  | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | − | − | ArmJogButton_Down; ArmJogButton_Up; ArmJogButton_Up | 3 | -1 | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | + | + | ArmJogButton_Down; ArmJogButton_Up; ArmJogButton_Up | 3 | 1 | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | 上电 | 上电 | ArmAxisEnable_Click | 4 |  | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | 复位 | 复位 | ArmAxisReset_Click | 4 |  | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | − | − | ArmJogButton_Down; ArmJogButton_Up; ArmJogButton_Up | 4 | -1 | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | + | + | ArmJogButton_Down; ArmJogButton_Up; ArmJogButton_Up | 4 | 1 | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | 上电 | 上电 | ArmAxisEnable_Click | 5 |  | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | 复位 | 复位 | ArmAxisReset_Click | 5 |  | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | − | − | ArmJogButton_Down; ArmJogButton_Up; ArmJogButton_Up | 5 | -1 | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | + | + | ArmJogButton_Down; ArmJogButton_Up; ArmJogButton_Up | 5 | 1 | {Binding CanOperate} |  | 通过 |
| PositioningArmPanel.xaml | 末端模式选择 + 原方向按钮 | CartesianReverse | ArmCartesian_Down / ArmCartesian_Up / LostMouseCapture | 1 | -1 | 无 | 无 | 通过 |
| PositioningArmPanel.xaml | 末端模式选择 + 原方向按钮 | CartesianForward | ArmCartesian_Down / ArmCartesian_Up / LostMouseCapture | 1 | 1 | 无 | 无 | 通过 |
| PositioningArmPanel.xaml | 末端模式选择 + 原方向按钮 | Cartesian2Reverse | ArmCartesian_Down / ArmCartesian_Up / LostMouseCapture | 2 | -1 | 无 | 无 | 通过 |
| PositioningArmPanel.xaml | 末端模式选择 + 原方向按钮 | Cartesian2Forward | ArmCartesian_Down / ArmCartesian_Up / LostMouseCapture | 2 | 1 | 无 | 无 | 通过 |
| PositioningArmPanel.xaml | 末端模式选择 + 原方向按钮 | Cartesian3Reverse | ArmCartesian_Down / ArmCartesian_Up / LostMouseCapture | 3 | -1 | 无 | 无 | 通过 |
| PositioningArmPanel.xaml | 末端模式选择 + 原方向按钮 | Cartesian3Forward | ArmCartesian_Down / ArmCartesian_Up / LostMouseCapture | 3 | 1 | 无 | 无 | 通过 |
| PositioningArmPanel.xaml | 末端模式选择 + 原方向按钮 | Cartesian4Reverse | ArmCartesian_Down / ArmCartesian_Up / LostMouseCapture | 4 | -1 | 无 | 无 | 通过 |
| PositioningArmPanel.xaml | 末端模式选择 + 原方向按钮 | Cartesian4Forward | ArmCartesian_Down / ArmCartesian_Up / LostMouseCapture | 4 | 1 | 无 | 无 | 通过 |
| PositioningArmPanel.xaml | 末端模式选择 + 原方向按钮 | Cartesian5Reverse | ArmCartesian_Down / ArmCartesian_Up / LostMouseCapture | 5 | -1 | 无 | 无 | 通过 |
| PositioningArmPanel.xaml | 末端模式选择 + 原方向按钮 | Cartesian5Forward | ArmCartesian_Down / ArmCartesian_Up / LostMouseCapture | 5 | 1 | 无 | 无 | 通过 |
| InterventionPanel.xaml | RecoveryText | MainWindow 提示行 | Text={Binding RecoveryText} |  |  | 只读显示 |  | 通过 |
| InterventionPanel.xaml | OperateHintText | MainWindow 提示行 | Text={Binding OperateHintText} |  |  | 只读显示 |  | 通过 |
| InterventionPanel.xaml | LastWarning | MainWindow 提示行 | Text={Binding LastWarning} |  |  | 只读显示 |  | 通过 |
| InterventionPanel.xaml | Force582F | 原绑定迁入对应卡片/行 | Text={Binding Force582F} |  |  | 只读显示 |  | 通过 |
| InterventionPanel.xaml | Force582N | 原绑定迁入对应卡片/行 | Text={Binding Force582N} |  |  | 只读显示 |  | 通过 |
| InterventionPanel.xaml | Force587F | 原绑定迁入对应卡片/行 | Text={Binding Force587F} |  |  | 只读显示 |  | 通过 |
| InterventionPanel.xaml | Force587N | 原绑定迁入对应卡片/行 | Text={Binding Force587N} |  |  | 只读显示 |  | 通过 |
| InterventionPanel.xaml | PrepareStatusText | 原绑定迁入对应卡片/行 | Text={Binding PrepareStatusText} |  |  | 只读显示 |  | 通过 |
| InterventionPanel.xaml | PrepareError | 原绑定迁入对应卡片/行 | Text={Binding PrepareError} |  |  | 只读显示 |  | 通过 |
| InterventionPanel.xaml | FfCaption | 原绑定迁入对应卡片/行 | Text={Binding FfCaption} |  |  | 只读显示 |  | 通过 |
| InterventionPanel.xaml | YValveCaption | 原绑定迁入对应卡片/行 | Text={Binding YValveCaption} |  |  | 只读显示 |  | 通过 |
| InterventionPanel.xaml | Axis4Text | 原绑定迁入对应卡片/行 | Text={Binding Axis4Text} |  |  | 只读显示 |  | 通过 |
| InterventionPanel.xaml | Injector1Text | 原绑定迁入对应卡片/行 | Text={Binding Injector1Text} |  |  | 只读显示 |  | 通过 |
| InterventionPanel.xaml | Injector2Text | 原绑定迁入对应卡片/行 | Text={Binding Injector2Text} |  |  | 只读显示 |  | 通过 |
| PositioningArmPanel.xaml | ArmFeedbackText | 原绑定迁入对应卡片/行 | Text={Binding ArmFeedbackText} |  |  | 只读显示 |  | 通过 |
| PositioningArmPanel.xaml | ArmCartesianJogStatusText | 原绑定迁入对应卡片/行 | Text={Binding ArmCartesianJogStatusText} |  |  | 只读显示 |  | 通过 |
