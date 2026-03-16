// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_KnowledgeRelevance.generated.h"

/**
 * Scores TQS items by knowledge entry relevance.
 * Returns the entry's Relevance value directly (already 0-1 normalized).
 */
USTRUCT(DisplayName = "Knowledge Relevance")
struct ARCKNOWLEDGE_API FArcTQSStep_KnowledgeRelevance : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_KnowledgeRelevance() { StepType = EArcTQSStepType::Score; }

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
