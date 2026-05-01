// Copyright Lukasz Baran. All Rights Reserved.

#include "StateTree/ArcListenNPCRoleChangedTask.h"
#include "ArcSettlementSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeAsyncExecutionContext.h"

FArcListenNPCRoleChangedTask::FArcListenNPCRoleChangedTask()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = true;
}

EStateTreeRunStatus FArcListenNPCRoleChangedTask::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.RoleChangedHandle.IsValid())
	{
		return EStateTreeRunStatus::Running;
	}

	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcSettlementSubsystem* SettlementSub = World->GetSubsystem<UArcSettlementSubsystem>();
	if (!SettlementSub)
	{
		return EStateTreeRunStatus::Failed;
	}

	UMassSignalSubsystem* SignalSub = World->GetSubsystem<UMassSignalSubsystem>();
	if (!SignalSub)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FMassEntityHandle Entity = MassCtx.GetEntity();
	FArcEconomyNPCDelegates& Delegates = SettlementSub->GetOrCreateNPCDelegates(Entity);

	InstanceData.RoleChangedHandle = Delegates.OnRoleChanged.AddLambda(
		[WeakCtx = Context.MakeWeakExecutionContext(), SignalSub, Entity]()
		{
			FStateTreeStrongExecutionContext StrongContext = WeakCtx.MakeStrongExecutionContext();
			FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (!InstanceDataPtr)
			{
				return;
			}
			StrongContext.BroadcastDelegate(InstanceDataPtr->OnRoleChanged);
			SignalSub->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
		});

	return EStateTreeRunStatus::Running;
}

void FArcListenNPCRoleChangedTask::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return;
	}

	UArcSettlementSubsystem* SettlementSub = World->GetSubsystem<UArcSettlementSubsystem>();
	if (!SettlementSub)
	{
		return;
	}

	const FMassEntityHandle Entity = MassCtx.GetEntity();
	if (FArcEconomyNPCDelegates* Delegates = SettlementSub->FindNPCDelegates(Entity))
	{
		if (InstanceData.RoleChangedHandle.IsValid())
		{
			Delegates->OnRoleChanged.Remove(InstanceData.RoleChangedHandle);
			InstanceData.RoleChangedHandle.Reset();
		}
	}
}

#if WITH_EDITOR
FText FArcListenNPCRoleChangedTask::GetDescription(
	const FGuid& ID, FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcEconomy", "ListenNPCRoleChangedDesc", "Listen NPC Role Changed");
}
#endif
