// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

#if !UE_BUILD_SHIPPING
#define WITH_ARCTACTICS_TRACE 1
#else
#define WITH_ARCTACTICS_TRACE 0
#endif

enum class EArcTacticsActionEndStatus : uint8
{
	Succeeded,
	Failed,
	Cancelled
};

enum class EArcTacticsActionEndReason : uint8
{
	Completed,
	EnterEnded,
	Replaced,
	Unresolved,
	InputsFailed,
	Teardown,
	PackageRemoved
};

enum class EArcTacticsQuerySource : uint8
{
	GlobalTask,
	Step
};

#if WITH_ARCTACTICS_TRACE

#include "Trace/Trace.h"

class UWorld;
struct FArcTQSQueryInstance;
struct FArcTacticsCandidate;
struct FArcTacticsCandidateIdentity;
struct FArcTacticsCompiledData;
struct FArcTacticsInstanceSlot;
struct FArcTacticsNodeHandle;
struct FArcTacticsOption;
struct FArcTacticsQueryReport;
struct FArcTacticsStepNode;
struct FArcTacticsTaskContext;
struct FArcUtilityConsiderationSnapshot;
struct FMassEntityManager;
struct FNavigationPath;

UE_TRACE_CHANNEL_EXTERN(ArcTacticsChannel, ARCTACTICS_API)

namespace ArcTacticsTrace
{
	ARCTACTICS_API uint64 GetWorldId(const UWorld* World);

	ARCTACTICS_API void OutputRecordingStarted(const UWorld* World);

	void OutputCandidate(
		const UWorld* World,
		const FMassEntityManager* EntityManager,
		const FArcTacticsInstanceSlot& Slot,
		const FArcTacticsCompiledData& CompiledData,
		const FArcTacticsCandidate& Candidate,
		TConstArrayView<FArcTacticsNodeHandle> Considerations,
		TConstArrayView<FArcUtilityConsiderationSnapshot> Snapshots,
		int32 EarlyOutIndex,
		int32 VetoIndex,
		TOptional<int32> Priority);

	void OutputDecision(
		const FArcTacticsTaskContext& Context,
		const FArcTacticsInstanceSlot& Slot,
		int32 SelectedOrder);

	void OutputActionEntered(
		const UWorld* World,
		const FMassEntityManager* EntityManager,
		const FArcTacticsInstanceSlot& Slot);

	void OutputActionStepChanged(
		const UWorld* World,
		const FMassEntityManager* EntityManager,
		FMassEntityHandle Entity,
		uint32 ActionSerial,
		int32 StepIndex,
		int32 StepCount,
		FName StepName,
		const FArcTacticsStepNode* StepNode);

	void OutputActionEnded(
		const UWorld* World,
		const FMassEntityManager* EntityManager,
		const FArcTacticsInstanceSlot& Slot,
		EArcTacticsActionEndStatus Status,
		EArcTacticsActionEndReason Reason,
		bool bCooldownStamped,
		bool bHistoryAppended);

	void OutputPathRequested(
		const UWorld* World,
		FMassEntityHandle Entity,
		uint32 ActionSerial,
		bool bRepath,
		const FVector& Start,
		const FVector& Destination,
		const FNavigationPath* Path);

	void OutputTQSQuery(
		const UWorld* World,
		const FMassEntityManager* EntityManager,
		FMassEntityHandle Entity,
		uint32 DecisionGeneration,
		EArcTacticsQuerySource SourceKind,
		FName SourceName,
		uint32 ActionSerial,
		const FArcTacticsQueryReport& Report,
		const FArcTQSQueryInstance& Query);
}

#define TRACE_ARCTACTICS_ENABLED() UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTacticsChannel)

#define TRACE_ARCTACTICS_RECORDING_STARTED(World) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTacticsChannel)) \
	{ ArcTacticsTrace::OutputRecordingStarted(World); }

#define TRACE_ARCTACTICS_DECISION(Context, Slot, SelectedOrder) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTacticsChannel)) \
	{ ArcTacticsTrace::OutputDecision(Context, Slot, SelectedOrder); }

#define TRACE_ARCTACTICS_ACTION_ENTERED(World, EntityManager, Slot) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTacticsChannel)) \
	{ ArcTacticsTrace::OutputActionEntered(World, EntityManager, Slot); }

#define TRACE_ARCTACTICS_ACTION_STEP_CHANGED(World, EntityManager, Entity, ActionSerial, StepIndex, StepCount, StepName, StepNode) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTacticsChannel)) \
	{ ArcTacticsTrace::OutputActionStepChanged(World, EntityManager, Entity, ActionSerial, StepIndex, StepCount, StepName, StepNode); }

#define TRACE_ARCTACTICS_ACTION_ENDED(World, EntityManager, Slot, Status, Reason, bCooldownStamped, bHistoryAppended) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTacticsChannel)) \
	{ ArcTacticsTrace::OutputActionEnded(World, EntityManager, Slot, Status, Reason, bCooldownStamped, bHistoryAppended); }

#define TRACE_ARCTACTICS_PATH_REQUESTED(World, Entity, ActionSerial, bRepath, Start, Destination, Path) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTacticsChannel)) \
	{ ArcTacticsTrace::OutputPathRequested(World, Entity, ActionSerial, bRepath, Start, Destination, Path); }

#define TRACE_ARCTACTICS_TQS_QUERY(World, EntityManager, Entity, DecisionGeneration, SourceKind, SourceName, ActionSerial, Report, Query) 	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTacticsChannel)) 	{ ArcTacticsTrace::OutputTQSQuery(World, EntityManager, Entity, DecisionGeneration, SourceKind, SourceName, ActionSerial, Report, Query); }

#else

#define TRACE_ARCTACTICS_ENABLED() false
#define TRACE_ARCTACTICS_RECORDING_STARTED(World) (void)0
#define TRACE_ARCTACTICS_DECISION(Context, Slot, SelectedOrder) (void)0
#define TRACE_ARCTACTICS_ACTION_ENTERED(World, EntityManager, Slot) (void)0
#define TRACE_ARCTACTICS_ACTION_STEP_CHANGED(World, EntityManager, Entity, ActionSerial, StepIndex, StepCount, StepName, StepNode) (void)0
#define TRACE_ARCTACTICS_ACTION_ENDED(World, EntityManager, Slot, Status, Reason, bCooldownStamped, bHistoryAppended) (void)0
#define TRACE_ARCTACTICS_PATH_REQUESTED(World, Entity, ActionSerial, bRepath, Start, Destination, Path) (void)0
#define TRACE_ARCTACTICS_TQS_QUERY(World, EntityManager, Entity, DecisionGeneration, SourceKind, SourceName, ActionSerial, Report, Query) (void)0

#endif
