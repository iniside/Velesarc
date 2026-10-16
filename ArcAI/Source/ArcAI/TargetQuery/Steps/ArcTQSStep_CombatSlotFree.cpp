// Copyright Lukasz Baran. All Rights Reserved.

#include "TargetQuery/Steps/ArcTQSStep_CombatSlotFree.h"

#include "Combat/ArcCombatSlots.h"
#include "Engine/World.h"
#include "Mass/EntityManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTQSStep_CombatSlotFree)

float FArcTQSStep_CombatSlotFree::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const FMassEntityManager* EntityManager = QueryContext.EntityManager;
	const UWorld* World = QueryContext.World.Get();
	if (!EntityManager || !World)
	{
		return 1.0f;
	}
	FVector Centre = FVector::ZeroVector;
	if (!ArcCombatSlots::GetTargetCentre(*EntityManager, SlotOwner, Centre))
	{
		return 1.0f;
	}
	const FVector ToItem = Item.GetLocation(EntityManager) - Centre;
	if (ToItem.IsNearlyZero())
	{
		return 1.0f;
	}
	const float YawDegrees = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(ToItem.Y, ToItem.X)));
	return ArcCombatSlots::IsAngleFree(
		*EntityManager, SlotOwner, YawDegrees, MinSeparationDegrees, Claimant, World->GetTimeSeconds())
		? 1.0f
		: 0.0f;
}
