// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SmartObjectPlanner/ArcSmartObjectPlanSensor.h"

#include "ArcSmartObjectPlanSpatialHashSensor.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Spatial Hash Sensor"))
struct ARCAI_API FArcSmartObjectPlanSpatialHashSensor : public FArcSmartObjectPlanSensor
{
	GENERATED_BODY()

	virtual void GatherCandidates(
		const FArcSmartObjectPlanEvaluationContext& Context,
		const FArcSmartObjectPlanRequest& Request,
		TArray<FArcPotentialEntity>& OutCandidates) const override;
};
