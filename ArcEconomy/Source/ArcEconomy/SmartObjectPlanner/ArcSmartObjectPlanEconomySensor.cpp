// Copyright Lukasz Baran. All Rights Reserved.

#include "SmartObjectPlanner/ArcSmartObjectPlanEconomySensor.h"

#include "SmartObjectPlanner/ArcSmartObjectPlanRequest.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"
#include "ArcSettlementSubsystem.h"
#include "MassEntityManager.h"
#include "Mass/ArcEconomyFragments.h"
#include "Mass/ArcMassItemFragments.h"
#include "Items/ArcItemDefinition.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Engine/World.h"

void FArcSmartObjectPlanEconomySensor::GatherCandidates(
	const FArcSmartObjectPlanEvaluationContext& Context,
	const FArcSmartObjectPlanRequest& Request,
	TArray<FArcPotentialEntity>& OutCandidates) const
{
	const FMassEntityManager* EntityManager = Context.EntityManager;
	if (EntityManager == nullptr)
	{
		return;
	}

	UWorld* World = EntityManager->GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UArcSettlementSubsystem* SettlementSubsystem = World->GetSubsystem<UArcSettlementSubsystem>();
	if (SettlementSubsystem == nullptr)
	{
		return;
	}

	FMassEntityHandle SettlementHandle = SettlementSubsystem->FindNearestSettlement(
		const_cast<FMassEntityManager&>(*EntityManager), Context.RequestingLocation);

	if (!SettlementHandle.IsValid())
	{
		return;
	}

	const TArray<FMassEntityHandle>& BuildingHandles = SettlementSubsystem->GetBuildingsForSettlement(SettlementHandle);

	OutCandidates.Reserve(OutCandidates.Num() + BuildingHandles.Num());

	for (const FMassEntityHandle& BuildingHandle : BuildingHandles)
	{
		if (!EntityManager->IsEntityValid(BuildingHandle))
		{
			continue;
		}

		const FArcBuildingFragment* BuildingFragment = EntityManager->GetFragmentDataPtr<FArcBuildingFragment>(BuildingHandle);
		const FArcBuildingEconomyConfig* EconomyConfig = EntityManager->GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(BuildingHandle);

		if (BuildingFragment == nullptr || EconomyConfig == nullptr)
		{
			continue;
		}

		if (EconomyConfig->ConsumptionNeeds.Num() == 0)
		{
			continue;
		}

		const FArcMassItemSpecArrayFragment* BuildingOwnedItems = EntityManager->GetFragmentDataPtr<FArcMassItemSpecArrayFragment>(BuildingHandle);
		if (BuildingOwnedItems == nullptr || BuildingOwnedItems->Items.Num() == 0)
		{
			continue;
		}

		FGameplayTagContainer ProvidesTags;
		bool bMatchesQuery = EconomyQueryTags.IsEmpty();

		for (const FArcBuildingConsumptionEntry& Entry : EconomyConfig->ConsumptionNeeds)
		{
			for (const FArcItemSpec& Spec : BuildingOwnedItems->Items)
			{
				const UArcItemDefinition* ItemDefinition = Spec.GetItemDefinition();
				if (ItemDefinition == nullptr)
				{
					continue;
				}

				bool bItemMatches = false;

				if (Entry.ItemTag.IsValid())
				{
					const FArcItemFragment_Tags* TagsFragment = ItemDefinition->FindFragment<FArcItemFragment_Tags>();
					if (TagsFragment != nullptr && TagsFragment->ItemTags.HasTag(Entry.ItemTag))
					{
						bItemMatches = true;
					}
				}
				else if (Entry.Item != nullptr && ItemDefinition == Entry.Item)
				{
					bItemMatches = true;
				}

				if (bItemMatches)
				{
					const FArcItemFragment_Tags* TagsFragment = ItemDefinition->FindFragment<FArcItemFragment_Tags>();
					if (TagsFragment != nullptr)
					{
						ProvidesTags.AppendTags(TagsFragment->ItemTags);

						if (!bMatchesQuery && EconomyQueryTags.HasAny(TagsFragment->ItemTags))
						{
							bMatchesQuery = true;
						}
					}
					break;
				}
			}
		}

		if (!bMatchesQuery || ProvidesTags.IsEmpty())
		{
			continue;
		}

		FArcPotentialEntity PotentialEntity;
		PotentialEntity.EntityHandle = BuildingHandle;
		PotentialEntity.Location = BuildingFragment->BuildingLocation;
		PotentialEntity.Provides = ProvidesTags;
		PotentialEntity.Requires = EconomyConfig->RequiresTags;

		OutCandidates.Add(PotentialEntity);
	}
}
