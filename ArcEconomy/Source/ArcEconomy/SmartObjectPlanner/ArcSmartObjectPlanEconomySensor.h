// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SmartObjectPlanner/ArcSmartObjectPlanSensor.h"
#include "GameplayTagContainer.h"

#include "ArcSmartObjectPlanEconomySensor.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Economy Sensor"))
struct ARCECONOMY_API FArcSmartObjectPlanEconomySensor : public FArcSmartObjectPlanSensor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTagContainer EconomyQueryTags;

	virtual void GatherCandidates(
		const FArcSmartObjectPlanEvaluationContext& Context,
		const FArcSmartObjectPlanRequest& Request,
		TArray<FArcPotentialEntity>& OutCandidates) const override;
};
