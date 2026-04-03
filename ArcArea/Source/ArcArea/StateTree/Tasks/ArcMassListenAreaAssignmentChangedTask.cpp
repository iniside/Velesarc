// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassListenAreaAssignmentChangedTask.h"
#include "ArcAreaSubsystem.h"
#include "ArcAreaAutoVacancyListener.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
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

	const USmartObjectSubsystem* SOSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
	if (!SOSubsystem)
	{
		return EStateTreeRunStatus::Succeeded;
	}
	
	const FMassEntityHandle Entity = MassCtx.GetEntity();
	FArcAreaEntityDelegates& Delegates = Subsystem->GetOrCreateEntityDelegates(Entity);

	UArcAreaAutoVacancyListener* VacancyListener = World->GetSubsystem<UArcAreaAutoVacancyListener>();

	// Bind OnAssigned
	InstanceData.AssignedHandle = Delegates.OnAssigned.AddLambda(
		[WeakCtx = Context.MakeWeakExecutionContext(), SignalSubsystem, Subsystem, Entity, VacancyListener, SOSubsystem](const FArcAreaSlotHandle& SlotHandle)
		{
			FStateTreeStrongExecutionContext StrongContext = WeakCtx.MakeStrongExecutionContext();
			FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (!InstanceDataPtr)
			{
				return;
			}
			InstanceDataPtr->SlotHandle = SlotHandle;
			const FArcAreaData* AreaData = Subsystem->GetAreaData(SlotHandle.AreaHandle);
			InstanceDataPtr->AreaEntity.EntityHandle = AreaData ? AreaData->OwnerEntity : FMassEntityHandle();
			InstanceDataPtr->AdvertisementHandle = VacancyListener
				? VacancyListener->FindVacancyHandle(SlotHandle)
				: FArcKnowledgeHandle();
				
			TArray<FSmartObjectSlotHandle> MatchedSlots;
			FSmartObjectRequestFilter Filter;
			SOSubsystem->FindSlots(AreaData->SmartObjectHandle, Filter, MatchedSlots);
				
			const int32 NumCandidates = FMath::Min(MatchedSlots.Num(),
			static_cast<int32>(FMassSmartObjectCandidateSlots::MaxNumCandidates));

			for (int32 i = 0; i < NumCandidates; ++i)
			{
				FSmartObjectCandidateSlot& Candidate = InstanceDataPtr->CandidateSlots.Slots[InstanceDataPtr->CandidateSlots.NumSlots];
				Candidate.Result.SmartObjectHandle = AreaData->SmartObjectHandle;
				Candidate.Result.SlotHandle = MatchedSlots[i];
				Candidate.Cost = 0.f;
				InstanceDataPtr->CandidateSlots.NumSlots++;
			}

			StrongContext.BroadcastDelegate(InstanceDataPtr->OnAreaAssigned);
			SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
		});

	// Bind OnUnassigned
	InstanceData.UnassignedHandle = Delegates.OnUnassigned.AddLambda(
		[WeakCtx = Context.MakeWeakExecutionContext(), SignalSubsystem, Entity](const FArcAreaSlotHandle& SlotHandle)
		{
			FStateTreeStrongExecutionContext StrongContext = WeakCtx.MakeStrongExecutionContext();
			FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (!InstanceDataPtr)
			{
				return;
			}
			InstanceDataPtr->SlotHandle = SlotHandle;
			InstanceDataPtr->AreaEntity.EntityHandle = FMassEntityHandle();
			InstanceDataPtr->AdvertisementHandle = FArcKnowledgeHandle();
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
