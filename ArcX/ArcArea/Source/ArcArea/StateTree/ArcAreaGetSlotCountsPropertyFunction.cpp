// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaGetSlotCountsPropertyFunction.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"
#include "ArcArea/Mass/ArcAreaFragments.h"
#include "MassEntityView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAreaGetSlotCountsPropertyFunction)

void FArcAreaGetSlotCountsPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData = {};

	const FMassStateTreeExecutionContext& MassCtx = static_cast<const FMassStateTreeExecutionContext&>(Context);
	const FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	const FMassEntityHandle Entity = MassCtx.GetEntity();

	const FMassEntityView EntityView(EntityManager, Entity);
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
	if (!AreaData)
	{
		return;
	}

	InstanceData.TotalSlots = AreaData->Slots.Num();
	for (const FArcAreaSlotRuntime& Slot : AreaData->Slots)
	{
		switch (Slot.State)
		{
		case EArcAreaSlotState::Vacant:   ++InstanceData.VacantCount; break;
		case EArcAreaSlotState::Assigned: ++InstanceData.AssignedCount; break;
		case EArcAreaSlotState::Active:   ++InstanceData.ActiveCount; break;
		case EArcAreaSlotState::Disabled: ++InstanceData.DisabledCount; break;
		}
	}
}
