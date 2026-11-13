#pragma once

#include <cstring>

// schema 10：value格式由事件码决定；50至57的耗时事件均为完整毫秒值，详见实验记录说明。
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
	PlannedReturnPlcError = 41,
	HandoffRequest = 42,
	HandoffAck = 43,
	HandoffDone = 44,
	HandoffTakeover = 45,
	HandoffRequestTimeout = 47,
	HandoffPhase10Ms = 50,
	HandoffPhase20Ms = 51,
	HandoffPhase30Ms = 52,
	HandoffPhase40Ms = 53,
	HandoffPhase50Ms = 54,
	HandoffAxis1MotionMs = 55,
	HandoffAxis6MotionMs = 56,
	HandoffReservedMs = 57
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

inline int experiment_event_value(int reason, int detail = 0)
{
	return reason | (detail << 16);
}

