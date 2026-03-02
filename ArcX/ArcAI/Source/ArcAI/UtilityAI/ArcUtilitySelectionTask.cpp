// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/ArcUtilitySelectionTask.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeAsyncExecutionContext.h"
#include "UtilityAI/ArcUtilityScoringSubsystem.h"

FArcUtilitySelectionTask::FArcUtilitySelectionTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = true;
}

EStateTreeRunStatus FArcUtilitySelectionTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcUtilityScoringSubsystem* ScoringSubsystem = World->GetSubsystem<UArcUtilityScoringSubsystem>();
	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	if (!ScoringSubsystem || !SignalSubsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();

	InstanceData.EntityHandle = MassCtx.GetEntity();

	TArray<FArcUtilityTarget> Targets = BuildTargets(InstanceData);
	if (Targets.IsEmpty())
	{
		return EStateTreeRunStatus::Failed;
	}

	FArcUtilityContext UtilityContext = BuildContext(MassCtx, EntityManager, World);
	FMassEntityHandle Entity = MassCtx.GetEntity();
	FStateTreeWeakExecutionContext WeakContext = Context.MakeWeakExecutionContext();

	// Copy entries (subsystem takes ownership via move)
	TArray<FArcUtilityEntry> EntriesCopy = InstanceData.Entries;

	const int32 RequestId = ScoringSubsystem->SubmitRequest(
		MoveTemp(EntriesCopy),
		MoveTemp(Targets),
		UtilityContext,
		InstanceData.SelectionMode,
		InstanceData.MinScore,
		InstanceData.TopPercent,
		FArcUtilityScoringFinished::CreateStatic(
			&FArcUtilitySelectionTask::OnScoringCompleted,
			WeakContext,
			SignalSubsystem,
			Entity)
	);

	if (RequestId == INDEX_NONE)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.RequestId = RequestId;
	return EStateTreeRunStatus::Running;
}

void FArcUtilitySelectionTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.RequestId != INDEX_NONE)
	{
		if (UArcUtilityScoringSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcUtilityScoringSubsystem>())
		{
			Subsystem->AbortRequest(InstanceData.RequestId);
		}
		InstanceData.RequestId = INDEX_NONE;
	}
}

void FArcUtilitySelectionTask::OnScoringCompleted(
	FArcUtilityScoringInstance& CompletedInstance,
	FStateTreeWeakExecutionContext WeakContext,
	UMassSignalSubsystem* SignalSubsystem,
	FMassEntityHandle Entity)
{
	const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
	FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
	if (!InstanceDataPtr)
	{
		return;
	}

	InstanceDataPtr->RequestId = INDEX_NONE;

	const FArcUtilityResult& Result = CompletedInstance.Result;

	if (Result.bSuccess)
	{
		// Write outputs
		InstanceDataPtr->SelectedEntryIndex = Result.WinningEntryIndex;
		InstanceDataPtr->SelectedScore = Result.Score;
		InstanceDataPtr->SelectedActor = Result.WinningTarget.GetActor(
			CompletedInstance.Context.EntityManager);
		InstanceDataPtr->SelectedEntity = Result.WinningTarget.EntityHandle;
		InstanceDataPtr->SelectedLocation = Result.WinningTarget.GetLocation(
			CompletedInstance.Context.EntityManager);

		// Transition to winning entry's linked state
		StrongContext.RequestTransition(Result.WinningState, EStateTreeTransitionPriority::Critical);
	}
	else
	{
		// Nothing passed — task fails
		InstanceDataPtr->SelectedEntryIndex = INDEX_NONE;
		InstanceDataPtr->SelectedScore = 0.0f;
		InstanceDataPtr->SelectedActor = nullptr;
		InstanceDataPtr->SelectedEntity = FMassEntityHandle();
		InstanceDataPtr->SelectedLocation = FVector::ZeroVector;

		StrongContext.FinishTask(EStateTreeFinishTaskType::Failed);
	}

	// Signal entity so State Tree wakes up
	if (SignalSubsystem)
	{
		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
	}
}

TArray<FArcUtilityTarget> FArcUtilitySelectionTask::BuildTargets(const FInstanceDataType& InstanceData)
{
	TArray<FArcUtilityTarget> Targets;
	Targets.Reserve(InstanceData.InActors.Num() + InstanceData.InEntities.Num() + InstanceData.InLocations.Num());

	for (AActor* Actor : InstanceData.InActors)
	{
		if (Actor)
		{
			FArcUtilityTarget Target;
			Target.TargetType = EArcUtilityTargetType::Actor;
			Target.Actor = Actor;
			Target.Location = Actor->GetActorLocation();
			Targets.Add(MoveTemp(Target));
		}
	}

	for (const FMassEntityHandle& Handle : InstanceData.InEntities)
	{
		if (Handle.IsSet())
		{
			FArcUtilityTarget Target;
			Target.TargetType = EArcUtilityTargetType::Entity;
			Target.EntityHandle = Handle;
			Targets.Add(MoveTemp(Target));
		}
	}

	for (const FVector& Loc : InstanceData.InLocations)
	{
		FArcUtilityTarget Target;
		Target.TargetType = EArcUtilityTargetType::Location;
		Target.Location = Loc;
		Targets.Add(MoveTemp(Target));
	}

	return Targets;
}

FArcUtilityContext FArcUtilitySelectionTask::BuildContext(
	FMassStateTreeExecutionContext& MassCtx,
	FMassEntityManager& EntityManager,
	UWorld* World)
{
	FArcUtilityContext Ctx;
	Ctx.QuerierEntity = MassCtx.GetEntity();
	Ctx.World = World;
	Ctx.EntityManager = &EntityManager;

	if (const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(MassCtx.GetEntity()))
	{
		Ctx.QuerierLocation = TransformFrag->GetTransform().GetLocation();
		Ctx.QuerierForward = TransformFrag->GetTransform().GetRotation().GetForwardVector();
	}

	if (FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(MassCtx.GetEntity()))
	{
		Ctx.QuerierActor = ActorFrag->GetMutable();
	}

	return Ctx;
}
