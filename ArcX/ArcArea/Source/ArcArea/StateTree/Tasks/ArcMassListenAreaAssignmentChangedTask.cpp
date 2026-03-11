// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassListenAreaAssignmentChangedTask.h"
#include "ArcAreaSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeAsyncExecutionContext.h"

FArcMassListenAreaAssignmentChangedTask::FArcMassListenAreaAssignmentChangedTask()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = true;
}

EStateTreeRunStatus FArcMassListenAreaAssignmentChangedTask::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Already bound (re-entry)
	if (InstanceData.AssignedHandle.IsValid() || InstanceData.UnassignedHandle.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}

	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcAreaSubsystem* Subsystem = World->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	if (!SignalSubsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FMassEntityHandle Entity = MassCtx.GetEntity();
	FArcAreaEntityDelegates& Delegates = Subsystem->GetOrCreateEntityDelegates(Entity);

	// Bind OnAssigned
	InstanceData.AssignedHandle = Delegates.OnAssigned.AddLambda(
		[WeakCtx = Context.MakeWeakExecutionContext(), SignalSubsystem, Entity]()
		{
			FStateTreeStrongExecutionContext StrongContext = WeakCtx.MakeStrongExecutionContext();
			FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (!InstanceDataPtr)
			{
				return;
			}
			StrongContext.BroadcastDelegate(InstanceDataPtr->OnAreaAssigned);
			SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
		});

	// Bind OnUnassigned
	InstanceData.UnassignedHandle = Delegates.OnUnassigned.AddLambda(
		[WeakCtx = Context.MakeWeakExecutionContext(), SignalSubsystem, Entity]()
		{
			FStateTreeStrongExecutionContext StrongContext = WeakCtx.MakeStrongExecutionContext();
			FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (!InstanceDataPtr)
			{
				return;
			}
			StrongContext.BroadcastDelegate(InstanceDataPtr->OnAreaUnassigned);
			SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
		});

	return EStateTreeRunStatus::Running;
}

void FArcMassListenAreaAssignmentChangedTask::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return;
	}

	UArcAreaSubsystem* Subsystem = World->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	const FMassEntityHandle Entity = MassCtx.GetEntity();
	if (FArcAreaEntityDelegates* Delegates = Subsystem->FindEntityDelegates(Entity))
	{
		if (InstanceData.AssignedHandle.IsValid())
		{
			Delegates->OnAssigned.Remove(InstanceData.AssignedHandle);
			InstanceData.AssignedHandle.Reset();
		}

		if (InstanceData.UnassignedHandle.IsValid())
		{
			Delegates->OnUnassigned.Remove(InstanceData.UnassignedHandle);
			InstanceData.UnassignedHandle.Reset();
		}
	}
}

#if WITH_EDITOR
FText FArcMassListenAreaAssignmentChangedTask::GetDescription(
	const FGuid& ID, FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcArea", "ListenAreaAssignmentChangedDesc", "Listen Area Assignment Changed");
}
#endif
