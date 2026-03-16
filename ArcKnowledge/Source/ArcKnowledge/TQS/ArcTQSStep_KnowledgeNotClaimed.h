// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_KnowledgeNotClaimed.generated.h"

/**
 * Filters out claimed knowledge advertisements.
 * Only passes items whose knowledge entry is not yet claimed.
 */
USTRUCT(DisplayName = "Knowledge Not Claimed")
struct ARCKNOWLEDGE_API FArcTQSStep_KnowledgeNotClaimed : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_KnowledgeNotClaimed() { StepType = EArcTQSStepType::Filter; }

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
