// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcSmartObjectPlanConditionEvaluator.h"
#include "ArcPotentialEntity.h"

#include "ArcSmartObjectPlanSensor.generated.h"

struct FArcSmartObjectPlanRequest;

USTRUCT(BlueprintType)
struct ARCAI_API FArcSmartObjectPlanSensor
{
	GENERATED_BODY()

	virtual void GatherCandidates(
		const FArcSmartObjectPlanEvaluationContext& Context,
		const FArcSmartObjectPlanRequest& Request,
		TArray<FArcPotentialEntity>& OutCandidates) const {}

	virtual ~FArcSmartObjectPlanSensor() = default;
};
