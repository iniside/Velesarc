// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanSpatialHashSensor.h"

#include "SmartObjectPlanner/ArcMassGoalPlanInfoSharedFragment.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanRequest.h"
#include "ArcMass/Spatial/ArcMassSpatialHashSubsystem.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"

void FArcSmartObjectPlanSpatialHashSensor::GatherCandidates(
	const FArcSmartObjectPlanEvaluationContext& Context,
	const FArcSmartObjectPlanRequest& Request,
	TArray<FArcPotentialEntity>& OutCandidates) const
{
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

	// Query spatial hash for nearby entities, filtering to only those with SmartObject + GoalPlanInfo
	TArray<FArcMassEntityInfo> NearbyEntities;
	NearbyEntities.Reserve(64);

	SpatialHash->GetSpatialHashGrid().QueryEntitiesInRadiusFiltered(
		Request.SearchOrigin,
		Request.SearchRadius,
		[&EntityManager](const FMassEntityHandle& Entity, const FVector& /*Location*/) -> bool
		{
			return EntityManager.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(Entity) != nullptr
				&& EntityManager.GetConstSharedFragmentDataPtr<FArcMassGoalPlanInfoSharedFragment>(Entity) != nullptr;
		},
		NearbyEntities);

	for (const FArcMassEntityInfo& Info : NearbyEntities)
	{
		const FArcSmartObjectOwnerFragment* SmartObjectOwner = EntityManager.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(Info.Entity);
		const FArcMassGoalPlanInfoSharedFragment* GoalPlanInfo = EntityManager.GetConstSharedFragmentDataPtr<FArcMassGoalPlanInfoSharedFragment>(Info.Entity);

		// Already filtered above, but guard anyway
		if (!SmartObjectOwner || !GoalPlanInfo)
		{
			continue;
		}

		FArcPotentialEntity PotentialEntity;
		PotentialEntity.EntityHandle = Info.Entity;
		PotentialEntity.Location = Info.Location;
		PotentialEntity.Provides = GoalPlanInfo->Provides;
		PotentialEntity.Requires = GoalPlanInfo->Requires;

		for (const TInstancedStruct<FArcSmartObjectPlanConditionEvaluator>& Cond : GoalPlanInfo->CustomConditions)
		{
			PotentialEntity.CustomConditions.Add(FConstStructView(Cond.GetScriptStruct(), Cond.GetMemory()));
		}

		TArray<FSmartObjectSlotHandle> OutSlots;
		SOSubsystem->FindSlots(SmartObjectOwner->SmartObjectHandle, FSmartObjectRequestFilter(), /*out*/ OutSlots, {});

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
