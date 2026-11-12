#pragma once

#include <cstring>

// schema 9：value 的打包格式由事件码决定；低 16 位为原因/阶段/结果，高 16 位为上下文；详见实验记录说明。
enum class ExperimentEventCode : int
{
	ModeSwitch = 1,
	SpacingRecoveryStart = 2,
	ConnectionHoldEnter = 10,
	ConnectionHoldExit = 11,
	ControlActiveChanged = 12,
	EstopEnter = 13,
	EstopExit = 14,
	ReturnFaultEnter = 15,
	ReturnFaultCleared = 16,
	ReturnResume = 17,
	DualHandoffIntent = 18,
	PlannedReturnEnter = 19,
	PlannedReturnPhase = 20,
	PlannedReturnPrepare = 21,
	PlannedReturnCommit = 22,
	PlannedReturnClear = 23,
	PlannedReturnAck = 24,
	PlannedReturnDone = 25,
	PlannedReturnRetry = 26,
	PlannedReturnFailed = 27,
	PlannedReturnRebase = 28,
	SpacingRecoveryButtonRise = 29,
	SpacingRecoveryButtonFall = 30,
	SpacingRecoveryRejected = 31,
	SpacingRecoveryPhase = 32,
	SpacingRecoveryHoldCleared = 33,
	Axis1LeadArmed = 34,
	Axis1LeadCompleted = 35,
	Axis1LeadCleared = 36,
	PlcRestart = 37,
	HomingReuseResult = 38,
	ConnectionHoldSources = 39,
	PlannedReturnPossiblyStarted = 40,
	PlannedReturnPlcError = 41
};

enum ExperimentHoldSource : int
{
	HoldAds = 1,
	HoldHandle587 = 2,
	HoldHandle582 = 4,
	HoldSettling587 = 8,
	HoldSettling582 = 16,
	HoldInitialSync = 32
};

enum class ExperimentEventReason : int
{
	None = 0,
	InitialSync = 1,
	SelfCheckLost = 2,
	PlcRestart = 3,
	ReturnFault = 4,
	Estop = 5,
	SelfCheckSync = 6,
	HandleReinit = 7,
	ReturnRecovered = 8,
	AutoSync = 9,
	SpacingExit = 10,
	StartControl = 11,
	SyncFailed = 12,
	LeadReset = 20,
	LeadReverse = 21,
	LeadDisabled = 22,
	LeadInactive = 23,
	LeadHold = 24,
	LeadFault = 25,
	LeadEstop = 26,
	LeadRecovery = 27,
	LeadReturn = 28,
	RejectStartup = 40,
	RejectInactive = 41,
	RejectWindow = 42,
	RejectNoTravel = 43,
	RejectConnection = 44,
	RejectEstop = 45,
	RejectFault = 46,
	RebaseBeforeHandoff = 50,
	RebaseAfterHandoff = 51,
	HomingAccepted = 60,
	HomingDisabled = 61,
	HomingIncomplete = 62,
	HomingApplication = 63,
	HomingPosition = 64,
	HomingReadFailed = 65,
	HomingWriteFailed = 66,
	HomingPlcRejected = 67,
	HomingSubmitted = 68
};

enum class ExperimentCommandResult : int
{
	Submitted = 0,
	Succeeded = 1,
	Failed = 2,
	Unknown = 3,
	TimedOut = 4,
	QueueRejected = 5,
	ObservedAck = 6
};

enum class ExperimentCommandContext : int
{
	FaultRecovery = 5,
	HandoffOutput = 6
};

inline int experiment_event_value(int reason, int detail = 0)
{
	return reason | (detail << 16);
}

// 原有故障文本只用于查找稳定原因码，不参与控制判定。
inline int experiment_return_reason(const char* reason)
{
	const char* const reasons[] = {
		"计划回退提交前检测到PLC已有Busy，禁止覆盖正在执行的动作",
		"计划回退参数命令未能进入通信队列",
		"计划回退参数批量写入失败",
		"计划回退参数命令等待超过250 ms",
		"计划回退参数阶段出现未知通信命令",
		"计划回退请求批量置位失败",
		"计划回退请求命令等待超过250 ms",
		"计划回退提交保护输出确认超时",
		"计划回退请求未能进入通信队列",
		"计划回退提交前PLC状态清理超时",
		"计划回退在PLC启动前报告错误",
		"计划回退请求确认超时",
		"计划回退运动报错",
		"计划回退执行超时",
		"清理待重试请求期间检测到PLC已经启动动作",
		"安全重试前清Req命令未能进入通信队列",
		"安全重试前清除PLC请求失败",
		"安全重试前清Req命令等待超过250 ms",
		"安全重试前目标重新校验失败",
		"安全重试前PLC状态复位超时",
		"本体急停打断回退"
	};
	for (int i = 0; i < static_cast<int>(sizeof(reasons) / sizeof(reasons[0])); ++i)
		if (std::strcmp(reason, reasons[i]) == 0) return 200 + i;
	return 0;
}
