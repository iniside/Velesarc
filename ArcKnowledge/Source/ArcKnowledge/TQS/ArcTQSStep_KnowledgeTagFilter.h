// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_KnowledgeTagFilter.generated.h"

/**
 * Filters TQS items by knowledge entry tags.
 * Only passes items whose knowledge entry tags match the query.
 */
USTRUCT(DisplayName = "Knowledge Tag Filter")
struct ARCKNOWLEDGE_API FArcTQSStep_KnowledgeTagFilter : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_KnowledgeTagFilter() { StepType = EArcTQSStepType::Filter; }

	/** Tag query to match against the knowledge entry's tags. */
	UPROPERTY(EditAnywhere, Category = "Step")
	FGameplayTagQuery TagQuery;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
