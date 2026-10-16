// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_DirectionFromOrigin.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"

float FArcTQSStep_DirectionFromOrigin::ExecuteStep(
	const FArcTQSTargetItem& Item,
	const FArcTQSQueryContext& QueryContext) const
{
	const FMassEntityManager* EntityManager = QueryContext.EntityManager;

	// Neutral (not rejected) score whenever OriginEntity is unusable: an origin that
	// is a bare location (no entity) must stay harmless rather than penalizing every item.
	if (!OriginEntity.IsValid() || !EntityManager || !EntityManager->IsEntityActive(OriginEntity))
	{
		// The bound handle may be stale (e.g. the entity died since); IsEntityActive guards
		// against reading a freed/recycled entity's fragments.
		return 0.5f;
	}

	const FTransformFragment* TransformFrag = EntityManager->GetFragmentDataPtr<FTransformFragment>(OriginEntity);
	if (!TransformFrag)
	{
		return 0.5f;
	}

	const FVector OriginForward = TransformFrag->GetTransform().GetRotation().GetForwardVector().GetSafeNormal2D();
	const FVector ItemLocation = Item.GetLocation(EntityManager);
	const FVector OriginToItem = (ItemLocation - Origin).GetSafeNormal2D();

	if (OriginForward.IsNearlyZero() || OriginToItem.IsNearlyZero())
	{
		return 0.5f;
	}

	const float Dot = FVector::DotProduct(OriginForward, OriginToItem);
	return (Dot + 1.0f) * 0.5f;
}
