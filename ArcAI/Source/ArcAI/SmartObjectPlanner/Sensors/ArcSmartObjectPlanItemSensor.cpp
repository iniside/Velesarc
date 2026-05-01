// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanItemSensor.h"

#include "SmartObjectPlanner/ArcMassGoalPlanInfoSharedFragment.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanRequest.h"
#include "ArcMass/Spatial/ArcMassSpatialHashSubsystem.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "Mass/ArcMassItemFragments.h"
#include "Items/ArcItemDefinition.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"

void FArcSmartObjectPlanItemSensor::GatherCandidates(
	const FArcSmartObjectPlanEvaluationContext& Context,
	const FArcSmartObjectPlanRequest& Request,
	TArray<FArcPotentialEntity>& OutCandidates) const
{
	if (ItemTagToCapabilityMap.IsEmpty())
	{
		return;
	}

	const UWorld* World = Context.EntityManager->GetWorld();
	if (!World)
	{
		return;
	}

	USmartObjectSubsystem* SOSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
	const UArcMassSpatialHashSubsystem* SpatialHash = World->GetSubsystem<UArcMassSpatialHashSubsystem>();
	if (!SOSubsystem || !SpatialHash)
	{
		return;
	}

	const FMassEntityManager& EntityManager = *Context.EntityManager;

	TArray<FArcMassEntityInfo> NearbyEntities;
	NearbyEntities.Reserve(64);

	SpatialHash->GetSpatialHashGrid().QueryEntitiesInRadiusFiltered(
		Request.SearchOrigin,
		Request.SearchRadius,
		[&EntityManager](const FMassEntityHandle& Entity, const FVector& /*Location*/) -> bool
		{
			return EntityManager.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(Entity) != nullptr
				&& EntityManager.GetFragmentDataPtr<FArcMassItemSpecArrayFragment>(Entity) != nullptr;
		},
		NearbyEntities);

	for (const FArcMassEntityInfo& Info : NearbyEntities)
	{
		const FArcSmartObjectOwnerFragment* SmartObjectOwner = EntityManager.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(Info.Entity);
		const FArcMassItemSpecArrayFragment* ItemsFragment = EntityManager.GetFragmentDataPtr<FArcMassItemSpecArrayFragment>(Info.Entity);

		if (!SmartObjectOwner || !ItemsFragment)
		{
			continue;
		}

		FGameplayTagContainer ProvidesTags;

		for (const FArcItemSpec& Spec : ItemsFragment->Items)
		{
			const UArcItemDefinition* ItemDefinition = Spec.GetItemDefinition();
			if (ItemDefinition == nullptr)
			{
				continue;
			}

			const FArcItemFragment_Tags* TagsFragment = ItemDefinition->FindFragment<FArcItemFragment_Tags>();
			if (TagsFragment == nullptr)
			{
				continue;
			}

			for (const TPair<FGameplayTag, FGameplayTag>& Mapping : ItemTagToCapabilityMap)
			{
				if (TagsFragment->ItemTags.HasTag(Mapping.Key))
				{
					ProvidesTags.AddTag(Mapping.Value);
				}
			}
		}

		if (ProvidesTags.IsEmpty())
		{
			continue;
		}

		FArcPotentialEntity PotentialEntity;
		PotentialEntity.EntityHandle = Info.Entity;
		PotentialEntity.Location = Info.Location;
		PotentialEntity.Provides = ProvidesTags;

		const FArcMassGoalPlanInfoSharedFragment* GoalPlanInfo = EntityManager.GetConstSharedFragmentDataPtr<FArcMassGoalPlanInfoSharedFragment>(Info.Entity);
		if (GoalPlanInfo != nullptr)
		{
			PotentialEntity.Provides.AppendTags(GoalPlanInfo->Provides);
			PotentialEntity.Requires = GoalPlanInfo->Requires;

			for (const TInstancedStruct<FArcSmartObjectPlanConditionEvaluator>& Cond : GoalPlanInfo->CustomConditions)
			{
				PotentialEntity.CustomConditions.Add(FConstStructView(Cond.GetScriptStruct(), Cond.GetMemory()));
			}
		}

		TArray<FSmartObjectSlotHandle> OutSlots;
		SOSubsystem->FindSlots(SmartObjectOwner->SmartObjectHandle, FSmartObjectRequestFilter(), OutSlots, {});

		for (int32 SlotIndex = 0; SlotIndex < OutSlots.Num(); SlotIndex++)
		{
			if (!SOSubsystem->CanBeClaimed(OutSlots[SlotIndex]))
			{
				continue;
			}

			if (PotentialEntity.FoundCandidateSlots.NumSlots >= 4)
			{
				PotentialEntity.FoundCandidateSlots.NumSlots = 4;
				break;
			}

			FSmartObjectCandidateSlot CandidateSlot;
			CandidateSlot.Result.SmartObjectHandle = SmartObjectOwner->SmartObjectHandle;
			CandidateSlot.Result.SlotHandle = OutSlots[SlotIndex];
			PotentialEntity.FoundCandidateSlots.Slots[PotentialEntity.FoundCandidateSlots.NumSlots] = CandidateSlot;
			PotentialEntity.FoundCandidateSlots.NumSlots++;
		}

		OutCandidates.Add(PotentialEntity);
	}
}
