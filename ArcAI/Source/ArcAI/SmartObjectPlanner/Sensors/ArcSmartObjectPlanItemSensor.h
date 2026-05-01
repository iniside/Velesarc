// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SmartObjectPlanner/ArcSmartObjectPlanSensor.h"
#include "GameplayTagContainer.h"

#include "ArcSmartObjectPlanItemSensor.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Item Sensor"))
struct ARCAI_API FArcSmartObjectPlanItemSensor : public FArcSmartObjectPlanSensor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TMap<FGameplayTag, FGameplayTag> ItemTagToCapabilityMap;

	virtual void GatherCandidates(
		const FArcSmartObjectPlanEvaluationContext& Context,
		const FArcSmartObjectPlanRequest& Request,
		TArray<FArcPotentialEntity>& OutCandidates) const override;
};
