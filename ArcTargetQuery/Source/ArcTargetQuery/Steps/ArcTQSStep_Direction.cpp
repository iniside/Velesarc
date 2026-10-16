// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Direction.h"

float FArcTQSStep_Direction::ExecuteStep(
	const FArcTQSTargetItem& Item,
	const FArcTQSQueryContext& QueryContext) const
{
	const FVector ToItem = (Item.GetLocation(QueryContext.EntityManager) - Origin).GetSafeNormal2D();
	if (ToItem.IsNearlyZero())
	{
		return 0.5f;
	}

	const FVector ToRef = (ReferenceLocation - Origin).GetSafeNormal2D();
	if (ToRef.IsNearlyZero())
	{
		return 0.5f;
	}

	const float Dot = FVector::DotProduct(ToRef, ToItem);
	return (Dot + 1.0f) * 0.5f;
}
