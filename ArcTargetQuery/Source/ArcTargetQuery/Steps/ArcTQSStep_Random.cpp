// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Random.h"

float FArcTQSStep_Random::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	return static_cast<float>(QueryContext.RandomStream->FRandRange(MinScore, MaxScore));
}
