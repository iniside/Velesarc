// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaGetSlotStatePropertyFunction.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"
#include "ArcArea/Mass/ArcAreaFragments.h"
#include "MassEntityView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAreaGetSlotStatePropertyFunction)

void FArcAreaGetSlotStatePropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.SlotState = EArcAreaSlotState::Vacant;
	InstanceData.AssignedEntity = FMassEntityHandle();

	const FMassStateTreeExecutionContext& MassCtx = static_cast<const FMassStateTreeExecutionContext&>(Context);
	const FMassEntityManager& EntityManager = MassCtx.GetEntityManager();

	const FMassEntityView EntityView(EntityManager, MassCtx.GetEntity());
	const FArcAreaFragment* AreaFragment = EntityView.GetFragmentDataPtr<FArcAreaFragment>();
	if (!AreaFragment || !AreaFragment->AreaHandle.IsValid())
	{
		return;
	}

	const UWorld* World = MassCtx.GetWorld();
	const UArcAreaSubsystem* Subsystem = World ? World->GetSubsystem<UArcAreaSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	const FArcAreaData* AreaData = Subsystem->GetAreaData(AreaFragment->AreaHandle);
	if (!AreaData || !AreaData->Slots.IsValidIndex(InstanceData.SlotIndex))
	{
		return;
	}

	const FArcAreaSlotRuntime& Slot = AreaData->Slots[InstanceData.SlotIndex];
	InstanceData.SlotState = Slot.State;
	InstanceData.AssignedEntity = Slot.AssignedEntity;
}
