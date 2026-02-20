// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Random.h"

float FArcTQSStep_Random::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const float RawScore = FMath::FRandRange(MinScore, MaxScore);
	return ResponseCurve.Evaluate(RawScore);
}
