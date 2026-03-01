// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaFindVacantSlotPropertyFunction.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"
#include "ArcArea/Mass/ArcAreaFragments.h"
#include "MassEntityView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAreaFindVacantSlotPropertyFunction)

void FArcAreaFindVacantSlotPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.SlotIndex = INDEX_NONE;
	InstanceData.bFound = false;

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
	if (!AreaData)
	{
		return;
	}

	const bool bHasFilter = InstanceData.RoleTagFilter.IsValid();

	for (int32 i = 0; i < AreaData->Slots.Num(); ++i)
	{
		if (AreaData->Slots[i].State != EArcAreaSlotState::Vacant)
		{
			continue;
		}

		if (bHasFilter && AreaData->SlotDefinitions.IsValidIndex(i))
		{
			if (!AreaData->SlotDefinitions[i].RoleTag.MatchesTag(InstanceData.RoleTagFilter))
			{
				continue;
			}
		}

		InstanceData.SlotIndex = i;
		InstanceData.bFound = true;
		return;
	}
}
