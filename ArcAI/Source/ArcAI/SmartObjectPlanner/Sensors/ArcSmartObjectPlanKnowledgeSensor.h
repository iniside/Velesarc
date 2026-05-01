// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SmartObjectPlanner/ArcSmartObjectPlanSensor.h"
#include "GameplayTagContainer.h"

#include "ArcSmartObjectPlanKnowledgeSensor.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Knowledge Sensor"))
struct ARCAI_API FArcSmartObjectPlanKnowledgeSensor : public FArcSmartObjectPlanSensor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTagContainer KnowledgeQueryTags;

	UPROPERTY(EditAnywhere, meta = (AllowAbstract = "true"))
	TObjectPtr<UScriptStruct> RequiredPayloadType = nullptr;

	virtual void GatherCandidates(
		const FArcSmartObjectPlanEvaluationContext& Context,
		const FArcSmartObjectPlanRequest& Request,
		TArray<FArcPotentialEntity>& OutCandidates) const override;
};
