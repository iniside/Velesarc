// Copyright Lukasz Baran. All Rights Reserved.

#include "Functions/ArcStateTreeGetAbilityItemMontageMarksPropertyFunction.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Sources/ArcSourceItemResolver.h"
#include "Items/ArcItemData.h"
#include "ItemFragments/ArcItemFragment_AbilityMontages.h"
#include "Animation/ArcMontageMarkExtractor.h"
#include "Animation/AnimMontage.h"

void FArcStateTreeGetAbilityItemMontageMarksPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FArcAbilityExecutionContext& Ctx = static_cast<FArcAbilityExecutionContext&>(Context);
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

	Data.Output.Reset();

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

	if (Entry->ExtractedMarks.Num() > 0)
	{
		Data.Output = Entry->ExtractedMarks;
	}
	else
	{
		UAnimMontage* LoadedMontage = Entry->Montage.LoadSynchronous();
		if (LoadedMontage != nullptr)
		{
			ArcMontageMarks::ExtractMarks(LoadedMontage, Data.Output);
		}
	}

	if (Data.PlayRate > 0.f)
	{
		for (FArcMontageMarkEntry& Mark : Data.Output)
		{
			Mark.Time /= Data.PlayRate;
		}
	}
}
