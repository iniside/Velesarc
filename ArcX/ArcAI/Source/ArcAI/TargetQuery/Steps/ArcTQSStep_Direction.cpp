// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Direction.h"
#include "TargetQuery/ArcTQSLocationProvider.h"

float FArcTQSStep_Direction::ExecuteStep(
	const FArcTQSTargetItem& Item,
	const FArcTQSQueryContext& QueryContext) const
{
	// Resolve reference location
	FVector RefLocation;
	if (bUseLocationProvider)
	{
		if (const FArcTQSLocationProvider* Provider = LocationProvider.GetPtr<FArcTQSLocationProvider>())
		{
			RefLocation = Provider->GetLocation(Item, QueryContext);
		}
		else
		{
			// No provider set — fall back to querier location (everything scores 0)
			RefLocation = QueryContext.QuerierLocation;
		}
	}
	else
	{
		RefLocation = ReferenceLocation;
	}

	const FVector Origin = QueryContext.QuerierLocation;
	const FVector ItemLocation = Item.GetLocation(QueryContext.EntityManager);

	// Direction from origin to reference
	const FVector ToRef = (RefLocation - Origin).GetSafeNormal2D();
	// Direction from origin to item
	const FVector ToItem = (ItemLocation - Origin).GetSafeNormal2D();

	// If either direction is degenerate, can't determine alignment
	if (ToRef.IsNearlyZero() || ToItem.IsNearlyZero())
	{
		return 0.5f;
	}

	// Dot product: 1.0 = same direction, -1.0 = opposite
	const float Dot = FVector::DotProduct(ToRef, ToItem);

	// Map [-1, 1] to [0, 1]
	const float NormalizedScore = (Dot + 1.0f) * 0.5f;

	return ResponseCurve.Evaluate(NormalizedScore);
}
