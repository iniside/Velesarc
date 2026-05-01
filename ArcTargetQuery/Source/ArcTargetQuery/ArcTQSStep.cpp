// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep.h"

void FArcTQSStep::ExecuteStepBatch(
	TArray<FArcTQSTargetItem>& Items,
	int32 StartIndex,
	int32 EndIndex,
	const FArcTQSQueryContext& QueryContext) const
{
	EndIndex = FMath::Min(EndIndex, Items.Num());

	for (int32 i = StartIndex; i < EndIndex; ++i)
	{
		FArcTQSTargetItem& Item = Items[i];
		if (!Item.bValid)
		{
			continue;
		}

		const float RawScore = ExecuteStep(Item, QueryContext);

		if (StepType == EArcTQSStepType::Filter)
		{
			// Filter: RawScore <= 0 means discard
			if (RawScore <= 0.0f)
			{
				Item.bValid = false;
			}
		}
		else
		{
			// Score: compensatory multiplicative — Pow(clamp(raw, 0, 1), Weight)
			const float ClampedScore = FMath::Clamp(RawScore, 0.0f, 1.0f);
			const float CompensatedScore = FMath::Pow(ClampedScore, Weight);
			Item.Score *= CompensatedScore;
		}
	}
}
