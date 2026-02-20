// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator.h"

void FArcTQSGenerator::GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const
{
	for (const FVector& Location : QueryContext.ContextLocations)
	{
		GenerateItemsAroundLocation(Location, QueryContext, OutItems);
	}
}
