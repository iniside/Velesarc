// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator.h"

void FArcTQSGenerator::GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const
{
	for (int32 ContextIdx = 0; ContextIdx < QueryContext.ContextLocations.Num(); ++ContextIdx)
	{
		const int32 ItemsBefore = OutItems.Num();
		GenerateItemsAroundLocation(QueryContext.ContextLocations[ContextIdx], QueryContext, OutItems);

		// Stamp the context index on all newly generated items
		for (int32 i = ItemsBefore; i < OutItems.Num(); ++i)
		{
			OutItems[i].ContextIndex = ContextIdx;
		}
	}
}
