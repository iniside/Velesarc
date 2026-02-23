// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSRunQueryTask.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeAsyncExecutionContext.h"
#include "TargetQuery/ArcTQSQueryDefinition.h"
#include "TargetQuery/ArcTQSQuerySubsystem.h"
#include "TargetQuery/ArcTQSQueryInstance.h"

FArcTQSRunQueryTask::FArcTQSRunQueryTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = true;
}

EStateTreeRunStatus FArcTQSRunQueryTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcTQSQuerySubsystem* TQSSubsystem = World->GetSubsystem<UArcTQSQuerySubsystem>();
	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	if (!TQSSubsystem || !SignalSubsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();

	InstanceData.EntityHandle = MassCtx.GetEntity();
	FArcTQSQueryContext QueryContext = BuildQueryContext(MassCtx, EntityManager, World, InstanceData);

	FMassEntityHandle Entity = MassCtx.GetEntity();
	FStateTreeWeakExecutionContext WeakContext = Context.MakeWeakExecutionContext();

	// Submit query with callback-based completion
	const int32 QueryId = SubmitQuery(
		TQSSubsystem,
		InstanceData,
		QueryContext,
		FArcTQSQueryFinished::CreateStatic(
			&FArcTQSRunQueryTask::OnQueryCompleted,
			WeakContext,
			SignalSubsystem,
			Entity,
			bFinishOnEnd,
			ReQueryInterval)
	);

	if (QueryId == INDEX_NONE)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.QueryId = QueryId;
	return EStateTreeRunStatus::Running;
}

void FArcTQSRunQueryTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.QueryId != INDEX_NONE)
	{
		if (UArcTQSQuerySubsystem* TQSSubsystem = Context.GetWorld()->GetSubsystem<UArcTQSQuerySubsystem>())
		{
			TQSSubsystem->AbortQuery(InstanceData.QueryId);
		}
		InstanceData.QueryId = INDEX_NONE;
	}
}

void FArcTQSRunQueryTask::OnQueryCompleted(
	FArcTQSQueryInstance& CompletedQuery,
	FStateTreeWeakExecutionContext WeakContext,
	UMassSignalSubsystem* SignalSubsystem,
	FMassEntityHandle Entity,
	bool bFinishOnEnd,
	float ReQueryInterval)
{
	const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
	FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
	if (!InstanceDataPtr)
	{
		return;
	}

	InstanceDataPtr->QueryId = INDEX_NONE;

	// Copy results to instance data
	bool bSuccess = false;
	if (CompletedQuery.Status == EArcTQSQueryStatus::Completed && CompletedQuery.Results.Num() > 0)
	{
		InstanceDataPtr->BestResult = CompletedQuery.Results[0];
		InstanceDataPtr->ResultArray = CompletedQuery.Results;
		InstanceDataPtr->bHasResults = true;
		bSuccess = true;
	}
	else
	{
		InstanceDataPtr->BestResult = FArcTQSTargetItem();
		InstanceDataPtr->ResultArray.Empty();
		InstanceDataPtr->bHasResults = false;
	}

	if (bFinishOnEnd)
	{
		StrongContext.FinishTask(bSuccess ? EStateTreeFinishTaskType::Succeeded : EStateTreeFinishTaskType::Failed);
	}
	else
	{
		// Schedule re-query via timer if interval is set
		if (ReQueryInterval > 0.0f && SignalSubsystem)
		{
			UWorld* World = SignalSubsystem->GetWorld();
			if (World)
			{
				FTimerManager& TimerManager = World->GetTimerManager();
				FTimerHandle TimerHandle;

				TimerManager.SetTimer(TimerHandle, [WeakContext, SignalSubsystem, Entity, ReQueryInterval]()
				{
					const FStateTreeStrongExecutionContext StrongCtx = WeakContext.MakeStrongExecutionContext();
					FInstanceDataType* InstData = StrongCtx.GetInstanceDataPtr<FInstanceDataType>();
					if (!InstData)
					{
						return;
					}

					UWorld* LocalWorld = SignalSubsystem->GetWorld();
					if (!LocalWorld)
					{
						return;
					}

					UArcTQSQuerySubsystem* TQSSubsystem = LocalWorld->GetSubsystem<UArcTQSQuerySubsystem>();
					if (!TQSSubsystem)
					{
						return;
					}

					UMassEntitySubsystem* MassSubsystem = LocalWorld->GetSubsystem<UMassEntitySubsystem>();
					if (!MassSubsystem)
					{
						return;
					}

					// Rebuild query context from entity's current state
					FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

					FArcTQSQueryContext QueryContext;
					QueryContext.QuerierEntity = Entity;
					QueryContext.World = LocalWorld;
					QueryContext.EntityManager = &EntityManager;
					QueryContext.ExplicitItems = InstData->ExplicitInput;
					QueryContext.ContextLocations = InstData->ContextLocations;

					if (const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity))
					{
						QueryContext.QuerierLocation = TransformFrag->GetTransform().GetLocation();
						QueryContext.QuerierForward = TransformFrag->GetTransform().GetRotation().GetForwardVector();
					}

					if (FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity))
					{
						QueryContext.QuerierActor = ActorFrag->GetMutable();
					}

					// Submit re-query
					InstData->QueryId = SubmitQuery(
						TQSSubsystem,
						*InstData,
						QueryContext,
						FArcTQSQueryFinished::CreateStatic(
							&FArcTQSRunQueryTask::OnQueryCompleted,
							WeakContext,
							SignalSubsystem,
							Entity,
							false, // bFinishOnEnd is always false in re-query path
							ReQueryInterval)
					);
				}, ReQueryInterval, false);
			}
		}
	}

	// Signal the entity so the State Tree wakes up
	if (SignalSubsystem)
	{
		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
	}
}

FArcTQSQueryContext FArcTQSRunQueryTask::BuildQueryContext(
	FMassStateTreeExecutionContext& MassCtx,
	FMassEntityManager& EntityManager,
	UWorld* World,
	const FInstanceDataType& InstanceData)
{
	FArcTQSQueryContext QueryContext;
	QueryContext.QuerierEntity = MassCtx.GetEntity();
	QueryContext.World = World;
	QueryContext.EntityManager = &EntityManager;
	QueryContext.ExplicitItems = InstanceData.ExplicitInput;
	QueryContext.ContextLocations = InstanceData.ContextLocations;

	if (const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(MassCtx.GetEntity()))
	{
		QueryContext.QuerierLocation = TransformFrag->GetTransform().GetLocation();
		QueryContext.QuerierForward = TransformFrag->GetTransform().GetRotation().GetForwardVector();
	}

	if (FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(MassCtx.GetEntity()))
	{
		QueryContext.QuerierActor = ActorFrag->GetMutable();
	}

	return QueryContext;
}

int32 FArcTQSRunQueryTask::SubmitQuery(
	UArcTQSQuerySubsystem* TQSSubsystem,
	const FInstanceDataType& InstanceData,
	const FArcTQSQueryContext& QueryContext,
	FArcTQSQueryFinished OnFinished)
{
	if (InstanceData.bUseInlineDefinition)
	{
		// Copy inline data — the subsystem takes ownership via move
		FInstancedStruct ContextProviderCopy = InstanceData.InlineContextProvider;
		FInstancedStruct GeneratorCopy = InstanceData.InlineGenerator;
		TArray<FInstancedStruct> StepsCopy = InstanceData.InlineSteps;

		return TQSSubsystem->RunQuery(
			MoveTemp(ContextProviderCopy),
			MoveTemp(GeneratorCopy),
			MoveTemp(StepsCopy),
			InstanceData.InlineSelectionMode,
			InstanceData.InlineTopN,
			InstanceData.InlineMinPassingScore,
			InstanceData.InlineTopPercent,
			QueryContext,
			MoveTemp(OnFinished));
	}
	else
	{
		return TQSSubsystem->RunQuery(
			InstanceData.QueryDefinition.Get(),
			QueryContext,
			MoveTemp(OnFinished));
	}
}
