// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassSelectApproachSlotTask.h"

#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "GameFramework/Actor.h"

void UArcDynamicSlotComponent::ClaimSlot(const FGameplayTag& SlotTag, UObject* Owner, FVector& OutLocation, int32 &OutSlotIndex)
{
	for (int32 Idx = 0; Idx < Slots.Num(); Idx++)
	{
		if (Slots[Idx].Owner == Owner)
		{
			OutLocation = Slots[Idx].SlotLocation.GetLocation();
			OutSlotIndex = Idx;
			return;
		}
	}
	
	for (int32 Idx = 0; Idx < Slots.Num(); Idx++)
	{
		if (Slots[Idx].SlotTag == SlotTag)
		{
			if (TakenSlots.Contains(Idx))
			{
				continue;
			}
			OutLocation = Slots[Idx].SlotLocation.GetLocation();
			Slots[Idx].Owner = Owner;
			TakenSlots.Add(Idx);
			OutSlotIndex = Idx;
			return;
		}
	}
	
	OutSlotIndex = INDEX_NONE;
}

void UArcDynamicSlotComponent::FreeSlot(int32 SlotIndex)
{
	if (Slots.IsValidIndex(SlotIndex))
	{
		Slots[SlotIndex].Owner = nullptr;	
	}
	
	TakenSlots.Remove(SlotIndex);
}

FArcMassSelectApproachSlotTask::FArcMassSelectApproachSlotTask()
{
	bShouldCallTick = false;
}

bool FArcMassSelectApproachSlotTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorHandle);
	
	return true;
}

void FArcMassSelectApproachSlotTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadOnly<FMassActorFragment>();
}

EStateTreeRunStatus FArcMassSelectApproachSlotTask::EnterState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.Target)
	{
		InstanceData.bSuccess = false;
		return EStateTreeRunStatus::Failed;	
	}
	
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* MassActor = MassStateTreeContext.GetExternalDataPtr(MassActorHandle);
	if (!MassActor)
	{
		UE_LOG(LogStateTree, Warning, TEXT("FArcGetMassActorTask::EnterState: MassActor is null"));
		return EStateTreeRunStatus::Failed;
	}
	
	UArcDynamicSlotComponent* DynamicSlotComp = InstanceData.Target->FindComponentByClass<UArcDynamicSlotComponent>();
	if (!DynamicSlotComp)
	{
		InstanceData.bSuccess = false;
		return EStateTreeRunStatus::Failed;
	}
	
	FVector SlotLocation;
	int32 SlotIndex;
	DynamicSlotComp->ClaimSlot(InstanceData.SlotTag, MassActor->GetMutable(),SlotLocation, SlotIndex);
	
	InstanceData.WorldLocation = InstanceData.Target->GetTransform().TransformPosition(SlotLocation);
	InstanceData.SlotIndex = SlotIndex;
	InstanceData.bSuccess = SlotIndex != INDEX_NONE;
	
	return EStateTreeRunStatus::Succeeded;
}

void FArcMassSelectApproachSlotTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeTaskBase::ExitState(Context, Transition);
}
