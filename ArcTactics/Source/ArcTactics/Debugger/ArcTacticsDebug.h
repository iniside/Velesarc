// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Actions/ArcTacticsAction.h"
#include "CoreMinimal.h"
#include "Debugger/ArcTacticsTrace.h"
#include "Decision/ArcTacticsInstance.h"
#include "GameplayTagContainer.h"
#include "Mass/ArcTacticsInstanceHandle.h"
#include "Mass/ArcTacticsOption.h"
#include "Mass/EntityHandle.h"

class UWorld;
struct FArcTacticsInstanceSlot;
struct FArcUtilityConsiderationSnapshot;

struct FArcTacticsDebugNode
{
	FName Name;
	FString TypeName;
};

struct FArcTacticsDebugStep
{
	FName Name;
	FString TypeName;
	bool bCurrent = false;
};

struct FArcTacticsDebugActiveStep
{
	FName Name;
	FString TypeName;
	bool bEntered = false;
};

struct FArcTacticsDebugBranch
{
	FName Name;
	bool bSelector = false;
	int32 SourceIndex = INDEX_NONE;
	FString TypeName;
	FString SelectorTypeName;
	int32 Priority = 0;
	float Weight = 0.0f;
	float Cooldown = 0.0f;
	EArcTacticsCooldownStart CooldownStart = EArcTacticsCooldownStart::OnEnd;
	double CooldownRemaining = 0.0;
	bool bActive = false;
	TArray<FName> Considerations;
	TArray<FArcTacticsDebugStep> Steps;
	TArray<FArcTacticsDebugActiveStep> ActiveSteps;
};

struct FArcTacticsDebugActiveAction
{
	bool bSet = false;
	FString Package;
	FName Name;
	FString TypeName;
	uint32 Serial = 0;
	bool bFromSelector = false;
	int32 SourceIndex = INDEX_NONE;
	FArcTacticsCandidateIdentity Target;
	bool bHasTargetLocation = false;
	FVector TargetLocation = FVector::ZeroVector;
	int32 Priority = 0;
	float Score = 0.0f;
	bool bEntered = false;
	EArcTacticsActionTickLane Lane = EArcTacticsActionTickLane::None;
	bool bBlocksDecision = false;
	double WakeTime = 0.0;
	int32 StepIndex = INDEX_NONE;
	int32 StepCount = 0;
	FName StepName;
	FString StepTypeName;
	bool bHoldOwned = false;
	uint32 HoldActionID = 0;
};

struct FArcTacticsDebugEndedAction
{
	bool bSet = false;
	FString Package;
	FName Name;
	EArcTacticsActionEndStatus Status = EArcTacticsActionEndStatus::Cancelled;
	EArcTacticsActionEndReason Reason = EArcTacticsActionEndReason::Completed;
	FName FailureReason;
	double Time = 0.0;
};

struct FArcTacticsDebugHistoryEntry
{
	FGameplayTag Tag;
	double Age = 0.0;
};

struct FArcTacticsDebugMemoryEntry
{
	FGameplayTag Key;
	FString TypeName;
	FString ValueText;
};

struct FArcTacticsDebugConsideration
{
	FName Name;
	float Raw = 0.0f;
	float Curved = 0.0f;
	bool bScored = false;
};

struct FArcTacticsDebugCandidate
{
	int32 Order = INDEX_NONE;
	FString Package;
	FName Name;
	bool bFromSelector = false;
	int32 SourceIndex = INDEX_NONE;
	int32 ItemIndex = INDEX_NONE;
	FArcTacticsCandidateIdentity Target;
	float Score = 0.0f;
	EArcTacticsCandidateOutcome Outcome = EArcTacticsCandidateOutcome::BelowMinScore;
	FName Reason;
	int32 Priority = 0;
	bool bHasPriority = false;
	bool bSelected = false;
	TArray<FArcTacticsDebugConsideration> Considerations;
	int32 EarlyOutIndex = INDEX_NONE;
	int32 VetoIndex = INDEX_NONE;
};

struct FArcTacticsDebugLastDecision
{
	bool bValid = false;
	uint32 DecisionGeneration = 0;
	double Time = 0.0;
	EArcTacticsDecisionStatus Status = EArcTacticsDecisionStatus::Running;
	FName Diagnostic;
	int32 SelectedOrder = INDEX_NONE;
	TArray<FArcTacticsDebugCandidate> Candidates;
};

struct FArcTacticsDebugCooldown
{
	FString ConfigName;
	int32 BranchIndex = INDEX_NONE;
	double Remaining = 0.0;
};

struct FArcTacticsDebugPackage
{
	FString ConfigName;
	uint32 SeenRevision = 0;
	uint32 Revision = 0;
	bool bActive = false;
	bool bExcluded = false;
	int32 StaticCount = 0;
	int32 RuntimeCount = 0;
	FName Diagnostic;
	float MinScore = 0.0f;
	int32 GlobalTaskIndex = INDEX_NONE;
	bool bGlobalTaskRunning = false;
	FName GlobalTaskName;
	TArray<FArcTacticsDebugNode> GlobalTasks;
	TArray<FArcTacticsDebugBranch> Branches;
};

struct FArcTacticsDebugSnapshot
{
	FMassEntityHandle Entity;
	double Now = 0.0;

	bool bEnabled = false;
	FArcTacticsInstanceHandle Pool;
	bool bPoolValid = false;

	EArcTacticsDecisionStatus DecisionStatus = EArcTacticsDecisionStatus::Idle;
	uint32 DecisionGeneration = 0;
	bool bDecisionRequested = false;
	double NextDecisionTime = 0.0;
	float DecisionInterval = 0.0f;
	EArcTacticsSelectionMode SelectionMode = EArcTacticsSelectionMode::HighestScore;
	float TopPercent = 0.0f;
	int32 HistorySize = 0;
	FName Diagnostic;
	int32 PackageCursor = INDEX_NONE;

	TArray<FArcTacticsDebugCooldown> Cooldowns;
	TArray<FArcTacticsDebugPackage> Packages;
	FArcTacticsDebugActiveAction ActiveAction;
	FArcTacticsDebugEndedAction LastEndedAction;
	TArray<FArcTacticsDebugHistoryEntry> History;
	bool bHasMemory = false;
	TArray<FArcTacticsDebugMemoryEntry> Memory;
	FArcTacticsDebugLastDecision LastDecision;
};

namespace ArcTacticsDebug
{
	ARCTACTICS_API bool IsCapturing(const UWorld& World, FMassEntityHandle Entity);
	ARCTACTICS_API void SetCaptureTarget(UWorld& World, FMassEntityHandle Entity);
	ARCTACTICS_API FMassEntityHandle GetCaptureTarget(const UWorld& World);
	ARCTACTICS_API void ClearCaptureTarget();
	ARCTACTICS_API bool BuildEntitySnapshot(UWorld& World, FMassEntityHandle Entity, FArcTacticsDebugSnapshot& OutSnapshot);
	ARCTACTICS_API void FormatSnapshot(const FArcTacticsDebugSnapshot& Snapshot, TArray<FString>& OutLines);
	ARCTACTICS_API const TCHAR* EndStatusText(EArcTacticsActionEndStatus Status);
	ARCTACTICS_API const TCHAR* EndReasonText(EArcTacticsActionEndReason Reason);

#if WITH_ARCTACTICS_TRACE
	void CaptureCandidate(
		const UWorld& World,
		const FArcTacticsInstanceSlot& Slot,
		const FArcTacticsCompiledData& CompiledData,
		const FArcTacticsCandidate& Candidate,
		TConstArrayView<FArcTacticsNodeHandle> Considerations,
		TConstArrayView<FArcUtilityConsiderationSnapshot> Snapshots,
		int32 EarlyOutIndex,
		int32 VetoIndex,
		TOptional<int32> Priority);

	void CaptureDecision(const UWorld& World, const FArcTacticsInstanceSlot& Slot, int32 SelectedOrder);
#endif
}
