// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSStep_KnowledgePayloadTypeFilter.generated.h"

/**
 * Filters TQS items by knowledge entry payload struct type.
 * Only passes items whose payload is set and derives from the required type.
 */
USTRUCT(DisplayName = "Knowledge Payload Type Filter")
struct ARCKNOWLEDGE_API FArcTQSStep_KnowledgePayloadTypeFilter : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_KnowledgePayloadTypeFilter() { StepType = EArcTQSStepType::Filter; }

	/** The payload must be this struct type or a subtype. */
	UPROPERTY(EditAnywhere, Category = "Step")
	TObjectPtr<UScriptStruct> RequiredPayloadType;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
