// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcProductionSlots.h"

#include "SmartObjectDefinition.h"
#include "Sim/ArcWorldSimTags.h"

namespace ArcWorldSim
{
int32 CountWorkerSlots(const USmartObjectDefinition& Definition)
{
	int32 Count = 0;
	for (int32 Index = 0; Index < Definition.GetSlots().Num(); ++Index)
	{
		FGameplayTagContainer Tags;
		Definition.GetSlotActivityTags(Index, Tags);
		if (Tags.HasTagExact(ArcWorldSimTags::Slot_Worker))
		{
			++Count;
		}
	}
	return Count;
}

void SyncProductionSlots(
	FArcProductionSlotsFragment& Production,
	const FArcProductionConfigFragment& Config,
	const USmartObjectDefinition* Definition)
{
	// Arc.Slot.Worker slots on the definition ARE the production slots.
	const int32 WorkerCount = Definition ? CountWorkerSlots(*Definition) : 0;
	const int32 OldNum = Production.Slots.Num();
	Production.Slots.SetNum(WorkerCount);
	for (int32 Index = OldNum; Index < WorkerCount; ++Index)
	{
		FArcProductionSlot& Slot = Production.Slots[Index];
		Slot.RequiredWorkerCount = 1;
		Slot.RecipeIndex = Config.Recipes.IsEmpty() ? INDEX_NONE : 0;
		Slot.State = EArcProductionState::Unstaffed;
	}
}
}
