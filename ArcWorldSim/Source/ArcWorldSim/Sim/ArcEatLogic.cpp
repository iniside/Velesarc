// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcEatLogic.h"

#include "ArcMass/Debugger/ArcEconomyTrace.h"
#include "Fragments/ArcNeedBands.h"
#include "Fragments/ArcNeedFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Nutrition.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Operations/ArcMassItemOperations.h"
#include "Sim/ArcBuildingFragments.h"
#include "Sim/ArcWorldSimStores.h"

namespace ArcEatLogicPrivate
{
bool ConsumeStacks(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle Entity,
	const UScriptStruct* StoreType,
	const TArray<TPair<FArcItemId, int32>>& Candidates,
	int32 Amount)
{
	int32 Remaining = Amount;
	for (const TPair<FArcItemId, int32>& Candidate : Candidates)
	{
		if (Remaining <= 0)
		{
			break;
		}

		const int32 ToTake = FMath::Min(Candidate.Value, Remaining);
		if (ToTake <= 0)
		{
			continue;
		}

		if (!ArcMassItems::ModifyStacks(EntityManager, Entity, StoreType, Candidate.Key, -ToTake))
		{
			continue;
		}

		Remaining -= ToTake;
		if (Candidate.Value - ToTake <= 0)
		{
			ArcMassItems::RemoveItem(EntityManager, Entity, StoreType, Candidate.Key);
		}
	}

	return Remaining == 0;
}
}

namespace ArcWorldSim
{
bool FindEatPortion(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle Consumer,
	FEatPortion& OutPortion)
{
	OutPortion = FEatPortion();
	if (!EntityManager.IsEntityValid(Consumer))
	{
		return false;
	}

	const FMassEntityView ConsumerView(EntityManager, Consumer);
	const FArcItemConsumerConfigFragment* ConsumerConfig =
		ConsumerView.GetConstSharedFragmentDataPtr<FArcItemConsumerConfigFragment>();
	const FArcBuildingInputStore* InputStore =
		ConsumerView.GetSparseFragmentDataPtr<FArcBuildingInputStore>();
	if (!ConsumerConfig || !InputStore)
	{
		return false;
	}

	for (const FArcMassReplicatedItem& Item : InputStore->ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (!Data)
		{
			continue;
		}

		bool bWanted = false;
		for (const FArcNamedPrimaryAssetId& Wanted : ConsumerConfig->WantedItems)
		{
			const FPrimaryAssetId WantedId = Wanted;
			if (WantedId == Data->GetItemDefinitionId())
			{
				bWanted = true;
				break;
			}
		}
		if (!bWanted)
		{
			continue;
		}

		const FArcItemFragment_Nutrition* NutritionFrag =
			ArcItemsHelper::FindFragment<FArcItemFragment_Nutrition>(Data);
		if (!NutritionFrag || NutritionFrag->Nutrition <= 0.0f)
		{
			continue;
		}

		OutPortion.ItemId = Data->GetItemId();
		OutPortion.StackCount = static_cast<int32>(Data->GetStacks());
		OutPortion.Nutrition = NutritionFrag->Nutrition;
		return true;
	}

	return false;
}

void ApplyEatNutrition(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle User,
	FArcHungerNeedFragment& Hunger,
	float Nutrition,
	double Now)
{
	Hunger.CurrentValue = FMath::Clamp(
		Hunger.CurrentValue - Nutrition,
		0.0f,
		ArcNeeds::NeedMaxValue);
	Hunger.LastUpdateTime = Now;
	if (!EntityManager.IsEntityValid(User))
	{
		return;
	}

	const FMassEntityView UserView(EntityManager, User);
	const FArcNeedBandTableFragment* BandTable =
		UserView.GetConstSharedFragmentDataPtr<FArcNeedBandTableFragment>();
	const TArray<FArcNeedBand>& HungerBands = ArcNeeds::HungerBandsFor(BandTable);
	const int32 NewBand = ArcNeeds::BandIndexFor(HungerBands, Hunger.CurrentValue);
	if (NewBand != INDEX_NONE)
	{
		Hunger.LastBand = static_cast<uint8>(NewBand);
	}
}

bool ConsumeOnePortion(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle Consumer,
	const FArcItemId ItemId,
	int32 CurrentStacks)
{
	TArray<TPair<FArcItemId, int32>> Candidates;
	Candidates.Add(TPair<FArcItemId, int32>(ItemId, CurrentStacks));
	return ArcEatLogicPrivate::ConsumeStacks(
		EntityManager,
		Consumer,
		FArcBuildingInputStore::StaticStruct(),
		Candidates,
		1);
}
}
