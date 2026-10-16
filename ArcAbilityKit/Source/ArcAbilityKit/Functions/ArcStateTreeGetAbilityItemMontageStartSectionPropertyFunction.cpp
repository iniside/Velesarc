// Copyright Lukasz Baran. All Rights Reserved.

#include "Functions/ArcStateTreeGetAbilityItemMontageStartSectionPropertyFunction.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Sources/ArcSourceItemResolver.h"
#include "Items/ArcItemData.h"
#include "ItemFragments/ArcItemFragment_AbilityMontages.h"

void FArcStateTreeGetAbilityItemMontageStartSectionPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FArcAbilityExecutionContext& Ctx = static_cast<FArcAbilityExecutionContext&>(Context);
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

	Data.Output = NAME_None;

	const FArcItemData* ItemData = ArcAbilityKit::FindSourceItem(Ctx.GetEntityManager(), Ctx.GetEntity(), Ctx.GetAbilityHandle());
	if (ItemData == nullptr)
	{
		return;
	}

	const uint8* Bytes = ItemData->FindFragment(FArcItemFragment_AbilityMontages::StaticStruct());
	if (Bytes == nullptr)
	{
		return;
	}

	const FArcItemFragment_AbilityMontages* Fragment = reinterpret_cast<const FArcItemFragment_AbilityMontages*>(Bytes);
	const FArcEventMontageItem* Entry = Fragment->MontageMap.Find(Data.MontageTag);
	if (Entry == nullptr)
	{
		return;
	}

	Data.Output = Entry->StartSection;
}
